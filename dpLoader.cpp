// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include "DynamicPatcher.h"
#include "dpInternal.h"
#include <psapi.h>
#pragma comment(lib, "psapi.lib")


static const int g_host_symbol_flags = dpE_Code|dpE_Read|dpE_Execute|dpE_HostSymbol|dpE_NameNeedsDelete;


dpSymbol* dpLoader::findHostSymbolByName(const char *name)
{
    if(dpSymbol *sym = m_hostsymbols.findSymbolByName(name)) {
        return sym;
    }

    char buf[sizeof(SYMBOL_INFO)+MAX_SYM_NAME+1];
    PSYMBOL_INFO sinfo = (PSYMBOL_INFO)buf;
    sinfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    sinfo->MaxNameLen = MAX_SYM_NAME+1;

    bool found = ::SymFromName(::GetCurrentProcess(), name, sinfo)==TRUE;
#ifdef _M_IX86
    if(!found && name[0]=='_') {
        found = ::SymFromName(::GetCurrentProcess(), name+1, sinfo)==TRUE;
    }
#endif // _M_IX86
    if(!found) { return nullptr; }

    size_t namelen = strlen(name)+1;
    char *namebuf = new char[namelen];
    memcpy(namebuf, name, namelen);
    m_hostsymbols.addSymbol(newSymbol(namebuf, (void*)sinfo->Address, g_host_symbol_flags, 0, nullptr));
    m_hostsymbols.sort();
    return m_hostsymbols.findSymbolByName(name);
}

dpSymbol* dpLoader::findHostSymbolByAddress(void *addr)
{
    if(dpSymbol *sym = m_hostsymbols.findSymbolByAddress(addr)) {
        return sym;
    }

    char buf[sizeof(SYMBOL_INFO)+MAX_SYM_NAME+1];
    PSYMBOL_INFO sinfo = (PSYMBOL_INFO)buf;
    sinfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    sinfo->MaxNameLen = MAX_SYM_NAME+1;
    if(::SymFromAddr(::GetCurrentProcess(), (DWORD64)addr, 0, sinfo)==FALSE) {
        return false;
    }
    char *namebuf = new char[sinfo->NameLen+1];
    strncpy(namebuf, sinfo->Name, sinfo->NameLen+1);
    m_hostsymbols.addSymbol(newSymbol(namebuf, (void*)sinfo->Address, g_host_symbol_flags, 0, nullptr));
    m_hostsymbols.sort();
    return m_hostsymbols.findSymbolByAddress(addr);
}


struct dpDummySymbolData
{
    const char *name;
    void *addr;
};

#ifdef _M_IX86
#   define dpSelecyByArch(A,B) A
#else  // x64
#   define dpSelecyByArch(A,B) B
#endif // _M_IX86


void __fastcall __security_check_cookie_dummy(UINT_PTR cookie)
{
}

dpDummySymbolData g_dpDummySymbols[] = {
    {dpSelecyByArch("@__security_check_cookie@4", "__security_check_cookie"), &__security_check_cookie_dummy}
};

dpLoader::dpLoader(dpContext *ctx)
    : m_context(ctx)
{
    setupDummySymbols();
    loadMapFiles();
}

dpLoader::~dpLoader()
{
    while(!m_binaries.empty()) { unloadImpl(m_binaries.front()); }
    m_hostsymbols.eachSymbols([&](dpSymbol *sym){ deleteSymbol(sym); });
    m_hostsymbols.clear();
}

void dpLoader::setupDummySymbols()
{
    for(size_t i=0; i<_countof(g_dpDummySymbols); ++i) {
        dpDummySymbolData &ds = g_dpDummySymbols[i];
        dpSymbol *sym = m_hostsymbols.findSymbolByName(ds.name);
        if(sym==nullptr) {
            const size_t namelen = strlen(ds.name);
            char *name = new char[namelen+1];
            strncpy(name, ds.name, namelen);
            name[namelen] = '\0';
            m_hostsymbols.addSymbol(newSymbol(name, ds.addr, g_host_symbol_flags, 0, nullptr));
        }
        else {
            sym->address = ds.addr;
        }
    }
}


bool dpLoader::loadMapFile(const char *path, void *imagebase)
{
    if(m_mapfiles_read.find(path)!=m_mapfiles_read.end()) {
        dpPrintInfo("loadMapFile(): already loaded. skip: %s\n", path);
        return true;
    }

    FILE *file = fopen(path, "rb");
    if(!file) { return false; }

    size_t line_max = MAX_SYM_NAME+1024;
    char *line = (char*)malloc(line_max);
    line[line_max-1] = '\0';

// C99 から size_t 用の scanf フォーマット %zx が加わったらしいが、VisualC++ は未対応な様子
#ifdef _WIN64
#   define ZX "%llx"
#else
#   define ZX "%x"
#endif

    size_t preferred_addr = 0;
    size_t gap = 0;
    {
        while(fgets(line, line_max-1, file)) {
            if(sscanf(line, " Preferred load address is " ZX, &preferred_addr)==1) {
                gap = (size_t)imagebase - preferred_addr;
                break;
            }
        }
    }
    {
        std::regex reg("^ [0-9a-f]{4}:[0-9a-f]{8}       ");
        std::cmatch m;
        while(fgets(line, line_max-1, file)) {
            if(std::regex_search(line, m, reg)) {
                size_t line_len = strlen(line);
                const char *name_src = m[0].second;
                auto name_end = std::find(name_src, const_cast<const char *>(line+line_len), ' ');
                auto rva_plus_base_start = std::find_if_not(name_end, const_cast<const char *>(line+line_len), [](char c){ return c == ' '; });
                *(const_cast<char *>(rva_plus_base_start)+8) = '\0';
                size_t rva_plus_base = 0;
                sscanf(rva_plus_base_start, ZX, &rva_plus_base);
                if(rva_plus_base <= preferred_addr) { continue; }

                void *addr = (void*)(rva_plus_base + gap);
                size_t name_length = std::distance(name_src, name_end);
                char *name = new char[name_length+1];
                strncpy(name, name_src, name_length);
                name[name_length] = '\0';
                m_hostsymbols.addSymbol(newSymbol(name, addr, g_host_symbol_flags, 0, nullptr));
            }
        }
    }

    m_hostsymbols.sort();
    free(line);
    fclose(file);
    m_mapfiles_read.insert(path);
    setupDummySymbols();
    dpPrintInfo("loadMapFile(): succeeded: %s\n", path);
    return true;
}

size_t dpLoader::loadMapFiles()
{
    size_t ret = 0;
    dpEachModules([&](HMODULE mod){
        char path[MAX_PATH];
        ::GetModuleFileNameA(mod, path, sizeof(path));
        std::string mappath = std::regex_replace(std::string(path), std::regex("\\.[^.]+$"), std::string(".map"));
        if(loadMapFile(mappath.c_str(), mod)) {
            ++ret;
        }
    });
    return ret;
}

void dpLoader::unloadImpl( dpBinary *bin )
{
    m_binaries.erase(std::find(m_binaries.begin(), m_binaries.end(), bin));

    bin->callHandler(dpE_OnUnload);
    if(dpSymbolFilters *filts = dpGetFilters()) {
        dpSymbolFilter *filt = filts->getFilter(bin->getPath());
        bin->eachSymbols([&](dpSymbol *sym){
            if(dpIsFunction(sym->flags) && !dpIsLinkFailed(sym->flags) && filt->matchOnUnload(sym->name)) {
                ((dpEventHandler)sym->address)();
            }
        });
    }

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

#ifdef dpWithObjFile
dpObjFile* dpLoader::loadObj(const char *path) { return loadBinaryImpl<dpObjFile>(path); }
#endif // dpWithObjFile

#ifdef dpWithLibFile
dpLibFile* dpLoader::loadLib(const char *path) { return loadBinaryImpl<dpLibFile>(path); }
#endif // dpWithLibFile

#ifdef dpWithDllFile
dpDllFile* dpLoader::loadDll(const char *path) { return loadBinaryImpl<dpDllFile>(path); }
#endif // dpWithDllFile

dpBinary* dpLoader::load(const char *path)
{
    size_t len = strlen(path);
    if(len>=4) {
#ifdef dpWithObjFile
        if(_stricmp(&path[len-4], ".obj")==0) {
            return loadObj(path);
        }
#endif // dpWithObjFile
#ifdef dpWithLibFile
        if(_stricmp(&path[len-4], ".lib")==0) {
            return loadLib(path);
        }
#endif // dpWithLibFile
#ifdef dpWithDllFile
        if(_stricmp(&path[len-4], ".dll")==0 || _stricmp(&path[len-4], ".exe")==0) {
            return loadDll(path);
        }
#endif // dpWithDllFile
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

    dpSymbolFilters *filts = dpGetFilters();
    if(filts) {
        dpEach(m_onload_queue, [&](dpBinary *b){
            dpSymbolFilter *filt = filts->getFilter(b->getPath());
            b->eachSymbols([&](dpSymbol *sym){
                if(filt->matchUpdate(sym->name)) {
                    sym->partialLink();
                    dpGetPatcher()->patch(findHostSymbolByName(sym->name), sym);
                }
            });
        });
    }

    // OnLoad があれば呼ぶ
    dpEach(m_onload_queue, [&](dpBinary *b){ b->callHandler(dpE_OnLoad); });
    if(filts) {
        dpEach(m_onload_queue, [&](dpBinary *b){
            dpSymbolFilter *filt = filts->getFilter(b->getPath());
            b->eachSymbols([&](dpSymbol *sym){
                if(dpIsFunction(sym->flags) && !dpIsLinkFailed(sym->flags) && filt->matchOnLoad(sym->name)) {
                    ((dpEventHandler)sym->address)();
                }
            });
        });
    }
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
    dpDemangleNameOnly(name, demangled, sizeof(demangled));
    bool ret = false;
    for(size_t i=0; i<m_force_host_symbol_patterns.size(); ++i) {
        if(std::regex_match(demangled, m_force_host_symbol_patterns[i])) {
            ret = true;
            break;
        }
    }
    return ret;
}
