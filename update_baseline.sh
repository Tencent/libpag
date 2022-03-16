#!/bin/sh
{
  CACHE_VERSION_FILE=./test/baseline/.cache/version.json
  if [ -f "$CACHE_VERSION_FILE" ]; then
    HAS_DIFF=$(git diff --name-only main:test/baseline/version.json $CACHE_VERSION_FILE)
    if [[ ${HAS_DIFF} == "" ]]; then
      exit 0
    fi
  fi
  echo "~~~~~~~~~~~~~~~~~~~Update Baseline Start~~~~~~~~~~~~~~~~~~~~~"
  CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  git stash push
  git switch main

  if [[ $1 == "1" ]]; then
    cd build
  elif [ -d "./cmake-build-debug" ]; then
    cd cmake-build-debug
  else
    mkdir build
    cd build
  fi

  if [ -f "./CMakeCache.txt" ]; then
    TEXT=$(cat ./CMakeCache.txt)
    TEXT=${TEXT#*CMAKE_COMMAND:INTERNAL=}
    for line in ${TEXT}; do
      CMAKE_COMMAND=$line
      break
    done
  fi
  if [ ! -f "$CMAKE_COMMAND" ]; then
    CMAKE_COMMAND="cmake"
  fi
  echo $CMAKE_COMMAND

  $CMAKE_COMMAND -DCMAKE_BUILD_TYPE=Debug ../
  $CMAKE_COMMAND --build . --target PAGBaseline -- -j 12
  ./PAGBaseline
  cd ..

  git switch $CURRENT_BRANCH
  git stash pop --index
  echo "~~~~~~~~~~~~~~~~~~~Update Baseline END~~~~~~~~~~~~~~~~~~~~~"
  exit
}
