
#include "CuTest.h"
#include "CuTestExtras.h"

#include "UTIL/ground.h"
#include "UTIL/util.h"
#include "UTIL/string.h"
#include "DRVR/compiler.h"
#include "LEX/lex.h"

static void TEST_lex_1(CuTest *test){
    compiler_t compiler;
    compiler_init(&compiler);

    object_t *object = compiler_new_object(&compiler);
    object->filename = strclone("fake_filename.adept");
    object->full_filename = strclone("fake_filename.adept");
    object->buffer = strclone("1 + 2 * 3 / 4\n");
    object->buffer_length = strlen(object->buffer);

    CuAssert(test, "Failed to lex", lex_buffer(&compiler, object) == SUCCESS);

    tokenid_t expected_token_ids[] = {
        TOKEN_GENERIC_INT, TOKEN_ADD, TOKEN_GENERIC_INT, TOKEN_MULTIPLY,
        TOKEN_GENERIC_INT, TOKEN_DIVIDE, TOKEN_GENERIC_INT, TOKEN_NEWLINE
    };
    source_t expected_sources[] = {
        {.object_index = 0, .index = 0, .stride = 1},
        {.object_index = 0, .index = 2, .stride = 1},
        {.object_index = 0, .index = 4, .stride = 1},
        {.object_index = 0, .index = 6, .stride = 1},
        {.object_index = 0, .index = 8, .stride = 1},
        {.object_index = 0, .index = 10, .stride = 1},
        {.object_index = 0, .index = 12, .stride = 1},
        {.object_index = 0, .index = 13, .stride = 1},
    };

    CuAssertTrue(test, object->tokenlist.length == sizeof(expected_token_ids) / sizeof(tokenid_t));

    for(length_t i = 0; i < object->tokenlist.length; i++){
        CuAssertTrue(test, object->tokenlist.tokens[i].id == expected_token_ids[i]);
        
        source_t actual = object->tokenlist.sources[i];
        source_t expected = expected_sources[i];

        CuAssertIntEquals_Msgf(test, "incorrect sources[%d].object_index", expected.object_index, actual.object_index, i);
        CuAssertIntEquals_Msgf(test, "incorrect sources[%d].index", expected.index, actual.index, i);
        CuAssertIntEquals_Msgf(test, "incorrect sources[%d].stride", expected.stride, actual.stride, i);
    }

    compiler_free(&compiler);
}

static void TEST_lex_2(CuTest *test){
    compiler_t compiler;
    compiler_init(&compiler);

    object_t *object = compiler_new_object(&compiler);
    object->filename = strclone("fake_filename.adept");
    object->full_filename = strclone("fake_filename.adept");
    object->buffer = strclone("func main() {\nprint(\"Hello World\")\n}\n");
    object->buffer_length = strlen(object->buffer);

    CuAssert(test, "Failed to lex", lex_buffer(&compiler, object) == SUCCESS);

    tokenid_t expected_token_ids[] = {
        TOKEN_FUNC, TOKEN_WORD, TOKEN_OPEN, TOKEN_CLOSE, TOKEN_BEGIN, TOKEN_NEWLINE,
        TOKEN_WORD, TOKEN_OPEN, TOKEN_STRING, TOKEN_CLOSE, TOKEN_NEWLINE,
        TOKEN_END, TOKEN_NEWLINE
    };
    source_t expected_sources[] = {
        {.object_index = 0, .index = 0, .stride = 4},
        {.object_index = 0, .index = 5, .stride = 4},
        {.object_index = 0, .index = 9, .stride = 1},
        {.object_index = 0, .index = 10, .stride = 1},
        {.object_index = 0, .index = 12, .stride = 1},
        {.object_index = 0, .index = 13, .stride = 1},
        {.object_index = 0, .index = 14, .stride = 5},
        {.object_index = 0, .index = 19, .stride = 1},
        {.object_index = 0, .index = 20, .stride = 13},
        {.object_index = 0, .index = 33, .stride = 1},
        {.object_index = 0, .index = 34, .stride = 1},
        {.object_index = 0, .index = 35, .stride = 1},
        {.object_index = 0, .index = 36, .stride = 1},
    };

    CuAssertIntEquals(test, object->tokenlist.length, sizeof(expected_token_ids) / sizeof(tokenid_t));
    CuAssertIntEquals(test, sizeof(expected_token_ids) / sizeof(tokenid_t), sizeof(expected_sources) / sizeof(source_t));

    for(length_t i = 0; i < object->tokenlist.length; i++){
        CuAssertTrue(test, object->tokenlist.tokens[i].id == expected_token_ids[i]);

        source_t actual = object->tokenlist.sources[i];
        source_t expected = expected_sources[i];

        CuAssertIntEquals_Msgf(test, "incorrect sources[%d].object_index", expected.object_index, actual.object_index, i);
        CuAssertIntEquals_Msgf(test, "incorrect sources[%d].index", expected.index, actual.index, i);
        CuAssertIntEquals_Msgf(test, "incorrect sources[%d].stride", expected.stride, actual.stride, i);
    }

    compiler_free(&compiler);
}

CuSuite *CuSuite_for_lex(){
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, TEST_lex_1);
    SUITE_ADD_TEST(suite, TEST_lex_2);
    return suite;
}
