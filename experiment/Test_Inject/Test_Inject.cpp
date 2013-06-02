// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <cstdio>

#include "../DynamicPatcher.h"


dpNoInline void Test_ThisMaybeOverridden()
{
    printf("Test_Inject.cpp: Overridden!\n");
}


dpOnLoad(
    dpPrint("loaded: Test_Inject.cpp\n");
    dpPatchByAddress(&Test_ThisMaybeOverridden);
)

dpOnUnload(
    printf("unloaded: Test_Inject.cpp\n");
)
