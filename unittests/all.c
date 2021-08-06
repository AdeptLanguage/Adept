
#include <stdio.h>
#include "CuTest.h"
#include "all.h"

int main(){
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
