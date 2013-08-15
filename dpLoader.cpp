// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include "DynamicPatcher.h"
#include "dpInternal.h"


static const int g_host_symbol_flags = dpE_Code|dpE_Read|dpE_Execute|dpE_HostSymbol|dpE_NameNeedsDelete;


dpSymbol* dpLoader::findHostSymbolByName(const char *name)
{
    if(dpSymbol *sym = m_hostsymbols.findSymbolByName(name)) {
        return sym;
    }

    char buf[sizeof(SYMBOL_INFO)+1024];
    PSYMBOL_INFO sinfo = (PSYMBOL_INFO)buf;
    sinfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    sinfo->MaxNameLen = 1024;
    if(::SymFromName(::GetCurrentProcess(), name, sinfo)==FALSE) {
        return nullptr;
    }
    char *namebuf = new char[sinfo->NameLen+1];
    strncpy(namebuf, sinfo->Name, sinfo->NameLen+1);
    m_hostsymbols.addSymbol(newSymbol(namebuf, (void*)sinfo->Address, g_host_symbol_flags, 0, nullptr));
    m_hostsymbols.sort();
    return m_hostsymbols.findSymbolByName(name);
}

dpSymbol* dpLoader::findHostSymbolByAddress(void *addr)
{
    if(dpSymbol *sym = m_hostsymbols.findSymbolByAddress(addr)) {
        return sym;
    }

    char buf[sizeof(SYMBOL_INFO)+1024];
    PSYMBOL_INFO sinfo = (PSYMBOL_INFO)buf;
    sinfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    sinfo->MaxNameLen = 1024;
    if(::SymFromAddr(::GetCurrentProcess(), (DWORD64)addr, 0, sinfo)==FALSE) {
        return false;
    }
    char *namebuf = new char[sinfo->NameLen+1];
    strncpy(namebuf, sinfo->Name, sinfo->NameLen+1);
    m_hostsymbols.addSymbol(newSymbol(namebuf, (void*)sinfo->Address, g_host_symbol_flags, 0, nullptr));
    m_hostsymbols.sort();
    return m_hostsymbols.findSymbolByAddress(addr);
}


dpLoader::dpLoader(dpContext *ctx)
    : m_context(ctx)
{
}

dpLoader::~dpLoader()
{
    while(!m_binaries.empty()) { unloadImpl(m_binaries.front()); }
    m_hostsymbols.eachSymbols([&](dpSymbol *sym){ deleteSymbol(sym); });
    m_hostsymbols.clear();
}

void dpLoader::unloadImpl( dpBinary *bin )
{
    m_binaries.erase(std::find(m_binaries.begin(), m_binaries.end(), bin));
    bin->callHandler(dpE_OnUnload);
    std::string path = bin->getPath();
    delete bin;
    dpPrintInfo("unloaded \"%s\"\n", path.c_str());
}

void dpLoader::addOnLoadList(dpBinary *bin)
{
    m_onload_queue.push_back(bin);
}

template<class BinaryType>
BinaryType* dpLoader::loadBinaryImpl(const char *path)
{
    dpBuilder::ScopedPreloadLock pl(dpGetBuilder());

    BinaryType *old = static_cast<BinaryType*>(findBinary(path));
    if(old) {
        dpTime t = dpGetMTime(path);
        if(t<=old->getLastModifiedTime()) { return old; }
    }

    BinaryType *ret = new BinaryType(m_context);
    if(ret->loadFile(path)) {
        if(old) { unloadImpl(old); }
        m_binaries.push_back(ret);
        addOnLoadList(ret);
        dpPrintInfo("loaded \"%s\"\n", ret->getPath());
    }
    else {
        delete ret;
        ret = nullptr;
    }
    return ret;
}

dpObjFile* dpLoader::loadObj(const char *path) { return loadBinaryImpl<dpObjFile>(path); }
dpLibFile* dpLoader::loadLib(const char *path) { return loadBinaryImpl<dpLibFile>(path); }
dpDllFile* dpLoader::loadDll(const char *path) { return loadBinaryImpl<dpDllFile>(path); }

dpBinary* dpLoader::load(const char *path)
{
    size_t len = strlen(path);
    if(len>=4) {
        if     (_stricmp(&path[len-4], ".obj")==0) {
            return loadObj(path);
        }
        else if(_stricmp(&path[len-4], ".lib")==0) {
            return loadLib(path);
        }
        else if(_stricmp(&path[len-4], ".dll")==0 || _stricmp(&path[len-4], ".exe")==0) {
            return loadDll(path);
        }
    }
    dpPrintError("unrecognized file %s\n", path);
    return nullptr;
}

bool dpLoader::unload(const char *path)
{
    if(dpBinary *bin=findBinary(path)) {
        unloadImpl(bin);
        return true;
    }
    return false;
}

size_t dpLoader::reload()
{
    size_t n = 0;
    eachBinaries([&](dpBinary *bin){
        if(dpBinary *r=load(bin->getPath())) {
            if(r!=bin) { ++n; }
        }
    });
    return n;
}

bool dpLoader::link()
{
    dpBuilder::ScopedPreloadLock pl(dpGetBuilder());

    if(m_onload_queue.empty()) {
        return true;
    }

    bool ret = true;
    eachBinaries([&](dpBinary *bin){
        if(!bin->link()) { ret=false; }
    });

    if((dpGetConfig().sys_flags&dpE_SysDelayedLink)==0) {
        if(ret) {
            dpPrintInfo("link succeeded\n");
        }
        else {
            dpPrintError("link error\n");
            ::DebugBreak();
        }
    }

    // 有効にされていれば dllexport な関数を自動的に patch
    if((dpGetConfig().sys_flags&dpE_SysPatchExports)!=0) {
        dpEach(m_onload_queue, [&](dpBinary *b){
            b->eachSymbols([&](dpSymbol *sym){
                if(dpIsExportFunction(sym->flags)) {
                    sym->partialLink();
                    dpGetPatcher()->patch(findHostSymbolByName(sym->name), sym);
                }
            });
        });
    }
    // OnLoad があれば呼ぶ
    dpEach(m_onload_queue, [&](dpBinary *b){ b->callHandler(dpE_OnLoad); });
    m_onload_queue.clear();

    return ret;
}

dpSymbol* dpLoader::findSymbolByName(const char *name)
{
    size_t n = m_binaries.size();
    for(size_t i=0; i<n; ++i) {
        if(dpSymbol *s = m_binaries[i]->getSymbolTable().findSymbolByName(name)) {
            return s;
        }
    }
    return nullptr;
}

dpSymbol* dpLoader::findSymbolByAddress(void *addr)
{
    size_t n = m_binaries.size();
    for(size_t i=0; i<n; ++i) {
        if(dpSymbol *s = m_binaries[i]->getSymbolTable().findSymbolByAddress(addr)) {
            return s;
        }
    }
    return nullptr;
}

size_t dpLoader::getNumBinaries() const
{
    return m_binaries.size();
}

dpBinary* dpLoader::getBinary(size_t i)
{
    return m_binaries[i];
}

dpBinary* dpLoader::findBinary(const char *name)
{
    auto p = dpFind(m_binaries,
        [=](dpBinary *b){ return _stricmp(b->getPath(), name)==0; }
    );
    return p!=m_binaries.end() ? *p : nullptr;
}

dpSymbol* dpLoader::newSymbol(const char *nam, void *addr, int fla, int sect, dpBinary *bin)
{
    return new (m_symalloc.allocate()) dpSymbol(nam, addr, fla, sect, bin);
}

void dpLoader::deleteSymbol(dpSymbol *sym)
{
    sym->~dpSymbol();
    m_symalloc.deallocate(sym);
}

void dpLoader::addForceHostSymbolPattern(const char *pattern)
{
    m_force_host_symbol_patterns.push_back(std::regex(pattern));
}

bool dpLoader::doesForceHostSymbol(const char *name)
{
    if(m_force_host_symbol_patterns.empty()) { return false; }

    char demangled[4096];
    dpDemangle(name, demangled, sizeof(demangled));
    bool ret = false;
    for(size_t i=0; i<m_force_host_symbol_patterns.size(); ++i) {
        if(std::regex_match(demangled, m_force_host_symbol_patterns[i])) {
            ret = true;
            break;
        }
    }
    return ret;
}
