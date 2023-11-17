#!/bin/bash -e
cd $(dirname $0)

if [[ `uname` == 'Darwin' ]]; then
  MAC_REQUIRED_TOOLS="node cmake ninja yasm git-lfs"
  for TOOL in ${MAC_REQUIRED_TOOLS[@]}; do
  if [ ! $(which $TOOL) ]; then
    if [ ! $(which brew) ]; then
      echo "Homebrew not found. Trying to install..."
      /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" ||
        exit 1
    fi
    echo "$TOOL not found. Trying to install..."
    brew install $TOOL || exit 1
  fi
  done
fi

NODE_REQUIRED_TOOLS="depsync"

for TOOL in ${NODE_REQUIRED_TOOLS[@]}; do
  if [ ! $(which $TOOL) ]; then
    echo "$TOOL not found. Trying to install..."
    npm install -g $TOOL > /dev/null
  fi
done

npm update -g depsync --silent