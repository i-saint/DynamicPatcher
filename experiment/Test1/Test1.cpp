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
#ifdef _WIN64
#   define Platform "x64"
#else
#   define Platform "Win32"
#endif
#ifdef _DEBUG
#   define Configuration "Debug"
#else
#   define Configuration "Release"
#endif
#define ObjDir "_tmp/Test1_" Platform Configuration 

    dpInitialize();
    dpAddLoadPath(ObjDir "/Test1.obj");
    dpAddSourcePath(".");

    printf("DynamicPatcher Test1\n");
    {
        Test test;
        while(!test.getEndFlag()) {
            test.doSomething();
            dpLoad(ObjDir "/Test1.obj");
            dpLink();

            ::Sleep(1000);
            dpUpdate();
        }
    }
    dpFinalize();
}
