#include "windows.h"
#include <string>

#ifdef _M_IX64
#   define InjectorDLL       "DynamicPatcherInjectorDLL64.dll"
#   define PatcherDLL        "DynamicPatcher64.dll"
#else // _M_IX86
#   define InjectorDLL       "DynamicPatcherInjectorDLL.dll"
#   define PatcherDLL        "DynamicPatcher.dll"
#endif // _M_IX64

bool InjectDLL(HANDLE hProcess, const char* dllname)
{
    SIZE_T bytesRet = 0;
    DWORD oldProtect = 0;
    LPVOID remote_addr = NULL;
    HANDLE hThread = NULL;
    size_t len = strlen(dllname) + 1;

    remote_addr = ::VirtualAllocEx(hProcess, 0, 1024, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if(remote_addr==NULL) { return false; }
    ::VirtualProtectEx(hProcess, remote_addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
    ::WriteProcessMemory(hProcess, remote_addr, dllname, len, &bytesRet);
    ::VirtualProtectEx(hProcess, remote_addr, len, oldProtect, &oldProtect);

    hThread = ::CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)((void*)&LoadLibraryA), remote_addr, 0, NULL);
    ::WaitForSingleObject(hThread, INFINITE); 
    ::VirtualFreeEx(hProcess, remote_addr, 0, MEM_RELEASE);
    return true;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE prev, LPWSTR cmd, int show)
{
    std::wstring exe_path = __wargv[1];
    const char dll_path[MAX_PATH] = InjectorDLL;

    DWORD flags = NORMAL_PRIORITY_CLASS;
    if(__argc>=3 && wcscmp(__wargv[2], L"/suspended")==0) {
        flags |= CREATE_SUSPENDED;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ::ZeroMemory(&si, sizeof(si));
    ::ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    BOOL ret = ::CreateProcessW(nullptr, (wchar_t*)exe_path.c_str(), nullptr, nullptr, FALSE,
        flags, nullptr, nullptr, &si, &pi);
    if(ret) {
        InjectDLL(pi.hProcess, dll_path);
        return pi.dwProcessId;
    }

    return 0;
}
