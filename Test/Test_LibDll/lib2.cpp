// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include <windows.h>
#include "DynamicPatcher.h"

dpPatch void OverriddenByLib2()
{
    printf("OverriddenByLib2(): overridden!\n");
}
