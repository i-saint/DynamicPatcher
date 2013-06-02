// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <cstdio>
#include <clocale>
#include <algorithm>

#include "../DynamicPatcher.h"


dpNoInline bool GetEndFlag()
{
    return false;
}

dpNoInline void Test_ThisMaybeOverridden()
{
    printf("Test_ThisMaybeOverridden()\n");
}

int main(int argc, char *argv[])
{
    printf("DynamicPatcher Test_Injected\n");
    {
        while(!GetEndFlag()) {
            Test_ThisMaybeOverridden();
            ::Sleep(3000);
        }
    }
}

