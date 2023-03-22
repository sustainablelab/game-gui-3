#include <cstdio>
#include "SDL.h"
#include "mg_Test.h"
#include "mg_colors.h"

void run_examples_for_mg_colors()
{
    { // List all color names
        printf("Greyscale colors from dark to light:\n");
        int i=0;
        for(; i<Colors::TOFFEE; i++)
        {
            printf("- %s\n",Colors::name[i]);
        }
        printf("Colorful colors:\n");
        for(; i<Colors::DRESS; i++)
        {
            printf("- %s\n",Colors::name[i]);
        }
    }
}

void run_tests_for_mg_colors()
{
    { // Placeholder : example test
        TESTeq(1,2);
    }
}
