#!/bin/bash
set -e
set -o pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 update_archive private_key"
  exit 1
fi

key_type=$(openssl pkey -in "$2" -text 2>/dev/null | grep -E "ED25519|EdDSA" || echo "DSA")

if [[ $key_type =~ "ED25519" || $key_type =~ "EdDSA" ]]; then
    # EdDSA 签名使用 pkeyutl，不需要额外的摘要步骤
    openssl pkeyutl -sign -inkey "$2" -rawin -in "$1" | openssl enc -base64
else
    # DSA 签名保持原有格式
    openssl dgst -sha1 -binary < "$1" | openssl dgst -sha1 -sign "$2" | openssl enc -base64
fi
