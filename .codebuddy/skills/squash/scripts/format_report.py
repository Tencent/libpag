#!/usr/bin/env python3
# Copyright (C) 2026 Tencent. All rights reserved.
"""
Generate the squash report from a decision table.

Usage:
    python format_report.py <decision_table.json>

Reads the decision table and produces a formatted squash report to stdout.
The report lists commits in reverse chronological order (newest first),
showing which commits are kept, squashed, or reworded.

Also outputs a JSON summary line at the end (after a --- separator) with:
  {"original_count": N, "new_count": M}
"""

import json
import subprocess
import sys


def git_subject(repo_dir, commit_hash):
    """Get the subject line of a commit."""
    result = subprocess.run(
        ["git", "-C", repo_dir, "log", "-1", "--format=%s", commit_hash],
        capture_output=True, text=True,
    )
    return result.stdout.strip() if result.returncode == 0 else commit_hash[:7]


def main():
    if len(sys.argv) != 2:
        print("Usage: python format_report.py <decision_table.json>",
              file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1]) as f:
        decision = json.load(f)

    repo_dir = decision["repo_dir"]
    actions = decision["actions"]

    original_count = sum(len(a["commits"]) for a in actions)
    new_count = len(actions)

    # Build report entries in chronological order, then reverse
    entries = []
    for action in actions:
        action_type = action["type"]
        commits = action["commits"]

        if action_type == "keep" or action_type == "merge":
            c = commits[0]
            subject = action.get("message") or git_subject(repo_dir, c)
            tag = "merge" if action_type == "merge" else "keep"
            entries.append({
                "display": f"[{tag}] {c[:7]} {subject}",
                "children": [],
            })

        elif action_type == "reword":
            new_msg = action["message"]
            c = commits[0]
            old_subject = git_subject(repo_dir, c)
            entries.append({
                "display": f"[reword] {new_msg}",
                "children": [f"{c[:7]} {old_subject}"],
            })

        elif action_type == "squash":
            new_msg = action["message"]
            children = []
            for c in commits:
                old_subject = git_subject(repo_dir, c)
                children.append(f"{c[:7]} {old_subject}")
            entries.append({
                "display": f"[squash {len(commits)}→1] {new_msg}",
                "children": children,
            })

    # Reverse for newest-first display
    entries.reverse()

    # Format output
    print(f"Squash report ({original_count} commits → {new_count} commits):")
    print()

    for idx, entry in enumerate(entries):
        num = idx + 1
        print(f"{num}. {entry['display']}")
        for child in entry["children"]:
            print(f"   - {child}")
        if entry["children"]:
            print()

    # JSON summary after separator
    print("---")
    print(json.dumps({"original_count": original_count,
                       "new_count": new_count}))


if __name__ == "__main__":
    main()
