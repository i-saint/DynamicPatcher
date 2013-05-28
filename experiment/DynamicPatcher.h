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
#include <functional>

#define dpDLLExport __declspec(dllexport)
#define dpDLLImport __declspec(dllimport)
#define dpCLinkage extern "C"

#ifdef _WIN64
#   define dpSymPrefix
#else // _WIN64
#   define dpSymPrefix "_"
#endif // _WIN64

#if defined(dpDLLImpl)
#   define dpAPI dpDLLExport
#elif defined(dpLinkDynamic)
#   define dpAPI dpDLLImport
#   ifdef _WIN64
#       pragma comment(lib,"DynamicPatcher64.lib")
#   else // _WIN64
#       pragma comment(lib,"DynamicPatcher.lib")
#   endif // _WIN64
#else
#   define dpAPI
#   ifdef _WIN64
#       pragma comment(lib,"DynamicPatcher64s.lib")
#   else // _WIN64
#       pragma comment(lib,"DynamicPatchers.lib")
#   endif // _WIN64
#endif // dpDLL_Impl


#define dpPatch         dpDLLExport
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

enum dpSymbolFlags {
    dpE_Data    = 1, // rw data
    dpE_RData   = 2, // readonly data
    dpE_Export  = 4, // dllexport
};
#define dpIsFunction(flag) ((flag & (dpE_Data|dpE_RData))==0)

struct dpSymbol {
    const char *name;
    void *address;
    DWORD flags;

    dpSymbol(const char *n=NULL, void *a=NULL, DWORD f=0);
};
inline bool operator< (const dpSymbol &a, const dpSymbol &b) { return strcmp(a.name, b.name)<0; }
inline bool operator==(const dpSymbol &a, const dpSymbol &b) { return strcmp(a.name, b.name)==0; }

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
    void eachSymbols(F &f) const
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
    dpAPI size_t                 getNumObjFiles() const;
    dpAPI dpObjFile*             getObjFile(size_t index);
    dpAPI dpObjFile*             findObjFile(const char *name);

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

    dpAPI dpBinary* loadBinary(const char *path); // path to .obj, .lib, .dll, .exe
    dpAPI bool      link();

    dpAPI size_t    getNumBinaries() const;
    dpAPI dpBinary* getBinary(size_t index);
    dpAPI dpBinary* findBinary(const char *name);

    dpAPI const dpSymbol* findLoadedSymbolByName(const char *name);
    dpAPI const dpSymbol* findHostSymbolByName(const char *name);
    dpAPI const dpSymbol* findHostSymbolByAddress(void *addr);

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
    dpAPI void*  patchByBinary(dpBinary *obj, const std::function<bool (const dpSymbol&)> &condition);
    dpAPI void*  patchByName(const char *name, void *hook);
    dpAPI void*  patchByAddress(void *addr, void *hook);
    dpAPI size_t unpatchByBinary(dpBinary *obj);
    dpAPI bool   unpatchByName(const char *name);
    dpAPI bool   unpatchByAddress(void *addr);
    dpAPI void   unpatchAll();

    dpAPI dpPatchData* findPatchByName(const char *name);
    dpAPI dpPatchData* findPatchByAddress(void *addr);

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
    dpAPI void addLoadPath(const char *path);
    dpAPI void addSourcePath(const char *path);
    dpAPI bool startAutoCompile(const char *build_options, bool create_console);
    dpAPI bool stopAutoCompile();

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

    dpAPI void update();

    dpAPI dpBuilder* getBuilder();
    dpAPI dpPatcher* getPatcher();
    dpAPI dpLoader*  getLoader();

private:
    dpBuilder *m_builder;
    dpPatcher *m_patcher;
    dpLoader  *m_loader;
};

dpAPI DynamicPatcher* dpGetInstance();
#define dpGetBuilder()  dpGetInstance()->getBuilder()
#define dpGetPatcher()  dpGetInstance()->getPatcher()
#define dpGetLoader()   dpGetInstance()->getLoader()



dpAPI bool   dpInitialize();
dpAPI bool   dpFinalize();

dpAPI size_t dpLoad(const char *path); // path to .obj .lib .dll .exe. accept wildcard (x64/Debug/*.obj)
dpAPI bool   dpLink();

dpAPI size_t dpPatchByFile(const char *filename, const char *filter_regex);
dpAPI size_t dpPatchByFile(const char *filename, const std::function<bool (const dpSymbol&)> &condition);
dpAPI bool   dpPatchByName(const char *symbol_name);
dpAPI bool   dpPatchByAddress(void *target, void *hook);
dpAPI void*  dpGetUnpatchedFunction(void *target);

dpAPI void   dpAddLoadPath(const char *path); // accept wildcard (x64/Debug/*.obj)
dpAPI void   dpAddSourcePath(const char *path);
dpAPI bool   dpStartAutoCompile(const char *msbuild_option, bool console=false);
dpAPI bool   dpStopAutoCompile();
dpAPI void   dpUpdate();

#endif // DynamicPatcher_h
