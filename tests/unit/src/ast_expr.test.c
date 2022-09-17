
#include "CuTest.h"

#include "UTIL/ground.h"
#include "UTIL/util.h"
#include "UTIL/string.h"
#include "AST/ast_expr.h"
#include "AST/ast_type.h"
#include "AST/TYPE/ast_type_make.h"

static void TEST_ast_expr_call_str_1(CuTest *test){
    // Parameters
    bool is_tentative = false;
    length_t args_length = 0;
    ast_expr_t **args = malloc(sizeof(ast_expr_t*) * args_length);

    // Create call expression
    ast_expr_t *call_expr = ast_expr_create_call(strclone("function_name"), args_length, args, is_tentative, NULL, NULL_SOURCE);

    // Validate stringified representation
    strong_cstr_t actual = ast_expr_str(call_expr);
    CuAssertStrEquals(test, "function_name()", actual);

    // Cleanup
    free(actual);
    ast_expr_free_fully(call_expr);
}

static void TEST_ast_expr_call_str_2(CuTest *test){
    // Parameters
    bool is_tentative = false;
    length_t args_length = 1;
    ast_expr_t **args = malloc(sizeof(ast_expr_t*) * args_length);

    args[0] = ast_expr_create_long(12345, NULL_SOURCE);

    // Create call expression
    ast_expr_t *call_expr = ast_expr_create_call(strclone("f"), args_length, args, is_tentative, NULL, NULL_SOURCE);

    // Validate stringified representation
    strong_cstr_t actual = ast_expr_str(call_expr);
    CuAssertStrEquals(test, "f(12345sl)", actual);

    // Cleanup
    free(actual);
    ast_expr_free_fully(call_expr);
}

static void TEST_ast_expr_call_str_3(CuTest *test){
    // Parameters
    bool is_tentative = false;
    length_t args_length = 2;
    ast_expr_t **args = malloc(sizeof(ast_expr_t*) * args_length);

    args[0] = ast_expr_create_long(12345, NULL_SOURCE);
    args[1] = ast_expr_create_long(67890, NULL_SOURCE);

    // Create call expression
    ast_expr_t *call_expr = ast_expr_create_call(strclone("iAmAFunction"), args_length, args, is_tentative, NULL, NULL_SOURCE);

    // Validate stringified representation
    strong_cstr_t actual = ast_expr_str(call_expr);
    CuAssertStrEquals(test, "iAmAFunction(12345sl, 67890sl)", actual);

    // Cleanup
    free(actual);
    ast_expr_free_fully(call_expr);
}

static void TEST_ast_expr_call_str_4(CuTest *test){
    // Parameters
    bool is_tentative = false;
    length_t args_length = 3;
    ast_expr_t **args = malloc(sizeof(ast_expr_t*) * args_length);

    args[0] = ast_expr_create_long(12345, NULL_SOURCE);
    args[1] = ast_expr_create_long(67890, NULL_SOURCE);
    args[2] = ast_expr_create_long(246810, NULL_SOURCE);

    // Create call expression
    ast_expr_t *call_expr = ast_expr_create_call(strclone("argargarg"), args_length, args, is_tentative, NULL, NULL_SOURCE);

    // Validate stringified representation
    strong_cstr_t actual = ast_expr_str(call_expr);
    CuAssertStrEquals(test, "argargarg(12345sl, 67890sl, 246810sl)", actual);

    // Cleanup
    free(actual);
    ast_expr_free_fully(call_expr);
}

static void TEST_ast_expr_call_str_5(CuTest *test){
    // Parameters
    bool is_tentative = true;
    length_t args_length = 3;
    ast_expr_t **args = malloc(sizeof(ast_expr_t*) * args_length);

    args[0] = ast_expr_create_long(12345, NULL_SOURCE);
    args[1] = ast_expr_create_long(67890, NULL_SOURCE);
    args[2] = ast_expr_create_long(246810, NULL_SOURCE);

    // Create call expression
    ast_expr_t *call_expr = ast_expr_create_call(strclone("threeArgs_tentative"), args_length, args, is_tentative, NULL, NULL_SOURCE);

    // Validate stringified representation
    strong_cstr_t actual = ast_expr_str(call_expr);
    CuAssertStrEquals(test, "threeArgs_tentative?(12345sl, 67890sl, 246810sl)", actual);

    // Cleanup
    free(actual);
    ast_expr_free_fully(call_expr);
}

static void TEST_ast_expr_call_str_6(CuTest *test){
    // Parameters
    bool is_tentative = false;
    length_t args_length = 3;
    ast_expr_t **args = malloc(sizeof(ast_expr_t*) * args_length);
    ast_type_t gives = ast_type_make_base(strclone("ResultingType"));

    args[0] = ast_expr_create_long(12345, NULL_SOURCE);
    args[1] = ast_expr_create_long(67890, NULL_SOURCE);
    args[2] = ast_expr_create_long(246810, NULL_SOURCE);

    // Create call expression
    ast_expr_t *call_expr = ast_expr_create_call(strclone("request_return_type"), args_length, args, is_tentative, &gives, NULL_SOURCE);

    // Validate stringified representation
    strong_cstr_t actual = ast_expr_str(call_expr);
    CuAssertStrEquals(test, "request_return_type(12345sl, 67890sl, 246810sl) ~> ResultingType", actual);

    // Cleanup
    free(actual);
    ast_expr_free_fully(call_expr);
}

static void TEST_ast_expr_call_str_7(CuTest *test){
    // Parameters
    bool is_tentative = true;
    length_t args_length = 3;
    ast_expr_t **args = malloc(sizeof(ast_expr_t*) * args_length);
    ast_type_t gives = ast_type_make_base(strclone("ResultingType"));

    args[0] = ast_expr_create_long(11111, NULL_SOURCE);
    args[1] = ast_expr_create_long(22222, NULL_SOURCE);
    args[2] = ast_expr_create_long(33333, NULL_SOURCE);

    // Create call expression
    ast_expr_t *call_expr = ast_expr_create_call(strclone("three_args_tentative_and_giving"), args_length, args, is_tentative, &gives, NULL_SOURCE);

    // Validate stringified representation
    strong_cstr_t actual = ast_expr_str(call_expr);
    CuAssertStrEquals(test, "three_args_tentative_and_giving?(11111sl, 22222sl, 33333sl) ~> ResultingType", actual);

    // Cleanup
    free(actual);
    ast_expr_free_fully(call_expr);
}

static void TEST_ast_expr_call_str_8(CuTest *test){
    // Parameters
    bool is_tentative = true;
    length_t args_length = 1;
    ast_expr_t **args = malloc(sizeof(ast_expr_t*) * args_length);
    ast_type_t gives = ast_type_make_base(strclone("Return"));

    args[0] = ast_expr_create_long(13579, NULL_SOURCE);

    // Create call expression
    ast_expr_t *call_expr = ast_expr_create_call(strclone("tentative_and_gives_with_single_arg"), args_length, args, is_tentative, &gives, NULL_SOURCE);

    // Validate stringified representation
    strong_cstr_t actual = ast_expr_str(call_expr);
    CuAssertStrEquals(test, "tentative_and_gives_with_single_arg?(13579sl) ~> Return", actual);

    // Cleanup
    free(actual);
    ast_expr_free_fully(call_expr);
}

static void TEST_ast_expr_call_str_9(CuTest *test){
    // Parameters
    bool is_tentative = false;
    length_t args_length = 1;
    ast_expr_t **args = malloc(sizeof(ast_expr_t*) * args_length);

    args[0] = ast_expr_create_long(12345, NULL_SOURCE);

    // Test using super long function name
    length_t function_name_length = 1234567;
    strong_cstr_t function_name = malloc(function_name_length + 1);
    for(length_t i = 0; i < function_name_length; i++){
        function_name[i] = 'a' + (i * i + i / 26) % 26;
    }
    function_name[function_name_length] = '\0';

    // Create call expression
    strong_cstr_t expected = mallocandsprintf("%s(12345sl)", function_name);
    ast_expr_t *call_expr = ast_expr_create_call(function_name, args_length, args, is_tentative, NULL, NULL_SOURCE);

    // Validate stringified representation
    strong_cstr_t actual = ast_expr_str(call_expr);
    CuAssertStrEquals(test, expected, actual);

    // Cleanup
    free(expected);
    free(actual);
    ast_expr_free_fully(call_expr);
}

CuSuite *CuSuite_for_ast_expr(){
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, TEST_ast_expr_call_str_1);
    SUITE_ADD_TEST(suite, TEST_ast_expr_call_str_2);
    SUITE_ADD_TEST(suite, TEST_ast_expr_call_str_3);
    SUITE_ADD_TEST(suite, TEST_ast_expr_call_str_4);
    SUITE_ADD_TEST(suite, TEST_ast_expr_call_str_5);
    SUITE_ADD_TEST(suite, TEST_ast_expr_call_str_6);
    SUITE_ADD_TEST(suite, TEST_ast_expr_call_str_7);
    SUITE_ADD_TEST(suite, TEST_ast_expr_call_str_8);
    SUITE_ADD_TEST(suite, TEST_ast_expr_call_str_9);
    return suite;
}
