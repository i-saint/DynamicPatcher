// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include <windows.h>

#define dpLinkDynamic
#include "DynamicPatcher.h"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if(fdwReason==DLL_PROCESS_ATTACH) {
        dpInitialize();
    }
    else if(fdwReason==DLL_PROCESS_DETACH) {
        dpFinalize();
    }
    return TRUE;
}
