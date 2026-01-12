---
description: 当用户发送Github PR链接、要求审查代码、或需要提交代码、发起PR时
alwaysApply: true
---

## 发起 PR 流程

1. 运行 `./codeformat.sh` 格式化代码，忽略输出的报错，只要运行就会完成格式化
2. 按 Code.md 代码审查要点自审本地变更
3. 输出以下内容供用户确认：
   - **分支名称**：格式见 Git.md 分支命名规范
   - **PR 标题**：英语，格式见 Git.md Commit 信息要求
   - **PR 描述**：中文简要说明变更内容
4. 确认后创建分支、推送代码、并使用上面的PR标题和PR描述内容执行 `gh pr create`，**禁止再重新写PR描述**

## Review PR 流程

1. 用 `gh` 命令获取 PR 变更和评论，结合本地代码理解上下文
2. 输出变更总结和优化后的 PR 标题
3. 按 Code.md 代码审查要点逐项检查
4. 验证已有评论是否真正修复（重点关注我提过的评论）
5. 新问题或未解决问题带序号汇总，经用户确认后添加**中文行级评论**

### 行级评论格式

- **禁止使用 `gh pr comment`、`gh pr review --comment`、`gh pr review --body` 或任何非行级评论命令**
- 使用 `gh api` 配合 `--input -` 和 heredoc 传递 JSON
```bash
gh api repos/{owner}/{repo}/pulls/{pr}/reviews --input - <<'EOF'
{"commit_id":"HEAD_SHA","event":"COMMENT","comments":[{"path":"文件路径","line":行号,"side":"RIGHT","body":"内容"}]}
EOF
```

