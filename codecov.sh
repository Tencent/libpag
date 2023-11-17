#!/bin/bash -e
cd $(dirname $0)

if [ `uname` == 'Darwin' ]; then
  if [ ! $(which gcovr) ]; then
    if [ ! $(which brew) ]; then
      echo "Homebrew not found. Trying to install..."
      /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" ||
        exit 1
    fi
    echo "gcovr not found. Trying to install..."
    brew install gcovr || exit 1
  fi
fi

mkdir -p result
gcovr -r . -f='src/' -f='include/' --xml-pretty -o ./result/coverage.xml