#!/usr/bin/env python3
# Copyright (C) 2026 Tencent. All rights reserved.
"""
Collect commits for squashing, compute file overlaps, and flag messages.

Usage:
    python collect_commits.py <repo_dir> <scope> [range_argument]

scope must be one of:
  - "unpushed"  — squash only unpushed commits (tracking..HEAD)
  - "branch"    — squash entire branch (merge-base..HEAD)
  - a commit range like "abc123...def456" or a branch name or commit hash

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


def resolve_range(repo_dir, scope, branch, default_branch):
    """Resolve squash_base and squash_end from the given scope."""
    head = git_ok(repo_dir, "rev-parse", "HEAD")
    merge_base = git_ok(repo_dir, "merge-base", branch, default_branch)

    if scope == "branch":
        if not merge_base:
            return None, None
        return merge_base, head

    if scope == "unpushed":
        remote = git_ok(repo_dir, "config", f"branch.{branch}.remote")
        merge_ref = git_ok(repo_dir, "config", f"branch.{branch}.merge")
        if remote and merge_ref:
            remote_branch = merge_ref.replace("refs/heads/", "")
            tracking = git_ok(repo_dir, "rev-parse",
                              f"{remote}/{remote_branch}")
            if tracking:
                return tracking, head
        # Fallback: no tracking info, use merge-base.
        if not merge_base:
            return None, None
        return merge_base, head

    # Custom range: "a...b" syntax.
    if "..." in scope:
        parts = scope.split("...")
        if len(parts) == 2:
            a_hash = git_ok(repo_dir, "rev-parse", parts[0])
            b_hash = git_ok(repo_dir, "rev-parse", parts[1])
            if not a_hash or not b_hash:
                print(f"ERROR: Cannot resolve range '{scope}'.",
                      file=sys.stderr)
                sys.exit(1)
            code_ab, _, _ = git(repo_dir, "merge-base", "--is-ancestor",
                                a_hash, b_hash)
            if code_ab == 0:
                older, newer = a_hash, b_hash
            else:
                code_ba, _, _ = git(repo_dir, "merge-base", "--is-ancestor",
                                    b_hash, a_hash)
                if code_ba == 0:
                    older, newer = b_hash, a_hash
                else:
                    print(
                        f"ERROR: '{parts[0]}' and '{parts[1]}' have no "
                        f"ancestor relationship.",
                        file=sys.stderr,
                    )
                    sys.exit(1)
            older_parent = git_ok(repo_dir, "rev-parse",
                                  f"{older}~1") or older
            return older_parent, newer

    # Branch name.
    if git_ok(repo_dir, "rev-parse", "--verify", f"refs/heads/{scope}"):
        target_head = git_ok(repo_dir, "rev-parse", f"refs/heads/{scope}")
        target_base = git_ok(repo_dir, "merge-base", scope, default_branch)
        if not target_base:
            print(f"ERROR: Cannot find merge-base for branch '{scope}'.",
                  file=sys.stderr)
            sys.exit(1)
        return target_base, target_head

    # Single commit hash.
    if git_ok(repo_dir, "cat-file", "-t", scope) == "commit":
        commit_hash = git_ok(repo_dir, "rev-parse", scope)
        parent = git_ok(repo_dir, "rev-parse", f"{commit_hash}~1") or commit_hash
        return parent, head

    print(f"ERROR: Cannot resolve scope '{scope}'.", file=sys.stderr)
    sys.exit(1)


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
        code, diff_out, _ = git(
            repo_dir, "diff-tree", "-r", "--name-only", "--no-commit-id",
            "-z", f"{c['hash']}~1", c["hash"],
        )
        if code == 0 and diff_out:
            c["files"] = [f for f in diff_out.split("\x00") if f]
        elif code != 0:
            code2, diff_out2, _ = git(
                repo_dir, "diff-tree", "-r", "--name-only", "--no-commit-id",
                "-z", "--root", c["hash"],
            )
            if code2 == 0 and diff_out2:
                c["files"] = [f for f in diff_out2.split("\x00") if f]

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

REVERT_PATTERN = re.compile(r"^Revert\s+\"", re.IGNORECASE)


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
    if len(sys.argv) < 3:
        print("Usage: python collect_commits.py <repo_dir> <scope>",
              file=sys.stderr)
        sys.exit(1)

    repo_dir = os.path.abspath(sys.argv[1])
    scope = sys.argv[2]

    branch = git_ok(repo_dir, "rev-parse", "--abbrev-ref", "HEAD")
    default_branch = "main"
    for candidate in ("main", "master"):
        if git_ok(repo_dir, "rev-parse", "--verify",
                  f"refs/heads/{candidate}"):
            default_branch = candidate
            break

    upstream_remote = git_ok(repo_dir, "config", f"branch.{branch}.remote")
    upstream_merge = git_ok(repo_dir, "config", f"branch.{branch}.merge")

    squash_base, squash_end = resolve_range(repo_dir, scope, branch,
                                            default_branch)
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
