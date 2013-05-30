// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <windows.h>

//#define dpDisable
//#define dpLinkStatic
#define dpLinkDynamic
#include "DynamicPatcher.h"

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

dpNoInline void OverriddenByLib1()
{
    printf("this will be overridden by lib1\n");
}

dpNoInline void OverriddenByLib2()
{
    printf("this will be overridden by lib2\n");
}

dpNoInline void OverriddenByDll()
{
    printf("this will be overridden by dll\n");
}


int main(int argc, char *argv[])
{
    dpInitialize();
    dpAddLoadPath("Test_Dll.dll");
    dpAddLoadPath("Test_Lib.lib");
    dpAddSourcePath("Test_LibDll");
    dpStartAutoBuild("Test_LibDll.sln /target:Build /m /p:Configuration="dpConfiguration";Platform="dpPlatform, false);

    printf("DynamicPatcher Test_UseLibDll\n");
    for(;;) {
        OverriddenByLib1();
        OverriddenByLib2();
        OverriddenByDll();
        ::Sleep(3000);
        dpUpdate();
    }
    dpFinalize();
}
