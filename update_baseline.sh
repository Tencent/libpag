#!/bin/sh

# Update local baseline cache from remote changes.
# Run this after pulling main branch that contains baseline changes from others.
# Without updating the cache, affected tests will be skipped, leading to inaccurate results.

{
  CACHE_VERSION_FILE=./test/baseline/.cache/version.json
  if [ -f "$CACHE_VERSION_FILE" ]; then
    HAS_DIFF=$(git diff --name-only origin/main:test/baseline/version.json $CACHE_VERSION_FILE)
    if [[ ${HAS_DIFF} == "" ]]; then
      exit 0
    fi
  fi
  echo "~~~~~~~~~~~~~~~~~~~Update Baseline Start~~~~~~~~~~~~~~~~~~~~~"
  CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  CURRENT_COMMIT=$(git rev-parse HEAD)
  STASH_LIST_BEFORE=$(git stash list)
  git stash push --quiet
  STASH_LIST_AFTER=$(git stash list)
  git switch main --quiet

  ./install_tools.sh
  depsync

  BUILD_DIR=build

  rm -rf ${BUILD_DIR}
  mkdir ${BUILD_DIR}
  cd ${BUILD_DIR}

  if [[ "$1" == "USE_SWIFTSHADER" ]]; then
    cmake -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage -g -O0" -DPAG_USE_SWIFTSHADER=ON -DPAG_SKIP_GENERATE_BASELINE_IMAGES=ON -DPAG_BUILD_TESTS=ON -DPAG_SKIP_BASELINE_CHECK=ON -DPAG_ENABLE_PROFILING=OFF -DCMAKE_BUILD_TYPE=Debug ../
  else
    cmake -DPAG_BUILD_TESTS=ON -DPAG_SKIP_BASELINE_CHECK=ON -DPAG_ENABLE_PROFILING=OFF -DCMAKE_BUILD_TYPE=Debug ../
  fi

  cmake --build . --target UpdateBaseline -- -j 12
  ./UpdateBaseline

  if test $? -eq 0; then
     echo "~~~~~~~~~~~~~~~~~~~Update Baseline Success~~~~~~~~~~~~~~~~~~~~~"
  else
    echo "~~~~~~~~~~~~~~~~~~~Update Baseline Failed~~~~~~~~~~~~~~~~~~"
    COMPLIE_RESULT=false
  fi

  cd ..

  if [[ $CURRENT_BRANCH == "HEAD" ]]; then
      git checkout $CURRENT_COMMIT --quiet
  else
      git switch $CURRENT_BRANCH --quiet
  fi
  if [[ $STASH_LIST_BEFORE != "$STASH_LIST_AFTER" ]]; then
    git stash pop --index --quiet
  fi

  depsync

  if [ "$COMPLIE_RESULT" == false ]; then
    mkdir -p result
    cp -r test/out result
    exit 1
  fi
  rm -rf ${BUILD_DIR}
}
