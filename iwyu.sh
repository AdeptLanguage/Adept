#!/bin/sh

# Script for automatically cleaning up #includes with the program 'include-what-you-use'

# With comments
# ----------------------
#make -k CC="include-what-you-use -Xiwyu --update_comments -Xiwyu --mapping_file=iwyu_mappings.imp" 2> iwyu.out
#fix_includes.py --update_comments --reorder --comments --nosafe_headers < iwyu.out
# ----------------------

# Without comments
# ----------------------
make -k CC="include-what-you-use -Xiwyu --no_comments -Xiwyu --update_comments -Xiwyu --mapping_file=iwyu_mappings.imp" 2> iwyu.out
fix_includes.py --update_comments --reorder < iwyu.out
# ----------------------
