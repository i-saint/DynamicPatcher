// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#ifndef DynamicPatcher_h
#define DynamicPatcher_h

#ifndef dpDisable

#if _MSC_VER>=1600 // dpPatchByFile() require C++11
#   define dpWithStdFunction
#endif // _MSC_VER>=1600

#ifdef _M_X64
#   define dpLibArch "64"
#   define dpSymPrefix
#else // _M_X64
#   define dpLibArch 
#   define dpSymPrefix "_"
#endif //_M_X64
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
#ifdef dpWithStdFunction
#   include <functional>
#endif // dpWithStdFunction

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
    dpE_Handler = 0x100, // dpOnLoad, dpOnUnload, etc
};
#define dpIsFunction(flag) ((flag&dpE_Code)!=0)
#define dpIsExportFunction(flag) ((flag&(dpE_Code|dpE_Export|dpE_Handler))==(dpE_Code|dpE_Export))

struct dpSymbolS
{
    const char *name;
    void *address;
    int flags;
};


enum dpLogLevel {
    dpE_LogError   = 0x1,
    dpE_LogWarning = 0x2,
    dpE_LogInfo    = 0x4,
    dpE_LogDetail  = 0x8,

    dpE_LogDetailed = dpE_LogError|dpE_LogWarning|dpE_LogInfo|dpE_LogDetail,
    dpE_LogSimple   = dpE_LogError|dpE_LogWarning|dpE_LogInfo,
    dpE_LogNone     = 0,
};
enum dpSystemFlags {
    dpE_AutoPatchExports = 0x1, // patch exported (dllexport==dpPatch) functions automatically when the module is loaded
    dpE_DelayedLink = 0x2,

    dpE_SysDefault = dpE_AutoPatchExports|dpE_DelayedLink,
};

struct dpConfig
{
    int log_level; // combination of dpLogLevel
    int sysflags; // combination of dpSystemFlags

    dpConfig(int log=dpE_LogDetailed, int f=dpE_SysDefault) : log_level(log), sysflags(f)
    {}
};

class dpContext;

dpAPI bool   dpInitialize(const dpConfig &conf=dpConfig());
dpAPI bool   dpFinalize();

dpAPI dpContext* dpGetDefaultContext();
dpAPI dpContext* dpCreateContext();
dpAPI void       dpDeleteContext(dpContext *ctx);
dpAPI void       dpSetCurrentContext(dpContext *ctx); // current context is thread local
dpAPI dpContext* dpGetCurrentContext(); // default is dpGetDefaultContext()

dpAPI size_t dpLoad(const char *path); // path to .obj .lib .dll .exe. accept wildcard (ex: x64/Debug/*.obj)
dpAPI bool   dpLoadObj(const char *path); // load as .obj regardless file extension
dpAPI bool   dpLoadLib(const char *path); // load as .lib regardless file extension
dpAPI bool   dpLoadDll(const char *path); // load as .dll regardless file extension
dpAPI bool   dpUnload(const char *path);
dpAPI bool   dpLink(); // must be called after dpLoad*()s & dpUnload()s. onload handler is called in this.

dpAPI size_t dpPatchByFile(const char *filename, const char *filter_regex);
#ifdef dpWithStdFunction
dpAPI size_t dpPatchByFile(const char *filename, const std::function<bool (const dpSymbolS&)> &condition);
#endif // dpWithStdFunction
dpAPI bool   dpPatchNameToName(const char *target_name, const char *hook_name);
dpAPI bool   dpPatchAddressToName(const char *target_name, void *hook_addr);
dpAPI bool   dpPatchAddressToAddress(void *target, void *hook_addr);
dpAPI bool   dpPatchByAddress(void *hook_addr); // patch the host symbol that have same name of hook
dpAPI bool   dpUnpatchByAddress(void *target_or_hook_addr);
dpAPI void*  dpGetUnpatched(void *target_or_hook_addr);

dpAPI void   dpAddLoadPath(const char *path); // accept wildcard.
dpAPI void   dpAddSourcePath(const char *path);
dpAPI bool   dpStartAutoBuild(const char *msbuild_option, bool console=false);
dpAPI bool   dpStopAutoBuild();
dpAPI void   dpUpdate();

dpAPI void   dpPrint(const char* fmt, ...);

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
#define dpPatchNameToName(...) 
#define dpPatchAddressToName(...) 
#define dpPatchAddressToAddress(...) 
#define dpPatchByAddress(...) 
#define dpUnpatchByAddress(...)
#define dpGetUnpatched(...) 

#define dpAddLoadPath(...) 
#define dpAddSourcePath(...) 
#define dpStartAutoBuild(...) 
#define dpStopAutoBuild(...) 
#define dpUpdate(...) 

#define dpPrint(...) 

#endif // dpDisable

#endif // DynamicPatcher_h
