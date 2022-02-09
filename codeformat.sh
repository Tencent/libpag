#!/usr/bin/env bash
echo "----begin to scan code format----"
find include/ -iname '*.h' -print0 | xargs clang-format -i
find tgfx/include -iname '*.h' -print0 | xargs clang-format -i
# shellcheck disable=SC2038
find tgfx/src -iname "*.h" -print -o -iname "*.cpp" -print  | xargs clang-format -i
# shellcheck disable=SC2038
find src -name "*.cpp" -print  -o -name "*.h" -print | xargs clang-format -i
# shellcheck disable=SC2038
find test \( -path test/framework/lzma \) -prune -o -name "*.cpp" -print  -o -name "*.h" -print | xargs clang-format -i

git diff
result=`git diff`
if [[ $result =~ "diff" ]]
then
    echo "----Failed----"
    exit 1
else
    echo "----Success----"
fi
echo "----Complete the scan code format-----"

