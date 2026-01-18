---
description: 管理 Git Worktree - 创建、切换或清理 worktree，自动同步测试缓存
---

# Git Worktree 管理

管理 git worktree，支持创建、切换和清理操作，切换时自动同步测试缓存目录。

---

## 公共步骤：获取主仓库信息

```bash
MAIN_REPO=$(git worktree list --porcelain | head -1 | sed 's/worktree //')
REPO_NAME=$(basename "$MAIN_REPO")
```

---

## 参数解析

| $ARGUMENTS | 操作 |
|------------|------|
| 空 | 列出现有 worktree，询问用户进入或删除 |
| `{name}` | 进入该 worktree（不存在则创建） |

Worktree 路径命名规范：`{项目名称}-{name}`，与主仓库同级目录。

---

## 无参数模式

```bash
git worktree list
```

若只有主仓库，提示用户使用 `/worktree {name}` 创建新的 worktree，**终止流程**。

若有其他 worktree，展示列表并询问用户要执行的操作：
- 进入某个 worktree（执行「同步缓存并切换」，`WT_PATH` 为用户选择的路径）
- 删除某个 worktree（执行「删除 worktree」流程）

---

## 进入 worktree

### 1. 计算路径并检查是否存在

```bash
WT_PATH="$MAIN_REPO/../$REPO_NAME-{name}"
test -d "$WT_PATH" && echo "exists" || echo "not found"
```

### 2. 若不存在则创建

```bash
git fetch origin main
git worktree add -b {name} "$WT_PATH" origin/main
```

### 3. 同步缓存并切换

从主仓库拷贝测试缓存（若存在）：

```bash
if [ -d "$MAIN_REPO/test/baseline/.cache" ]; then
    mkdir -p "$WT_PATH/test/baseline"
    cp -r "$MAIN_REPO/test/baseline/.cache" "$WT_PATH/test/baseline/"
    echo "已同步 test/baseline/.cache"
fi

if [ -d "$MAIN_REPO/test/out" ]; then
    mkdir -p "$WT_PATH/test"
    cp -r "$MAIN_REPO/test/out" "$WT_PATH/test/"
    echo "已同步 test/out"
fi
```

切换到 worktree：

```bash
cd "$WT_PATH"
```

输出（新建时）：

```
**Worktree 已创建**：{WT_PATH}
**分支**：{name}
**已同步缓存**：{同步的目录列表}
```

输出（已存在时）：

```
**已切换到 worktree**：{WT_PATH}
**已同步缓存**：{同步的目录列表}
```

---

## 删除 worktree

### 1. 检查未提交变更

```bash
git -C "{worktree 路径}" status --porcelain
```

若有未提交变更，提示用户并询问是否继续删除。用户取消则**终止流程**。

### 2. 切换工作目录（如需要）

若当前工作目录在待删除的 worktree 内，先切换到主仓库：

```bash
cd "$MAIN_REPO"
```

### 3. 移除 worktree

```bash
git worktree remove "{worktree 路径}" --force
```

输出：

```
**已删除 worktree**：{worktree 路径}
```

---

## 重要限制

- **NEVER** 删除主仓库
- 创建 worktree 时始终基于 `origin/main`
