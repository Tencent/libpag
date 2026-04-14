#!/usr/bin/env python3
# Copyright (C) 2026 Tencent. All rights reserved.
"""
Build new commit history in a temporary worktree using git commit-tree.

Usage:
    python build_history.py <decision_table.json>

The decision table JSON must contain:
  - session_dir: path to the session temp directory
  - repo_dir: absolute path to the main repository
  - branch: current branch name
  - squash_base: base commit hash (exclusive, new history starts here)
  - squash_end: end commit hash (inclusive, original range endpoint)
  - upstream_remote: original upstream remote name (or null)
  - upstream_merge: original upstream merge ref (or null)
  - actions: ordered list of actions to process

Each action has:
  - type: "keep" | "reword" | "squash" | "merge"
  - commits: list of full commit hashes
  - message: new commit message (required for reword/squash, optional for keep/merge)

After building the squash history, the script automatically detects whether
new commits were added to the branch after squash_end. If so, it appends
them to the new history using commit-tree (zero conflict guarantee), and
verifies the final tree matches the branch HEAD tree.

Output on success (JSON to stdout):
  - status: "success"
  - worktree_path: path to the temporary worktree
  - tmp_branch: name of the temporary branch
  - final_head: HEAD hash of the complete new history (squash + appended)
  - current_head: the branch HEAD at time of execution (for replace_branch)
  - original_commit_count: number of commits processed by squash actions
  - new_commit_count: number of commits produced by squash actions
  - skipped_commit_count: number of commits in zero-diff groups that were dropped
  - appended_commit_count: number of post-squash_end commits auto-appended

On failure, exits with non-zero code and prints error to stderr.
The temporary worktree is preserved for investigation on failure.
"""

import json
import os
import subprocess
import sys
import tempfile


TMP_PREFIX = "squash-tmp-"


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


class GitError(Exception):
    def __init__(self, command, stderr):
        self.command = command
        self.stderr = stderr
        super().__init__(f"Command failed: {command}\n{stderr}")


def get_author_env(repo_dir, commit_hash):
    """Extract original author info from a commit for use with commit-tree."""
    raw = git(repo_dir, "log", "-1", "--format=%aN%x00%aE%x00%aI", commit_hash)
    parts = raw.split("\x00")
    return {
        "GIT_AUTHOR_NAME": parts[0],
        "GIT_AUTHOR_EMAIL": parts[1],
        "GIT_AUTHOR_DATE": parts[2],
    }


def get_parents(repo_dir, commit_hash):
    """Get parent hashes of a commit."""
    try:
        raw = git(repo_dir, "rev-parse", f"{commit_hash}^@")
    except GitError:
        return []
    if not raw:
        return []
    return raw.split("\n")


def write_message_file(message):
    """Write commit message to a temporary file. Returns the file path."""
    fd, path = tempfile.mkstemp(prefix="squash-msg-", suffix=".txt")
    with os.fdopen(fd, "w") as f:
        f.write(message)
    return path


def commit_tree(worktree_path, tree_hash, parent_args, message, author_env):
    """Create a new commit object using git commit-tree and update HEAD."""
    msg_file = write_message_file(message)
    try:
        new_hash = git(
            worktree_path,
            "commit-tree", tree_hash,
            *parent_args,
            "-F", msg_file,
            env_override=author_env,
        )
    finally:
        os.unlink(msg_file)
    git(worktree_path, "update-ref", "HEAD", new_hash)
    return new_hash


def validate_decision_table(decision):
    """Validate the decision table for safety constraints."""
    actions = decision["actions"]
    all_commits = []
    for action in actions:
        all_commits.extend(action["commits"])

    # Check for duplicate commits across actions.
    seen = set()
    for c in all_commits:
        if c in seen:
            raise ValueError(f"Commit {c[:7]} appears in multiple actions")
        seen.add(c)

    repo_dir = decision["repo_dir"]

    for i, action in enumerate(actions):
        action_type = action["type"]
        commits = action["commits"]

        if not commits:
            raise ValueError(f"Action #{i + 1} has no commits")

        if action_type in ("keep", "reword", "merge") and len(commits) != 1:
            raise ValueError(
                f"Action #{i + 1} ({action_type}) must have exactly 1 commit, "
                f"got {len(commits)}"
            )

        if action_type in ("squash", "reword") and not action.get("message"):
            raise ValueError(
                f"Action #{i + 1} ({action_type}) requires a 'message' field"
            )

        if action_type == "squash":
            for c in commits:
                parents = get_parents(repo_dir, c)
                if len(parents) > 1:
                    raise ValueError(
                        f"Cannot squash merge commit {c[:7]} in action #{i + 1}"
                    )

        if action_type == "squash" and len(commits) > 1:
            first_idx = all_commits.index(commits[0])
            last_idx = all_commits.index(commits[-1])
            for j in range(first_idx, last_idx + 1):
                between_commit = all_commits[j]
                if between_commit not in commits:
                    parents = get_parents(repo_dir, between_commit)
                    if len(parents) > 1:
                        raise ValueError(
                            f"Squash group in action #{i + 1} spans across "
                            f"merge commit {between_commit[:7]}"
                        )


def process_keep(worktree_path, repo_dir, action):
    """Process a keep action: preserve commit with original tree and message."""
    commit = action["commits"][0]
    tree = git(repo_dir, "rev-parse", f"{commit}^{{tree}}")
    message = action.get("message") or git(
        repo_dir, "log", "-1", "--format=%B", commit
    )
    author_env = get_author_env(repo_dir, commit)
    head = git(worktree_path, "rev-parse", "HEAD")
    return commit_tree(worktree_path, tree, ["-p", head], message, author_env)


def process_reword(worktree_path, repo_dir, action):
    """Process a reword action: use original tree but new message."""
    commit = action["commits"][0]
    tree = git(repo_dir, "rev-parse", f"{commit}^{{tree}}")
    message = action["message"]
    author_env = get_author_env(repo_dir, commit)
    head = git(worktree_path, "rev-parse", "HEAD")
    return commit_tree(worktree_path, tree, ["-p", head], message, author_env)


def process_squash(worktree_path, repo_dir, action):
    """Process a squash action: use the last commit's tree with new message.

    If the group's net diff is empty (tree unchanged from HEAD), the group is
    skipped entirely (e.g. a commit and its revert cancelling each other out).
    Returns the new commit hash, or None if skipped.
    """
    commits = action["commits"]
    last_commit = commits[-1]
    first_commit = commits[0]
    tree = git(repo_dir, "rev-parse", f"{last_commit}^{{tree}}")
    head = git(worktree_path, "rev-parse", "HEAD")
    head_tree = git(worktree_path, "rev-parse", "HEAD^{tree}")
    if tree == head_tree:
        return None
    message = action["message"]
    author_env = get_author_env(repo_dir, first_commit)
    return commit_tree(worktree_path, tree, ["-p", head], message, author_env)


def process_merge(worktree_path, repo_dir, action):
    """Process a merge commit: preserve tree and all non-first parents."""
    commit = action["commits"][0]
    tree = git(repo_dir, "rev-parse", f"{commit}^{{tree}}")
    message = action.get("message") or git(
        repo_dir, "log", "-1", "--format=%B", commit
    )
    author_env = get_author_env(repo_dir, commit)

    original_parents = get_parents(repo_dir, commit)
    head = git(worktree_path, "rev-parse", "HEAD")
    parent_args = ["-p", head]
    for extra_parent in original_parents[1:]:
        parent_args.extend(["-p", extra_parent])

    return commit_tree(worktree_path, tree, parent_args, message, author_env)


PROCESSORS = {
    "keep": process_keep,
    "reword": process_reword,
    "squash": process_squash,
    "merge": process_merge,
}


def append_extra_commits(worktree_path, repo_dir, base_commit, end_commit):
    """Append commits in (base_commit..end_commit] to worktree HEAD.

    Uses commit-tree to replay each commit's tree snapshot on top of the
    current worktree HEAD. This guarantees zero conflicts because no file
    merging is involved — each commit simply takes the exact tree from the
    original commit.

    Collects ALL commits in the range (no --first-parent), including commits
    reachable through merge branches, to avoid losing sideline histories.

    Returns the number of commits appended.
    """
    commits_raw = git(repo_dir, "log", "--format=%H",
                      f"{base_commit}..{end_commit}")
    if not commits_raw:
        return 0

    # git log returns newest-first; reverse for chronological replay.
    commit_hashes = [h for h in commits_raw.split("\n") if h]
    commit_hashes.reverse()

    for commit_hash in commit_hashes:
        tree = git(repo_dir, "rev-parse", f"{commit_hash}^{{tree}}")
        message = git(repo_dir, "log", "-1", "--format=%B", commit_hash)
        author_env = get_author_env(repo_dir, commit_hash)
        head = git(worktree_path, "rev-parse", "HEAD")
        commit_tree(worktree_path, tree, ["-p", head], message, author_env)

    return len(commit_hashes)


def cleanup_current_tmp(repo_dir, tmp_branch):
    """Remove the specific tmp worktree and branch for the current run."""
    try:
        worktrees_raw = git(repo_dir, "worktree", "list", "--porcelain")
        current_path = None
        for line in worktrees_raw.split("\n"):
            if line.startswith("worktree "):
                current_path = line[len("worktree "):]
            elif line.startswith("branch refs/heads/") and current_path:
                if line == f"branch refs/heads/{tmp_branch}":
                    git(repo_dir, "worktree", "remove", "--force",
                        current_path)
                    break
            elif line == "":
                current_path = None
    except GitError:
        pass
    try:
        git(repo_dir, "branch", "-D", tmp_branch)
    except GitError:
        pass


def main():
    if len(sys.argv) != 2:
        print("Usage: python build_history.py <decision_table.json>",
              file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1]) as f:
        decision = json.load(f)

    repo_dir = decision["repo_dir"]
    branch = decision["branch"]
    squash_base = decision["squash_base"]
    squash_end = decision["squash_end"]
    actions = decision["actions"]
    session_dir = decision["session_dir"]

    # Validate that the branch exists (prevent operating on a wrong branch
    # if decision.json was manually modified).
    try:
        git(repo_dir, "rev-parse", "--verify", f"refs/heads/{branch}")
    except GitError:
        print(f"ERROR: Branch '{branch}' not found in repository.",
              file=sys.stderr)
        sys.exit(1)

    tmp_branch = TMP_PREFIX + branch.replace("/", "-")
    worktree_path = os.path.join(session_dir, "worktree")

    # --- Validate environment ---
    try:
        is_shallow = git(repo_dir, "rev-parse", "--is-shallow-repository")
    except GitError:
        # Older git versions don't support this check; assume not shallow
        is_shallow = "false"
    
    if is_shallow == "true":
        print(
            "ERROR: Shallow clone detected. Cannot safely rewrite history.\n"
            "Run 'git fetch --unshallow' first.",
            file=sys.stderr,
        )
        sys.exit(1)

    # --- Validate decision table ---
    try:
        validate_decision_table(decision)
    except ValueError as e:
        print(f"ERROR: Invalid decision table: {e}", file=sys.stderr)
        sys.exit(1)

    # --- Clean up if same tmp branch exists from a previous attempt ---
    cleanup_current_tmp(repo_dir, tmp_branch)

    # --- Create temporary worktree ---
    try:
        git(repo_dir, "worktree", "add", "-b", tmp_branch, worktree_path, squash_base)
    except GitError as e:
        print(f"ERROR: Failed to create worktree: {e.stderr}", file=sys.stderr)
        sys.exit(1)

    # --- Process actions ---
    original_count = sum(len(a["commits"]) for a in actions)
    new_count = 0
    skipped_count = 0

    try:
        for i, action in enumerate(actions):
            action_type = action["type"]
            processor = PROCESSORS.get(action_type)
            if not processor:
                raise ValueError(f"Unknown action type: {action_type}")

            result = processor(worktree_path, repo_dir, action)
            if result is None:
                skipped_commits = len(action["commits"])
                skipped_count += skipped_commits
            else:
                new_count += 1

    except (GitError, ValueError) as e:
        print(
            f"ERROR: Failed at action #{i + 1}: {e}\n"
            f"Worktree preserved at: {worktree_path}\n"
            f"Temporary branch: {tmp_branch}",
            file=sys.stderr,
        )
        sys.exit(1)

    # --- Integrity check 1: squash result tree must match squash_end ---
    expected_tree = git(repo_dir, "rev-parse", f"{squash_end}^{{tree}}")
    actual_tree = git(worktree_path, "rev-parse", "HEAD^{tree}")

    if expected_tree != actual_tree:
        print(
            f"ERROR: Tree mismatch after building squash history.\n"
            f"  Expected (from {squash_end[:7]}): {expected_tree}\n"
            f"  Actual (new HEAD): {actual_tree}\n"
            f"Worktree preserved at: {worktree_path}\n"
            f"Temporary branch: {tmp_branch}",
            file=sys.stderr,
        )
        sys.exit(1)

    # --- Auto-append commits added after squash_end ---
    branch_ref = f"refs/heads/{branch}"
    appended_count = 0

    # Re-read current_head immediately before appending to minimize window.
    # If new commits were added to the branch since we started, append them.
    current_head = git(repo_dir, "rev-parse", branch_ref)

    if current_head != squash_end:
        try:
            appended_count = append_extra_commits(
                worktree_path, repo_dir, squash_end, current_head
            )
        except GitError as e:
            print(
                f"ERROR: Failed to append extra commits "
                f"({squash_end[:7]}..{current_head[:7]}): {e}\n"
                f"Worktree preserved at: {worktree_path}\n"
                f"Temporary branch: {tmp_branch}",
                file=sys.stderr,
            )
            sys.exit(1)

        # --- Integrity check 2: final tree must match current_head ---
        current_tree = git(repo_dir, "rev-parse",
                           f"{current_head}^{{tree}}")
        final_tree = git(worktree_path, "rev-parse", "HEAD^{tree}")
        if current_tree != final_tree:
            print(
                f"ERROR: Tree mismatch after appending extra commits.\n"
                f"  Expected (from {current_head[:7]}): {current_tree}\n"
                f"  Actual (new HEAD): {final_tree}\n"
                f"Worktree preserved at: {worktree_path}\n"
                f"Temporary branch: {tmp_branch}",
                file=sys.stderr,
            )
            sys.exit(1)

    final_head = git(worktree_path, "rev-parse", "HEAD")

    # --- Output result ---
    result = {
        "status": "success",
        "worktree_path": worktree_path,
        "tmp_branch": tmp_branch,
        "final_head": final_head,
        "current_head": current_head,
        "original_commit_count": original_count,
        "new_commit_count": new_count,
        "skipped_commit_count": skipped_count,
        "appended_commit_count": appended_count,
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
