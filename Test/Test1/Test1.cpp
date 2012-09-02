#include "stdafx.h"
#include <windows.h>
#include "DynamicObjLoader.h"
#include "Test1.h"
#pragma warning(disable: 4996) // _s じゃない CRT 関数使うとでるやつ


#define istPrint(...) DebugPrint(__VA_ARGS__)

template<size_t N>
inline int istvsprintf(char (&buf)[N], const char *format, va_list vl)
{
    return _vsnprintf(buf, N, format, vl);
}

static const int DPRINTF_MES_LENGTH  = 4096;
void DebugPrintV(const char* fmt, va_list vl)
{
    char buf[DPRINTF_MES_LENGTH];
    istvsprintf(buf, fmt, vl);
    ::OutputDebugStringA(buf);
}

void DebugPrint(const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    DebugPrintV(fmt, vl);
    va_end(vl);
}



// .obj から呼ぶ関数。最適化で消えないように DOL_Fixate つけておく
DOL_Fixate void FuncInExe()
{
    istPrint("FuncInExe()\n");
}


class Hoge : public IHoge
{
public:
    virtual void DoSomething()
    {
        istPrint("Hoge::DoSomething()\n");
    }
};


DOL_ImportFunction(float, FloatAdd, (float, float));
DOL_ImportFunction(void, CallExternalFunc, ());
DOL_ImportFunction(void, CallExeFunc, ());
DOL_ImportFunction(void, IHogeReceiver, (IHoge*));
DOL_ImportFunction(IHoge*, CreateObjHoge, ());
DOL_ImportVariable(int, g_test);

int main(int argc, _TCHAR* argv[])
{
    DOL_AddSourceDirectory(".\\");
#ifdef _WIN64
    DOL_StartAutoRecompile("/m /p:Configuration=Release;Platform=x64", true);
    DOL_LoadObjDirectory("x64\\Release");
#else // _WIN64
    DOL_StartAutoRecompile("/m /p:Configuration=Release;Platform=Win32", true);
    DOL_LoadObjDirectory("Release");
#endif // _WIN64
    DOL_Link();

    for(;;) {
        g_test += 10;
        istPrint("g_test: %d\n", (int)g_test);
        istPrint("%.2f\n", FloatAdd(1.0f, 2.0f));
        CallExternalFunc();
        CallExeFunc();
        {
            Hoge hoge;
            IHogeReceiver(&hoge);
        }
        {
            IHoge *hoge = CreateObjHoge();
            hoge->DoSomething();
            delete hoge;
        }

        ::Sleep(5000);
        DOL_ReloadAndLink();
    }

    DOL_UnloadAll();
    return 0;
}
