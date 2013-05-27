// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"

void dpCallOnLoadHandler(dpBinary *v);

bool dpLoader::findHostSymbolByName(const char *name, dpSymbol &sym)
{
    char buf[sizeof(SYMBOL_INFO)+1024];
    PSYMBOL_INFO sinfo = (PSYMBOL_INFO)buf;
    sinfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    sinfo->MaxNameLen = MAX_PATH;
    if(::SymFromName(::GetCurrentProcess(), name, sinfo)==FALSE) {
        return false;
    }
    sym.address = (void*)sinfo->Address;
    sym.name = name;
    return true;
}

bool dpLoader::findHostSymbolByAddress(void *addr, dpSymbol &sym, char *namebuf, size_t namebuflen)
{
    char buf[sizeof(SYMBOL_INFO)+MAX_PATH];
    PSYMBOL_INFO sinfo = (PSYMBOL_INFO)buf;
    sinfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    sinfo->MaxNameLen = MAX_PATH;
    if(::SymFromAddr(::GetCurrentProcess(), (DWORD64)addr, 0, sinfo)==FALSE) {
        return false;
    }
    sym.address = addr;
    sym.name = namebuf;
    strncpy(namebuf, sinfo->Name, std::min<size_t>(sinfo->NameLen+1, namebuflen));
    return false;
}


dpLoader::dpLoader()
{
}

dpLoader::~dpLoader()
{
    eachBinaries([](dpBinary *bin){ delete bin; });
    m_binaries.clear();
}

bool dpLoader::link()
{
    if(!m_onload_queue.empty()) {
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

    // 新規ロードされているオブジェクトに OnLoad があれば呼ぶ
    for(size_t i=0; i<m_onload_queue.size(); ++i) {
        dpCallOnLoadHandler(m_onload_queue[i]);
    }
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

    dpBinary *ret = nullptr;
    if(strcmp(&path[len-4], ".obj")==0) {
        ret = new dpObjFile();
    }
    else if(strcmp(&path[len-4], ".lib")==0) {
        ret = new dpLibFile();
    }
    else if(strcmp(&path[len-4], ".dll")==0 || strcmp(&path[len-4], ".exe")==0) {
        ret = new dpDllFile();
    }

    if(ret) {
        if(ret->loadFile(path)) {
            m_binaries.push_back(ret);
        }
        else {
            delete ret;
            ret = nullptr;
        }
    }
    return ret;
}

void* dpLoader::findLoadedSymbol(const char *name)
{
    void *ret = nullptr;
    eachBinaries([&](dpBinary *bin){
        if(ret) { return; }
        if(const dpSymbol *s = bin->getSymbolTable().findSymbol(name)) {
            ret = s->address;
        }
    });
    return ret;
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
    auto p = std::find_if(m_binaries.begin(), m_binaries.end(),
        [=](dpBinary *b){ return _stricmp(b->getPath(), name)==0; }
    );
    return p!=m_binaries.end() ? *p : nullptr;
}


dpCLinkage dpAPI dpLoader* dpCreateLoader()
{
    return new dpLoader();
}
