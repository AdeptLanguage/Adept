#!/bin/bash

set +v
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
pushd $SCRIPT_DIR > /dev/null

function compile() {
    echo "=> Compiling Test Program: '$1'"
    adept_debug $1/main.adept --no-result $2
}

compile address || exit $?
compile aliases || exit $?
compile andor || exit $?
compile andor_circuit || exit $?
compile anonymous_composites || exit $?
compile anonymous_fields || exit $?
compile any_fixed_array || exit $?
compile any_function_pointer || exit $?
compile any_type_as || exit $?
compile any_type_info || exit $?
compile any_type_inventory || exit $?
compile any_type_kind || exit $?
compile any_type_list || exit $?
compile any_type_offsets || exit $?
compile any_type_sizes || exit $?
compile array_access || exit $?
compile as || exit $?
compile assign_func || exit $?
compile assignment || exit $?
compile at || exit $?
compile bitwise || exit $?
compile bitwise_assign || exit $?
compile break || exit $?
compile break_to || exit $?
compile cast || exit $?
compile character_literals || exit $?
compile circular_pointers || exit $?
compile colons_alternative_syntax || exit $?
compile complement || exit $?
compile complex_composite_rtti || exit $?
compile conditionless_break_label || exit $?
compile const_variables || exit $?
compile constants || exit $?
compile constants_old_style || exit $?
compile constants_scoped || exit $?
compile continue || exit $?
compile continue_to || exit $?
compile default_args || exit $?
compile defer || exit $?
compile defer_global || exit $?
compile deprecated || exit $?
compile dereference || exit $?
compile dropped_values || exit $?
compile each_in || exit $?
compile each_in_fixed || exit $?
compile each_in_list || exit $?
compile each_in_static || exit $?
compile either_way_multiply || exit $?
compile elif || exit $?
compile embed || exit $?
compile entry_point || exit $?
compile enums || exit $?
compile equals_func || exit $?
compile external || exit $?
compile fallthrough || exit $?
compile fixed_array || exit $?
compile fixed_array_alternative_syntax || exit $?
compile fixed_array_assign || exit $?
compile fixed_array_deference || exit $?
compile fixed_array_pass || exit $?
compile for || exit $?
compile foreign_in_namespace || exit $?
compile func_aliases || exit $?
compile func_aliases_va || exit $?
compile funcaddr || exit $?
compile funcaddr_autogen || exit $?
compile funcaddrnull || exit $?
compile funcptr || exit $?
compile functions || exit $?
compile globals || exit $?
compile hello_world || exit $?
compile hexadecimal || exit $?
compile idx_manipulation || exit $?
compile if || exit $?
compile ifelse || exit $?
compile import || exit $?
compile import_std || exit $?
compile import_std_c_like || exit $?
compile increment || exit $?
compile increment_stmt || exit $?
compile initializer_list || exit $?
compile initializer_list_abstract || exit $?
compile inline_declaration || exit $?
compile inner_struct || exit $?
compile int_ptr_cast || exit $?
compile internal_deference || exit $?
compile internal_deference_generic || exit $?
compile list_map || exit $?
compile llvm_asm || exit $?
compile management_access || exit $?
compile management_as || exit $?
compile management_assign || exit $?
compile management_defer || exit $?
compile management_math || exit $?
compile management_pass || exit $?
compile mathassign || exit $?
compile member || exit $?
compile meta_dynamic || exit $?
compile meta_get || exit $?
compile methods || exit $?
compile multiple_declaration || exit $?
compile namespace_alternative_syntax || exit $?
compile namespaces || exit $?

#compile native_linking || exit $?
printf "Skipping \033[0;31mnative_linking\033[0m test program\n"

compile negate || exit $?
compile new_cstring || exit $?
compile new_delete || exit $?
compile new_dynamic || exit $?
compile new_undef || exit $?
compile newline_tolerance || exit $?
compile not || exit $?
compile null || exit $?
compile null_checks --null-checks || exit $?
./null_checks/main
compile order || exit $?
compile package || exit $?
compile package_use || exit $?
compile pass_func || exit $?
compile permissive_blocks || exit $?
compile poly_default_args || exit $?
compile polycount || exit $?
compile polymorphic_functions || exit $?
compile polymorphic_inner || exit $?
compile polymorphic_methods || exit $?
compile polymorphic_structs || exit $?
compile pragma || exit $?
compile primitives || exit $?
compile repeat || exit $?
compile repeat_args || exit $?
compile repeat_fields || exit $?
compile repeat_static || exit $?
compile repeat_using || exit $?
compile return_matching || exit $?
compile return_ten || exit $?
compile scientific || exit $?
compile scoped_variables || exit $?
compile search_path || exit $?
compile similar || exit $?
compile sizeof || exit $?
compile sizeof_value || exit $?
compile small_functions || exit $?
compile standard || exit $?
compile static_arrays || exit $?
compile static_structs || exit $?
compile static_variables || exit $?

#compile stdcall || exit $?
printf "Skipping \033[0;31mstdcall\033[0m test program\n"

compile string || exit $?
compile structs || exit $?
compile struct_association || exit $?
compile successful || exit $?
compile switch || exit $?
compile switch_more || exit $?
compile switch_exhaustive || exit $?
compile temporary_mutable || exit $?
compile tentative_function_calls || exit $?
compile tentative_method_calls || exit $?
compile terminate_join || exit $?
compile ternary || exit $?
compile ternary_circuit || exit $?
compile toggle || exit $?
compile truefalse || exit $?
compile typenameof || exit $?
compile undef || exit $?
compile union || exit $?
compile unless || exit $?
compile unlesselse || exit $?
# This file should always fail
!(compile unsupported) || exit $?
compile until || exit $?
compile until_break || exit $?
compile va_args || exit $?
compile varargs || exit $?
compile variables || exit $?
compile variadic || exit $?
compile variadic_method || exit $?
compile variadic_print || exit $?
compile version || exit $?
compile void_ptr || exit $?
compile while || exit $?
compile while_continue || exit $?

# Delete debugging dump files if present
rm -f ast.txt
rm -f infer.txt
rm -f ir.txt

echo All tests compiled successfully!
popd > /dev/null
