// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <cstdio>

#include "../DynamicPatcher.h"

#ifdef _M_X64
#   define dpPlatform "x64"
#else
#   define dpPlatform "Win32"
#endif
#ifdef _DEBUG
#   define dpConfiguration "Debug"
#else
#   define dpConfiguration "Release"
#endif
#define dpObjDir "_tmp/Test_Inject_" dpPlatform dpConfiguration 


dpNoInline void Test_ThisMaybeOverridden()
{
    printf("Test_Inject.cpp: Overridden!\n");
    dpUpdate();
}


dpOnLoad(
    dpPrint("loaded: Test_Inject.cpp\n");

    dpAddLoadPath(dpObjDir"/*.obj");
    dpAddSourcePath("Test_Inject");
    dpStartAutoBuild("Test_Inject.vcxproj /target:ClCompile /m /p:Configuration="dpConfiguration";Platform="dpPlatform, false);

    dpPatchByAddress(&Test_ThisMaybeOverridden);
)

dpOnUnload(
    printf("unloaded: Test_Inject.cpp\n");
)
