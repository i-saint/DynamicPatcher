// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <windows.h>
#include "DynamicPatcher.h"

dpPatch void OverriddenByDll()
{
    printf("OverriddenByDll(): overridden!!!\n");
}
