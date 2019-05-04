#!/bin/bash

set +v
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
pushd $SCRIPT_DIR > /dev/null

function compile() {
    echo "=> Compiling Test Program: '$1'"
    adept_debug $1/main.adept $2
}

compile address || exit $?
compile aliases || exit $?
compile andor || exit $?
compile any_type_as || exit $?
compile any_type_info || exit $?
compile any_type_inventory || exit $?
compile any_type_kind || exit $?
compile any_type_list || exit $?
compile array_access || exit $?
compile as || exit $?
compile assignment || exit $?
compile at || exit $?
compile bitwise || exit $?
compile break || exit $?
compile break_to || exit $?
compile cast || exit $?
compile character_literals || exit $?
compile complement || exit $?
compile conditionless_break_label || exit $?
compile constants || exit $?
compile continue || exit $?
compile continue_to || exit $?
compile defer || exit $?
compile deprecated || exit $?
compile dereference || exit $?
compile each_in || exit $?
compile enums || exit $?
compile external || exit $?
compile fixed_array || exit $?
compile fixed_array_assign || exit $?
compile funcptr || exit $?
compile functions || exit $?
compile globals || exit $?
compile hello_world || exit $?
compile hexadecimal || exit $?
compile if || exit $?
compile ifelse || exit $?
compile import || exit $?
compile inline_declaration || exit $?
compile int_ptr_cast || exit $?
compile management_assign || exit $?
compile management_defer || exit $?
compile management_math || exit $?
compile management_pass || exit $?
compile mathassign || exit $?
compile member || exit $?
compile methods || exit $?
compile multiple_declaration || exit $?

#compile native_linking || exit $?
printf "Skipping \033[0;31mnative_linking\033[0m test program\n"

compile negate || exit $?
compile new_cstring || exit $?
compile new_delete || exit $?
compile new_dynamic || exit $?
compile not || exit $?
compile null || exit $?
compile null_checks --null-checks || exit $?
./null_checks/main
compile order || exit $?
compile package || exit $?
compile package_use || exit $?
compile polymorphic_functions || exit $?
compile polymorphic_inner || exit $?
compile polymorphic_structs || exit $?
compile pragma || exit $?
compile primitives || exit $?
compile repeat || exit $?
compile repeat_args || exit $?
compile repeat_fields || exit $?
compile return_ten || exit $?
compile scoped_variables || exit $?
compile sizeof || exit $?
compile standard || exit $?
compile static_arrays || exit $?
compile static_structs || exit $?
compile stdcall || exit $?
compile string || exit $?
compile structs || exit $?
compile successful || exit $?
compile terminate_join || exit $?
compile truefalse || exit $?
compile undef || exit $?
compile unless || exit $?
compile unlesselse || exit $?
# This file should always fail
!(compile unsupported) || exit $?
compile until || exit $?
compile until_break || exit $?
compile varargs || exit $?
compile variables || exit $?
compile while || exit $?
compile while_continue || exit $?

# Delete debugging dump files if present
rm -f ast.txt
rm -f infer.txt
rm -f ir.txt

echo All tests compiled successfully!
popd > /dev/null
