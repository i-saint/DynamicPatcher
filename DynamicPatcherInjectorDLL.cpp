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


void OnLoad()
{
    Options opt;
    opt.loadDLL();
    dpInitialize();
    for(size_t i=0; i<opt.load_module.size(); ++i) {
        dpLoad(opt.load_module[i].c_str());
    }
    dpLink();
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
