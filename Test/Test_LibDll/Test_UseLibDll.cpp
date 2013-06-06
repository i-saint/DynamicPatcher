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

dpScope(
    dpContext *g_dp_context;
)

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
    dpScope(g_dp_context=dpCreateContext());
    dpSetCurrentContext(g_dp_context);

    dpAddModulePath("Test_Dll.dll");
    dpAddModulePath("Test_Lib.lib");
    dpAddSourcePath("Test_LibDll");
    dpAddMSBuildCommand("Test_Lib.vcxproj /target:Build /m /p:Configuration="dpConfiguration";Platform="dpPlatform);
    dpAddMSBuildCommand("Test_Dll.vcxproj /target:Build /m /p:Configuration="dpConfiguration";Platform="dpPlatform);
    dpStartAutoBuild();

    // fail test
    //dpLoadLib("Test_Dll.dll");
    //dpLoadDll("Test_Lib.lib");

    printf("DynamicPatcher Test_UseLibDll\n");
    for(;;) {
        OverriddenByLib1();
        OverriddenByLib2();
        OverriddenByDll();
        ::Sleep(3000);
        dpUpdate();
    }
    dpScope(dpDeleteContext(g_dp_context));
    dpFinalize();
}
