#!/bin/bash
cd "$(dirname "$0")" || exit 1

if [[ $(uname) == 'Darwin' ]]; then
  if ! command -v gcovr >/dev/null 2>&1; then
    if ! command -v brew >/dev/null 2>&1; then
      echo "Homebrew not found. Trying to install..."
      /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    echo "gcovr not found. Trying to install..."
    brew install gcovr
  fi
fi

mkdir -p result || exit 1
# Optional gcovr arguments can select a single instrumented build directory and add reports:
# ./codecov.sh build --json result/coverage.json --html-details result/coverage.html
exec gcovr -r . -f='src/' -f='include/' --xml-pretty \
  --merge-mode-functions=merge-use-line-min -o ./result/coverage.xml "$@"
