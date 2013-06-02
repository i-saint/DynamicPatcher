// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"

void dpCallOnLoadHandler(dpBinary *v);

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
    m_hostsymbols.addSymbol(newSymbol(namebuf, (void*)sinfo->Address, 0, 0, nullptr));
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
    m_hostsymbols.addSymbol(newSymbol(namebuf, (void*)sinfo->Address, 0, 0, nullptr));
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
    m_hostsymbols.eachSymbols([&](dpSymbol *sym){
        delete[] sym->name;
        deleteSymbol(sym);
    });
}

void dpLoader::unloadImpl( dpBinary *bin )
{
    m_binaries.erase(std::find(m_binaries.begin(), m_binaries.end(), bin));
    bin->callHandler(dpE_OnUnload);
    dpPrintInfo("unloaded \"%s\"\n", bin->getPath());
    delete bin;
}

void dpLoader::addOnLoadList(dpBinary *bin)
{
    m_onload_queue.push_back(bin);
}

template<class BinaryType>
BinaryType* dpLoader::loadBinaryImpl(const char *path)
{
    BinaryType *old = static_cast<BinaryType*>(findBinary(path));
    if(old) {
        dpTime t = dpGetFileModifiedTime(path);
        if(t<=old->getLastModifiedTime()) { return old; }
    }

    BinaryType *ret = new BinaryType(m_context);
    if(ret->loadFile(path)) {
        if(old) { unloadImpl(old); }
        m_binaries.push_back(ret);
        dpPrintInfo("loaded \"%s\"\n", path);
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

bool dpLoader::link()
{
    if(m_onload_queue.empty()) {
        return true;
    }

    bool ret = true;
    eachBinaries([&](dpBinary *bin){
        if(!bin->link()) { ret=false; }
    });

    if(ret) {
        dpPrintInfo("link completed\n");
    }
    else {
        dpPrintError("link error\n");
        ::DebugBreak();
    }

    // 新規ロードされているオブジェクトに OnLoad があれば呼ぶ & dpPatch つき symbol を patch する
    dpEach(m_onload_queue, [&](dpBinary *b){
        b->eachSymbols([&](dpSymbol *sym){
            if(dpIsExportFunction(sym->flags)) {
                dpGetPatcher()->patch(findHostSymbolByName(sym->name), sym);
            }
        });
        b->callHandler(dpE_OnLoad);
    });
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

dpSymbol* dpLoader::findAndLinkSymbolByName(const char *name)
{
    if(dpSymbol *sym=findSymbolByName(name)) {
        // todo: partial link
        return sym;
    }
    return nullptr;
}

dpSymbol* dpLoader::findAndLinkSymbolByAddress(void *addr)
{
    if(dpSymbol *sym=findSymbolByAddress(addr)) {
        // todo: partial link
        return sym;
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
    m_symalloc.deallocate(sym);
}
