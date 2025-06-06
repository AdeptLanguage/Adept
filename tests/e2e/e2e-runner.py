#!/usr/bin/python3

import sys
import time
from os.path import join, dirname, abspath
from framework import test, e2e_framework_run

e2e_root_dir = dirname(abspath(__file__))
src_dir = join(e2e_root_dir, "src")

def run_all_tests():
    executable = sys.argv[1]
    compiles = lambda _: True
    
    test("Adept",
        [executable],
        lambda output: b"     /\xe2\xe2\\\n    /    \\    \n   /      \\    \n  /   /\\   \\        The Adept Compiler v2.8 - (c) 2016-2024 Isaac Shelton\n /   /\\__   \\\n/___/    \\___\\\n\nUsage: adept [options] [filename]\n\nOptions:\n    -h, --help        Display this message\n    -e                Execute resulting executable\n    -w                Disable compiler warnings\n    -o FILENAME       Output to FILENAME (relative to working directory)\n    -n FILENAME       Output to FILENAME (relative to file)\n    -c                Emit object file\n    -O0,-O1,-O2,-O3   Set optimization level\n    --windowed        Don't open console with executable (only applies to Windows)\n    -std=2.x          Set standard library version\n    --version         Display compiler version\n    --root            Display root folder\n    --help-advanced   Show lesser used compiler flags\n" in output
    )
    test("address", [executable, join(src_dir, "address/main.adept")], compiles)
    test("aliases", [executable, join(src_dir, "aliases/main.adept")], compiles)
    test("aliases_polymorphic", [executable, join(src_dir, "aliases_polymorphic/main.adept")], compiles)
    test("alignof", [executable, join(src_dir, "alignof/main.adept")], compiles)
    test("andor", [executable, join(src_dir, "andor/main.adept")], compiles)
    test("andor_circuit", [executable, join(src_dir, "andor_circuit/main.adept")], compiles)
    test("anonymous_composites", [executable, join(src_dir, "anonymous_composites/main.adept")], compiles)
    test("anonymous_enums", [executable, join(src_dir, "anonymous_enums/main.adept")], compiles)
    test("anonymous_enums layout check",
        [executable, join(src_dir, "anonymous_enums/main.adept"), "-e"],
        lambda output: b"0\n1\n2\nenum (A, B, C)\n - member 0 is A\n - member 1 is B\n - member 2 is C\nA\nB\nC\n" in output)
    test("anonymous_fields", [executable, join(src_dir, "anonymous_fields/main.adept")], compiles)
    test("any_fixed_array", [executable, join(src_dir, "any_fixed_array/main.adept")], compiles)
    test("any_function_pointer", [executable, join(src_dir, "any_function_pointer/main.adept")], compiles)
    test("any_type_as", [executable, join(src_dir, "any_type_as/main.adept")], compiles)
    test("any_type_info", [executable, join(src_dir, "any_type_info/main.adept")], compiles)
    test("any_type_inventory", [executable, join(src_dir, "any_type_inventory/main.adept")], compiles)
    test("any_type_kind", [executable, join(src_dir, "any_type_kind/main.adept")], compiles)
    test("any_type_list", [executable, join(src_dir, "any_type_list/main.adept")], compiles)
    test("any_type_offsets", [executable, join(src_dir, "any_type_offsets/main.adept")], compiles)
    test("any_type_sizes", [executable, join(src_dir, "any_type_sizes/main.adept")], compiles)
    test("array_access", [executable, join(src_dir, "array_access/main.adept")], compiles)
    test("as", [executable, join(src_dir, "as/main.adept")], compiles)
    test("assert_simple", [executable, join(src_dir, "assert_simple/main.adept")], compiles)
    test("assert_with_message", [executable, join(src_dir, "assert_with_message/main.adept")], compiles)
    test("assign_func", [executable, join(src_dir, "assign_func/main.adept")], compiles)
    test("assign_func_autogen", [executable, join(src_dir, "assign_func_autogen/main.adept")], compiles)
    test("assignment", [executable, join(src_dir, "assignment/main.adept")], compiles)
    test("at", [executable, join(src_dir, "at/main.adept")], compiles)
    test("bitwise", [executable, join(src_dir, "bitwise/main.adept")], compiles)
    test("bitwise_assign", [executable, join(src_dir, "bitwise_assign/main.adept")], compiles)
    test("break", [executable, join(src_dir, "break/main.adept")], compiles)
    test("break_to", [executable, join(src_dir, "break_to/main.adept")], compiles)
    test("cast", [executable, join(src_dir, "cast/main.adept")], compiles)
    test("character_literals", [executable, join(src_dir, "character_literals/main.adept")], compiles)
    test("circular_pointers", [executable, join(src_dir, "circular_pointers/main.adept")], compiles)
    test("class", [executable, join(src_dir, "class/main.adept")], compiles)
    test("class_extends", [executable, join(src_dir, "class_extends/main.adept")], compiles)
    test("class_extends check layout", 
        [join(src_dir, "class_extends/main")], 
        lambda output: b"Animal:\n - __vtable__\n - name\nDog:\n - __vtable__\n - name\n - age\nGoldenRetriever:\n - __vtable__\n - name\n - age" in output)
    test("class_missing_constructor",
        [executable, join(src_dir, "class_missing_constructor/main.adept")],
        lambda output: b"main.adept:2:1: error: Class is missing constructor\n  2| class ThisIsMissingConstructor ()" in output,
        expected_exitcode=1)
    test("class_virtual_methods_1", [executable, join(src_dir, "class_virtual_methods_1/main.adept")], compiles)
    test("class_virtual_methods_2", [executable, join(src_dir, "class_virtual_methods_2/main.adept")], compiles)
    test("class_virtual_methods_3_missing_override",
        [executable, join(src_dir, "class_virtual_methods_3_missing_override/main.adept")],
        lambda output: b"main.adept:20:5: error: Method is used as virtual dispatchee but is missing 'override' keyword\n  20|     func getName *ubyte {" in output,
        expected_exitcode=1)
    test("class_virtual_methods_4", [executable, join(src_dir, "class_virtual_methods_4/main.adept")], compiles)
    test("class_virtual_methods_5", [executable, join(src_dir, "class_virtual_methods_5/main.adept")], compiles)
    test("class_virtual_methods_6", [executable, join(src_dir, "class_virtual_methods_6/main.adept")], compiles)
    test("class_virtual_methods_7_incorrect_return_type",
        [executable, join(src_dir, "class_virtual_methods_7_incorrect_return_type/main.adept")],
        lambda output: b"main.adept:23:27: error: Incorrect return type for method override, expected '*ubyte'\n  23|     override func getName int {\n                                ^^^" in output,
        expected_exitcode=1)
    test("class_virtual_methods_8_unused_override",
        [executable, join(src_dir, "class_virtual_methods_8_unused_override/main.adept")],
        lambda output: b"main.adept:10:5: error: No corresponding virtual method exists to override\n  10|     override func myUnusedOverride {\n          ^^^^^^^^" in output,
        expected_exitcode=1)
    test("class_virtual_methods_9", [executable, join(src_dir, "class_virtual_methods_9/main.adept")], compiles)
    test("colons_alternative_syntax", [executable, join(src_dir, "colons_alternative_syntax/main.adept")], compiles)
    test("complement", [executable, join(src_dir, "complement/main.adept")], compiles)
    test("complex_composite_rtti", [executable, join(src_dir, "complex_composite_rtti/main.adept")], compiles)
    test("conditionless_block", [executable, join(src_dir, "conditionless_block/main.adept")], compiles)
    test("conditionless_break_label", [executable, join(src_dir, "conditionless_break_label/main.adept")], compiles)
    test("const_variables", [executable, join(src_dir, "const_variables/main.adept")], compiles)
    test("constructor", [executable, join(src_dir, "constructor/main.adept")], compiles)
    test("constructor_in_only",
        [executable, join(src_dir, "constructor_in_only/main.adept")],
        lambda output: b"main.adept:20:20: error: Undeclared function 'Thing'\n  20|     thing2 Thing = Thing()" in output,
        expected_exitcode=1)
    test("constructor_with_defaults", [executable, join(src_dir, "constructor_with_defaults/main.adept")], compiles)
    test("continue", [executable, join(src_dir, "continue/main.adept")], compiles)
    test("continue_to", [executable, join(src_dir, "continue_to/main.adept")], compiles)
    test("default_args", [executable, join(src_dir, "default_args/main.adept")], compiles)
    test("defer", [executable, join(src_dir, "defer/main.adept")], compiles)
    test("defer_auto_noop", [executable, join(src_dir, "defer_auto_noop/main.adept")], compiles)
    test("defer_global", [executable, join(src_dir, "defer_global/main.adept")], compiles)
    test("deprecated", [executable, join(src_dir, "deprecated/main.adept")], compiles)
    test("disallow",
        [executable, join(src_dir, "disallow/main.adept")],
        lambda output: b"main.adept:20:11: error: Cannot use disallowed 'func print(this *Thing, value int) void = delete'\n  20|     thing.print(1234)\n                ^^^^^\n" in output,
        expected_exitcode=1
    )
    test("disallow_assignment",
        [executable, join(src_dir, "disallow_assignment/main.adept")],
        lambda output: b"main.adept:11:5: error: Assignment for type 'Thing' is not allowed\n  11|     a = b\n          ^\n" in output,
        expected_exitcode=1
    )
    test("disallow_assignment_container",
        [executable, join(src_dir, "disallow_assignment_container/main.adept")],
        lambda output: b"main.adept:13:5: error: Assignment for type 'ThingContainer' is not allowed\n  13|     a = b\n          ^\n" in output,
        expected_exitcode=1
    )
    test("disallow_funcaddr",
        [executable, join(src_dir, "disallow_funcaddr/main.adept")],
        lambda output: b"main.adept:6:38: error: Cannot use disallowed 'func disallowedFunction(value int) int = delete'\n  6|     function_pointer func(int) int = func &disallowedFunction\n                                          ^^^^\n" in output,
        expected_exitcode=1
    )
    test("disallow_poly",
        [executable, join(src_dir, "disallow_poly/main.adept")],
        lambda output: b"main.adept:6:5: error: Cannot call disallowed 'func testWithoutBody(value $T) void = delete'\n  6|     testWithoutBody(10)\n         ^^^^^^^^^^^^^^^" in output,
        expected_exitcode=1
    )
    test("dropped_values", [executable, join(src_dir, "dropped_values/main.adept")], compiles)
    test("dylib (library)", [executable, join(src_dir, "dylib/main.adept")], compiles)
    test("dylib (usage)", [executable, join(src_dir, "dylib/usage.adept")], compiles)
    test("each_in", [executable, join(src_dir, "each_in/main.adept")], compiles)
    test("each_in_fixed", [executable, join(src_dir, "each_in_fixed/main.adept")], compiles)
    test("each_in_list", [executable, join(src_dir, "each_in_list/main.adept")], compiles)
    test("each_in_static", [executable, join(src_dir, "each_in_static/main.adept")], compiles)
    test("either_way_multiply", [executable, join(src_dir, "either_way_multiply/main.adept")], compiles)
    test("elif", [executable, join(src_dir, "elif/main.adept")], compiles)
    test("embed", [executable, join(src_dir, "embed/main.adept")], compiles)
    test("entry_point", [executable, join(src_dir, "entry_point/main.adept")], compiles)
    test("enums", [executable, join(src_dir, "enums/main.adept")], compiles)
    test("enums_foreign", [executable, join(src_dir, "enums_foreign/main.adept")], compiles)
    test("enums_relaxed_syntax", [executable, join(src_dir, "enums_relaxed_syntax/main.adept")], compiles)
    test("equals_func", [executable, join(src_dir, "equals_func/main.adept")], compiles)
    test("external", [executable, join(src_dir, "external/main.adept")], compiles)
    test("fallthrough", [executable, join(src_dir, "fallthrough/main.adept")], compiles)
    test("fixed_array", [executable, join(src_dir, "fixed_array/main.adept")], compiles)
    test("fixed_array_alternative_syntax", [executable, join(src_dir, "fixed_array_alternative_syntax/main.adept")], compiles)
    test("fixed_array_assign", [executable, join(src_dir, "fixed_array_assign/main.adept")], compiles)
    test("fixed_array_deference", [executable, join(src_dir, "fixed_array_deference/main.adept")], compiles)
    test("fixed_array_pass", [executable, join(src_dir, "fixed_array_pass/main.adept")], compiles)
    test("fixed_array_var_size", [executable, join(src_dir, "fixed_array_var_size/main.adept")], compiles)
    test("fixed_array_var_size_field", [executable, join(src_dir, "fixed_array_var_size_field/main.adept")], compiles)
    test("for", [executable, join(src_dir, "for/main.adept")], compiles)
    test("foreign_func_optional_names", [executable, join(src_dir, "foreign_func_optional_names/main.adept")], compiles)
    test("foreign_in_namespace", [executable, join(src_dir, "foreign_in_namespace/main.adept")], compiles)
    test("func_aliases", [executable, join(src_dir, "func_aliases/main.adept")], compiles)
    test("func_aliases_va", [executable, join(src_dir, "func_aliases_va/main.adept")], compiles)
    test("funcaddr", [executable, join(src_dir, "funcaddr/main.adept")], compiles)
    test("funcaddr_autogen", [executable, join(src_dir, "funcaddr_autogen/main.adept")], compiles)
    test("funcaddr_autogen_noop_defer", [executable, join(src_dir, "funcaddr_autogen_noop_defer/main.adept")], compiles)
    test("funcaddrnull", [executable, join(src_dir, "funcaddrnull/main.adept")], compiles)
    test("funcptr", [executable, join(src_dir, "funcptr/main.adept")], compiles)
    test("functions", [executable, join(src_dir, "functions/main.adept")], compiles)
    test("globals", [executable, join(src_dir, "globals/main.adept")], compiles)
    test("hello_world", [executable, join(src_dir, "hello_world/main.adept")], compiles)
    test("hexadecimal", [executable, join(src_dir, "hexadecimal/main.adept")], compiles)
    test("idx_manipulation", [executable, join(src_dir, "idx_manipulation/main.adept")], compiles)
    test("if", [executable, join(src_dir, "if/main.adept")], compiles)
    test("ifelse", [executable, join(src_dir, "ifelse/main.adept")], compiles)
    test("import", [executable, join(src_dir, "import/main.adept")], compiles)
    test("import_std", [executable, join(src_dir, "import_std/main.adept")], compiles)
    test("import_std_c_like", [executable, join(src_dir, "import_std_c_like/main.adept")], compiles)
    test("increment", [executable, join(src_dir, "increment/main.adept")], compiles)
    test("increment_stmt", [executable, join(src_dir, "increment_stmt/main.adept")], compiles)
    test("initializer_list", [executable, join(src_dir, "initializer_list/main.adept")], compiles)
    test("initializer_list_abstract", [executable, join(src_dir, "initializer_list_abstract/main.adept")], compiles)
    test("initializer_list_fixed", [executable, join(src_dir, "initializer_list_fixed/main.adept")], compiles)
    test("inline_declaration", [executable, join(src_dir, "inline_declaration/main.adept")], compiles)
    test("inner_struct", [executable, join(src_dir, "inner_struct/main.adept")], compiles)
    test("inner_struct_polymorphic", [executable, join(src_dir, "inner_struct_polymorphic/main.adept")], compiles)
    test("int_ptr_cast", [executable, join(src_dir, "int_ptr_cast/main.adept")], compiles)
    test("int_to_float_promotion_in_math", [executable, join(src_dir, "int_to_float_promotion_in_math/main.adept")], compiles)
    test("internal_deference", [executable, join(src_dir, "internal_deference/main.adept")], compiles)
    test("internal_deference_generic", [executable, join(src_dir, "internal_deference_generic/main.adept")], compiles)
    test("list_map", [executable, join(src_dir, "list_map/main.adept")], compiles)
    test("llvm_asm", [executable, join(src_dir, "llvm_asm/main.adept")], compiles)
    test("loose_struct_syntax", [executable, join(src_dir, "loose_struct_syntax/main.adept")], compiles)
    test("major_minor_release", [executable, join(src_dir, "major_minor_release/main.adept")], compiles)
    test("management_access", [executable, join(src_dir, "management_access/main.adept")], compiles)
    test("management_as", [executable, join(src_dir, "management_as/main.adept")], compiles)
    test("management_assign", [executable, join(src_dir, "management_assign/main.adept")], compiles)
    test("management_defer", [executable, join(src_dir, "management_defer/main.adept")], compiles)
    test("management_math", [executable, join(src_dir, "management_math/main.adept")], compiles)
    test("management_pass", [executable, join(src_dir, "management_pass/main.adept")], compiles)
    test("mathassign", [executable, join(src_dir, "mathassign/main.adept")], compiles)
    test("mathassign_managed", [executable, join(src_dir, "mathassign_managed/main.adept")], compiles)
    test("member", [executable, join(src_dir, "member/main.adept")], compiles)
    test("meta", [executable, join(src_dir, "meta/main.adept")], compiles)
    test("meta_dynamic", [executable, join(src_dir, "meta_dynamic/main.adept")], compiles)
    test("meta_get", [executable, join(src_dir, "meta_get/main.adept")], compiles)
    test("methods", [executable, join(src_dir, "methods/main.adept")], compiles)
    test("multiple_declaration", [executable, join(src_dir, "multiple_declaration/main.adept")], compiles)
    test("named_expressions", [executable, join(src_dir, "named_expressions/main.adept")], compiles)
    test("named_expressions_old_style", [executable, join(src_dir, "named_expressions_old_style/main.adept")], compiles)
    test("named_expressions_scoped", [executable, join(src_dir, "named_expressions_scoped/main.adept")], compiles)
    test("namespace_alternative_syntax", [executable, join(src_dir, "namespace_alternative_syntax/main.adept")], compiles)
    test("namespaces", [executable, join(src_dir, "namespaces/main.adept")], compiles)
    test("native_linking_windows", [executable, join(src_dir, "native_linking_windows/main.adept")], compiles, only_on='windows')
    test("negate", [executable, join(src_dir, "negate/main.adept")], compiles)
    test("new_cstring", [executable, join(src_dir, "new_cstring/main.adept")], compiles)
    test("new_delete", [executable, join(src_dir, "new_delete/main.adept")], compiles)
    test("new_dynamic", [executable, join(src_dir, "new_dynamic/main.adept")], compiles)
    test("new_undef", [executable, join(src_dir, "new_undef/main.adept")], compiles)
    test("newline_tolerance", [executable, join(src_dir, "newline_tolerance/main.adept")], compiles)
    test("no_discard",
        [executable,
        join(src_dir, "no_discard/main.adept")],
        lambda output: b"main.adept:17:19: error: Not allowed to discard value returned from 'func getMessage(this *Thing) exhaustive String'\n  17|             thing.getMessage()\n                        ^^^^^^^^^^" in output,
        expected_exitcode=1
    )
    test("not", [executable, join(src_dir, "not/main.adept")], compiles)
    test("null", [executable, join(src_dir, "null/main.adept")], compiles)
    test("null_checks", [executable, join(src_dir, "null_checks/main.adept"), "--null-checks"], compiles)
    test("null_checks show runtime error",
        [join(src_dir, "null_checks/main")],
        lambda output: b"===== RUNTIME ERROR: NULL POINTER DEREFERENCE, MEMBER-ACCESS, OR ELEMENT-ACCESS! =====\nIn file:\t" in output and b"main.adept\nIn function:\ttriggerNullCheck(*int) void\nLine:\t10\nColumn:\t5" in output,
        expected_exitcode=1
    )
    test("numeric_separators",
        [executable,
        join(src_dir, "numeric_separators/main.adept"), "-e"],
        lambda output: b"123456789\n" in output
    )
    test("order", [executable, join(src_dir, "order/main.adept")], compiles)
    test("pass_func", [executable, join(src_dir, "pass_func/main.adept")], compiles)
    test("permissive_blocks", [executable, join(src_dir, "permissive_blocks/main.adept")], compiles)
    test("poly_default_args", [executable, join(src_dir, "poly_default_args/main.adept")], compiles)
    test("poly_prereq_extends", [executable, join(src_dir, "poly_prereq_extends/main.adept")], compiles)
    test("poly_prereq_extends_fail",
        [executable,
        join(src_dir, "poly_prereq_extends_fail/main.adept")],
        lambda output: b"main.adept:28:9: error: Undeclared function do_something(String)\n" in output,
        expected_exitcode=1
    )
    test("polycount", [executable, join(src_dir, "polycount/main.adept")], compiles)
    test("polymorphic_anonymous_composites", [executable, join(src_dir, "polymorphic_anonymous_composites/main.adept")], compiles)
    test("polymorphic_anonymous_composites_mismatch",
        [executable, join(src_dir, "polymorphic_anonymous_composites_mismatch/main.adept")],
        lambda output: b"Undeclared function other(*struct (a int, b float, c 20 String))" in output,
        expected_exitcode=1
    )
    test("polymorphic_functions", [executable, join(src_dir, "polymorphic_functions/main.adept")], compiles)
    test("polymorphic_inner", [executable, join(src_dir, "polymorphic_inner/main.adept")], compiles)
    test("polymorphic_methods", [executable, join(src_dir, "polymorphic_methods/main.adept")], compiles)
    test("polymorphic_prereqs", [executable, join(src_dir, "polymorphic_prereqs/main.adept")], compiles)
    test("polymorphic_structs", [executable, join(src_dir, "polymorphic_structs/main.adept")], compiles)
    test("pragma", [executable, join(src_dir, "pragma/main.adept")], compiles)
    test("primitives", [executable, join(src_dir, "primitives/main.adept")], compiles)
    test("records", [executable, join(src_dir, "records/main.adept")], compiles)
    test("records_polymorphic", [executable, join(src_dir, "records_polymorphic/main.adept")], compiles)
    test("repeat", [executable, join(src_dir, "repeat/main.adept")], compiles)
    test("repeat_args", [executable, join(src_dir, "repeat_args/main.adept")], compiles)
    test("repeat_fields", [executable, join(src_dir, "repeat_fields/main.adept")], compiles)
    test("repeat_static", [executable, join(src_dir, "repeat_static/main.adept")], compiles)
    test("repeat_using", [executable, join(src_dir, "repeat_using/main.adept")], compiles)
    test("return_every_case", [executable, join(src_dir, "return_every_case/main.adept")], compiles)
    test("return_matching", [executable, join(src_dir, "return_matching/main.adept")], compiles)
    test("return_ten", [executable, join(src_dir, "return_ten/main.adept")], compiles)
    test("rtti_enum",
        [executable, join(src_dir, "rtti_enum/main.adept"), "-e"],
        lambda output: b"Information about every enum used:\n  AnyTypeKind : ['VOID', 'BOOL', 'BYTE', 'UBYTE', 'SHORT', 'USHORT', 'INT', 'UINT', 'LONG', 'ULONG', 'FLOAT', 'DOUBLE', 'PTR', 'STRUCT', 'UNION', 'FUNC_PTR', 'FIXED_ARRAY', 'ENUM']\n  MyEnum : ['NONE', 'ERROR', 'WARNING', 'INFO', 'ICON']\n  Ownership : ['REFERENCE', 'OWN', 'GIVEN', 'DONOR']\n  StringOwnership : ['REFERENCE', 'OWN', 'GIVEN', 'DONOR']\n  StringReplaceNothingBehavior : ['OR_CLONE', 'OR_VIEW']\n" in output
    )
    test("runtime_resource", [executable, join(src_dir, "runtime_resource/main.adept")], compiles)
    test("scientific", [executable, join(src_dir, "scientific/main.adept")], compiles)
    test("scoped_variables", [executable, join(src_dir, "scoped_variables/main.adept")], compiles)
    test("search_path", [executable, join(src_dir, "search_path/main.adept")], compiles)
    test("similar", [executable, join(src_dir, "similar/main.adept")], compiles)
    test("sizeof", [executable, join(src_dir, "sizeof/main.adept")], compiles)
    test("sizeof_value", [executable, join(src_dir, "sizeof_value/main.adept")], compiles)
    test("small_functions", [executable, join(src_dir, "small_functions/main.adept")], compiles)
    test("standard", [executable, join(src_dir, "standard/main.adept")], compiles)
    test("static_arrays", [executable, join(src_dir, "static_arrays/main.adept")], compiles)
    test("static_structs", [executable, join(src_dir, "static_structs/main.adept")], compiles)
    test("static_variables", [executable, join(src_dir, "static_variables/main.adept")], compiles)
    test("stdcall", [executable, join(src_dir, "stdcall/main.adept")], compiles, only_on='windows')
    test("string", [executable, join(src_dir, "string/main.adept")], compiles)
    test("struct_association", [executable, join(src_dir, "struct_association/main.adept")], compiles)
    test("structs", [executable, join(src_dir, "structs/main.adept")], compiles)
    test("successful", [executable, join(src_dir, "successful/main.adept")], compiles)
    test("super",
        [executable,
        join(src_dir, "super/main.adept"), "-e"],
        lambda output: b"[shape constructor]\n[circle constructor]\nName is: Circle\nX is: 21\n" in output
    )
    test("super_polymorphic",
        [executable,
        join(src_dir, "super_polymorphic/main.adept"), "-e"],
        lambda output: b"x=10.0, y=11.0\nx=a, y=b\nx=21\n" in output
    )
    test("switch", [executable, join(src_dir, "switch/main.adept")], compiles)
    test("switch_exhaustive", [executable, join(src_dir, "switch_exhaustive/main.adept")], compiles)
    test("switch_more", [executable, join(src_dir, "switch_more/main.adept")], compiles)
    test("temporary_mutable", [executable, join(src_dir, "temporary_mutable/main.adept")], compiles)
    test("tentative_function_calls", [executable, join(src_dir, "tentative_function_calls/main.adept")], compiles)
    test("tentative_method_calls", [executable, join(src_dir, "tentative_method_calls/main.adept")], compiles)
    test("terminate_join", [executable, join(src_dir, "terminate_join/main.adept")], compiles)
    test("ternary", [executable, join(src_dir, "ternary/main.adept")], compiles)
    test("ternary_circuit", [executable, join(src_dir, "ternary_circuit/main.adept")], compiles)
    test("toggle", [executable, join(src_dir, "toggle/main.adept")], compiles)
    test("truefalse", [executable, join(src_dir, "truefalse/main.adept")], compiles)
    test("typenameof", [executable, join(src_dir, "typenameof/main.adept")], compiles)
    test("undef", [executable, join(src_dir, "undef/main.adept")], compiles)
    test("union", [executable, join(src_dir, "union/main.adept")], compiles)
    test("unknown_enum_values", [executable, join(src_dir, "unknown_enum_values/main.adept")], compiles)
    test("unknown_plural_enum_values", [executable, join(src_dir, "unknown_plural_enum_values/main.adept")], compiles)
    test("unknown_plural_enum_values check",
        [join(src_dir, "unknown_plural_enum_values/main"), "-e"],
        lambda output: b"BANANA\nAPPLE\nAPPLE\nAPPLE\nBANANA\nBANANA\nORANGE\nORANGE\nORANGE\nAPPLE\nCAT\nCAT\nBAT\nBAT\nBAT\nBAT\nBAT\nBAT\nBAT\nBAT\n-----------\nCAT\nDOG\nBAT\n" in output,
    )
    test("unless", [executable, join(src_dir, "unless/main.adept")], compiles)
    test("unlesselse", [executable, join(src_dir, "unlesselse/main.adept")], compiles)
    test("unnecessary_manual_defer_call", [executable, join(src_dir, "unnecessary_manual_defer_call/main.adept")], compiles)
    test("unsupported",
        [executable,
        join(src_dir, "unsupported/main.adept")],
        lambda output: b"main.adept: This file is no longer supported or was never supported to begin with!\n" in output and b"Use 'newer/supported.adept' instead" in output,
        expected_exitcode=1
    )
    test("unterminated_comment",
        [executable,
        join(src_dir, "unterminated_comment/main.adept")],
        lambda output: b"main.adept:3:5: error: Unterminated multi-line comment\n  3|     /* todo\n         ^^" in output,
        expected_exitcode=1
    )
    test("unterminated_cstring",
        [executable,
        join(src_dir, "unterminated_cstring/main.adept")],
        lambda output: b"main.adept:3:20: error: Unterminated string literal\n  3|     value *ubyte = '\n                        ^" in output,
        expected_exitcode=1
    )
    test("unterminated_string",
        [executable,
        join(src_dir, "unterminated_string/main.adept")],
        lambda output: b"main.adept:3:20: error: Unterminated string literal\n  3|     value String = \"\n                        ^" in output,
        expected_exitcode=1
    )
    test("until", [executable, join(src_dir, "until/main.adept")], compiles)
    test("until_break", [executable, join(src_dir, "until_break/main.adept")], compiles)
    test("va_args", [executable, join(src_dir, "va_args/main.adept")], compiles)
    test("varargs", [executable, join(src_dir, "varargs/main.adept")], compiles)
    test("variables", [executable, join(src_dir, "variables/main.adept")], compiles)
    test("variadic", [executable, join(src_dir, "variadic/main.adept")], compiles)
    test("variadic_method", [executable, join(src_dir, "variadic_method/main.adept")], compiles)
    test("variadic_print", [executable, join(src_dir, "variadic_print/main.adept")], compiles)
    test("version", [executable, join(src_dir, "version/main.adept")], compiles)
    test("void_ptr", [executable, join(src_dir, "void_ptr/main.adept")], compiles)
    test("vtable_checks", [executable, join(src_dir, "vtable_checks/main.adept")], compiles)
    test("vtable_checks show runtime error",
        [join(src_dir, "vtable_checks/main")],
        lambda output: b"===== RUNTIME ERROR: MISTAKENLY CALLING VIRTUAL METHOD ON UNCONSTRUCTED INSTANCE OF CLASS! =====\nIn file:\t" in output and b"main.adept\nIn function:\tmain() void\nLine:\t27\nColumn:\t31\n\nDid you forget to construct your instance?\n - `my_instance MyClass()`\n - `my_instance *MyClass = new MyClass()`\n - `my_instance.__constructor__()`" in output,
        expected_exitcode=1
    )
    test("while", [executable, join(src_dir, "while/main.adept")], compiles)
    test("while_continue", [executable, join(src_dir, "while_continue/main.adept")], compiles)
    test("windowed", [executable, join(src_dir, "windowed/main.adept")], compiles)
    test("winmain_entry", [executable, join(src_dir, "winmain_entry/main.adept")], compiles, only_on='windows')
    test("z_curl",
        [executable,
        join(src_dir, "z_curl/main.adept"),
        "-e"],
        lambda output: b"<html>" in output and b"</html>" in output,
        expected_exitcode=0)

e2e_framework_run(run_all_tests)
