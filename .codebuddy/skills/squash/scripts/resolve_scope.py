#!/usr/bin/env python3
# Copyright (C) 2026 Tencent. All rights reserved.
"""
Clean up stale resources from previous squash runs.

Usage:
    python resolve_scope.py <repo_dir>

Cleans up stale worktrees, branches, and session directories.
Does not perform branch state detection (handled implicitly by collect_commits.py).
"""

import json
import os
import shutil
import subprocess
import sys


TMP_PREFIX = "squash-tmp-"
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


def cleanup_stale(repo_dir):
    """Remove all leftover squash worktrees, branches, and session dirs.

    Only touches worktrees whose branch name starts with squash-tmp-,
    and temp directories starting with squash-session-.
    """
    # Pass 1: remove worktrees on a squash-tmp- branch.
    code, raw, _ = git(repo_dir, "worktree", "list", "--porcelain")
    if code != 0:
        return
    current_path = None
    to_remove = []
    for line in raw.split("\n"):
        if line.startswith("worktree "):
            current_path = line[len("worktree "):]
        elif line.startswith("branch refs/heads/") and current_path:
            branch_name = line[len("branch refs/heads/"):]
            if branch_name.startswith(TMP_PREFIX):
                to_remove.append((current_path, branch_name))
        elif line == "":
            current_path = None
    for path, branch_name in to_remove:
        git(repo_dir, "worktree", "remove", "--force", path)
        git_ok(repo_dir, "branch", "-D", branch_name)

    # Pass 2: remove orphaned squash-tmp- branches.
    code, branches_raw, _ = git(
        repo_dir, "branch", "--list", f"{TMP_PREFIX}*",
        "--format=%(refname:short)",
    )
    if code == 0 and branches_raw:
        for name in branches_raw.split("\n"):
            name = name.strip()
            if name:
                git_ok(repo_dir, "branch", "-D", name)

    # Pass 3: remove stale squash-session- temp directories.
    git(repo_dir, "worktree", "prune")
    try:
        for entry in os.listdir("/tmp"):
            if entry.startswith(SESSION_PREFIX):
                full_path = os.path.join("/tmp", entry)
                if os.path.isdir(full_path):
                    shutil.rmtree(full_path, ignore_errors=True)
    except OSError:
        pass


def main():
    if len(sys.argv) != 2:
        print("Usage: python resolve_scope.py <repo_dir>", file=sys.stderr)
        sys.exit(1)

    repo_dir = os.path.abspath(sys.argv[1])

    # Verify we're on a branch (not detached HEAD).
    branch = git_ok(repo_dir, "rev-parse", "--abbrev-ref", "HEAD")
    if not branch or branch == "HEAD":
        print("ERROR: Not on a branch (detached HEAD).", file=sys.stderr)
        sys.exit(1)

    # Detect default branch.
    default_branch = "main"
    for candidate in ("main", "master"):
        if git_ok(repo_dir, "rev-parse", "--verify", f"refs/heads/{candidate}"):
            default_branch = candidate
            break

    # Refuse to squash the default branch.
    if branch == default_branch:
        print(f"ERROR: Cannot squash the default branch ({default_branch}).",
              file=sys.stderr)
        sys.exit(1)

    # Clean up stale resources.
    cleanup_stale(repo_dir)

    result = {
        "status": "ok",
        "repo_dir": repo_dir,
        "branch": branch,
        "default_branch": default_branch,
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

