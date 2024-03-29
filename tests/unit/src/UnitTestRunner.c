
#include <stdio.h>

#include "CuTest.h"

CuSuite *CuSuite_for_ast_expr(void);
CuSuite *CuSuite_for_lex(void);

int RunAllTests(void){
    printf("Running all unit tests:\n");

    CuString *output = CuStringNew();
    CuSuite* suite = CuSuiteNew();

    CuSuiteAddSuite(suite, CuSuite_for_ast_expr());
    CuSuiteAddSuite(suite, CuSuite_for_lex());

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);
    return suite->failCount;
}

int main(void){
    return RunAllTests();
}
