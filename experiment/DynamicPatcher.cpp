// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"
#pragma comment(lib, "dbghelp.lib")

static dpContext *g_dpDefaultContext = nullptr;
static __declspec(thread) dpContext *g_dpCurrentContext = nullptr;

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


dpAPI size_t dpLoad(const char *path)  { return dpGetCurrentContext()->load(path); }
dpAPI bool dpLoadObj(const char *path) { return dpGetCurrentContext()->loadObj(path)!=nullptr; }
dpAPI bool dpLoadLib(const char *path) { return dpGetCurrentContext()->loadLib(path)!=nullptr; }
dpAPI bool dpLoadDll(const char *path) { return dpGetCurrentContext()->loadDll(path)!=nullptr; }
dpAPI bool dpUnload(const char *path)  { return dpGetCurrentContext()->unload(path); };
dpAPI bool dpLink() { return dpGetCurrentContext()->link(); }

dpAPI size_t dpPatchByFile(const char *filename, const char *filter_regex)
{
    return dpGetCurrentContext()->patchByFile(filename, filter_regex);
}

dpAPI size_t dpPatchByFile(const char *filename, const std::function<bool (const dpSymbolS&)> &condition)
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
