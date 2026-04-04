#!/usr/bin/env python3
# Copyright (C) 2026 Tencent. All rights reserved.
"""
Validate integrity and replace branch ref atomically.

Usage:
    python replace_branch.py <params.json>

The params JSON must contain:
  - repo_dir: absolute path to the main repository
  - branch: current branch name
  - tmp_branch: name of the temporary branch
  - worktree_path: path to the temporary worktree
  - current_head: the branch HEAD known when the new history was last synced
  - upstream_remote: original upstream remote name (or null)
  - upstream_merge: original upstream merge ref (or null)

If new commits were added to the branch after current_head, the script
automatically appends them to the tmp branch using commit-tree (zero conflict
guarantee), then verifies tree equality before proceeding.

Pre-checks enforced before touching the original branch:
  1. Input: current_head must be a valid 40-char commit hash
  2. Ref validity: tmp branch HEAD must be a valid commit object
  3. Ancestry: current_head must be an ancestor of (or equal to) actual HEAD
  4. Auto-append: if actual HEAD ≠ current_head, append missing commits
  5. Tree diff: tmp branch HEAD tree must exactly match actual branch HEAD tree

Only after ALL checks pass does it atomically update the branch ref using
compare-and-swap (old-value = actual_head read during check). If the branch
HEAD changes between check and update-ref, the atomic swap fails safely and
the original branch is untouched.

Output on success (JSON to stdout):
  - status: "success"
  - old_head: previous branch HEAD
  - new_head: new branch HEAD
  - appended_commit_count: number of commits auto-appended
  - upstream_restored: whether upstream tracking was restored

On failure, exits with non-zero code. The original branch is never modified
unless all checks pass. If update-ref fails, an automatic rollback is attempted.
"""

import json
import os
import subprocess
import sys
import tempfile


def git(repo_dir, *args, env_override=None):
    """Run a git command in the specified directory. Returns stdout stripped."""
    cmd = ["git", "-C", repo_dir] + list(args)
    merged_env = os.environ.copy()
    if env_override:
        merged_env.update(env_override)
    result = subprocess.run(cmd, capture_output=True, text=True, env=merged_env)
    if result.returncode != 0:
        raise GitError(f"git {' '.join(args)}", result.stderr.strip())
    return result.stdout.strip()


def git_rc(repo_dir, *args):
    """Run a git command and return its exit code (no exception on failure)."""
    cmd = ["git", "-C", repo_dir] + list(args)
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode


class GitError(Exception):
    def __init__(self, command, stderr):
        self.command = command
        self.stderr = stderr
        super().__init__(f"Command failed: {command}\n{stderr}")


def write_message_file(message):
    """Write commit message to a temporary file. Returns the file path."""
    fd, path = tempfile.mkstemp(prefix="squash-msg-", suffix=".txt")
    with os.fdopen(fd, "w") as f:
        f.write(message)
    return path


def get_author_env(repo_dir, commit_hash):
    """Extract original author info from a commit for use with commit-tree."""
    raw = git(repo_dir, "log", "-1", "--format=%aN%x00%aE%x00%aI", commit_hash)
    parts = raw.split("\x00")
    return {
        "GIT_AUTHOR_NAME": parts[0],
        "GIT_AUTHOR_EMAIL": parts[1],
        "GIT_AUTHOR_DATE": parts[2],
    }


def append_extra_commits(repo_dir, tmp_ref, base_commit, end_commit):
    """Append commits in (base_commit..end_commit] to tmp branch HEAD.

    Uses commit-tree to replay each commit's tree snapshot. Zero conflicts.
    Updates the tmp branch ref directly (no worktree needed) with
    compare-and-swap to detect concurrent modifications.

    Collects ALL commits in the range (no --first-parent) to avoid losing
    sideline histories from merge commits.

    Returns the number of commits appended.
    """
    old_tip = git(repo_dir, "rev-parse", tmp_ref)
    commits_raw = git(repo_dir, "log", "--format=%H",
                      f"{base_commit}..{end_commit}")
    if not commits_raw:
        return 0

    commit_hashes = [h for h in commits_raw.split("\n") if h]
    commit_hashes.reverse()

    current_tip = old_tip
    for commit_hash in commit_hashes:
        tree = git(repo_dir, "rev-parse", f"{commit_hash}^{{tree}}")
        message = git(repo_dir, "log", "-1", "--format=%B", commit_hash)
        author_env = get_author_env(repo_dir, commit_hash)
        msg_file = write_message_file(message)
        try:
            current_tip = git(
                repo_dir,
                "commit-tree", tree,
                "-p", current_tip,
                "-F", msg_file,
                env_override=author_env,
            )
        finally:
            os.unlink(msg_file)

    git(repo_dir, "update-ref", tmp_ref, current_tip, old_tip)
    return len(commit_hashes)


def main():
    if len(sys.argv) != 2:
        print("Usage: python replace_branch.py <params.json>",
              file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1]) as f:
        params = json.load(f)

    repo_dir = params["repo_dir"]
    branch = params["branch"]
    tmp_branch = params["tmp_branch"]
    worktree_path = params["worktree_path"]
    current_head = params["current_head"]
    upstream_remote = params.get("upstream_remote")
    upstream_merge = params.get("upstream_merge")

    # Validate that the branch exists (prevent operating on a wrong branch
    # if replace.json was manually modified).
    if git_rc(repo_dir, "rev-parse", "--verify", f"refs/heads/{branch}") != 0:
        print(f"ERROR: Branch '{branch}' not found in repository.",
              file=sys.stderr)
        sys.exit(1)

    branch_ref = f"refs/heads/{branch}"
    tmp_ref = f"refs/heads/{tmp_branch}"

    # ======================================================================
    # PRE-CHECK 0: Validate current_head is a full 40-char commit hash
    # ======================================================================
    if not current_head or len(current_head) != 40:
        print(
            f"ERROR: current_head must be a full 40-character hash, "
            f"got '{current_head}'.",
            file=sys.stderr,
        )
        sys.exit(1)

    obj_type_cur = git(repo_dir, "cat-file", "-t", current_head)
    if obj_type_cur != "commit":
        print(
            f"ERROR: current_head {current_head[:7]} is {obj_type_cur}, "
            f"expected commit.",
            file=sys.stderr,
        )
        sys.exit(1)

    # ======================================================================
    # PRE-CHECK 1: Verify tmp branch HEAD exists and is a valid commit
    # ======================================================================
    try:
        new_head = git(repo_dir, "rev-parse", tmp_ref)
    except GitError:
        print(
            f"ERROR: Temporary branch '{tmp_branch}' not found.\n"
            f"It may have been deleted prematurely.",
            file=sys.stderr,
        )
        sys.exit(1)

    obj_type = git(repo_dir, "cat-file", "-t", new_head)
    if obj_type != "commit":
        print(
            f"ERROR: {tmp_ref} points to {obj_type}, expected commit.",
            file=sys.stderr,
        )
        sys.exit(1)

    # ======================================================================
    # PRE-CHECK 2: Read actual branch HEAD and validate relationship
    # ======================================================================
    actual_head = git(repo_dir, "rev-parse", branch_ref)

    # current_head must be an ancestor of (or equal to) actual_head.
    # If not, the branch was rebased/reset/amended — unsafe to proceed.
    if actual_head != current_head:
        rc = git_rc(repo_dir, "merge-base", "--is-ancestor",
                     current_head, actual_head)
        if rc != 0:
            print(
                f"ABORT: Branch '{branch}' has been rewritten "
                f"(rebase/reset/amend).\n"
                f"  Expected ancestor: {current_head[:7]}\n"
                f"  Actual HEAD:       {actual_head[:7]}\n"
                f"\n"
                f"Cannot safely proceed. The squash session is invalid.\n"
                f"Temporary worktree preserved at: {worktree_path}\n"
                f"Temporary branch preserved: {tmp_branch}",
                file=sys.stderr,
            )
            sys.exit(1)

    # ======================================================================
    # AUTO-APPEND: If new commits exist after current_head, append them
    # ======================================================================
    appended_count = 0

    if actual_head != current_head:
        try:
            appended_count = append_extra_commits(
                repo_dir, tmp_ref, current_head, actual_head
            )
        except GitError as e:
            print(
                f"ERROR: Failed to append new commits to tmp branch.\n"
                f"  Range: {current_head[:7]}..{actual_head[:7]}\n"
                f"  {e.stderr}\n"
                f"Original branch is untouched.\n"
                f"Temporary worktree preserved at: {worktree_path}\n"
                f"Temporary branch preserved: {tmp_branch}",
                file=sys.stderr,
            )
            sys.exit(1)

        # Re-read tmp branch HEAD after appending.
        new_head = git(repo_dir, "rev-parse", tmp_ref)

    # ======================================================================
    # PRE-CHECK 3: Tree diff — tmp branch tree must exactly match actual HEAD
    # This is the CRITICAL safety check. Guarantees the replacement preserves
    # every byte of the working tree content.
    # ======================================================================
    actual_tree = git(repo_dir, "rev-parse", f"{actual_head}^{{tree}}")
    new_tree = git(repo_dir, "rev-parse", f"{new_head}^{{tree}}")

    if actual_tree != new_tree:
        print(
            f"ABORT: Tree mismatch — cannot safely replace branch.\n"
            f"  Branch HEAD ({actual_head[:7]}) tree: {actual_tree}\n"
            f"  New HEAD ({new_head[:7]}) tree:       {new_tree}\n"
            f"\n"
            f"The file contents would change after replacement.\n"
            f"Temporary worktree preserved at: {worktree_path}\n"
            f"Temporary branch preserved: {tmp_branch}",
            file=sys.stderr,
        )
        sys.exit(1)

    # ======================================================================
    # PRE-CHECK 4: Re-verify branch HEAD hasn't changed since we read it
    # If someone pushed between our read and now, the swap must fail safely.
    # ======================================================================
    recheck_head = git(repo_dir, "rev-parse", branch_ref)
    if recheck_head != actual_head:
        print(
            f"ABORT: Branch '{branch}' HEAD changed during replace.\n"
            f"  Read:    {actual_head[:7]}\n"
            f"  Current: {recheck_head[:7]}\n"
            f"\n"
            f"Please retry. Original branch is untouched.\n"
            f"Temporary worktree preserved at: {worktree_path}\n"
            f"Temporary branch preserved: {tmp_branch}",
            file=sys.stderr,
        )
        sys.exit(1)

    # ======================================================================
    # All checks passed — proceed with atomic ref update
    # ======================================================================

    # Step 1: Atomically update the branch ref FIRST, before removing
    # the worktree — so if update-ref fails, the worktree is still
    # available for investigation.
    try:
        git(repo_dir, "update-ref", branch_ref, new_head, actual_head)
    except GitError as e:
        print(
            f"ERROR: Atomic ref update failed (race condition).\n"
            f"  {e.stderr}\n"
            f"Original branch is untouched.\n"
            f"Temporary worktree preserved at: {worktree_path}\n"
            f"Temporary branch '{tmp_branch}' preserved for manual recovery.",
            file=sys.stderr,
        )
        sys.exit(1)

    # Step 2: Remove the temporary worktree (branch still exists)
    try:
        git(repo_dir, "worktree", "remove", "--force", worktree_path)
    except GitError as e:
        print(
            f"WARNING: Failed to remove worktree: {e.stderr}\n"
            f"Ref update succeeded. Worktree can be cleaned up manually.",
            file=sys.stderr,
        )

    # Step 3: Verify the update succeeded
    verify_head = git(repo_dir, "rev-parse", branch_ref)
    if verify_head != new_head:
        # This should never happen, but if it does, attempt rollback
        print(
            f"ERROR: Ref update verification failed.\n"
            f"  Expected: {new_head[:7]}\n"
            f"  Actual:   {verify_head[:7]}\n"
            f"Attempting rollback...",
            file=sys.stderr,
        )
        try:
            git(repo_dir, "update-ref", branch_ref, actual_head)
            print("Rollback successful. Original branch restored.",
                  file=sys.stderr)
        except GitError:
            print(
                f"CRITICAL: Rollback failed! Manual recovery needed.\n"
                f"  Original HEAD was: {actual_head}\n"
                f"  Run: git update-ref {branch_ref} {actual_head}",
                file=sys.stderr,
            )
        sys.exit(1)

    # Step 4: Delete the temporary branch (worktree is already removed)
    try:
        git(repo_dir, "branch", "-D", tmp_branch)
    except GitError:
        pass  # Non-critical, branch will be cleaned up eventually

    # Step 5: Restore upstream tracking if it existed
    upstream_restored = False
    if upstream_remote and upstream_merge:
        upstream_branch = upstream_merge.replace("refs/heads/", "")
        upstream_full = f"{upstream_remote}/{upstream_branch}"
        try:
            git(repo_dir, "branch", f"--set-upstream-to={upstream_full}",
                branch)
            upstream_restored = True
        except GitError:
            pass  # Non-critical

    # --- Output result ---
    result = {
        "status": "success",
        "old_head": actual_head,
        "new_head": new_head,
        "appended_commit_count": appended_count,
        "upstream_restored": upstream_restored,
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
