// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#pragma comment(lib, "dbghelp.lib")

static DynamicPatcher *g_instance;

DynamicPatcher::DynamicPatcher()
{
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

dpCLinkage dpAPI bool dpPatchByName(const char *symbol_name)
{
    if(void *hook=dpGetLoader()->findLoadedSymbol(symbol_name)) {
        dpGetPatcher()->patchByName(symbol_name, hook);
        return true;
    }
    return false;
}

dpCLinkage dpAPI bool dpPatchByAddress(void *target, const char *symbol_name)
{
    if(void *hook=dpGetLoader()->findLoadedSymbol(symbol_name)) {
        dpGetPatcher()->patchByAddress(target, hook);
        return true;
    }
    return false;
}


dpCLinkage dpAPI void dpAutoCompileAddLoadPath(const char *path)
{
    dpGetBuilder()->addLoadPath(path);
}

dpCLinkage dpAPI void dpAutoCompileAddSourcePath(const char *path)
{
    dpGetBuilder()->addSourcePath(path);
}

dpCLinkage dpAPI bool dpAutoCompileStart(const char *option, bool console)
{
    return dpGetBuilder()->startAutoCompile(option, console);
}

dpCLinkage dpAPI bool dpAutoCompileStop()
{
    return dpGetBuilder()->stopAutoCompile();
}

dpCLinkage dpAPI void dpUpdate()
{
    dpGetInstance()->update();
}
