#!/bin/sh

# Small script for automatically cleaning up #includes using 'include-what-you-use'
# Usage: `dev/iwyu.sh`

pushd build/macOS-Debug
iwyu_tool.py -p . -- -Xiwyu --mapping_file=../../dev/iwyu_mappings.imp > ../../dev/iwyu.out
fix_includes.py --nocomments --reorder < ../../dev/iwyu.out
popd
