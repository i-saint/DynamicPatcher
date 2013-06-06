// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include <windows.h>
#include "DynamicPatcher.h"

dpPatch void OverriddenByLib1()
{
    printf("OverriddenByLib1(): overridden!\n");
}
