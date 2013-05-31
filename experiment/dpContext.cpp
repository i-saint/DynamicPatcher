// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"
#include <regex>

dpContext::dpContext()
{
    m_builder = new dpBuilder(this);
    m_patcher = new dpPatcher(this);
    m_loader  = new dpLoader(this);
}

dpContext::~dpContext()
{
    // バイナリ unload 時に適切に unpatch するには patcher より先に loader を破棄する必要がある
    delete m_loader;  m_loader=nullptr;
    delete m_patcher; m_patcher=nullptr;
    delete m_builder; m_builder=nullptr;
}

dpBuilder* dpContext::getBuilder() { return m_builder; }
dpPatcher* dpContext::getPatcher() { return m_patcher; }
dpLoader*  dpContext::getLoader()  { return m_loader; }

size_t dpContext::load(const char *path)
{
    size_t ret = 0;
    dpGlob(path, [&](const std::string &p){
        if(m_loader->load(p.c_str())) { ++ret; }
    });
    return ret;
}
dpObjFile* dpContext::loadObj(const char *path) { return m_loader->loadObj(path); }
dpLibFile* dpContext::loadLib(const char *path) { return m_loader->loadLib(path); }
dpDllFile* dpContext::loadDll(const char *path) { return m_loader->loadDll(path); }
bool dpContext::unload( const char *path ) { return m_loader->unload(path); }
bool dpContext::link() { return m_loader->link(); }

size_t dpContext::patchByFile(const char *filename, const char *filter_regex)
{
    if(dpBinary *bin=m_loader->findBinary(filename)) {
        std::regex reg(filter_regex);
        m_patcher->patchByBinary(bin,
            [&](const dpSymbol &sym){
                return std::regex_search(sym.name, reg);
        });
        return true;
    }
    return false;
}

size_t dpContext::patchByFile(const char *filename, const std::function<bool (const dpSymbol&)> &condition)
{
    if(dpBinary *bin=m_loader->findBinary(filename)) {
        m_patcher->patchByBinary(bin, condition);
        return true;
    }
    return false;
}

bool dpContext::patchByName(const char *name)
{
    if(const dpSymbol *sym=m_loader->findHostSymbolByName(name)) {
        if(const dpSymbol *hook=m_loader->findLoadedSymbolByName(sym->name)) {
            m_patcher->patchByAddress(sym->address, hook->address);
            return true;
        }
    }
    return false;
}

bool dpContext::patchByAddress(void *target, void *hook)
{
    if(m_patcher->patchByAddress(target, hook)) {
        return true;
    }
    return false;
}

void* dpContext::getUnpatched(void *target)
{
    if(dpPatchData *pd = m_patcher->findPatchByAddress(target)) {
        return pd->orig;
    }
    return nullptr;
}

void dpContext::addLoadPath(const char *path)
{
    m_builder->addLoadPath(path);
}

void dpContext::addSourcePath(const char *path)
{
    m_builder->addSourcePath(path);
}

bool dpContext::startAutoBuild(const char *msbuild_option, bool console)
{
    return m_builder->startAutoBuild(msbuild_option, console);
}

bool dpContext::stopAutoBuild()
{
    return m_builder->stopAutoBuild();
}

void dpContext::update()
{
    m_builder->update();
    m_loader->link();
}