#!/usr/bin/env bash
if [[ $(uname) == 'Darwin' ]]; then
  MAC_REQUIRED_TOOLS="python"
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
  if [[ $clangformat =~ "13." ]]
  then
      echo "----$clangformat----"
  else
      echo "----install clang-format----"
      pip install clang-format
  fi

fi

echo "----begin to scan code format----"
find include/ -iname '*.h' -print0 | xargs clang-format -i
find tgfx/include -iname '*.h' -print0 | xargs clang-format -i
# shellcheck disable=SC2038
find tgfx/src -iname "*.h" -print -o -iname "*.cpp" -print  | xargs clang-format -i
# shellcheck disable=SC2038
find src -name "*.cpp" -print  -o -name "*.h" -print  -o -name "*.mm" -print  -o -name "*.m" -print | xargs clang-format -i
# shellcheck disable=SC2038
find test \( -path test/framework/lzma \) -prune -o -name "*.cpp" -print  -o -name "*.h" -print | xargs clang-format -i

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

