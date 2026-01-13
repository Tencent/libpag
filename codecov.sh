#!/bin/bash
cd $(dirname $0)

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
gcovr -r . -f='src/' -f='include/' --xml-pretty --merge-mode-functions=merge-use-line-0 -o ./result/coverage.xml
if test $? -ne 0; then
   exit 1
fi
