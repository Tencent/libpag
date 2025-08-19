#!/bin/bash

# Qt库复制脚本 - 简化版
# 核心功能：将PAGViewer的Qt库复制到PAGExporter插件中

set -euo pipefail

# 配置
CONFIG_FILE="${CONFIG_FILE:-./config/qt_shared_config.json}"
DRY_RUN="${DRY_RUN:-false}"

# 检查JSON解析器
if command -v jq >/dev/null 2>&1; then
    JSON_PARSER="jq"
elif command -v python3 >/dev/null 2>&1; then
    JSON_PARSER="python3"
else
    echo "错误: 需要安装jq或python3来解析JSON配置" >&2
    exit 1
fi

# 解析JSON
parse_json() {
    local key="$1"
    if [[ "$JSON_PARSER" == "jq" ]]; then
        jq -r "$key" "$CONFIG_FILE"
    else
        python3 -c "import json; print(json.load(open('$CONFIG_FILE'))$key)"
    fi
}

# 检查权限
check_permissions() {
    local target_path="$1"
    
    if [[ ! -w "$(dirname "$target_path")" ]]; then
        echo "警告: 需要管理员权限来修改 $target_path"
        
        if command -v sudo >/dev/null 2>&1; then
            echo "将使用sudo获取权限..."
            return 0
        else
            echo "错误: 没有sudo权限，无法继续操作" >&2
            return 1
        fi
    fi
    return 0
}

# 复制文件
copy_files() {
    local source="$1"
    local target="$2"
    
    [[ "$DRY_RUN" == "true" ]] && { echo "[DRY RUN] cp -r $source $target"; return 0; }
    
    [[ ! -e "$source" ]] && { echo "错误: 源文件不存在: $source" >&2; return 1; }
    
    # 检查权限
    check_permissions "$target" || return 1
    
    mkdir -p "$(dirname "$target")"
    
    if [[ -w "$(dirname "$target")" ]]; then
        [[ -d "$source" ]] && cp -r "$source" "$target" || cp "$source" "$target"
    else
        sudo mkdir -p "$(dirname "$target")"
        [[ -d "$source" ]] && sudo cp -r "$source" "$target" || sudo cp "$source" "$target"
    fi
}

# 部署插件
deploy_plugin() {
    local plugin_name="$1"
    echo "部署插件: $plugin_name"
    
    local install_path=$(parse_json ".qt_shared_configuration.plugins[\"$plugin_name\"].install_path")
    local pagviewer_path=$(parse_json ".qt_shared_configuration.host_application.path")
    local frameworks_dir=$(parse_json ".qt_shared_configuration.host_application.frameworks_directory")
    local host_frameworks_path="$pagviewer_path/$frameworks_dir"
    local plugin_frameworks_path="$install_path/Contents/Frameworks"
    
    [[ ! -d "$host_frameworks_path" ]] && { echo "错误: PAGViewer Qt框架路径不存在: $host_frameworks_path" >&2; return 1; }
    [[ ! -d "$install_path" ]] && { echo "错误: 插件路径不存在: $install_path" >&2; return 1; }
    
    echo "复制Qt库到插件..."
    
    # 清理旧的Frameworks目录
    [[ -d "$plugin_frameworks_path" ]] && {
        if [[ -w "$plugin_frameworks_path" ]]; then
            rm -rf "$plugin_frameworks_path"
        else
            sudo rm -rf "$plugin_frameworks_path"
        fi
    }
    
    # 复制新的Qt库
    copy_files "$host_frameworks_path" "$plugin_frameworks_path"
    echo "Qt库复制完成"
}

# 主函数
main() {
    echo "开始Qt库复制部署..."
    echo "配置文件: $CONFIG_FILE"
    
    [[ ! -f "$CONFIG_FILE" ]] && { echo "错误: 配置文件不存在: $CONFIG_FILE" >&2; exit 1; }
    
    local plugins=$(parse_json '.qt_shared_configuration.plugins | keys[]')
    [[ -z "$plugins" ]] && { echo "错误: 未找到任何插件配置" >&2; exit 1; }
    
    echo "发现插件: $plugins"
    
    local failed=()
    for plugin in $plugins; do
        [[ "$DRY_RUN" == "true" ]] && { echo "[DRY RUN] 将部署插件: $plugin"; continue; }
        deploy_plugin "$plugin" || failed+=("$plugin")
    done
    
    [[ ${#failed[@]} -eq 0 ]] && { echo "所有插件部署成功"; exit 0; }
    echo "以下插件部署失败: ${failed[*]}" >&2
    exit 1
}

# 命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            echo "用法: $0 [-c 配置文件] [--dry-run]"
            echo "功能: 将PAGViewer的Qt库复制到PAGExporter插件中"
            exit 0
            ;;
        -c|--config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        *)
            echo "错误: 未知选项: $1" >&2
            exit 1
            ;;
    esac
done

main