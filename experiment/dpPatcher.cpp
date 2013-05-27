// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"

dpPatcher::dpPatcher()
{
}

dpPatcher::~dpPatcher()
{
    unpatchAll();
}

void dpPatcher::release()
{
    delete this;
}

void* dpPatcher::patch(dpBinary *obj, const char *filter_regex)
{
    return nullptr;
}

void* dpPatcher::patch(const char *symbol_name, void *replacement_symbol)
{
    return nullptr;
}

bool dpPatcher::unpatch(const char *symbol_name)
{
    return false;
}

void dpPatcher::unpatchAll()
{
}


dpCLinkage dpAPI dpPatcher* dpCreatePatcher()
{
    return new dpPatcher();
}
