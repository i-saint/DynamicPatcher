// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpFoundation_h
#define dpFoundation_h

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <process.h>
#include <cstdint>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <algorithm>
#include <regex>
#include "DynamicPatcher.h"
#include "dpFeatures.h"

typedef unsigned long long dpTime;
class dpBinary;
class dpObjFile;
class dpLibFile;
class dpDllFile;
class dpElfFile;
class dpLoader;
class dpPatcher;
class dpBuilder;
class dpCommunicator;
class dpContext;
class dpSymbolFilter;
class dpSymbolFilters;



template<class Container, class F> inline void dpEach(Container &cont, const F &f);
template<class Container, class F> inline auto dpFind(Container &cont, const F &f) -> decltype(cont.begin());

class dpMutex
{
public:
    class ScopedLock
    {
    public:
        ScopedLock(dpMutex &v);
        ~ScopedLock();
        dpMutex &mutex;
    };
    dpMutex();
    ~dpMutex();
    void lock();
    void unlock();
private:
    CRITICAL_SECTION m_cs;
};
std::string& dpGetLastError();
void    dpPrintError(const char* fmt, ...);
void    dpPrintWarning(const char* fmt, ...);
void    dpPrintInfo(const char* fmt, ...);
void    dpPrintDetail(const char* fmt, ...);

// location より大きいアドレスの最寄りの位置に実行可能メモリを確保する。
void*   dpAllocateForward(size_t size, void *location);

// location より小さいアドレスの最寄りの位置に実行可能メモリを確保する。
void*   dpAllocateBackward(size_t size, void *location);

// メインモジュール (.exe) がマップされている領域の後ろの最寄りの場所にメモリを確保する。
// jmp 命令などの移動量は x64 でも 32bit なため、32bit に収まらない距離を飛ぼうとした場合あらぬところに着地して死ぬ。
// そして new や malloc() だと 32bit に収まらない遥か彼方にメモリが確保されてしまうため、これが必要になる。
void*   dpAllocateModule(size_t size);

// dpAllocateForward(), dpAllocateBackward(), dpAllocateModule() で確保したメモリはこれで開放する
void    dpDeallocate(void *location);


dpTime  dpGetMTime(const char *path);
dpTime  dpGetSystemTime();

bool    dpCopyFile(const char *srcpath, const char *dstpath);
bool    dpWriteFile(const char *path, const void *data, size_t size);
bool    dpDeleteFile(const char *path);
bool    dpFileExists(const char *path);
template<class F> void dpGlob(const char *path, const F &f);
template<class F> bool dpMapFile(const char *path, void *&o_data, size_t &o_size, const F &alloc);

size_t  dpSeparateDirFile(const char *path, std::string *dir, std::string *file);
size_t  dpSeparateFileExt(const char *filename, std::string *file, std::string *ext);
size_t  dpGetCurrentModulePath(char *buf, size_t buflen);
size_t  dpGetMainModulePath(char *buf, size_t buflen);
void    dpSanitizePath(std::string &path);

// proc を新規スレッドで実行。VC2010 が std::thread 使えないのでこれで代用
void dpRunThread(const std::function<void ()> &proc);
void dpEnumerateThreads(const std::function<void (DWORD)> &proc);
// 現在のスレッド以外を一時的に全部 suspend して proc を実行
dpAPI void dpExecExclusive(const std::function<void ()> &proc);


inline bool dpIsValidMemory(void *p)
{
    MEMORY_BASIC_INFORMATION meminfo;
    return p!=nullptr && ::VirtualQuery(p, &meminfo, sizeof(meminfo))!=0 && meminfo.State!=MEM_FREE;
}

// F: [](const char *filepath)
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

template<class F>
void dpEachLines(char *str, size_t len, const F &f)
{
    char *start = str;
    for(size_t i=0; i<len; ++i) {
        if(*str=='\n' || *str=='\0') {
            *str = '\0';
            if(str!=start) {
                f(start);
            }
            start = str+1;
        }
        ++str;
    }
}

// F: [](HMODULE mod)->void
template<class F>
void dpEachModules(const F &f)
{
    std::vector<HMODULE> modules;
    DWORD num_modules;
    ::EnumProcessModules(::GetCurrentProcess(), nullptr, 0, &num_modules);
    modules.resize(num_modules/sizeof(HMODULE));
    ::EnumProcessModules(::GetCurrentProcess(), &modules[0], num_modules, &num_modules);
    for(size_t i=0; i<modules.size(); ++i) {
        f(modules[i]);
    }
}

#endif // dpFoundation_h
