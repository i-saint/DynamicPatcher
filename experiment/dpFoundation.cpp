// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"

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
}

void dpPrint(const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    dpPrintV(fmt, vl);
    va_end(vl);
}



// 位置指定版 VirtualAlloc()
// location より前の最寄りの位置にメモリを確保する。
void* dpAllocate(size_t size, void *location)
{
    if(size==0) { return NULL; }
    static size_t base = (size_t)location;

    // ドキュメントには、アドレス指定の VirtualAlloc() は指定先が既に予約されている場合最寄りの領域を返す、
    // と書いてるように見えるが、実際には NULL が返ってくるようにしか見えない。
    // なので成功するまでアドレスを進めつつリトライ…。
    void *ret = NULL;
    const size_t step = 0x10000; // 64kb
    for(size_t i=0; ret==NULL; ++i) {
        ret = ::VirtualAlloc((void*)((size_t)base-(step*i)), size, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    return ret;
}

// exe がマップされている領域の後ろの最寄りの場所にメモリを確保する。
// jmp 命令などの移動量は x64 でも 32bit なため、32bit に収まらない距離を飛ぼうとした場合あらぬところに着地して死ぬ。
// そして new や malloc() だと 32bit に収まらない遥か彼方にメモリが確保されてしまうため、これが必要になる。
// .exe がマップされている領域を調べ、その近くに VirtualAlloc() するという内容。
void* dpAllocateModule(size_t size)
{
    return dpAllocate(size, GetModuleHandleA(nullptr));
}

dpTime dpGetFileModifiedTime(const char *path)
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

bool dpDemangle(const char *mangled, char *demangled, size_t buflen)
{
    return ::UnDecorateSymbolName(mangled, demangled, (DWORD)buflen, UNDNAME_COMPLETE)!=0;
}
