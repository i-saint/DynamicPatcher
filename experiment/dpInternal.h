// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <windows.h>
#include <dbghelp.h>
#include <process.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

typedef unsigned long long dpTime;
class dpBinary;
class dpObjFile;
class dpLibFile;
class dpDllFile;
class dpLoader;
class dpPatcher;
class dpBuilder;
class DynamicPatcher;

enum dpFileType {
    dpE_Obj,
    dpE_Lib,
    dpE_Dll,
};

struct dpPatchData {
    dpSymbol symbol;
    void *orig;
    void *hook;
    size_t size;
};
inline bool operator< (const dpPatchData &a, const dpPatchData &b) { return strcmp(a.symbol.name, b.symbol.name)<0; }
inline bool operator==(const dpPatchData &a, const dpPatchData &b) { return strcmp(a.symbol.name, b.symbol.name)==0; }




class dpSymbolTable
{
public:
    void addSymbol(const dpSymbol &v);
    void merge(const dpSymbolTable &v);
    void sort();
    void clear();
    size_t          getNumSymbols() const;
    dpSymbol*       getSymbol(size_t i);
    dpSymbol*       findSymbolByName(const char *name);
    dpSymbol*       findSymbolByAddress(void *sym);
    const dpSymbol* getSymbol(size_t i) const;
    const dpSymbol* findSymbolByName(const char *name) const;
    const dpSymbol* findSymbolByAddress(void *sym) const;

    // F: [](const dpSymbol &sym)
    template<class F>
    void eachSymbols(const F &f) const
    {
        size_t n = getNumSymbols();
        for(size_t i=0; i<n; ++i) { f(*getSymbol(i)); }
    }

private:
    typedef std::vector<dpSymbol> symbol_cont;
    symbol_cont m_symbols;
};


class dpBinary
{
public:
    virtual ~dpBinary() {}
    virtual bool loadFile(const char *path)=0;
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime)=0;
    virtual void unload()=0;
    virtual bool link()=0;

    virtual const dpSymbolTable& getSymbolTable() const=0;
    virtual const dpSymbolTable& getExportTable() const=0;
    virtual const char*          getPath() const=0;
    virtual dpTime               getLastModifiedTime() const=0;
    virtual dpFileType           getFileType() const=0;

    // F: [](const dpSymbol &sym)
    template<class F> void eachSymbols(const F &f) const { getSymbolTable().eachSymbols(f); }

    // F: [](const dpSymbol &sym)
    template<class F> void eachExports(const F &f) const { getExportTable().eachSymbols(f); }
};

class dpObjFile : public dpBinary
{
public:
    dpObjFile();
    ~dpObjFile();
    virtual bool loadFile(const char *path);
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime);
    virtual void unload();
    virtual bool link();

    virtual const dpSymbolTable& getSymbolTable() const;
    virtual const dpSymbolTable& getExportTable() const;
    virtual const char*          getPath() const;
    virtual dpTime               getLastModifiedTime() const;
    virtual dpFileType           getFileType() const;
    void*                        getBaseAddress() const;

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
    dpSymbolTable m_exports;
};

class dpLibFile : public dpBinary
{
public:
    dpLibFile();
    ~dpLibFile();
    virtual bool loadFile(const char *path);
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime);
    virtual void unload();
    virtual bool link();

    virtual const dpSymbolTable& getSymbolTable() const;
    virtual const dpSymbolTable& getExportTable() const;
    virtual const char*          getPath() const;
    virtual dpTime               getLastModifiedTime() const;
    virtual dpFileType           getFileType() const;
    size_t                       getNumObjFiles() const;
    dpObjFile*                   getObjFile(size_t index);
    dpObjFile*                   findObjFile(const char *name);

    template<class F>
    void eachObjs(const F &f) { std::for_each(m_objs.begin(), m_objs.end(), f); }

private:
    typedef std::vector<dpObjFile*> obj_cont;
    obj_cont m_objs;
    dpSymbolTable m_symbols;
    dpSymbolTable m_exports;
    std::string m_path;
    dpTime m_mtime;
};

// ロード中の dll は上書き不可能で、そのままだと実行時リビルドできない。
// そのため、指定のファイルをコピーしてそれを扱う。(関連する .pdb もコピーする)
// また、.dll だけでなく .exe も扱える (export された symbol がないと意味がないが)
class dpDllFile : public dpBinary
{
public:
    dpDllFile();
    ~dpDllFile();
    virtual bool loadFile(const char *path);
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime);
    virtual void unload();
    virtual bool link();

    virtual const dpSymbolTable& getSymbolTable() const;
    virtual const dpSymbolTable& getExportTable() const;
    virtual const char*          getPath() const;
    virtual dpTime               getLastModifiedTime() const;
    virtual dpFileType           getFileType() const;

private:
    HMODULE m_module;
    bool m_needs_freelibrary;
    std::string m_path;
    std::string m_actual_file;
    dpTime m_mtime;
    dpSymbolTable m_symbols;
};


class dpLoader
{
public:
    dpLoader();
    ~dpLoader();

    dpBinary* loadBinary(const char *path); // path to .obj, .lib, .dll, .exe
    bool      unloadBinary(const char *path);
    bool      link();

    size_t    getNumBinaries() const;
    dpBinary* getBinary(size_t index);
    dpBinary* findBinary(const char *name);

    const dpSymbol* findLoadedSymbolByName(const char *name);
    const dpSymbol* findHostSymbolByName(const char *name);
    const dpSymbol* findHostSymbolByAddress(void *addr);

    // F: [](dpBinary *bin)
    template<class F>
    void eachBinaries(const F &f)
    {
        size_t n = getNumBinaries();
        for(size_t i=0; i<n; ++i) { f(getBinary(i)); }
    }

    void addOnLoadList(dpBinary *bin);

private:
    typedef std::vector<dpBinary*> binary_cont;
    binary_cont m_binaries;
    binary_cont m_onload_queue;
    dpSymbolTable m_hostsymbols;
};


class dpPatcher
{
public:
    dpPatcher();
    ~dpPatcher();
    void*  patchByBinary(dpBinary *obj, const std::function<bool (const dpSymbol&)> &condition);
    void*  patchByName(const char *name, void *hook);
    void*  patchByAddress(void *addr, void *hook);
    size_t unpatchByBinary(dpBinary *obj);
    bool   unpatchByName(const char *name);
    bool   unpatchByAddress(void *addr);
    void   unpatchAll();

    dpPatchData* findPatchByName(const char *name);
    dpPatchData* findPatchByAddress(void *addr);

    template<class F>
    void eachPatchData(const F &f)
    {
        size_t n = m_patches.size();
        for(size_t i=0; i<n; ++i) { f(m_patches[i]); }
    }

private:
    typedef std::vector<dpPatchData> patch_cont;
    patch_cont m_patches;
};


class dpBuilder
{
public:
    dpBuilder();
    ~dpBuilder();
    void addLoadPath(const char *path);
    void addSourcePath(const char *path);
    bool startAutoCompile(const char *build_options, bool create_console);
    bool stopAutoCompile();

    void update();
    void watchFiles();
    bool build();

private:
    struct SourcePath
    {
        std::string path;
        HANDLE notifier;
    };
    std::string m_vcvars;
    std::string m_msbuild;
    std::string m_msbuild_option;
    std::vector<SourcePath> m_srcpathes;
    std::vector<std::string> m_loadpathes;
    bool m_create_console;
    mutable bool m_build_done;
    bool m_flag_exit;
    HANDLE m_thread_watchfile;
};


class DynamicPatcher
{
public:
    DynamicPatcher();
    ~DynamicPatcher();

    void update();

    dpBuilder* getBuilder();
    dpPatcher* getPatcher();
    dpLoader*  getLoader();

private:
    dpBuilder *m_builder;
    dpPatcher *m_patcher;
    dpLoader  *m_loader;
};


dpAPI DynamicPatcher* dpGetInstance();
#define dpGetBuilder()  dpGetInstance()->getBuilder()
#define dpGetPatcher()  dpGetInstance()->getPatcher()
#define dpGetLoader()   dpGetInstance()->getLoader()


void    dpPrint(const char* fmt, ...);
void*   dpAllocate(size_t size, void *location);
void*   dpAllocateModule(size_t size);
dpTime  dpGetFileModifiedTime(const char *path);
bool    dpDemangle(const char *mangled, char *demangled, size_t buflen);


template<class F>
inline void dpGlob(const std::string &path, const F &f)
{
    std::string dir;
    size_t dir_len=0;
    for(size_t l=0; l<path.size(); ++l) {
        if(path[l]=='\\' || path[l]=='/') { dir_len=l+1; }
    }
    dir.insert(dir.end(), path.begin(), path.begin()+dir_len);

    WIN32_FIND_DATAA wfdata;
    HANDLE handle = ::FindFirstFileA(path.c_str(), &wfdata);
    if(handle!=INVALID_HANDLE_VALUE) {
        do {
            f( dir+wfdata.cFileName );
        } while(::FindNextFileA(handle, &wfdata));
        ::FindClose(handle);
    }
}

// F: [](size_t size) -> void* : alloc func
template<class F>
inline bool dpMapFile(const char *path, void *&o_data, size_t &o_size, const F &alloc)
{
    o_data = NULL;
    o_size = 0;
    if(FILE *f=fopen(path, "rb")) {
        fseek(f, 0, SEEK_END);
        o_size = ftell(f);
        if(o_size > 0) {
            o_data = alloc(o_size);
            fseek(f, 0, SEEK_SET);
            fread(o_data, 1, o_size, f);
        }
        fclose(f);
        return true;
    }
    return false;
}

