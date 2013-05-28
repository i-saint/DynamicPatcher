// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#ifndef DynamicPatcher_h
#define DynamicPatcher_h

#ifndef dpDisable
#include <cstdint>
#include <cstring>
#include <functional>

#define dpDLLExport __declspec(dllexport)
#define dpDLLImport __declspec(dllimport)
#define dpCLinkage extern "C"

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

#ifdef _WIN64
#   define dpSymPrefix
#else // _WIN64
#   define dpSymPrefix "_"
#endif // _WIN64


#define dpPatch         dpDLLExport
#define dpScope(...)    __VA_ARGS__
#define dpOnLoad(...)   dpCLinkage static void dpOnLoadHandler()  { __VA_ARGS__ } __declspec(selectany) void *_dpOnLoadHandler  =dpOnLoadHandler;
#define dpOnUnload(...) dpCLinkage static void dpOnUnloadHandler(){ __VA_ARGS__ } __declspec(selectany) void *_dpOnUnloadHandler=dpOnUnloadHandler;

enum dpSymbolFlags {
    dpE_Data    = 1, // rw data
    dpE_RData   = 2, // readonly data
    dpE_Export  = 4, // dllexport
};
#define dpIsFunction(flag) ((flag & (dpE_Data|dpE_RData))==0)

struct dpSymbol
{
    const char *name;
    void *address;
    uint32_t flags;

    dpSymbol(const char *n=NULL, void *a=NULL, uint32_t f=0)
        : name(n), address(a), flags(f)
    {}
};
inline bool operator< (const dpSymbol &a, const dpSymbol &b) { return strcmp(a.name, b.name)<0; }
inline bool operator==(const dpSymbol &a, const dpSymbol &b) { return strcmp(a.name, b.name)==0; }


dpAPI bool   dpInitialize();
dpAPI bool   dpFinalize();

dpAPI size_t dpLoad(const char *path); // path to .obj .lib .dll .exe. accept wildcard (x64/Debug/*.obj)
dpAPI bool   dpLink();

dpAPI size_t dpPatchByFile(const char *filename, const char *filter_regex);
#if _MSC_VER>=1600 // require C++11
dpAPI size_t dpPatchByFile(const char *filename, const std::function<bool (const dpSymbol&)> &condition);
#endif // _MSC_VER>=1600
dpAPI bool   dpPatchByName(const char *symbol_name);
dpAPI bool   dpPatchByAddress(void *target, void *hook);
dpAPI void*  dpGetUnpatchedFunction(void *target);

dpAPI void   dpAddLoadPath(const char *path); // accept wildcard (x64/Debug/*.obj)
dpAPI void   dpAddSourcePath(const char *path);
dpAPI bool   dpStartAutoCompile(const char *msbuild_option, bool console=false);
dpAPI bool   dpStopAutoCompile();
dpAPI void   dpUpdate();

#else  // dpDisable

#define dpPatch         
#define dpScope(...)    
#define dpOnLoad(...)   
#define dpOnUnload(...) 

#define dpInitialize(...) 
#define dpFinalize(...) 
#define dpLoad(...) 
#define dpLink(...) 
#define dpPatchByFile(...) 
#define dpPatchByName(...) 
#define dpPatchByAddress(...) 
#define dpGetUnpatchedFunction(...) 

#define dpAddLoadPath(...) 
#define dpAddSourcePath(...) 
#define dpStartAutoCompile(...) 
#define dpStopAutoCompile(...) 
#define dpUpdate(...) 

#endif // dpDisable

#endif // DynamicPatcher_h
