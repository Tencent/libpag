#!/usr/bin/env bash
if [[ $(uname) == 'Darwin' ]]; then
  MAC_REQUIRED_TOOLS="python3"
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
  clangformat=`clang-format --version`
  if [[ $clangformat =~ "14." ]]
  then
      echo "----$clangformat----"
  else
      echo "----install clang-format----"
      pip3 install clang-format==14
  fi
fi

echo "----begin to scan code format----"
find include/ -iname '*.h' -print0 | xargs clang-format -i
# shellcheck disable=SC2038
find src -name "*.cpp" -print  -o -name "*.h" -print  -o -name "*.mm" -print  -o -name "*.m" -print | xargs clang-format -i
# shellcheck disable=SC2038
find test \( -path test/framework/lzma \) -prune -o -name "*.cpp" -print  -o -name "*.h" -print | xargs clang-format -i
find viewer/src  -name "*.cpp" -print  -o -name "*.h" -print  -o -name "*.mm" -print -o -name "*.m" -print -o -name "*.hpp" -print | xargs clang-format -i
find exporter/src  -name "*.cpp" -print  -o -name "*.h" -print  -o -name "*.mm" -print -o -name "*.m" -print -o -name "*.hpp" -print | xargs clang-format -i

git diff
result=`git diff`
if [[ $result =~ "diff" ]]
then
    echo "----Failed to pass coding specification----"
    exit 1
else
    echo "----Pass coding specification----"
fi

echo "----Complete the scan code format-----"

