#!/bin/bash -e

git-clang-format --diff
result=`git-clang-format --diff | grep "diff"`
echo $result
if [[ $result =~ "diff" ]]
then
    exit 1
fi


