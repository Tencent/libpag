#!/usr/bin/env bash

cd "$(brew --repo homebrew/core)" && git checkout d69507037bf458dd4a090dee764f8106fcd8cefd
HOMEBREW_NO_AUTO_UPDATE=1 brew install emscripten || exit 1
git checkout master
brew pin emscripten || exit 1