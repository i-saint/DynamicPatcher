// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#pragma comment(lib, "psapi.lib")
#include <windows.h>
#include <psapi.h>
#include <string>
#include "DynamicPatcherInjector.h"

#define dpLinkDynamic
#include "DynamicPatcher.h"

#ifdef _M_IX64
#   define InjectorDLL "DynamicPatcher64.dll"
#else
#   define InjectorDLL "DynamicPatcher.dll"
#endif // _M_IX86


void OnUpdate()
{
    dpUpdate();
}

void OnLoad()
{
    Options opt;
    dpInitialize();
    dpStartAutoBuild(opt.msbuild_command.c_str(), false);
    for(size_t i=0; i<opt.load_list.size(); ++i) {
        dpLoad(opt.load_list[i].c_str());
    }
    dpLink();
    dpPatchNameToName(opt.target_process.c_str(), opt.hook_update_function.c_str());
}

void OnUnload()
{
    dpFinalize();
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if(fdwReason==DLL_PROCESS_ATTACH) {
        OnLoad();
    }
    else if(fdwReason==DLL_PROCESS_DETACH) {
        OnUnload();
    }
    return TRUE;
}
