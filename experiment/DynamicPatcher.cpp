// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include <regex>
#pragma comment(lib, "dbghelp.lib")

static DynamicPatcher *g_instance;

DynamicPatcher::DynamicPatcher()
{
    ::SymInitialize(::GetCurrentProcess(), NULL, TRUE);
    ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    g_instance = this;
    m_loader = new dpLoader();
    m_patcher = new dpPatcher();
    m_builder = new dpBuilder();
}

DynamicPatcher::~DynamicPatcher()
{
    delete m_builder; m_builder=nullptr;
    delete m_patcher; m_builder=nullptr;
    delete m_loader;  m_builder=nullptr;
    g_instance = nullptr;
}

dpLoader*  DynamicPatcher::getLoader()  { return m_loader; }
dpPatcher* DynamicPatcher::getPatcher() { return m_patcher; }
dpBuilder* DynamicPatcher::getBuilder() { return m_builder; }

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


dpAPI dpBinary* dpLoad(const char *path)
{
    return dpGetLoader()->loadBinary(path);
}

dpAPI bool dpLink()
{
    return dpGetLoader()->link();
}

dpAPI size_t dpPatchByFile(const char *filename, const char *filter_regex)
{
    if(dpBinary *bin=dpGetLoader()->findBinary(filename)) {
        std::regex reg(filter_regex);
        dpGetPatcher()->patchByBinary(bin, [&](const char *name){ return std::regex_search(name, reg); });
        return true;
    }
    return false;
}

dpAPI size_t dpPatchByFile(const char *filename, const std::function<bool (const char *symname)> &condition)
{
    if(dpBinary *bin=dpGetLoader()->findBinary(filename)) {
        dpGetPatcher()->patchByBinary(bin, condition);
        return true;
    }
    return false;
}

dpAPI bool dpPatchByName(const char *name)
{
    dpSymbol sym;
    if(dpGetLoader()->findHostSymbolByName(name, sym)) {
        if(void *hook=dpGetLoader()->findLoadedSymbol(sym.name)) {
            dpGetPatcher()->patchByAddress(sym.address, hook);
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
