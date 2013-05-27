// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include <regex>

void* dpHook(void *target, void *relacement)
{
    // todo
    return target;
}

void* dpUnhook(void *target, void *orig)
{
    // todo
    return target;
}


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

void* dpPatcher::patchByBinary(dpBinary *obj, const char *filter_regex)
{
    std::regex reg(filter_regex);
    obj->eachSymbols([&](const char *name, void *addr){
        if(std::regex_search(name, reg)) {
            patchByName(name, addr);
        }
    });
    return nullptr;
}

void* dpPatcher::patchByName(const char *symbol_name, void *replacement)
{
    dpSymbol sym;
    if(dpLoader::findHostSymbolByName(symbol_name, sym)) {
        unpatchByAddress(sym.address);
        void *orig = dpHook(sym.address, replacement);
        dpPatch pi = {sym, orig, replacement};
        m_patchers.push_back(pi);
        return orig;
    }
    return nullptr;
}

void* dpPatcher::patchByAddress(void *symbol_addr, void *replacement)
{
    dpSymbol sym;
    if(dpLoader::findHostSymbolByAddress(symbol_addr, sym)) {
        unpatchByAddress(sym.address);
        void *orig = dpHook(sym.address, replacement);
        dpPatch pi = {sym, orig, replacement};
        m_patchers.push_back(pi);
        return orig;
    }
    return nullptr;
}

bool dpPatcher::unpatchByBinary(dpBinary *obj)
{
    obj->eachSymbols([&](const char *name, void *addr){
        unpatchByAddress(addr);
    });
    return false;
}

bool dpPatcher::unpatchByName(const char *symbol_name)
{
    return false;
}

bool dpPatcher::unpatchByAddress(void *symbol_addr)
{
    return false;
}

void dpPatcher::unpatchAll()
{
}

dpPatch* dpPatcher::findPatchByName(const char *name)
{
    dpPatch *r = nullptr;
    eachPatches([&](dpPatch &p){
        if(strcmp(p.symbol.name, name)==0) { r=&p; }
    });
    return r;
}

dpPatch* dpPatcher::findPatchByAddress(void *addr)
{
    dpPatch *r = nullptr;
    eachPatches([&](dpPatch &p){
        if(p.symbol.address==addr) { r=&p; }
    });
    return r;
}


dpCLinkage dpAPI dpPatcher* dpCreatePatcher()
{
    return new dpPatcher();
}
