#!/bin/bash
exec > emscripten_version.txt 2>&1
emcc -v
