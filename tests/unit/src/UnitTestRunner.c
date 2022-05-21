
#include <stdio.h>
#include <stdlib.h>
#include "CuTest.h"

CuSuite *CuSuite_for_ast_expr();
CuSuite *CuSuite_for_lex();

int RunAllTests(){
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

int main(){
    return RunAllTests();
}
