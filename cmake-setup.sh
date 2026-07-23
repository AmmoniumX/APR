#!/bin/bash
# Configure debug and release presets, then symlink compile_commands.json to project root
set -e
rm -f ./compile_commands.json
cmake --preset debug
cmake --preset release
ln -s build/debug/compile_commands.json ./compile_commands.json
