---
description: Git 操作规范
alwaysApply: true
---

## **!! IMPORTANT - 操作限制**

- **NEVER** 自动执行 `git stash`、`git reset`、`git checkout` 等改变暂存区或工作区状态的命令，除非**暂存区为空**或**用户明确要求**
- **NEVER** 在 main 分支直接提交推送代码，必须通过 PR 流程
- **NEVER** 使用 `--amend` 修改已有 commit，始终创建新 commit 以保留完整修改历史

## **!! CRITICAL - 自动提交**

完成用户请求后，检查 `git status`，若有任何变更（暂存区、工作区、未跟踪文件），执行提交（仅 commit，**不自动推送**）：

1. 若当前在 main 分支，先创建新分支
2. **仅提交本次会话中实际修改过的文件**，使用 `git commit --only <file1> <file2> ... -m "{Commit 信息}"`
3. **忽略非本次会话修改的文件**：不恢复，保持原样

## Worktree

创建 worktree 新分支时必须加 `--no-track`，避免继承源分支的 tracking 关系。创建完成后，将主仓库的 `test/out/`、`test/baseline/`、`third_party/` 拷贝到新 worktree 目录下：

```bash
git worktree add <path> -b <branch> --no-track main
cp -R test/out/ <path>/test/out/
cp -R test/baseline/ <path>/test/baseline/
cp -R third_party/ <path>/third_party/
```

## 分支命名

格式：`feature/{username}_模块名` 或 `bugfix/{username}_模块名`

- `{username}`：GitHub 用户 ID，全小写
- 模块名用下划线连接，最多两个单词

## Commit 信息

120 字符内的英语概括，以英文句号结尾，中间无其他标点，侧重描述用户可感知的变化
