#!/bin/sh

# Script for automatically cleaning up #includes with the program 'include-what-you-use'
# Usage: `dev/iwyu.sh`

make -k CC="include-what-you-use -Xiwyu --mapping_file=dev/iwyu_mappings.imp" 2> dev/iwyu.out
fix_includes.py --nocomments --reorder < dev/iwyu.out
