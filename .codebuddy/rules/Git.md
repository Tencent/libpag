---
description: Git 操作规范
alwaysApply: true
---

## 分支命名

格式：`feature/{username}_模块名` 或 `bugfix/{username}_模块名`

- `{username}`：GitHub 用户 ID，全小写
- 模块名用下划线连接，最多两个单词

## Commit 信息

120 字符内的英语概括，以英文句号结尾，中间无其他标点，侧重描述用户可感知的变化。

## 提交

- **NEVER** 自动执行 `git stash`、`git reset`、`git checkout` 等改变暂存区或工作区状态的命令，除非**暂存区为空**或**用户明确要求**

### 自动提交

完成任务后自动提交（仅 commit，**不自动推送**），无需用户额外指示：

1. **NEVER** 在 main 分支直接提交，先按「分支命名」规范创建新分支
2. **NEVER** 使用 `--amend` 修改已有 commit，始终创建新 commit
3. 仅提交本次会话中由你的操作引起的变更（包括但不限于：代码编辑、文件删除/重命名、代码格式化、截图基准更新等），使用 `git commit --only <file1> <file2> ... -m "{Commit 信息}"`
4. 忽略非本次会话引起的变更：不恢复，保持原样

### 手动提交

用户执行 `/commit` 或主动要求提交时，**不受自动提交规则限制**：

- 允许在 main 分支直接提交
- 提交范围由用户决定或按 commit skill 流程处理工作区中的**所有变更**，不区分是否为本次会话引起

## Worktree

创建 worktree 新分支时必须加 `--no-track`，避免继承源分支的 tracking 关系。创建完成后，将主仓库的 `test/out/`、`test/baseline/`、`third_party/` 拷贝到新 worktree 目录下：

```bash
git worktree add <path> -b <branch> --no-track main
cp -R test/out/ <path>/test/out/
cp -R test/baseline/ <path>/test/baseline/
cp -R third_party/ <path>/third_party/
```
