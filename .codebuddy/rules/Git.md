---
description: Git 操作规范
alwaysApply: true
---

## 操作限制

- 禁止自动执行 `git stash`、`git reset`、`git checkout` 等改变暂存区或工作区状态的命令，除非用户明确要求提交代码
- 必须 stash 时需用户确认，完成后用 `git stash pop --index` 完全还原原始的暂存区和工作区状态
- 禁止在 main 分支直接提交推送代码，必须通过 PR 流程
- 禁止 force push，补充 PR 的提交使用正常追加

## 分支命名

格式：`feature/{username}_模块名` 或 `bugfix/{username}_模块名`

- `{username}`：GitHub 用户 ID，全小写
- 模块名用下划线连接，最多两个单词

## Commit 信息

120 字符内的英语概括，以英文句号结尾，中间无其他标点，侧重描述用户可感知的变化

