#!/bin/bash -e
cd $(dirname $0)
mkdir -p result
gcovr -r . -f='src/' -f='include/' --xml-pretty -o ./result/coverage.xml