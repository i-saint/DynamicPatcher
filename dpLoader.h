// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpLoader_h
#define dpLoader_h

#include "DynamicPatcher.h"
#include "dpBinary.h"


class dpLoader
{
public:
    dpLoader(dpContext *ctx);
    ~dpLoader();

    dpBinary*  load(const char *path); // path to .obj, .lib, .dll, .exe
#ifdef dpWithObjFile
    dpObjFile* loadObj(const char *path);
#endif // dpWithObjFile
#ifdef dpWithLibFile
    dpLibFile* loadLib(const char *path);
#endif // dpWithLibFile
#ifdef dpWithDllFile
    dpDllFile* loadDll(const char *path);
#endif // dpWithDllFile

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

    void addForceHostSymbolPattern(const char *pattern);
    bool doesForceHostSymbol(const char *name);

    bool   loadMapFile(const char *path, void *imagebase);
    size_t loadMapFiles();

private:
    typedef std::vector<dpBinary*>  binary_cont;
    typedef std::vector<std::regex> pattern_cont;
    typedef std::set<std::string>   string_set;

    dpContext *m_context;
    string_set m_mapfiles_read;
    pattern_cont m_force_host_symbol_patterns;
    binary_cont m_binaries;
    binary_cont m_onload_queue;
    dpSymbolTable m_hostsymbols;
    dpSymbolAllocator m_symalloc;

    void        unloadImpl(dpBinary *bin);
    template<class BinaryType>
    BinaryType* loadBinaryImpl(const char *path);
    void        setupDummySymbols();
};

#endif // dpLoader_h
