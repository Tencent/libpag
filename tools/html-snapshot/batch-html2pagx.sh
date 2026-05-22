#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HTML2PAGX_BIN="$SCRIPT_DIR/html2pagx"

print_usage() {
  cat <<'EOF'
Usage:
  batch-html2pagx.sh <input-dir> [options] [-- <html2pagx args...>]

Recursively scans <input-dir> for .html/.htm files, excluding *.subset.html and
*.subset.htm, then runs html2pagx on each match.

Options:
  --html2pagx <path>     Path to html2pagx binary/script
  --output-root <dir>    Mirror outputs under <dir> instead of next to inputs
  --skip-existing        Skip files whose target .pagx already exists
  --dry-run              Print planned commands without executing
  -h, --help             Show this help

Examples:
  batch-html2pagx.sh /Users/yucong/Desktop/tmp_pagx
  batch-html2pagx.sh /Users/yucong/Desktop/tmp_pagx --output-root /tmp/pagx_out
  batch-html2pagx.sh /Users/yucong/Desktop/tmp_pagx -- --no-render --scale 2
EOF
}

INPUT_DIR=""
OUTPUT_ROOT=""
SKIP_EXISTING=0
DRY_RUN=0
FORWARD_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      print_usage
      exit 0
      ;;
    --html2pagx)
      HTML2PAGX_BIN="$2"
      shift 2
      ;;
    --output-root)
      OUTPUT_ROOT="$2"
      shift 2
      ;;
    --skip-existing)
      SKIP_EXISTING=1
      shift
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --)
      shift
      while [[ $# -gt 0 ]]; do
        FORWARD_ARGS+=("$1")
        shift
      done
      ;;
    -*)
      echo "batch-html2pagx.sh: unknown option '$1'" >&2
      print_usage >&2
      exit 2
      ;;
    *)
      if [[ -n "$INPUT_DIR" ]]; then
        echo "batch-html2pagx.sh: unexpected positional argument '$1'" >&2
        print_usage >&2
        exit 2
      fi
      INPUT_DIR="$1"
      shift
      ;;
  esac
done

if [[ -z "$INPUT_DIR" ]]; then
  echo "batch-html2pagx.sh: missing <input-dir>" >&2
  print_usage >&2
  exit 2
fi

if [[ ! -d "$INPUT_DIR" ]]; then
  echo "batch-html2pagx.sh: input directory not found: $INPUT_DIR" >&2
  exit 1
fi

if [[ ! -x "$HTML2PAGX_BIN" ]]; then
  echo "batch-html2pagx.sh: html2pagx is not executable: $HTML2PAGX_BIN" >&2
  exit 1
fi

INPUT_DIR="$(cd "$INPUT_DIR" && pwd)"

if [[ -n "$OUTPUT_ROOT" ]]; then
  mkdir -p "$OUTPUT_ROOT"
  OUTPUT_ROOT="$(cd "$OUTPUT_ROOT" && pwd)"
fi

files=()
while IFS= read -r -d '' file; do
  files+=("$file")
done < <(
  find "$INPUT_DIR" -type f \
    \( -iname '*.html' -o -iname '*.htm' \) \
    ! -iname '*.subset.html' \
    ! -iname '*.subset.htm' \
    -print0 | sort -z
)

total="${#files[@]}"
if [[ "$total" -eq 0 ]]; then
  echo "No non-subset HTML files found under: $INPUT_DIR"
  exit 0
fi

echo "Found $total non-subset HTML files under: $INPUT_DIR"

processed=0
skipped=0
failed=0
index=0

for file in "${files[@]}"; do
  index=$((index + 1))
  rel_path="${file#$INPUT_DIR/}"
  out_dir=""

  if [[ -n "$OUTPUT_ROOT" ]]; then
    rel_dir="$(dirname "$rel_path")"
    if [[ "$rel_dir" == "." ]]; then
      out_dir="$OUTPUT_ROOT"
    else
      out_dir="$OUTPUT_ROOT/$rel_dir"
    fi
    mkdir -p "$out_dir"
  fi

  base_name="$(basename "$file")"
  stem="${base_name%.*}"
  pagx_target="${file%.*}.pagx"
  if [[ -n "$out_dir" ]]; then
    pagx_target="$out_dir/$stem.pagx"
  fi

  if [[ "$SKIP_EXISTING" -eq 1 && -f "$pagx_target" ]]; then
    skipped=$((skipped + 1))
    echo "[$index/$total] skip      $rel_path"
    continue
  fi

  cmd=("$HTML2PAGX_BIN" "$file")
  if [[ -n "$out_dir" ]]; then
    cmd+=(--output-dir "$out_dir")
  fi
  if [[ "${#FORWARD_ARGS[@]}" -gt 0 ]]; then
    cmd+=("${FORWARD_ARGS[@]}")
  fi

  echo "[$index/$total] process   $rel_path"
  if [[ "$DRY_RUN" -eq 1 ]]; then
    printf '  '
    printf '%q ' "${cmd[@]}"
    printf '\n'
    processed=$((processed + 1))
    continue
  fi

  if "${cmd[@]}"; then
    processed=$((processed + 1))
  else
    failed=$((failed + 1))
    echo "  failed: $rel_path" >&2
  fi
done

echo "Summary: processed=$processed skipped=$skipped failed=$failed total=$total"

if [[ "$failed" -gt 0 ]]; then
  exit 1
fi

