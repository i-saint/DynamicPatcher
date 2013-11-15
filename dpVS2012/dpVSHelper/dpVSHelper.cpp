#include "stdafx.h"
#include "dpVSHelper.h"
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <cstdio>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

#ifdef _M_IX64
#   define InjectorExecutable "DynamicPatcherInjector64.exe"
#else  // _M_IX64
#   define InjectorExecutable "DynamicPatcherInjector.exe"
#endif // _M_IX64


DWORD dpVSHelper::ExecuteSuspended( String^ _path_to_exe, String^ _addin_dir, String^ _environment )
{
    IntPtr path_to_exe = Marshal::StringToHGlobalAnsi(_path_to_exe);
    IntPtr addin_dir = Marshal::StringToHGlobalAnsi(_addin_dir);
    IntPtr environment = Marshal::StringToHGlobalAnsi(_environment);

    char injector[MAX_PATH];
    sprintf(injector, "%s\\%s", (const char*)addin_dir.ToPointer(), InjectorExecutable);

    char command[4096];
    sprintf(command, "\"%s\" \"%s\" /suspended", injector, (const char*)path_to_exe.ToPointer());

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ::ZeroMemory(&si, sizeof(si));
    ::ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    BOOL ret = ::CreateProcessA(nullptr, command, nullptr, nullptr, FALSE,
        NORMAL_PRIORITY_CLASS, nullptr, (const char*)addin_dir.ToPointer(), &si, &pi);
    if(ret) {
        DWORD ret;
        ::WaitForSingleObject(pi.hProcess, INFINITE);
        ::GetExitCodeProcess(pi.hProcess, &ret);
        return ret;
    }
    return 0;
}

// F: [](DWORD thread_id)->void
template<class F>
inline void EnumerateThreads(DWORD pid, const F &f)
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
                    f(te.th32ThreadID);
                }
                te.dwSize = sizeof(te);
            } while(::Thread32Next(ss, &te));
        }
        ::CloseHandle(ss);
    }
}

bool dpVSHelper::Resume( DWORD pid )
{
    bool r = false;
    EnumerateThreads(pid, [&](DWORD thread_id){
        if(HANDLE hthread = ::OpenThread(THREAD_ALL_ACCESS, FALSE, thread_id)) {
            DWORD ret = ::ResumeThread(hthread);
            if(ret!=DWORD(-1)) { r=true; }
            ::CloseHandle(hthread);
        }
    });
    return r;
}
