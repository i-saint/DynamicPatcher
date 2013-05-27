// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#ifndef DynamicPatcher_h
#define DynamicPatcher_h

#include <windows.h>
#include <dbghelp.h>
#include <process.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

#define dpDLLExport __declspec(dllexport)
#define dpDLLImport __declspec(dllimport)

#if defined(dpDLLImpl)
#   define dpAPI dpDLLExport
#elif defined(dpLinkDynamic)
#   define dpAPI dpDLLImport
#else
#   define dpAPI
#endif // dpDLL_Impl
#define dpCLinkage extern "C"

#define dpScope(...)    __VA_ARGS__
#define dpOnLoad(...)   dpCLinkage static void dpOnLoadHandler()  { __VA_ARGS__ } __declspec(selectany) void *_dpOnLoadHandler  =dpOnLoadHandler;
#define dpOnUnload(...) dpCLinkage static void dpOnUnloadHandler(){ __VA_ARGS__ } __declspec(selectany) void *_dpOnUnloadHandler=dpOnUnloadHandler;


typedef unsigned long long dpTime;
class dpBinary;
class dpObjFile;
class dpLibFile;
class dpDllFile;
class dpLoader;
class dpPatcher;
class dpBuilder;

enum dpFileType {
    dpE_Obj,
    dpE_Lib,
    dpE_Dll,
};

struct dpSymbol {
    const char *name;
    void *address;
};
inline bool operator< (const dpSymbol &a, const dpSymbol &b) { return strcmp(a.name, b.name)<0; }
inline bool operator==(const dpSymbol &a, const dpSymbol &b) { return strcmp(a.name, b.name)==0; }

struct dpPatch {
    dpSymbol symbol;
    void *orig;
    void *patch;
};
inline bool operator< (const dpPatch &a, const dpPatch &b) { return strcmp(a.symbol.name, b.symbol.name)<0; }
inline bool operator==(const dpPatch &a, const dpPatch &b) { return strcmp(a.symbol.name, b.symbol.name)==0; }

class dpSymbolTable
{
public:
    void addSymbol(const char *name, void *address);
    void merge(const dpSymbolTable &v);
    void sort();
    void clear();
    dpAPI size_t            getNumSymbols() const;
    dpAPI const dpSymbol*   getSymbol(size_t i) const;
    dpAPI void*             findSymbol(const char *name) const;

    // F: [](const char *name, void *address)
    template<class F>
    void eachSymbols(const F &f)
    {
        size_t num = getNumSymbols();
        for(size_t i=0; i<num; ++i) {
            const dpSymbol *sym = getSymbol(i);
            f(sym->name, sym->address);
        }
    }

private:
    typedef std::vector<dpSymbol> symbol_cont;
    symbol_cont m_symbols;
};


class dpBinary
{
public:
    virtual ~dpBinary() {}
    virtual void release()=0;
    virtual bool loadFile(const char *path)=0;
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime)=0;
    virtual void unload()=0;
    virtual bool link(dpLoader *linker)=0;

    virtual const dpSymbolTable& getSymbolTable() const=0;
    virtual const char*          getPath() const=0;
    virtual dpTime               getLastModifiedTime() const=0;
    virtual dpFileType           getFileType() const=0;

    // F: [](const char *name, void *address)
    template<class F>
    void eachSymbols(const F &f)
    {
        const dpSymbolTable& st = getSymbolTable();
        size_t num = st.getNumSymbols();
        for(size_t i=0; i<num; ++i) {
            const dpSymbol *sym = st.getSymbol(i);
            f(sym->name, sym->address);
        }
    }
};

class dpObjFile : public dpBinary
{
public:
    dpObjFile();
    ~dpObjFile();
    virtual void release();
    virtual bool loadFile(const char *path);
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime);
    virtual void unload();
    virtual bool link(dpLoader *linker);

    virtual const dpSymbolTable& getSymbolTable() const;
    virtual const char*          getPath() const;
    virtual dpTime               getLastModifiedTime() const;
    virtual dpFileType           getFileType() const;
    dpAPI void*                  getBaseAddress() const;

private:
    typedef std::map<size_t, size_t> RelocBaseMap;
    void  *m_data;
    size_t m_size;
    void  *m_aligned_data;
    size_t m_aligned_datasize;
    std::string m_path;
    dpTime m_mtime;
    RelocBaseMap m_reloc_bases;
    dpSymbolTable m_symbols;
};

class dpLibFile : public dpBinary
{
public:
    dpLibFile();
    ~dpLibFile();
    virtual void release();
    virtual bool loadFile(const char *path);
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime);
    virtual void unload();
    virtual bool link(dpLoader *linker);

    virtual const dpSymbolTable& getSymbolTable() const;
    virtual const char*          getPath() const;
    virtual dpTime               getLastModifiedTime() const;
    virtual dpFileType           getFileType() const;
    dpAPI size_t                 getNumObjFiles() const;
    dpAPI dpObjFile*             getObjFile(size_t index);

    template<class F>
    void eachObjs(const F &f) { std::for_each(m_objs.begin(), m_objs.end(), f); }

private:
    typedef std::vector<dpObjFile*> obj_cont;
    obj_cont m_objs;
    dpSymbolTable m_symbols;
    std::string m_path;
    dpTime m_mtime;
};

// .dll だけでなく .exe も扱える (export された symbol がないと意味がないが)
class dpDllFile : public dpBinary
{
public:
    dpDllFile();
    ~dpDllFile();
    virtual void release();
    virtual bool loadFile(const char *path);
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime);
    virtual void unload();
    virtual bool link(dpLoader *linker);

    virtual const dpSymbolTable& getSymbolTable() const;
    virtual const char*          getPath() const;
    virtual dpTime               getLastModifiedTime() const;
    virtual dpFileType           getFileType() const;

private:
    HMODULE m_module;
    bool m_needs_freelibrary;
    std::string m_path;
    dpTime m_mtime;
    dpSymbolTable m_symbols;
};


class dpLoader
{
public:
    dpAPI static bool findHostSymbolByName(const char *name, dpSymbol &sym);
    dpAPI static bool findHostSymbolByAddress(void *addr, dpSymbol &sym);

    dpLoader();
    ~dpLoader();

    dpAPI void      release();
    dpAPI dpBinary* loadBinary(const char *path); // path to .obj, .lib, .dll, .exe
    dpAPI void*     findLoadedSymbol(const char *name);

    dpAPI size_t    getNumBinaries() const;
    dpAPI dpBinary* getBinary(size_t index);
    dpAPI dpBinary* findBinary(const char *name);

    // F: [](dpBinary *bin)
    template<class F>
    void eachBinaries(const F &f)
    {
        size_t num = getNumBinaries();
        for(size_t i=0; i<num; ++i) {
            f(getBinary(i));
        }
    }

private:
    typedef std::vector<dpBinary*> binary_cont;
    binary_cont m_binaries;
};


class dpPatcher
{
public:
    dpPatcher();
    ~dpPatcher();
    dpAPI void  release();
    dpAPI void* patchByBinary(dpBinary *obj, const char *filter_regex);
    dpAPI void* patchByName(const char *symbol_name, void *replacement_symbol);
    dpAPI void* patchByAddress(void *symbol_addr, void *replacement_symbol);
    dpAPI bool  unpatchByBinary(dpBinary *obj);
    dpAPI bool  unpatchByName(const char *symbol_name);
    dpAPI bool  unpatchByAddress(void *symbol_addr);
    dpAPI void  unpatchAll();

    dpAPI dpPatch* findPatchByName(const char *name);
    dpAPI dpPatch* findPatchByAddress(void *addr);

    template<class F>
    void eachPatches(const F &f)
    {
        std::for_each(m_patchers.begin(), m_patchers.end(), f);
    }

private:
    typedef std::vector<dpPatch> patch_cont;
    patch_cont m_patchers;
};


class dpBuilder
{
public:
    dpBuilder();
    ~dpBuilder();
    dpAPI void release();
    dpAPI void setConfig();
    dpAPI bool startAutoCompile();
    dpAPI bool stopAutoCompile();

public:
};

dpCLinkage dpAPI dpLoader*  dpCreateLoader();
dpCLinkage dpAPI dpPatcher* dpCreatePatcher();
dpCLinkage dpAPI dpBuilder* dpCreateBuilder();

#endif // DynamicPatcher_h
