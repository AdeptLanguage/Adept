#!/bin/sh

# Script for automatically cleaning up #includes with the program 'include-what-you-use'
# Usage: `misc/dev/iwyu.sh`

# With comments
# ----------------------
#make -k CC="include-what-you-use -Xiwyu --update_comments -Xiwyu --mapping_file=misc/dev/iwyu_mappings.imp" 2> iwyu.out
#fix_includes.py --update_comments --reorder --comments --nosafe_headers < iwyu.out
# ----------------------

# Without comments
# ----------------------
make -k CC="include-what-you-use -Xiwyu --no_comments -Xiwyu --update_comments -Xiwyu --mapping_file=misc/dev/iwyu_mappings.imp" 2> misc/dev/iwyu.out
fix_includes.py --update_comments --reorder < misc/dev/iwyu.out
# ----------------------
