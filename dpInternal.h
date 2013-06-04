// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <windows.h>
#include <dbghelp.h>
#include <process.h>
#include <cstdint>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <algorithm>
#include "DynamicPatcher.h"

typedef unsigned long long dpTime;
class dpBinary;
class dpObjFile;
class dpLibFile;
class dpDllFile;
class dpLoader;
class dpPatcher;
class dpBuilder;
class dpContext;

enum dpFileType {
    dpE_Obj,
    dpE_Lib,
    dpE_Dll,
};
enum dpEventType {
    dpE_OnLoad,
    dpE_OnUnload,
};
enum dpLinkFlags {
    dpE_NeedsLink=1,
    dpE_NeedsBase=2,
};
enum dpSymbolFlagsEx {
    dpE_HostSymbol      = 0x10000,
    dpE_NameNeedsDelete = 0x20000,
    dpE_LinkFailed      = 0x40000,
};
#define dpIsLinkFailed(flag) ((flag&dpE_LinkFailed)!=0)

struct dpSymbol
{
    const char *name;
    void *address;
    int flags;
    int section;
    dpBinary *binary;

    dpSymbol(const char *nam, void *addr, int fla, int sect, dpBinary *bin);
    ~dpSymbol();
    const dpSymbolS& simplify() const;
    bool partialLink();
};
inline bool operator< (const dpSymbol &a, const dpSymbol &b) { return strcmp(a.name, b.name)<0; }
inline bool operator==(const dpSymbol &a, const dpSymbol &b) { return strcmp(a.name, b.name)==0; }
inline bool operator< (const dpSymbol &a, const char *b) { return strcmp(a.name, b)<0; }
inline bool operator==(const dpSymbol &a, const char *b) { return strcmp(a.name, b)==0; }

template<class T>
struct dpLTPtr {
    bool operator()(const T *a, const T *b) const { return *a<*b; }
};

template<class T>
struct dpEQPtr {
    bool operator()(const T *a, const T *b) const { return *a==*b; }
};

struct dpPatchData
{
    const dpSymbol *target;
    const dpSymbol *hook;
    void *unpatched;
    void *trampoline;
    size_t unpatched_size;

    dpPatchData() : target(), hook(), unpatched(), trampoline(), unpatched_size() {}
};
inline bool operator< (const dpPatchData &a, const dpPatchData &b) { return a.target< b.target; }
inline bool operator==(const dpPatchData &a, const dpPatchData &b) { return a.target==b.target; }


template<class Container, class F> inline void dpEach(Container &cont, const F &f);
template<class Container, class F> inline auto dpFind(Container &cont, const F &f) -> decltype(cont.begin());

dpConfig& dpGetConfig();
void    dpPrintError(const char* fmt, ...);
void    dpPrintWarning(const char* fmt, ...);
void    dpPrintInfo(const char* fmt, ...);
void    dpPrintDetail(const char* fmt, ...);

void*   dpAllocateForward(size_t size, void *location);
void*   dpAllocateBackward(size_t size, void *location);
void*   dpAllocateModule(size_t size);
void    dpDeallocate(void *location, size_t size);
dpTime  dpGetMTime(const char *path);
dpTime  dpGetSystemTime();

char*   dpGetPDBPathFromModule(void *pModule, bool fill_gap=false);
bool    dpCopyFile(const char *srcpath, const char *dstpath);

template<class F> void dpGlob(const char *path, const F &f);
template<class F> bool dpMapFile(const char *path, void *&o_data, size_t &o_size, const F &alloc);
bool    dpWriteFile(const char *path, const void *data, size_t size);
bool    dpDeleteFile(const char *path);
bool    dpFileExists(const char *path);
size_t  dpSeparateDirFile(const char *path, std::string *dir, std::string *file);
size_t  dpSeparateFileExt(const char *filename, std::string *file, std::string *ext);
size_t  dpGetCurrentModulePath(char *buf, size_t buflen);
size_t  dpGetMainModulePath(char *buf, size_t buflen);
void    dpSanitizePath(std::string &path);


// アラインが必要な section データを再配置するための単純なアロケータ
class dpSectionAllocator
{
public:
    // data=NULL, size_t size=0xffffffff で初期化した場合、必要な容量を調べるのに使える
    dpSectionAllocator(void *data=NULL, size_t size=0xffffffff);
    // align: 2 の n 乗である必要がある
    void* allocate(size_t size, size_t align);
    size_t getUsed() const;
private:
    void *m_data;
    size_t m_size;
    size_t m_used;
};

class dpTrampolineAllocator
{
public:
    static const size_t page_size = 1024*64;
    static const size_t block_size = 32;

    dpTrampolineAllocator();
    ~dpTrampolineAllocator();
    void* allocate(void *location);
    bool deallocate(void *v);

private:
    class Page;
    typedef std::vector<Page*> page_cont;
    page_cont m_pages;

    Page* createPage(void *location);
    Page* findOwnerPage(void *location);
    Page* findCandidatePage(void *location);
};

template<size_t PageSize, size_t BlockSize>
class dpBlockAllocator
{
public:
    static const size_t page_size = PageSize;
    static const size_t block_size = BlockSize;

    dpBlockAllocator();
    ~dpBlockAllocator();
    void* allocate();
    bool deallocate(void *v);

private:
    class Page;
    typedef std::vector<Page*> page_cont;
    page_cont m_pages;
};
typedef dpBlockAllocator<1024*256, sizeof(dpSymbol)> dpSymbolAllocator;

class dpSymbolTable
{
public:
    dpSymbolTable();
    void addSymbol(dpSymbol *v);
    void merge(const dpSymbolTable &v);
    void sort();
    void clear();
    void            enablePartialLink(bool v);
    size_t          getNumSymbols() const;
    dpSymbol*       getSymbol(size_t i);
    dpSymbol*       findSymbolByName(const char *name);
    dpSymbol*       findSymbolByAddress(void *sym);

    // F: [](const dpSymbol *sym)
    template<class F>
    void eachSymbols(const F &f)
    {
        size_t n = getNumSymbols();
        for(size_t i=0; i<n; ++i) { f(getSymbol(i)); }
    }

private:
    typedef std::vector<dpSymbol*> symbol_cont;
    symbol_cont m_symbols;
    bool m_partial_link;
};

#define dpGetBuilder()  m_context->getBuilder()
#define dpGetPatcher()  m_context->getPatcher()
#define dpGetLoader()   m_context->getLoader()


struct dpConfigFile
{
    int log_flags;
    int sys_flags;
    int vc_ver;
    std::vector<std::string> loads;
    std::vector<std::string> source_paths;
    std::vector<std::string> load_paths;
    std::vector<std::string> msbuild_commands;
    std::vector<std::string> build_commands;
    std::string target_process;
    std::string config_path;

    dpConfigFile();
    bool copy(const char *name);
    bool load();
    bool load(const char *path);
    bool load(const wchar_t *path);
};


class dpBinary
{
public:
    dpBinary(dpContext *ctx);
    virtual ~dpBinary();
    virtual bool loadFile(const char *path)=0;
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime)=0;
    virtual bool link()=0;
    virtual bool partialLink(size_t section)=0;
    virtual bool callHandler(dpEventType e)=0;

    virtual dpSymbolTable& getSymbolTable()=0;
    virtual const char*    getPath() const=0;
    virtual dpTime         getLastModifiedTime() const=0;
    virtual dpFileType     getFileType() const=0;

    // F: [](const dpSymbol *sym)
    template<class F> void eachSymbols(const F &f) { getSymbolTable().eachSymbols(f); }

protected:
    dpContext *m_context;
};

class dpObjFile : public dpBinary
{
public:
    static const dpFileType FileType= dpE_Obj;
    dpObjFile(dpContext *ctx);
    ~dpObjFile();
    void unload();
    virtual bool loadFile(const char *path);
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime);
    virtual bool link();
    virtual bool partialLink(size_t section);
    virtual bool callHandler(dpEventType e);

    virtual dpSymbolTable& getSymbolTable();
    virtual const char*    getPath() const;
    virtual dpTime         getLastModifiedTime() const;
    virtual dpFileType     getFileType() const;

    void* getBaseAddress() const;

private:
    struct RelocationData
    {
        uint32_t base;
        RelocationData() : base(0) {}
    };
    struct LinkData // section と対になるデータ
    {
        uint32_t flags;
        LinkData() : flags(dpE_NeedsLink|dpE_NeedsBase) {}
    };
    typedef std::vector<LinkData>       link_cont;
    typedef std::vector<RelocationData> reloc_cont;
    void  *m_data;
    size_t m_size;
    void  *m_aligned_data;
    size_t m_aligned_datasize;
    std::string m_path;
    dpTime m_mtime;
    dpSymbolTable m_symbols;
    link_cont m_linkdata;
    reloc_cont m_relocdata;

    void* resolveSymbol(const char *name);
};

class dpLibFile : public dpBinary
{
public:
    static const dpFileType FileType = dpE_Lib;
    dpLibFile(dpContext *ctx);
    ~dpLibFile();
    void unload();
    virtual bool loadFile(const char *path);
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime);
    virtual bool link();
    virtual bool partialLink(size_t section);
    virtual bool callHandler(dpEventType e);

    virtual dpSymbolTable& getSymbolTable();
    virtual const char*    getPath() const;
    virtual dpTime         getLastModifiedTime() const;
    virtual dpFileType     getFileType() const;
    size_t                 getNumObjFiles() const;
    dpObjFile*             getObjFile(size_t index);
    dpObjFile*             findObjFile(const char *name);

    template<class F>
    void eachObjs(const F &f) { dpEach(m_objs, f); }

private:
    typedef std::vector<dpObjFile*> obj_cont;
    obj_cont m_objs;
    dpSymbolTable m_symbols;
    std::string m_path;
    dpTime m_mtime;
};

// ロード中の dll は上書き不可能で、そのままだと実行時リビルドできない。
// そのため、指定のファイルをコピーしてそれを扱う。(関連する .pdb もコピーする)
// また、.dll だけでなく .exe も扱える (export された symbol がないと意味がないが)
class dpDllFile : public dpBinary
{
public:
    static const dpFileType FileType = dpE_Dll;
    dpDllFile(dpContext *ctx);
    ~dpDllFile();
    void unload();
    virtual bool loadFile(const char *path);
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime);
    virtual bool link();
    virtual bool partialLink(size_t section);
    virtual bool callHandler(dpEventType e);

    virtual dpSymbolTable& getSymbolTable();
    virtual const char*    getPath() const;
    virtual dpTime         getLastModifiedTime() const;
    virtual dpFileType     getFileType() const;

private:
    HMODULE m_module;
    bool m_needs_freelibrary;
    std::string m_path;
    std::string m_actual_file;
    std::string m_pdb_path;
    dpTime m_mtime;
    dpSymbolTable m_symbols;
};


class dpLoader
{
public:
    dpLoader(dpContext *ctx);
    ~dpLoader();

    dpBinary*  load(const char *path); // path to .obj, .lib, .dll, .exe
    dpObjFile* loadObj(const char *path);
    dpLibFile* loadLib(const char *path);
    dpDllFile* loadDll(const char *path);
    bool       unload(const char *path);
    size_t     reload();
    bool       link();

    size_t    getNumBinaries() const;
    dpBinary* getBinary(size_t index);
    dpBinary* findBinary(const char *name);

    dpSymbol* findSymbolByName(const char *name);
    dpSymbol* findSymbolByAddress(void *addr);
    dpSymbol* findHostSymbolByName(const char *name);
    dpSymbol* findHostSymbolByAddress(void *addr);

    // F: [](dpBinary *bin)
    template<class F>
    void eachBinaries(const F &f)
    {
        size_t n = getNumBinaries();
        for(size_t i=0; i<n; ++i) { f(getBinary(i)); }
    }

    void addOnLoadList(dpBinary *bin);
    dpSymbol* newSymbol(const char *nam=nullptr, void *addr=nullptr, int fla=0, int sect=0, dpBinary *bin=nullptr);
    void deleteSymbol(dpSymbol *sym);

private:
    typedef std::vector<dpBinary*> binary_cont;

    dpContext *m_context;
    binary_cont m_binaries;
    binary_cont m_onload_queue;
    dpSymbolTable m_hostsymbols;
    dpSymbolAllocator m_symalloc;

    void       unloadImpl(dpBinary *bin);
    template<class BinaryType>
    BinaryType* loadBinaryImpl(const char *path);
};


class dpPatcher
{
public:
    dpPatcher(dpContext *ctx);
    ~dpPatcher();
    void*  patchByBinary(dpBinary *obj, const std::function<bool (const dpSymbolS&)> &condition);
    void*  patch(dpSymbol *target, dpSymbol *hook);
    size_t unpatchByBinary(dpBinary *obj);
    bool   unpatchByAddress(void *patched);
    void   unpatchAll();

    dpPatchData* findPatchByName(const char *name);
    dpPatchData* findPatchByAddress(void *addr);

private:
    typedef std::set<dpPatchData> patch_cont;

    dpContext             *m_context;
    dpTrampolineAllocator m_talloc;
    patch_cont            m_patches;

    void         patchImpl(dpPatchData &pi);
    void         unpatchImpl(const dpPatchData &pi);
    patch_cont::iterator findPatchByNameImpl(const char *name);
    patch_cont::iterator findPatchByAddressImpl(void *addr);
};


class dpBuilder
{
public:
    dpBuilder(dpContext *ctx);
    ~dpBuilder();
    void addLoadPath(const char *path);
    void addSourcePath(const char *path);
    void addMSBuildCommand(const char *msbuild_options);
    void addBuildCommand(const char *any_command);
    bool startAutoBuild();
    bool stopAutoBuild();

    void   update();
    size_t reload();
    void   watchFiles();
    bool   build();

    const char* getVCVars() const;

private:
    struct SourcePath
    {
        std::string path;
        HANDLE notifier;
    };

    dpContext *m_context;
    std::string m_vcvars;
    std::string m_msbuild;
    std::vector<std::string> m_build_commands;
    std::vector<SourcePath>  m_srcpathes;
    std::vector<std::string> m_loadpathes;
    mutable bool m_build_done;
    bool m_watchfile_stop;
    HANDLE m_thread_autobuild;
};


class dpContext
{
public:
    dpContext();
    ~dpContext();
    dpBuilder* getBuilder();
    dpPatcher* getPatcher();
    dpLoader*  getLoader();

    size_t     load(const char *path);
    dpObjFile* loadObj(const char *path);
    dpLibFile* loadLib(const char *path);
    dpDllFile* loadDll(const char *path);
    bool       unload(const char *path);
    bool       link();

    size_t patchByFile(const char *filename, const char *filter_regex);
    size_t patchByFile(const char *filename, const std::function<bool (const dpSymbolS&)> &condition);
    bool   patchNameToName(const char *target_name, const char *hook_name);
    bool   patchAddressToName(const char *target_name, void *hook);
    bool   patchAddressToAddress(void *target, void *hook);
    bool   patchByAddress(void *hook);
    bool   unpatchByAddress(void *target_or_hook_addr);
    void*  getUnpatched(void *target_or_hook_addr);

private:
    dpBuilder *m_builder;
    dpPatcher *m_patcher;
    dpLoader  *m_loader;
};





template<class F>
inline void dpGlob(const char *path, const F &f)
{
    std::string dir;
    dpSeparateDirFile(path, &dir, nullptr);
    WIN32_FIND_DATAA wfdata;
    HANDLE handle = ::FindFirstFileA(path, &wfdata);
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

template<class Container, class F>
inline void dpEach(Container &cont, const F &f)
{
    std::for_each(cont.begin(), cont.end(), f);
}

template<class Container, class F>
inline auto dpFind(Container &cont, const F &f) -> decltype(cont.begin())
{
    return std::find_if(cont.begin(), cont.end(), f);
}
