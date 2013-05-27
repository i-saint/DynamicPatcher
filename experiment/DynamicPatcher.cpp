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

dpAPI void DynamicPatcher::update()
{
    m_loader->link();
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

dpCLinkage dpAPI DynamicPatcher* dpGetInstance()
{
    return g_instance;
}
