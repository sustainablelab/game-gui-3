#include <cstdio>
#include "mg_Test.h"
#include "mg_colors_tests.cpp"

int main()
{
    if(1)
    { // Examples : mg_colors
        puts("Running examples...");
        run_examples_for_mg_colors();
    }
    if(0)
    { // Tests : mg_colors
        puts("Running tests...");
        run_tests_for_mg_colors();
    }
    printf("FAIL/PASS/TOTAL: %d/%d/%d\n",Tests::fail,Tests::pass,Tests::total);
    TESTeq(Tests::pass+Tests::fail,Tests::total);
}
