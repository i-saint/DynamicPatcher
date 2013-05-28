// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
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

dpCLinkage dpAPI DynamicPatcher* dpGetInstance()
{
    return g_instance;
}


dpCLinkage dpAPI bool dpInitialize()
{
    if(!g_instance) {
        new DynamicPatcher();
        return true;
    }
    return false;
}

dpCLinkage dpAPI bool dpFinalize()
{
    if(g_instance) {
        delete g_instance;
        return true;
    }
    return false;
}


dpCLinkage dpAPI dpBinary* dpLoad(const char *path)
{
    return dpGetLoader()->loadBinary(path);
}

dpCLinkage dpAPI bool dpLink()
{
    return dpGetLoader()->link();
}

dpCLinkage dpAPI size_t dpPatchByFile(const char *filename, const char *filter_regex)
{
    if(dpBinary *bin=dpGetLoader()->findBinary(filename)) {
        dpGetPatcher()->patchByBinary(bin, filter_regex);
        return true;
    }
    return false;
}

dpCLinkage dpAPI bool dpPatchByName(const char *name)
{
    dpSymbol sym;
    if(dpGetLoader()->findHostSymbolByName(name, sym)) {
        if(void *hook=dpGetLoader()->findLoadedSymbol(sym.name)) {
            dpGetPatcher()->patchByAddress(sym.address, hook);
            dpPrint("dp info: patching %s succeeded.\n", sym.name);
            return true;
        }
    }
    return false;
}

dpCLinkage dpAPI bool dpPatchByAddress(void *target, void *hook)
{
    if(dpGetPatcher()->patchByAddress(target, hook)) {
        dpPrint("dp info: patching 0x%p succeeded.\n", target);
        return true;
    }
    return false;
}

dpCLinkage dpAPI void* dpGetUnpatchedFunction( void *target )
{
    if(dpPatchData *pd = dpGetPatcher()->findPatchByAddress(target)) {
        return pd->orig;
    }
    return nullptr;
}


dpCLinkage dpAPI void dpAddLoadPath(const char *path)
{
    dpGetBuilder()->addLoadPath(path);
}

dpCLinkage dpAPI void dpAddSourcePath(const char *path)
{
    dpGetBuilder()->addSourcePath(path);
}

dpCLinkage dpAPI bool dpStartAutoCompile(const char *option, bool console)
{
    return dpGetBuilder()->startAutoCompile(option, console);
}

dpCLinkage dpAPI bool dpStopAutoCompile()
{
    return dpGetBuilder()->stopAutoCompile();
}

dpCLinkage dpAPI void dpUpdate()
{
    dpGetInstance()->update();
}
