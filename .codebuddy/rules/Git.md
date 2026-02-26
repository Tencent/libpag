---
description: Git 操作规范
alwaysApply: true
---

## **!! IMPORTANT - 操作限制**

- **NEVER** 自动执行 `git stash`、`git reset`、`git checkout` 等改变暂存区或工作区状态的命令，除非**暂存区为空**或**用户明确要求**
- **NEVER** 在 main 分支直接提交推送代码，必须通过 PR 流程

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

