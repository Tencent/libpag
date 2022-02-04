#!/usr/bin/env bash
echo "----begin coding format----"
find ./include -iname "*.h" -print0 | xargs clang-format -i
find ./src -iname "*.h" -0 -iname "*.cpp" -path "./src/platform/ios" -path "./src/platform/mac" -print0 | xargs clang-format -i
find ./test -iname "*.cpp" -print0 | xargs clang-format -i
find ./tgfx -iname "*.h" -o -iname "*.cpp" -print0 | xargs clang-format -i
echo $result

if [[ $result =~ "diff" ]]
then
    echo "----Failed to pass coding specification----"
    exit 1
else
    echo "----Pass coding specification----"
fi
echo "----end coding format----"

