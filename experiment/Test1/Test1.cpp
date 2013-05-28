// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <cstdio>
#include <clocale>
#include <algorithm>

#define dpLinkDynamic
#include "../DynamicPatcher.h"


#ifdef _WIN64
#   define dpPlatform "x64"
#else
#   define dpPlatform "Win32"
#endif
#ifdef _DEBUG
#   define dpConfiguration "Debug"
#else
#   define dpConfiguration "Release"
#endif
#define dpObjDir "_tmp/Test1_" dpPlatform dpConfiguration 



int puts_hook(const char *s)
{
    typedef int (*puts_t)(const char *s);
    puts_t orig_puts = (puts_t)dpGetUnpatchedFunction(&puts);
    orig_puts("puts_hook()");
    return orig_puts(s);
}

class Test
{
public:
    Test() : m_end_flag(false) {}
    virtual ~Test() {}

    virtual void doSomething()
    {
        puts("Test::print()");
    }

    bool getEndFlag() const { return m_end_flag; }

private:
    bool m_end_flag;
};

dpOnLoad(
    dpPatchByAddress(&puts, &puts_hook);
    //dpPatchByFile("_tmp/Test1_Win32Debug/Test1.obj", ".*Test.*");
)

int main(int argc, char *argv[])
{
    dpInitialize();
    dpAddLoadPath(dpObjDir"/Test1.obj");
    dpAddSourcePath(".");
    dpStartAutoCompile("Test1.vcxproj /target:ClCompile /m /p:Configuration="dpConfiguration";Platform="dpPlatform, false);

    printf("DynamicPatcher Test1\n");
    {
        Test test;
        while(!test.getEndFlag()) {
            test.doSomething();

            ::Sleep(1000);
            dpUpdate();
        }
    }
    dpFinalize();
}
