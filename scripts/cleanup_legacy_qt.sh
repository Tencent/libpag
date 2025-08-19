#!/bin/bash

# 清理旧的符号链接和独立Qt库脚本 - 简化版
# 安全清理插件中的符号链接和旧的独立Qt库

set -euo pipefail

# 配置
CONFIG_FILE="${CONFIG_FILE:-./config/qt_shared_config.json}"
BACKUP_DIR="/tmp/qt_shared_backup"
DRY_RUN="${DRY_RUN:-false}"

# 检查JSON解析器
check_json_parser() {
    if command -v jq >/dev/null 2>&1; then
        JSON_PARSER="jq"
    elif command -v python3 >/dev/null 2>&1; then
        JSON_PARSER="python3"
    else
        echo "错误: 需要安装jq或python3来解析JSON配置" >&2
        exit 1
    fi
}

# 解析JSON
parse_config() {
    local key="$1"
    if [[ "$JSON_PARSER" == "jq" ]]; then
        jq -r "$key" "$CONFIG_FILE"
    else
        python3 -c "import json; print(json.load(open('$CONFIG_FILE'))$key)"
    fi
}

# 计算目录大小
get_dir_size() {
    local dir="$1"
    if [[ -d "$dir" ]]; then
        du -sh "$dir" 2>/dev/null | cut -f1
    else
        echo "0B"
    fi
}

# 检查是否为符号链接
is_symlink() {
    [[ -L "$1" ]]
}

# 检查是否为实际目录
is_real_directory() {
    [[ -d "$1" ]] && [[ ! -L "$1" ]]
}

# 安全删除
safe_remove() {
    local target="$1"
    local description="$2"
    
    [[ "$DRY_RUN" == "true" ]] && { echo "[DRY RUN] 将删除: $target ($description)"; return 0; }
    
    if [[ -e "$target" ]]; then
        local size=$(get_dir_size "$target")
        echo "删除: $target (大小: $size)"
        
        [[ -d "$target" ]] && rm -rf "$target" || rm -f "$target"
        echo "已删除: $target"
    fi
}

# 创建备份
create_backup() {
    local source="$1"
    local backup_name="$2"
    
    [[ ! -e "$source" ]] && return 0
    
    local backup_path="$BACKUP_DIR/${backup_name}_$(date +%Y%m%d_%H%M%S)"
    
    [[ "$DRY_RUN" == "true" ]] && { echo "[DRY RUN] 将创建备份: $source -> $backup_path"; return 0; }
    
    mkdir -p "$BACKUP_DIR"
    [[ -d "$source" ]] && cp -r "$source" "$backup_path" || cp "$source" "$backup_path"
    echo "已创建备份: $backup_path"
    echo "$backup_path"
}

# 分析插件Qt库
analyze_plugin_qt() {
    local plugin_path="$1"
    local plugin_name="$2"
    
    local frameworks_path="$plugin_path/Contents/Frameworks"
    local plugins_path="$plugin_path/Contents/PlugIns"
    
    local items_to_cleanup=()
    
    # 检查Frameworks目录
    if [[ -e "$frameworks_path" ]]; then
        if is_symlink "$frameworks_path"; then
            items_to_cleanup+=("$frameworks_path:符号链接")
            echo "发现符号链接: $frameworks_path -> $(readlink "$frameworks_path")"
        elif is_real_directory "$frameworks_path"; then
            items_to_cleanup+=("$frameworks_path:实际目录")
            echo "发现独立Qt框架: $frameworks_path (大小: $(get_dir_size "$frameworks_path"))"
        fi
    fi
    
    # 检查PlugIns目录中的Qt插件
    if [[ -d "$plugins_path" ]]; then
        local qt_plugins=$(find "$plugins_path" -name "*qt*" -type f 2>/dev/null)
        [[ -n "$qt_plugins" ]] && items_to_cleanup+=("$plugins_path:Qt插件")
    fi
    
    echo "${items_to_cleanup[@]}"
}

# 清理单个插件
cleanup_plugin() {
    local plugin_name="$1"
    local plugin_path="$2"
    
    echo "开始清理插件: $plugin_name"
    
    # 分析Qt库
    local items=($(analyze_plugin_qt "$plugin_path" "$plugin_name"))
    
    [[ ${#items[@]} -eq 0 ]] && { echo "插件 $plugin_name 没有需要清理的项目"; return 0; }
    
    # 创建备份
    local backup_name="cleanup_${plugin_name}"
    for item in "${items[@]}"; do
        local path="${item%:*}"
        local type="${item#*:}"
        
        [[ -e "$path" ]] && create_backup "$path" "$backup_name"
    done
    
    # 执行清理
    for item in "${items[@]}"; do
        local path="${item%:*}"
        local type="${item#*:}"
        
        [[ -e "$path" ]] && safe_remove "$path" "$type"
    done
    
    echo "插件 $plugin_name 清理完成"
}

# 主清理函数
main() {
    echo "开始清理旧的符号链接和独立Qt库..."
    echo "配置文件: $CONFIG_FILE"
    
    # 检查JSON解析器
    check_json_parser
    
    # 检查配置文件
    [[ ! -f "$CONFIG_FILE" ]] && { echo "错误: 配置文件不存在: $CONFIG_FILE" >&2; exit 1; }
    
    # 获取插件列表
    local plugins=$(parse_config '.qt_shared_configuration.plugins | keys[]')
    [[ -z "$plugins" ]] && { echo "错误: 未找到任何插件配置" >&2; exit 1; }
    
    echo "发现插件: $plugins"
    
    # 分析所有插件
    local plugins_to_cleanup=()
    for plugin in $plugins; do
        local install_path=$(parse_config ".qt_shared_configuration.plugins[\"$plugin\"].install_path")
        [[ -d "$install_path" ]] || continue
        
        local items=($(analyze_plugin_qt "$install_path" "$plugin"))
        [[ ${#items[@]} -gt 0 ]] && plugins_to_cleanup+=("$plugin")
    done
    
    [[ ${#plugins_to_cleanup[@]} -eq 0 ]] && { echo "没有发现需要清理的项目"; exit 0; }
    
    echo "将清理插件: ${plugins_to_cleanup[*]}"
    
    # 获取用户确认
    [[ "$DRY_RUN" != "true" ]] && {
        read -p "是否继续清理？(y/N): " -n 1 -r
        echo
        [[ ! $REPLY =~ ^[Yy]$ ]] && { echo "用户取消清理操作"; exit 0; }
    }
    
    # 执行清理
    for plugin in "${plugins_to_cleanup[@]}"; do
        local install_path=$(parse_config ".qt_shared_configuration.plugins[\"$plugin\"].install_path")
        cleanup_plugin "$plugin" "$install_path"
    done
    
    echo "清理完成！"
    [[ "$DRY_RUN" == "true" ]] && echo "本次为模拟运行，未实际执行清理"
}

# 显示帮助信息
show_help() {
    cat << EOF
清理旧的符号链接和独立Qt库脚本 - 简化版

用法: $0 [选项]

选项:
    -h, --help      显示帮助信息
    -c, --config    指定配置文件路径
    -n, --dry-run   模拟运行，不实际执行清理

示例:
    $0                          # 交互式清理
    $0 -n                       # 模拟运行
    $0 -c custom_config.json    # 使用自定义配置
EOF
}

# 处理命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -c|--config)
            CONFIG_FILE="$2"
            shift 2
            ;;
        -n|--dry-run)
            DRY_RUN=true
            shift
            ;;
        *)
            echo "错误: 未知选项: $1" >&2
            show_help
            exit 1
            ;;
    esac
done

# 执行主函数
main