#!/usr/bin/env bash
echo "----begin coding format----"
git-clang-format --diff
result=`git-clang-format --diff | grep "diff"`
echo $result
if [[ $result =~ "diff" ]]
then
    echo "----Failed to pass coding specification----"
    exit 1
else
    echo "----Pass coding specification----"
fi
echo "----end coding format----"

