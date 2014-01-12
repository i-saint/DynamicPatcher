// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include "DynamicPatcher.h"
#include "dpFoundation.h"
#include "dpConfigFile.h"
#pragma comment(lib, "dbghelp.lib")

dpMutex::ScopedLock::ScopedLock(dpMutex &v) : mutex(v) { mutex.lock(); }
dpMutex::ScopedLock::~ScopedLock() { mutex.unlock(); }
dpMutex::dpMutex() { ::InitializeCriticalSection(&m_cs); }
dpMutex::~dpMutex() { ::DeleteCriticalSection(&m_cs); }
void dpMutex::lock() { ::EnterCriticalSection(&m_cs); }
void dpMutex::unlock() { ::LeaveCriticalSection(&m_cs); }


std::string& dpGetLastError()
{
    static std::string s_mes;
    return s_mes;
}


template<size_t N>
inline int dpVSprintf(char (&buf)[N], const char *format, va_list vl)
{
    return _vsnprintf(buf, N, format, vl);
}

static const int DPRINTF_MES_LENGTH  = 4096;
void dpPrintV(const char* fmt, va_list vl)
{
    char buf[DPRINTF_MES_LENGTH];
    dpVSprintf(buf, fmt, vl);
    ::OutputDebugStringA(buf);
#ifndef alcImpl
    dpGetLastError() = buf;
#endif // alcImpl
}

dpAPI void dpPrint(const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    dpPrintV(fmt, vl);
    va_end(vl);
}

void dpPrintError(const char* fmt, ...)
{
#ifndef alcImpl
    if((dpGetConfig().log_flags&dpE_LogError)==0) { return; }
#endif // alcImpl
    std::string format = std::string(dpLogHeader " error: ")+fmt; // OutputDebugStringA() は超遅いので dpPrint() 2 回呼ぶよりこうした方がまだ速い
    va_list vl;
    va_start(vl, fmt);
    dpPrintV(format.c_str(), vl);
    va_end(vl);
}

void dpPrintWarning(const char* fmt, ...)
{
#ifndef alcImpl
    if((dpGetConfig().log_flags&dpE_LogWarning)==0) { return; }
#endif // alcImpl
    std::string format = std::string(dpLogHeader " warning: ")+fmt;
    va_list vl;
    va_start(vl, fmt);
    dpPrintV(format.c_str(), vl);
    va_end(vl);
}

void dpPrintInfo(const char* fmt, ...)
{
#ifndef alcImpl
    if((dpGetConfig().log_flags&dpE_LogInfo)==0) { return; }
#endif // alcImpl
    std::string format = std::string(dpLogHeader " info: ")+fmt;
    va_list vl;
    va_start(vl, fmt);
    dpPrintV(format.c_str(), vl);
    va_end(vl);
}

void dpPrintDetail(const char* fmt, ...)
{
#ifndef alcImpl
    if((dpGetConfig().log_flags&dpE_LogDetail)==0) { return; }
#endif // alcImpl
    std::string format = std::string(dpLogHeader " detail: ")+fmt;
    va_list vl;
    va_start(vl, fmt);
    dpPrintV(format.c_str(), vl);
    va_end(vl);
}


bool dpDemangleSignatured(const char *mangled, char *demangled, size_t buflen)
{
    return ::UnDecorateSymbolName(mangled, demangled, (DWORD)buflen, 
        UNDNAME_NO_MS_KEYWORDS|UNDNAME_NO_ALLOCATION_MODEL|UNDNAME_NO_ALLOCATION_LANGUAGE|
        UNDNAME_NO_MS_THISTYPE|UNDNAME_NO_CV_THISTYPE|UNDNAME_NO_THISTYPE|UNDNAME_NO_ACCESS_SPECIFIERS|
        UNDNAME_NO_RETURN_UDT_MODEL)!=0;
}

dpAPI bool dpDemangleNameOnly(const char *mangled, char *demangled, size_t buflen)
{
    return ::UnDecorateSymbolName(mangled, demangled, (DWORD)buflen, UNDNAME_NAME_ONLY)!=0;
}



void* dpAllocateForward(size_t size, void *location)
{
    if(size==0) { return NULL; }
    size_t base = (size_t)location;

    // ドキュメントには、アドレス指定の VirtualAlloc() は指定先が既に予約されている場合最寄りの領域を返す、
    // と書いてるように見えるが、実際には NULL が返ってくるようにしか見えない。
    // なので成功するまでアドレスを進めつつリトライ…。
    void *ret = NULL;
    const size_t step = 0x10000; // 64kb
    for(size_t i=0; ret==NULL; ++i) {
        ret = ::VirtualAlloc((void*)((size_t)base+(step*i)), size, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    return ret;
}

void* dpAllocateBackward(size_t size, void *location)
{
    if(size==0) { return NULL; }
    size_t base = (size_t)location;

    void *ret = NULL;
    const size_t step = 0x10000; // 64kb
    for(size_t i=0; ret==NULL; ++i) {
        ret = ::VirtualAlloc((void*)((size_t)base-(step*i)), size, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    return ret;
}

void* dpAllocateModule(size_t size)
{
    return dpAllocateBackward(size, GetModuleHandleA(nullptr));
}

void dpDeallocate(void *location)
{
    ::VirtualFree(location, 0, MEM_RELEASE);
}

dpTime dpGetMTime(const char *path)
{
    union RetT
    {
        FILETIME filetime;
        dpTime qword;
    } ret;
    HANDLE h = ::CreateFileA(path, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ::GetFileTime(h, NULL, NULL, &ret.filetime);
    ::CloseHandle(h);
    return ret.qword;
}

dpTime dpGetSystemTime()
{
    union RetT
    {
        FILETIME filetime;
        dpTime qword;
    } ret;
    SYSTEMTIME systime;
    ::GetSystemTime(&systime);
    ::SystemTimeToFileTime(&systime, &ret.filetime);
    return ret.qword;
}

bool dpCopyFile(const char *srcpath, const char *dstpath)
{
    return ::CopyFileA(srcpath, dstpath, FALSE)==TRUE;
}

bool dpWriteFile(const char *path, const void *data, size_t size)
{
    if(FILE *f=fopen(path, "wb")) {
        fwrite((const char*)data, 1, size, f);
        fclose(f);
        return true;
    }
    return false;
}

bool dpDeleteFile(const char *path)
{
    return ::DeleteFileA(path)==TRUE;
}

bool dpFileExists( const char *path )
{
    return ::GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES;
}

size_t dpSeparateDirFile(const char *path, std::string *dir, std::string *file)
{
    size_t f_len=0;
    size_t l = strlen(path);
    for(size_t i=0; i<l; ++i) {
        if(path[i]=='\\' || path[i]=='/') { f_len=i+1; }
    }
    if(dir)  { dir->insert(dir->end(), path, path+f_len); }
    if(file) { file->insert(file->end(), path+f_len, path+l); }
    return f_len;
}

size_t dpSeparateFileExt(const char *filename, std::string *file, std::string *ext)
{
    size_t dir_len=0;
    size_t l = strlen(filename);
    for(size_t i=0; i<l; ++i) {
        if(filename[i]=='.') { dir_len=i+1; }
    }
    if(file){ file->insert(file->end(), filename, filename+dir_len); }
    if(ext) { ext->insert(ext->end(), filename+dir_len, filename+l); }
    return dir_len;
}

size_t dpGetCurrentModulePath(char *buf, size_t buflen)
{
    HMODULE mod = 0;
    ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)&dpGetCurrentModulePath, &mod);
    return ::GetModuleFileNameA(mod, buf, (DWORD)buflen);
}

size_t dpGetMainModulePath(char *buf, size_t buflen)
{
    HMODULE mod = ::GetModuleHandleA(nullptr);
    return ::GetModuleFileNameA(mod, buf, (DWORD)buflen);
}

void dpSanitizePath(std::string &path)
{
    dpEach(path, [](char &c){
        if(c=='/') { c='\\'; }
    });
}


// F: [](DWORD thread_id)->void
template<class F>
inline void dpEnumerateThreadsImpl(DWORD pid, const F &proc)
{
    HANDLE ss = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if(ss!=INVALID_HANDLE_VALUE) {
        THREADENTRY32 te;
        te.dwSize = sizeof(te);
        if(::Thread32First(ss, &te)) {
            do {
                if(te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID)+sizeof(te.th32OwnerProcessID) &&
                    te.th32OwnerProcessID==pid)
                {
                    proc(te.th32ThreadID);
                }
                te.dwSize = sizeof(te);
            } while(::Thread32Next(ss, &te));
        }
        ::CloseHandle(ss);
    }
}

void dpEnumerateThreads(const std::function<void (DWORD)> &proc)
{
    dpEnumerateThreadsImpl(::GetCurrentProcessId(), proc);
}

void dpExecExclusive(const std::function<void ()> &proc)
{
    std::vector<HANDLE> threads;
    dpEnumerateThreadsImpl(::GetCurrentProcessId(), [&](DWORD tid){
        if(tid==::GetCurrentThreadId()) { return; }
        if(HANDLE thread=::OpenThread(THREAD_ALL_ACCESS, FALSE, tid)) {
            ::SuspendThread(thread);
            threads.push_back(thread);
        }
    });
    proc();
    std::for_each(threads.begin(), threads.end(), [](HANDLE thread){
        ::ResumeThread(thread);
        ::CloseHandle(thread);
    });
}

unsigned int __stdcall dpRunThread_(void *proc_)
{
    auto *proc = (std::function<void ()>*)proc_;
    (*proc)();
    delete proc;
    return 0;
}

void dpRunThread(const std::function<void ()> &proc)
{
    _beginthreadex(nullptr, 0, &dpRunThread_, new std::function<void ()>(proc), 0, nullptr);
}

