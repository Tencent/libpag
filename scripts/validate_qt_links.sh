#!/bin/bash

# Qt库验证脚本 - 简化版
# 验证Qt库文件的完整性和版本兼容性

set -euo pipefail

# 配置
CONFIG_FILE="${CONFIG_FILE:-./config/qt_shared_config.json}"
VERBOSE="${VERBOSE:-false}"

# 验证结果计数器
TOTAL_CHECKS=0
FAILED_CHECKS=0

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

# 检查文件是否存在
file_exists() {
    [[ -e "$1" ]]
}

# 检查是否为目录
is_directory() {
    [[ -d "$1" ]]
}

# 验证Qt框架
validate_qt_framework() {
    local framework_path="$1"
    local framework_name=$(basename "$framework_path" .framework)
    
    # 检查框架是否存在
    [[ ! -d "$framework_path" ]] && { echo "错误: Qt框架不存在: $framework_path"; return 1; }
    
    # 检查框架二进制文件
    local framework_binary="$framework_path/$framework_name"
    [[ -f "$framework_binary" ]] || { echo "错误: 框架二进制文件不存在: $framework_binary"; return 1; }
    
    ((TOTAL_CHECKS++))
    return 0
}

# 验证插件Qt依赖
validate_plugin_qt_dependencies() {
    local plugin_path="$1"
    local plugin_name="$2"
    local qt_dependencies="$3"
    
    local frameworks_path="$plugin_path/Contents/Frameworks"
    [[ ! -d "$frameworks_path" ]] && { echo "错误: 插件Frameworks目录不存在: $frameworks_path"; return 1; }
    
    # 验证每个Qt依赖
    for dep in $qt_dependencies; do
        local dep_framework="$frameworks_path/$dep.framework"
        validate_qt_framework "$dep_framework" || return 1
    done
    
    return 0
}

# 验证插件完整性
validate_plugin_integrity() {
    local plugin_path="$1"
    local plugin_name="$2"
    
    # 检查插件是否存在
    [[ ! -d "$plugin_path" ]] && { echo "错误: 插件不存在: $plugin_path"; return 1; }
    
    # 检查Info.plist
    local info_plist="$plugin_path/Contents/Info.plist"
    [[ ! -f "$info_plist" ]] && { echo "错误: Info.plist不存在: $info_plist"; return 1; }
    
    # 检查可执行文件
    local executable_path="$plugin_path/Contents/MacOS/$plugin_name"
    [[ ! -f "$executable_path" ]] && { echo "错误: 可执行文件不存在: $executable_path"; return 1; }
    
    return 0
}

# 验证Qt库一致性
validate_qt_library_consistency() {
    local host_frameworks="$1"
    local plugin_frameworks="$2"
    
    # 检查插件Frameworks是否为实际目录
    if [[ -d "$plugin_frameworks" ]]; then
        echo "插件使用实际Qt库目录: $plugin_frameworks"
    else
        echo "警告: 插件Frameworks目录不存在: $plugin_frameworks"
        return 1
    fi
    
    return 0
}

# 主验证函数
main() {
    echo "开始验证Qt库文件..."
    echo "配置文件: $CONFIG_FILE"
    
    # 检查JSON解析器
    check_json_parser
    
    # 检查配置文件
    [[ ! -f "$CONFIG_FILE" ]] && { echo "错误: 配置文件不存在: $CONFIG_FILE" >&2; exit 1; }
    
    # 获取主机应用信息
    local host_app_path=$(parse_config '.qt_shared_configuration.host_application.path')
    local host_frameworks_dir=$(parse_config '.qt_shared_configuration.host_application.frameworks_directory')
    local host_frameworks_path="$host_app_path/$host_frameworks_dir"
    
    echo "主机框架路径: $host_frameworks_path"
    [[ ! -d "$host_frameworks_path" ]] && { echo "错误: 主机框架目录不存在: $host_frameworks_path" >&2; exit 1; }
    
    # 获取插件列表
    local plugins=$(parse_config '.qt_shared_configuration.plugins | keys[]')
    [[ -z "$plugins" ]] && { echo "错误: 未找到任何插件配置" >&2; exit 1; }
    
    echo "发现插件: $plugins"
    
    # 验证每个插件
    for plugin in $plugins; do
        echo "验证插件: $plugin"
        
        local install_path=$(parse_config ".qt_shared_configuration.plugins[\"$plugin\"].install_path")
        local qt_dependencies=$(parse_config ".qt_shared_configuration.plugins[\"$plugin\"].qt_dependencies[]" | tr '\n' ' ')
        
        # 验证插件完整性
        validate_plugin_integrity "$install_path" "$plugin" || ((FAILED_CHECKS++))
        
        # 验证Qt依赖
        [[ -n "$qt_dependencies" ]] && validate_plugin_qt_dependencies "$install_path" "$plugin" "$qt_dependencies" || ((FAILED_CHECKS++))
        
        # 验证Qt库一致性
        local plugin_frameworks="$install_path/Contents/Frameworks"
        validate_qt_library_consistency "$host_frameworks_path" "$plugin_frameworks" || ((FAILED_CHECKS++))
    done
    
    # 生成验证报告
    echo ""
    echo "验证完成:"
    echo "- 总检查项: $TOTAL_CHECKS"
    echo "- 失败检查: $FAILED_CHECKS"
    
    [[ $FAILED_CHECKS -eq 0 ]] && exit 0 || exit 1
}

# 显示帮助信息
show_help() {
    cat << EOF
Qt库验证脚本 - 简化版

用法: $0 [选项]

选项:
    -h, --help      显示帮助信息
    -c, --config    指定配置文件路径
    -v, --verbose   详细输出

示例:
    $0                          # 完整验证
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
        -v|--verbose)
            VERBOSE=true
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