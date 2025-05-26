#!/bin/bash
# 从PEM私钥生成P12证书的脚本（仅支持直接密码输入）
# 使用方法: 
#   ./gen_p12.sh 私钥文件.pem [输出文件名.p12] [证书有效期(天)] [通用名称] [密码]

# 检查输入参数
if [ $# -lt 1 ]; then
    echo "错误: 需要至少指定私钥文件参数"
    echo "用法: $0 私钥文件.pem [输出文件名.p12] [证书有效期(天)] [通用名称] [密码]"
    echo "密码可:"
    echo "  - 直接作为参数输入"
    echo "  - 不指定则自动生成"
    exit 1
fi

# 设置默认参数
PRIVATE_KEY=$1
OUTPUT_P12=${2:-"certificate.p12"}
DAYS=${3:-365}                  # 默认有效期1年
COMMON_NAME=${4:-"My Self-Signed Certificate"}  # 默认通用名称
PASSWORD=${5:-""}               # 密码参数

# 检查私钥文件是否存在
if [ ! -f "$PRIVATE_KEY" ]; then
    echo "错误: 私钥文件 $PRIVATE_KEY 不存在"
    exit 1
fi

# 临时文件
TEMP_CSR=$(mktemp)
TEMP_CRT=$(mktemp)

# 清理函数
cleanup() {
    rm -f "$TEMP_CSR" "$TEMP_CRT"
    echo "已清理临时文件"
}
trap cleanup EXIT

# 生成CSR
echo "正在从私钥生成证书签名请求(CSR)..."
openssl req -new -key "$PRIVATE_KEY" -out "$TEMP_CSR" -subj "/CN=$COMMON_NAME" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "错误: 生成CSR失败"
    exit 1
fi

# 生成自签名证书
echo "正在生成自签名证书(有效期 $DAYS 天)..."
openssl x509 -req -days "$DAYS" -in "$TEMP_CSR" -signkey "$PRIVATE_KEY" -out "$TEMP_CRT" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "错误: 生成证书失败"
    exit 1
fi

# 生成P12文件
echo "正在打包为P12文件..."
if [ -z "$PASSWORD" ]; then
    PASSWORD=$(openssl rand -base64 18 | tr -d '\n=' | head -c 24)
fi

openssl pkcs12 -export -out "$OUTPUT_P12" -inkey "$PRIVATE_KEY" -in "$TEMP_CRT" -name "$COMMON_NAME" -passout pass:"$PASSWORD"

if [ $? -ne 0 ]; then
    echo "错误: 生成P12文件失败"
    exit 1
fi

echo "成功生成P12文件: $OUTPUT_P12"
echo "密码: $PASSWORD"
echo "请妥善保管此文件和密码"
