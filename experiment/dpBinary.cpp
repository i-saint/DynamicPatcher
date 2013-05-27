// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include <algorithm>

void dpSymbolTable::addSymbol(const char *name, void *address)
{
    dpSymbol tmp = {name, address};
    m_symbols.push_back(tmp);
}

void dpSymbolTable::merge(const dpSymbolTable &v)
{
    std::for_each(v.m_symbols.begin(), v.m_symbols.end(), [&](const dpSymbol &sym){
        m_symbols.push_back(sym);
    });
    sort();
    m_symbols.erase(std::unique(m_symbols.begin(), m_symbols.end()), m_symbols.end());
}

void dpSymbolTable::sort()
{
    std::sort(m_symbols.begin(), m_symbols.end());
}

void dpSymbolTable::clear()
{
    m_symbols.clear();
}

size_t dpSymbolTable::getNumSymbols() const
{
    return m_symbols.size();
}

const dpSymbol* dpSymbolTable::getSymbol(size_t i) const
{
    return &m_symbols[i];
}

void* dpSymbolTable::findSymbol(const char *name) const
{
    dpSymbol tmp = {name, nullptr};
    auto p = std::lower_bound(m_symbols.begin(), m_symbols.end(), tmp);
    return p==m_symbols.end() ? nullptr : p->address;
}



dpObjFile::dpObjFile()
    : m_data(nullptr), m_size(0)
    , m_aligned_data(nullptr), m_aligned_datasize(0)
    , m_path(), m_mtime(0)
    , m_reloc_bases(), m_symbols()
{
}

dpObjFile::~dpObjFile()
{
    unload();
}

void dpObjFile::release()
{
    delete this;
}

bool dpObjFile::loadFile(const char *path)
{
    unload();
    // todo
    return false;
}

bool dpObjFile::loadMemory(const char *name, void *data, size_t datasize, dpTime filetime)
{
    unload();
    // todo
    return false;
}

void dpObjFile::unload()
{
    if(m_data!=NULL) {
        ::VirtualFree(m_data, m_size, MEM_RELEASE);
        m_data = NULL;
        m_size = 0;
    }
    if(m_aligned_data!=NULL) {
        ::VirtualFree(m_aligned_data, m_aligned_datasize, MEM_RELEASE);
        m_aligned_data = NULL;
        m_aligned_datasize = 0;
    }
    m_path.clear();
    m_symbols.clear();
}

bool dpObjFile::link(dpLoader *linker)
{
    return false;
}

const dpSymbolTable& dpObjFile::getSymbolTable() const      { return m_symbols; }
const char*          dpObjFile::getPath() const             { return m_path.c_str(); }
dpTime               dpObjFile::getLastModifiedTime() const { return m_mtime; }
dpFileType           dpObjFile::getFileType() const         { return dpE_Obj; }
void*                dpObjFile::getBaseAddress() const      { return m_data; }



dpLibFile::dpLibFile()
    : m_mtime(0)
{
}

dpLibFile::~dpLibFile()
{
    unload();
}

void dpLibFile::release()
{
    delete this;
}

bool dpLibFile::loadFile(const char *path)
{
    unload();

    // todo

    eachObjs([&](dpObjFile *o){
        m_symbols.merge(o->getSymbolTable());
    });
    return false;
}

bool dpLibFile::loadMemory(const char *name, void *data, size_t datasize, dpTime filetime)
{
    unload();
    // todo
    return false;
}

void dpLibFile::unload()
{
    eachObjs([](dpObjFile *o){ o->release(); });
    m_objs.clear();
}

bool dpLibFile::link(dpLoader *linker)
{
    return false;
}

const dpSymbolTable& dpLibFile::getSymbolTable() const      { return m_symbols; }
const char*          dpLibFile::getPath() const             { return m_path.c_str(); }
dpTime               dpLibFile::getLastModifiedTime() const { return m_mtime; }
dpFileType           dpLibFile::getFileType() const         { return dpE_Lib; }
size_t               dpLibFile::getNumObjFiles() const      { return m_objs.size(); }
dpObjFile*           dpLibFile::getObjFile(size_t i)        { return m_objs[i]; }



dpDllFile::dpDllFile()
    : m_module(nullptr), m_needs_freelibrary(false)
    , m_mtime(0)
{
}

dpDllFile::~dpDllFile()
{
}

void dpDllFile::release()
{
    delete this;
}

bool dpDllFile::loadFile(const char *path)
{
    unload();
    m_path = path;
    m_module = ::LoadLibraryA(path);
    if(m_module!=nullptr) {
        m_needs_freelibrary = true;
        return true;
    }
    // todo
    return false;
}

bool dpDllFile::loadMemory(const char *name, void *data, size_t datasize, dpTime filetime)
{
    unload();

    if(data==nullptr) { return false; }
    m_path = name;
    m_module = (HMODULE)data;

    // todo

    return true;
}

void dpDllFile::unload()
{
    if(m_module && m_needs_freelibrary) {
        ::FreeLibrary(m_module);
    }
    m_needs_freelibrary = false;
}

bool dpDllFile::link(dpLoader *linker)
{
    return m_module!=nullptr;
}

const dpSymbolTable& dpDllFile::getSymbolTable() const      { return m_symbols; }
const char*          dpDllFile::getPath() const             { return m_path.c_str(); }
dpTime               dpDllFile::getLastModifiedTime() const { return m_mtime; }
dpFileType           dpDllFile::getFileType() const         { return dpE_Dll; }
