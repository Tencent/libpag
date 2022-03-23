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
  STASH_LIST_BEFORE=$(git stash list)
  git stash push --quiet
  STASH_LIST_AFTER=$(git stash list)
  git switch main --quiet

  if [[ $1 == "1" ]]; then
    BUILD_DIR=build
  else
    BUILD_DIR=cmake-build-debug
  fi

  if [ ! -d "./${BUILD_DIR}" ]; then
    mkdir ${BUILD_DIR}
  fi
  cd ${BUILD_DIR}

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

  if [[ $1 == "1" ]]; then
    $CMAKE_COMMAND -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage -g -O0" -DPAG_USE_SWIFTSHADER=ON -DCMAKE_BUILD_TYPE=Debug ../
  else
    $CMAKE_COMMAND -DCMAKE_BUILD_TYPE=Debug ../
  fi

  $CMAKE_COMMAND --build . --target PAGBaseline -- -j 12
  ./PAGBaseline
  cd ..

  git switch $CURRENT_BRANCH --quiet
  if [[ $STASH_LIST_BEFORE != "$STASH_LIST_AFTER" ]]; then
    git stash pop --index --quiet
  fi

  echo "~~~~~~~~~~~~~~~~~~~Update Baseline END~~~~~~~~~~~~~~~~~~~~~"
  exit
}
