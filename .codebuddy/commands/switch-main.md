---
description: 切换到主分支 - 将本地未提交的变更转移到更新后的 main 分支，并清理已合并的旧分支
---

# 切换到主分支

将本地未提交的变更转移到更新后的 main 分支，并清理已合并的旧分支。

## 执行流程

1. 记录当前分支名称，检查是否有本地变更
2. 如有变更，执行 `git stash push --include-untracked -m "switch-main"`
3. 切换到 main 并更新：`git checkout main && git pull --rebase origin main`
4. 如有 stash，执行 `git stash pop --index` 还原变更
5. 如原分支有已合并的 PR（`gh pr list --head "{分支}" --state merged`），删除本地分支

## 冲突处理

如果 `git stash pop` 遇到冲突：

1. 列出冲突文件
2. 阅读每个冲突文件的代码，分析冲突原因
3. 提供自动修复方案（说明如何解决每个冲突）
4. 询问用户是否执行自动修复
5. 用户确认后执行修复，然后 `git stash drop`

**注意**：解决后不修改暂存区状态，保持还原后的原始状态。
