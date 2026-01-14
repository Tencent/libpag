---
description: 发起 PR - 格式化代码并提交 PR
---

# 发起 PR 流程

严格按以下步骤顺序执行：

---

## 步骤 1：格式化代码

```bash
./codeformat.sh
```

忽略输出的报错信息，只要运行就会完成格式化。

---

## 步骤 2：输出 PR 信息供用户确认

用 `git status`、`git diff`、`git diff --cached` 查看变更内容，输出以下信息供用户确认：

- **分支名称**：`feature/{username}_模块名` 或 `bugfix/{username}_模块名`（模块名用下划线连接，最多两个单词）
- **PR 标题**：英语，120 字符内，以句号结尾，侧重描述用户可感知的变化
- **PR 描述**：中文，简要说明变更内容和目的

**必须等待用户明确确认后才能继续。**

---

## 步骤 3：创建分支、推送代码、发起 PR

用户确认后，依次执行：

```bash
git checkout -b {分支名称}
git add .
git commit -m "{PR标题}"
git push -u origin {分支名称}
gh pr create --title "{PR标题}" --body "{PR描述}"
```

**禁止重新编写 PR 标题和描述**，必须使用步骤 2 中用户确认的内容。

完成后输出 PR 链接。

---

## 重要限制

- **禁止**在 main 分支直接提交推送代码
- **禁止** force push
