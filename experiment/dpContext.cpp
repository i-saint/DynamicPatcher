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
            [&](const dpSymbolS &sym){
                return std::regex_search(sym.name, reg);
        });
        return true;
    }
    return false;
}

size_t dpContext::patchByFile(const char *filename, const std::function<bool (const dpSymbolS&)> &condition)
{
    if(dpBinary *bin=m_loader->findBinary(filename)) {
        m_patcher->patchByBinary(bin, condition);
        return true;
    }
    return false;
}

bool dpContext::patchNameToName(const char *target_name, const char *hook_name)
{
    dpSymbol *target = m_loader->findHostSymbolByName(target_name);
    dpSymbol *hook   = m_loader->findSymbolByName(hook_name);
    if(target && hook) {
        return m_patcher->patch(target, hook)!=nullptr;
    }
    return false;
}

bool dpContext::patchAddressToName(const char *target_name, void *hook_addr)
{
    dpSymbol *target = m_loader->findHostSymbolByName(target_name);
    dpSymbol *hook   = m_loader->findSymbolByAddress(hook_addr);
    if(target && hook) {
            return m_patcher->patch(target, hook)!=nullptr;
    }
    return false;
}

bool dpContext::patchAddressToAddress(void *target_addr, void *hook_addr)
{
    dpSymbol *target = m_loader->findHostSymbolByAddress(target_addr);
    dpSymbol *hook   = m_loader->findSymbolByAddress(hook_addr);
    if(target && hook) {
        return m_patcher->patch(target, hook)!=nullptr;
    }
    return false;
}

bool dpContext::patchByAddress(void *hook_addr)
{
    if(dpSymbol *hook=m_loader->findSymbolByAddress(hook_addr)) {
        if(dpSymbol *target=m_loader->findHostSymbolByName(hook->name)) {
            return m_patcher->patch(target, hook)!=nullptr;
        }
    }
    return false;
}

bool dpContext::unpatchByAddress(void *target_or_hook_addr)
{
    return m_patcher->unpatchByAddress(target_or_hook_addr);
}

void* dpContext::getUnpatched(void *target)
{
    if(dpPatchData *pd = m_patcher->findPatchByAddress(target)) {
        return pd->unpatched;
    }
    return nullptr;
}
