// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"
#include <regex>
#pragma comment(lib, "dbghelp.lib")

static DynamicPatcher *g_instance;

DynamicPatcher::DynamicPatcher()
{
    ::SymInitialize(::GetCurrentProcess(), NULL, TRUE);
    ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    g_instance = this;
    m_builder = new dpBuilder();
    m_patcher = new dpPatcher();
    m_loader  = new dpLoader();
}

DynamicPatcher::~DynamicPatcher()
{
    // バイナリ unload 時に適切に unpatch するには patcher より先に loader を破棄する必要がある
    delete m_loader;  m_loader=nullptr;
    delete m_patcher; m_patcher=nullptr;
    delete m_builder; m_builder=nullptr;
    g_instance = nullptr;
}

dpBuilder* DynamicPatcher::getBuilder() { return m_builder; }
dpPatcher* DynamicPatcher::getPatcher() { return m_patcher; }
dpLoader*  DynamicPatcher::getLoader()  { return m_loader; }

void DynamicPatcher::update()
{
    m_builder->update();
    m_loader->link();
}

dpAPI DynamicPatcher* dpGetInstance()
{
    return g_instance;
}


dpAPI bool dpInitialize()
{
    if(!g_instance) {
        new DynamicPatcher();
        return true;
    }
    return false;
}

dpAPI bool dpFinalize()
{
    if(g_instance) {
        delete g_instance;
        return true;
    }
    return false;
}


dpAPI size_t dpLoad(const char *path)
{
    size_t ret = 0;
    dpGlob(path, [&](const std::string &p){
        if(dpGetLoader()->loadBinary(p.c_str())) { ++ret; }
    });
    return ret;
}

dpAPI bool dpLink()
{
    return dpGetLoader()->link();
}

dpAPI size_t dpPatchByFile(const char *filename, const char *filter_regex)
{
    if(dpBinary *bin=dpGetLoader()->findBinary(filename)) {
        std::regex reg(filter_regex);
        dpGetPatcher()->patchByBinary(bin,
            [&](const dpSymbol &sym){
                return std::regex_search(sym.name, reg);
            });
        return true;
    }
    return false;
}

dpAPI size_t dpPatchByFile(const char *filename, const std::function<bool (const dpSymbol&)> &condition)
{
    if(dpBinary *bin=dpGetLoader()->findBinary(filename)) {
        dpGetPatcher()->patchByBinary(bin, condition);
        return true;
    }
    return false;
}

dpAPI bool dpPatchByName(const char *name)
{
    if(const dpSymbol *sym=dpGetLoader()->findHostSymbolByName(name)) {
        if(const dpSymbol *hook=dpGetLoader()->findLoadedSymbolByName(sym->name)) {
            dpGetPatcher()->patchByAddress(sym->address, hook->address);
            return true;
        }
    }
    return false;
}

dpAPI bool dpPatchByAddress(void *target, void *hook)
{
    if(dpGetPatcher()->patchByAddress(target, hook)) {
        return true;
    }
    return false;
}

dpAPI void* dpGetUnpatchedFunction( void *target )
{
    if(dpPatchData *pd = dpGetPatcher()->findPatchByAddress(target)) {
        return pd->orig;
    }
    return nullptr;
}


dpAPI void dpAddLoadPath(const char *path)
{
    dpGetBuilder()->addLoadPath(path);
}

dpAPI void dpAddSourcePath(const char *path)
{
    dpGetBuilder()->addSourcePath(path);
}

dpAPI bool dpStartAutoCompile(const char *option, bool console)
{
    return dpGetBuilder()->startAutoCompile(option, console);
}

dpAPI bool dpStopAutoCompile()
{
    return dpGetBuilder()->stopAutoCompile();
}

dpAPI void dpUpdate()
{
    dpGetInstance()->update();
}
