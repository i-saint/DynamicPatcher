// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpBinary_h
#define dpBinary_h

#include "DynamicPatcher.h"
#include "dpFoundation.h"

enum dpFileType {
    dpE_ObjFile,
    dpE_LibFile,
    dpE_DllFile,
    dpE_ElfFile,
};
enum dpArch {
    dpE_Arch_unknown,
    dpE_Arch_x86,
    dpE_Arch_x86_64,
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


enum dpE_FilterFlags {
    dpE_IncludeUpdate     = 0x1,
    dpE_IncludeLinkToLocal= 0x2,
    dpE_IncludeOnLoad     = 0x4,
    dpE_IncludeOnUnload   = 0x8,
};

struct dpSymbolPattern
{
    uint32_t flags;
    std::string name;

    dpSymbolPattern(uint32_t f=0, const std::string &n="")
        : flags(f), name(n)
    {}
};
inline bool operator<(const dpSymbolPattern &a, const dpSymbolPattern &b) { return a.name<b.name; }

class dpSymbolFilter
{
public:
    dpSymbolFilter();
    ~dpSymbolFilter();
    void clear();
    void addPattern(const dpSymbolPattern &pattern);
    const dpSymbolPattern* findPattern(const char *sym_name) const;
    bool matchUpdate(const char *sym_name) const;
    bool matchLinkToLocal(const char *sym_name) const;
    bool matchOnLoad(const char *sym_name) const;
    bool matchOnUnload(const char *sym_name) const;

    // F [](const dpSymbolPattern &)
    template<class F>
    void eachPatterns(const F &f)
    {
        std::for_each(m_patterns.begin(), m_patterns.end(), f);
    }

private:
    typedef std::set<dpSymbolPattern> PatternCont;
    PatternCont m_patterns;
};

class dpSymbolFilters
{
public:
    dpSymbolFilters();
    ~dpSymbolFilters();
    dpSymbolFilter* getFilter(const char *module_name);
    bool            eraseFilter(const char *module_name);

private:
    typedef std::map<std::string, dpSymbolFilter*> FilterCont;
    FilterCont m_filters;
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


#ifdef _M_X64
#   define dpSymPrefix
#else // _M_X64
#   define dpSymPrefix "_"
#endif //_M_X64

typedef unsigned long long QWORD;
extern const char* g_dpSymName_OnLoad;
extern const char* g_dpSymName_OnUnload;

typedef void (*dpEventHandler)();
void dpCallOnLoadHandler(dpBinary *v);
void dpCallOnUnloadHandler(dpBinary *v);


#endif // dpBinary_h
