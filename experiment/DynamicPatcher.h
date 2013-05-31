// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#ifndef DynamicPatcher_h
#define DynamicPatcher_h

#ifndef dpDisable
#include <functional>

#ifdef _WIN64
#   define dpLibArch "64"
#   define dpSymPrefix
#else // _WIN64
#   define dpLibArch 
#   define dpSymPrefix "_"
#endif //_WIN64
#ifdef _DEBUG
#   define dpLibConfig "d"
#else // _DEBUG
#   define dpLibConfig 
#endif //_DEBUG

#if defined(dpDLLImpl)
#   define dpAPI dpDLLExport
#elif defined(dpLinkDynamic)
#   define dpAPI dpDLLImport
#   pragma comment(lib,"DynamicPatcher" dpLibArch ".lib")
#elif defined(dpLinkStatic)
#   define dpAPI
#   pragma comment(lib,"DynamicPatchers" dpLibConfig dpLibArch ".lib")
#else
#   define dpAPI
#endif // dpDLL_Impl

#define dpDLLExport     __declspec(dllexport)
#define dpDLLImport     __declspec(dllimport)
#define dpCLinkage      extern "C"

#define dpPatch         dpDLLExport
#define dpNoInline      __declspec(noinline)
#define dpScope(...)    __VA_ARGS__
#define dpOnLoad(...)   dpCLinkage static void dpOnLoadHandler()  { __VA_ARGS__ } __declspec(selectany) void *_dpOnLoadHandler  =dpOnLoadHandler;
#define dpOnUnload(...) dpCLinkage static void dpOnUnloadHandler(){ __VA_ARGS__ } __declspec(selectany) void *_dpOnUnloadHandler=dpOnUnloadHandler;

enum dpSymbolFlags {
    dpE_Code    = 0x1,  // code
    dpE_IData   = 0x2,  // initialized data
    dpE_UData   = 0x4,  // uninitialized data
    dpE_Read    = 0x8,  // readable
    dpE_Write   = 0x10, // writable
    dpE_Execute = 0x20, // executable
    dpE_Shared  = 0x40, // shared
    dpE_Export  = 0x80, // dllexport
};
#define dpIsFunction(flag) ((flag&dpE_Code)!=0)

struct dpSymbol
{
    const char *name;
    void *address;
    int flags;

    dpSymbol(const char *n=NULL, void *a=NULL, int f=0)
        : name(n), address(a), flags(f)
    {}
};

class dpContext;

dpAPI bool   dpInitialize();
dpAPI bool   dpFinalize();

dpAPI dpContext* dpGetDefaultContext();
dpAPI dpContext* dpCreateContext();
dpAPI void       dpDeleteContext(dpContext *ctx);
dpAPI void       dpSetCurrentContext(dpContext *ctx); // current context is thread local
dpAPI dpContext* dpGetCurrentContext(); // default is dpGetDefaultContext()

dpAPI size_t dpLoad(const char *path); // path to .obj .lib .dll .exe. accept wildcard. (ex: x64/Debug/*.obj)
dpAPI bool   dpLoadObj(const char *path); // load as obj regardless file extension
dpAPI bool   dpLoadLib(const char *path); // load as lib regardless file extension
dpAPI bool   dpLoadDll(const char *path); // load as dll regardless file extension
dpAPI bool   dpUnload(const char *path);
dpAPI bool   dpLink(); // must be called after dpLoad*()s & dpUnload()s. onload handler is called in this.

dpAPI size_t dpPatchByFile(const char *filename, const char *filter_regex);
#if _MSC_VER>=1600 // require C++11
dpAPI size_t dpPatchByFile(const char *filename, const std::function<bool (const dpSymbol&)> &condition);
#endif // _MSC_VER>=1600
dpAPI bool   dpPatchByName(const char *symbol_name);
dpAPI bool   dpPatchByAddress(void *target, void *hook);
dpAPI void*  dpGetUnpatched(void *target);

dpAPI void   dpAddLoadPath(const char *path); // accept wildcard.
dpAPI void   dpAddSourcePath(const char *path);
dpAPI bool   dpStartAutoBuild(const char *msbuild_option, bool console=false);
dpAPI bool   dpStopAutoBuild();
dpAPI void   dpUpdate();

#else  // dpDisable

#define dpPatch 
#define dpNoInline 
#define dpScope(...) 
#define dpOnLoad(...) 
#define dpOnUnload(...) 

#define dpInitialize(...) 
#define dpFinalize(...) 

#define dpGetDefaultContext(...) 
#define dpCreateContext(...) 
#define dpDeleteContext(...) 
#define dpSetCurrentContext(...) 
#define dpGetCurrentContext(...) 

#define dpLoad(...) 
#define dpLoadObj(...) 
#define dpLoadLib(...) 
#define dpLoadDll(...) 
#define dpUnload(...) 
#define dpLink(...) 
#define dpPatchByFile(...) 
#define dpPatchByName(...) 
#define dpPatchByAddress(...) 
#define dpGetUnpatched(...) 

#define dpAddLoadPath(...) 
#define dpAddSourcePath(...) 
#define dpStartAutoBuild(...) 
#define dpStopAutoBuild(...) 
#define dpUpdate(...) 

#endif // dpDisable

#endif // DynamicPatcher_h
