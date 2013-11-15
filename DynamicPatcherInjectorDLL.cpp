// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include <windows.h>

#include "DynamicPatcher.h"
dpAPI void dpBeginPeriodicUpdate();
dpAPI void dpEndPeriodicUpdate();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if(fdwReason==DLL_PROCESS_ATTACH) {
        dpBeginPeriodicUpdate();
    }
    else if(fdwReason==DLL_PROCESS_DETACH) {
        dpEndPeriodicUpdate();
    }
    return TRUE;
}
