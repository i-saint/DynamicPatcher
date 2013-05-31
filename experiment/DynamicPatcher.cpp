// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"
#include <regex>
#pragma comment(lib, "dbghelp.lib")

static dpContext *g_dpDefaultContext = nullptr;
static __declspec(thread) dpContext *g_dpCurrentContext = nullptr;

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
        if(m_loader->loadBinary(p.c_str())) { ++ret; }
    });
    return ret;
}

bool dpContext::link()
{
    return m_loader->link();
}

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
    return m_builder->startAutoCompile(msbuild_option, console);
}

bool dpContext::stopAutoBuild()
{
    return m_builder->stopAutoCompile();
}

void dpContext::update()
{
    m_builder->update();
    m_loader->link();
}



dpAPI dpContext* dpCreateContext()
{
    return new dpContext();
}

dpAPI void dpDeleteContext(dpContext *ctx)
{
    delete ctx;
};

dpAPI dpContext* dpGetDefaultContext()
{
    return g_dpDefaultContext;
}

dpAPI void dpSetCurrentContext(dpContext *ctx)
{
    g_dpCurrentContext = ctx;
}

dpAPI dpContext* dpGetCurrentContext()
{
    if(!g_dpCurrentContext) { g_dpCurrentContext=g_dpDefaultContext; }
    return g_dpCurrentContext;
}


dpAPI bool dpInitialize()
{
    ::SymInitialize(::GetCurrentProcess(), NULL, TRUE);
    ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    if(!g_dpDefaultContext) {
        g_dpDefaultContext = new dpContext();
        return true;
    }
    return false;
}

dpAPI bool dpFinalize()
{
    if(g_dpDefaultContext) {
        delete g_dpDefaultContext;
        g_dpDefaultContext = nullptr;
        return true;
    }
    return false;
}


dpAPI size_t dpLoad(const char *path)
{
    return dpGetCurrentContext()->load(path);
}

dpAPI bool dpLink()
{
    return dpGetCurrentContext()->link();
}

dpAPI size_t dpPatchByFile(const char *filename, const char *filter_regex)
{
    return dpGetCurrentContext()->patchByFile(filename, filter_regex);
}

dpAPI size_t dpPatchByFile(const char *filename, const std::function<bool (const dpSymbol&)> &condition)
{
    return dpGetCurrentContext()->patchByFile(filename, condition);
}

dpAPI bool dpPatchByName(const char *name)
{
    return dpGetCurrentContext()->patchByName(name);
}

dpAPI bool dpPatchByAddress(void *target, void *hook)
{
    return dpGetCurrentContext()->patchByAddress(target, hook);
}

dpAPI void* dpGetUnpatched(void *target)
{
    return dpGetCurrentContext()->getUnpatched(target);
}


dpAPI void dpAddLoadPath(const char *path)
{
    dpGetCurrentContext()->addLoadPath(path);
}

dpAPI void dpAddSourcePath(const char *path)
{
    dpGetCurrentContext()->addSourcePath(path);
}

dpAPI bool dpStartAutoBuild(const char *option, bool console)
{
    return dpGetCurrentContext()->startAutoBuild(option, console);
}

dpAPI bool dpStopAutoBuild()
{
    return dpGetCurrentContext()->stopAutoBuild();
}

dpAPI void dpUpdate()
{
    dpGetCurrentContext()->update();
}
