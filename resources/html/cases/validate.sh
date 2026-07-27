#!/usr/bin/env bash
# Validate the subset-tier HTML cases against manifest.csv:
#   - import each case through the direct importer (no browser)
#   - verify the emitted .pagx
#   - assert observed subset:* warning codes match expected_warnings
#
# Usage: resources/html/cases/validate.sh [path/to/pagx]
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
CASES="$ROOT/resources/html/cases"
PAGX="${1:-$ROOT/cmake-build-debug/pagx}"
MANIFEST="$CASES/manifest.csv"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0; xfail=0
# skip header; read id,category,feature,tier,expected_warnings,notes (feature/notes may be quoted)
while IFS= read -r line; do
  [ -z "$line" ] && continue
  id="${line%%,*}"
  # parse the quoted CSV row with python's csv module
  tier="$(printf '%s' "$line" | python3 -c 'import csv,sys; r=next(csv.reader(sys.stdin)); print(r[3])' 2>/dev/null)"
  expected="$(printf '%s' "$line" | python3 -c 'import csv,sys; r=next(csv.reader(sys.stdin)); print(r[4])' 2>/dev/null)"
  notes="$(printf '%s' "$line" | python3 -c 'import csv,sys; r=next(csv.reader(sys.stdin)); print(r[5] if len(r)>5 else "")' 2>/dev/null)"
  [ "$tier" != "subset" ] && continue
  known_diag=0
  case "$notes" in *known-verify-diagnostic*) known_diag=1 ;; esac

  rel="$(printf '%s' "$id" | sed 's#__#/#')"       # 01-structure__foo -> 01-structure/foo
  file="$CASES/$rel.html"
  if [ ! -f "$file" ]; then echo "MISSING  $id"; fail=$((fail+1)); continue; fi

  out="$(PAGX_HTML_SNAPSHOT=0 "$PAGX" import --input "$file" --output "$TMP/o.pagx" -v 2>&1 | grep -v gcda)"
  if printf '%s' "$out" | grep -q 'error:'; then echo "IMPORTERR $id"; fail=$((fail+1)); continue; fi
  if ! "$PAGX" verify --skip-render --skip-layout "$TMP/o.pagx" >/dev/null 2>&1; then
    if [ "$known_diag" -eq 1 ]; then echo "XFAIL    $id  (known verify diagnostic)"; xfail=$((xfail+1)); continue; fi
    echo "VERIFYERR $id"; fail=$((fail+1)); continue
  fi

  got="$(printf '%s' "$out" | grep -oE 'subset:[a-z-]+' | sort -u | paste -sd, -)"
  exp="$(printf '%s' "$expected" | tr ',' '\n' | sort -u | paste -sd, -)"
  if [ "$got" = "$exp" ]; then
    pass=$((pass+1))
  else
    echo "WARNDIFF $id  expected=[$exp] got=[$got]"; fail=$((fail+1))
  fi
done < <(tail -n +2 "$MANIFEST")

echo "=== subset cases: pass=$pass xfail=$xfail fail=$fail ==="
[ "$fail" -eq 0 ]
