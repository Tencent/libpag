#!/bin/bash -e
cd $(dirname $0)
# install all the tools required for the autotest.

if [[ `uname` == 'Darwin' ]]; then
  MAC_REQUIRED_TOOLS="node cmake ninja yasm git-lfs"
  for TOOL in ${MAC_REQUIRED_TOOLS[@]}; do
    if [ ! $(which $TOOL) ]; then
      if [ ! $(which brew) ]; then
        echo "Homebrew not found. Trying to install..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
      fi
      echo "$TOOL not found. Trying to install..."
      brew install $TOOL
    fi
  done
fi

