#!/bin/bash
# Configure debug and release presets, then symlink compile_commands.json to project root
[[ -L compile_commands.json || -f compile_commands.json ]] && rm ./compile_commands.json
cmake --preset debug
cmake --preset release
ln -s build/debug/compile_commands.json ./compile_commands.json
