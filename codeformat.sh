#!/bin/bash -e

git-clang-format --diff
result=`git-clang-format --diff | grep "diff"`
echo $result
if [[ $result =~ "diff" ]]
then
    echo "----Failed to pass coding specification----"
    exit 1
fi


