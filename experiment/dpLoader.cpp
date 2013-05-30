// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"

void dpCallOnLoadHandler(dpBinary *v);

const dpSymbol* dpLoader::findHostSymbolByName(const char *name)
{
    if(const dpSymbol *sym = m_hostsymbols.findSymbolByName(name)) {
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
    m_hostsymbols.addSymbol(dpSymbol(namebuf, (void*)sinfo->Address, 0));
    m_hostsymbols.sort();
    return m_hostsymbols.findSymbolByName(name);
}

const dpSymbol* dpLoader::findHostSymbolByAddress(void *addr)
{
    if(const dpSymbol *sym = m_hostsymbols.findSymbolByAddress(addr)) {
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
    m_hostsymbols.addSymbol(dpSymbol(namebuf, (void*)sinfo->Address, 0));
    m_hostsymbols.sort();
    return m_hostsymbols.findSymbolByAddress(addr);
}


dpLoader::dpLoader()
{
}

dpLoader::~dpLoader()
{
    m_hostsymbols.eachSymbols([&](const dpSymbol &sym){ delete[] sym.name; });
    while(!m_binaries.empty()) { unload(m_binaries.front()); }
}

void dpLoader::unload( dpBinary *bin )
{
    m_binaries.erase(std::find(m_binaries.begin(), m_binaries.end(), bin));
    bin->callHandler(dpE_OnUnload);
    delete bin;
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
        dpPrint("dp info: link completed\n");
    }
    else {
        dpPrint("dp fatal: link error\n");
        ::DebugBreak();
    }

    // 新規ロードされているオブジェクトに OnLoad があれば呼ぶ & dpPatch つき symbol を patch する
    dpEach(m_onload_queue, [&](dpBinary *b){
        b->eachExports([&](const dpSymbol &sym){
            if(dpIsFunction(sym.flags)) {
                dpGetPatcher()->patchByName(sym.name, sym.address);
            }
        });
        b->callHandler(dpE_OnLoad);
    });
    m_onload_queue.clear();

    return ret;
}

void dpLoader::addOnLoadList(dpBinary *bin)
{
    m_onload_queue.push_back(bin);
}

dpBinary* dpLoader::loadBinary(const char *path)
{
    size_t len = strlen(path);
    if(len < 4) { return nullptr; }

    dpBinary *old = findBinary(path);
    if(old) {
        dpTime t = dpGetFileModifiedTime(path);
        if(t<=old->getLastModifiedTime()) { return old; }
    }

    dpBinary *ret = nullptr;
    if(!ret) {
        if(_stricmp(&path[len-4], ".obj")==0) {
            ret = new dpObjFile();
        }
        else if(_stricmp(&path[len-4], ".lib")==0) {
            ret = new dpLibFile();
        }
        else if(_stricmp(&path[len-4], ".dll")==0 || _stricmp(&path[len-4], ".exe")==0) {
            ret = new dpDllFile();
        }
        else {
            dpPrint("dp error: unsupported file %s\n", path);
            return nullptr;
        }
    }

    if(ret->loadFile(path)) {
        if(old) { unload(old); }
        m_binaries.push_back(ret);
    }
    else {
        delete ret;
        ret = nullptr;
    }
    return ret;
}

bool dpLoader::unloadBinary( const char *path )
{
    if(dpBinary *bin=findBinary(path)) {
        unload(bin);
        return true;
    }
    return false;
}

const dpSymbol* dpLoader::findLoadedSymbolByName(const char *name)
{
    size_t n = m_binaries.size();
    for(size_t i=0; i<n; ++i) {
        if(const dpSymbol *s = m_binaries[i]->getSymbolTable().findSymbolByName(name)) {
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
