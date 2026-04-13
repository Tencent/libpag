#!/usr/bin/env python3
# Copyright (C) 2026 Tencent. All rights reserved.
"""
Collect commits for squashing, compute file overlaps, and flag messages.

Usage:
    python collect_commits.py <repo_dir> [branch_name]

  - No branch_name: squash unpushed commits on the current branch
                    (tracking..HEAD, falls back to merge-base if no tracking).
  - branch_name:    squash the entire named branch (merge-base..branch HEAD).
                    The named branch does not need to be checked out.

Creates a unique session directory under /tmp and outputs a JSON object to
stdout with all data needed for AI to make grouping decisions.
"""

import json
import os
import re
import subprocess
import sys
import tempfile


SESSION_PREFIX = "squash-session-"


def git(repo_dir, *args):
    """Run a git command. Returns (returncode, stdout, stderr)."""
    cmd = ["git", "-C", repo_dir] + list(args)
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode, result.stdout.strip(), result.stderr.strip()


def git_ok(repo_dir, *args):
    """Run a git command. Returns stdout if success, None if failure."""
    code, out, _ = git(repo_dir, *args)
    return out if code == 0 else None


def resolve_range_unpushed(repo_dir, branch, default_branch):
    """Resolve squash range for unpushed commits on the given branch."""
    head = git_ok(repo_dir, "rev-parse", f"refs/heads/{branch}")
    if not head:
        print(f"ERROR: Cannot resolve HEAD for branch '{branch}'.",
              file=sys.stderr)
        sys.exit(1)
    merge_base = git_ok(repo_dir, "merge-base", branch, default_branch)

    remote = git_ok(repo_dir, "config", f"branch.{branch}.remote")
    merge_ref = git_ok(repo_dir, "config", f"branch.{branch}.merge")
    if remote and merge_ref:
        remote_branch = merge_ref.replace("refs/heads/", "")
        tracking = git_ok(repo_dir, "rev-parse", f"{remote}/{remote_branch}")
        if tracking:
            if tracking == head:
                print(
                    f"ERROR: No unpushed commits on '{branch}' "
                    f"— all commits are already pushed.",
                    file=sys.stderr,
                )
                sys.exit(1)
            return tracking, head

    # Fallback: no tracking info, use merge-base.
    if not merge_base:
        return None, None
    return merge_base, head


def resolve_range_branch(repo_dir, branch, default_branch):
    """Resolve squash range for the entire named branch."""
    target_head = git_ok(repo_dir, "rev-parse", f"refs/heads/{branch}")
    if not target_head:
        print(f"ERROR: Cannot resolve HEAD for branch '{branch}'.",
              file=sys.stderr)
        sys.exit(1)
    target_base = git_ok(repo_dir, "merge-base", branch, default_branch)
    if not target_base:
        print(f"ERROR: Cannot find merge-base for branch '{branch}'.",
              file=sys.stderr)
        sys.exit(1)
    return target_base, target_head


def collect_commits(repo_dir, squash_base, squash_end):
    """Collect all commits in the range with metadata and file lists."""
    sep = "\x01"
    code, meta_out, err = git(
        repo_dir, "log", "--first-parent",
        f"--format=%H{sep}%P{sep}%s",
        f"{squash_base}..{squash_end}",
    )
    if code != 0:
        print(f"ERROR: git log failed: {err}", file=sys.stderr)
        sys.exit(1)
    if not meta_out:
        return []

    commits = []
    for line in meta_out.split("\n"):
        line = line.strip()
        if not line:
            continue
        parts = line.split(sep, 2)
        if len(parts) < 3:
            continue
        full_hash, parents_str, subject = parts
        parents = parents_str.split() if parents_str else []
        commits.append({
            "hash": full_hash,
            "short": full_hash[:7],
            "parents": parents,
            "subject": subject,
            "is_merge": len(parents) > 1,
            "files": [],
        })

    for c in commits:
        # Try normal diff against parent first.
        code, diff_out, _ = git(
            repo_dir, "diff-tree", "-r", "--name-only", "--no-commit-id",
            "-z", f"{c['hash']}~1", c["hash"],
        )
        if code == 0 and diff_out:
            c["files"] = [f for f in diff_out.split("\x00") if f]
        elif code != 0:
            # Only use --root for actual root commits (no parents).
            if not c["parents"]:
                code2, diff_out2, _ = git(
                    repo_dir, "diff-tree", "-r", "--name-only",
                    "--no-commit-id", "-z", "--root", c["hash"],
                )
                if code2 == 0 and diff_out2:
                    c["files"] = [f for f in diff_out2.split("\x00") if f]
            # For non-root commits, leave files as [] on failure rather than
            # falling back to --root (which would return all files in the tree).

    commits.reverse()
    return commits


def compute_overlaps(commits):
    """Compute file overlap ratios between adjacent commits."""
    overlaps = []
    for i in range(len(commits) - 1):
        files_a = set(commits[i]["files"])
        files_b = set(commits[i + 1]["files"])
        shared = files_a & files_b
        min_size = min(len(files_a), len(files_b))
        ratio = len(shared) / min_size if min_size > 0 else 0.0
        overlaps.append({
            "from_idx": i,
            "to_idx": i + 1,
            "overlap": round(ratio, 2),
        })
    return overlaps


FOLLOWUP_PATTERNS = [
    re.compile(r"^wip\b", re.IGNORECASE),
    re.compile(r"^fix\b(?!.*\b(?:bug|crash|issue|error|regression)\b)",
               re.IGNORECASE),
    re.compile(r"^update\b(?!.*\b(?:to|from|version|dependency)\b)",
               re.IGNORECASE),
    re.compile(r"^tweak\b", re.IGNORECASE),
    re.compile(r"^cleanup\b", re.IGNORECASE),
    re.compile(r"^clean up\b", re.IGNORECASE),
    re.compile(r"^temp\b", re.IGNORECASE),
    re.compile(r"^todo\b", re.IGNORECASE),
    re.compile(r"^minor\b", re.IGNORECASE),
    re.compile(r"^typo\b", re.IGNORECASE),
    re.compile(r"^nit\b", re.IGNORECASE),
    re.compile(r"^address review", re.IGNORECASE),
    re.compile(r"^review feedback", re.IGNORECASE),
]

REVERT_PATTERN = re.compile(r'^Revert "', re.IGNORECASE)


def annotate_commits(commits):
    """Add quality_flag to each commit in-place."""
    for c in commits:
        subject = c["subject"].strip().rstrip(".")
        if REVERT_PATTERN.search(subject):
            c["quality_flag"] = "revert"
        elif len(subject) < 5:
            c["quality_flag"] = "too_short"
        else:
            c["quality_flag"] = None
            for pattern in FOLLOWUP_PATTERNS:
                if pattern.search(subject):
                    c["quality_flag"] = "vague"
                    break


def format_commit_table(commits):
    """Format the commit table as a displayable string."""
    lines = [" #  | Hash    | Subject"]
    for idx, c in enumerate(commits):
        num = idx + 1
        merge_tag = " [merge]" if c["is_merge"] else ""
        lines.append(f"{num:>3} | {c['short']}{merge_tag} | {c['subject']}")
    return "\n".join(lines)


def main():
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        print("Usage: python collect_commits.py <repo_dir> [branch_name]",
              file=sys.stderr)
        sys.exit(1)

    repo_dir = os.path.abspath(sys.argv[1])
    branch_arg = sys.argv[2] if len(sys.argv) == 3 else None

    current_branch = git_ok(repo_dir, "rev-parse", "--abbrev-ref", "HEAD")
    default_branch = "main"
    for candidate in ("main", "master"):
        if git_ok(repo_dir, "rev-parse", "--verify",
                  f"refs/heads/{candidate}"):
            default_branch = candidate
            break

    if branch_arg:
        # Explicit branch name: validate it exists, then squash entire branch.
        if not git_ok(repo_dir, "rev-parse", "--verify",
                      f"refs/heads/{branch_arg}"):
            print(f"ERROR: Branch '{branch_arg}' not found.", file=sys.stderr)
            sys.exit(1)
        if branch_arg == default_branch:
            print(
                f"ERROR: Cannot squash the default branch ({default_branch}).",
                file=sys.stderr,
            )
            sys.exit(1)
        branch = branch_arg
        squash_base, squash_end = resolve_range_branch(repo_dir, branch,
                                                       default_branch)
    else:
        # No argument: squash unpushed commits on the current branch.
        branch = current_branch
        squash_base, squash_end = resolve_range_unpushed(repo_dir, branch,
                                                         default_branch)

    upstream_remote = git_ok(repo_dir, "config", f"branch.{branch}.remote")
    upstream_merge = git_ok(repo_dir, "config", f"branch.{branch}.merge")
    if not squash_base or not squash_end:
        print(
            f"ERROR: Cannot determine squash range.\n"
            f"Hint: Branch '{branch}' may not share a common ancestor with "
            f"'{default_branch}'.",
            file=sys.stderr,
        )
        sys.exit(1)

    commits = collect_commits(repo_dir, squash_base, squash_end)
    if not commits:
        print("ERROR: No commits in the specified range.", file=sys.stderr)
        sys.exit(1)

    overlaps = compute_overlaps(commits)
    annotate_commits(commits)
    commit_table = format_commit_table(commits)

    session_dir = tempfile.mkdtemp(prefix=SESSION_PREFIX, dir="/tmp")
    result = {
        "session_dir": session_dir,
        "repo_dir": repo_dir,
        "branch": branch,
        "default_branch": default_branch,
        "squash_base": squash_base,
        "squash_end": squash_end,
        "upstream_remote": upstream_remote,
        "upstream_merge": upstream_merge,
        "commit_table": commit_table,
        "commits": commits,
        "overlaps": overlaps,
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
