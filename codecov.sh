#!/bin/bash
set -e
cd "$(dirname "$0")"

if [[ `uname` == 'Darwin' ]]; then
  if [ ! $(which gcovr) ]; then
    if [ ! $(which brew) ]; then
      echo "Homebrew not found. Trying to install..."
      /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    echo "gcovr not found. Trying to install..."
    brew install gcovr
  fi
fi

mkdir -p result
# Apple's clang produces gcov data that must be read by its matching llvm-cov.
gcov_options=()
if [[ $(uname) == 'Darwin' && -z ${GCOV:-} ]]; then
  gcov_options=(--gcov-executable 'xcrun llvm-cov gcov')
fi
# Optional gcovr arguments include a build directory, so independent coverage runs
# do not accidentally merge stale counters from every build in the checkout.
gcovr -r . -f='src/' -f='include/' "${gcov_options[@]}" --xml-pretty \
  --merge-mode-functions=merge-use-line-min -o ./result/coverage.xml "$@"
