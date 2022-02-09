#!/usr/bin/env bash
echo "----Start code formatting----"
find include/ -iname '*.h' -print0 | xargs clang-format -i
find tgfx/include -iname '*.h' -print0 | xargs clang-format -i
# shellcheck disable=SC2038
find tgfx/src -iname "*.h" -print -o -iname "*.cpp" -print  | xargs clang-format -i
# shellcheck disable=SC2038
find src \( -path src/platform/ios -o -path src/platform/mac -o -path src/platform/cocoa \) -prune -o -name "*.cpp" -print  -o -name "*.h" -print | xargs clang-format -i
# shellcheck disable=SC2038
find test \( -path test/framework/lzma \) -prune -o -name "*.cpp" -print  -o -name "*.h" -print | xargs clang-format -i

git diff
result=`git diff`
if [[ $result =~ "diff" ]]
then
    echo "----Failed to pass coding specification----"
    exit 1
else
    echo "----Pass coding specification----"
fi
echo "----Finish code formatting----"

