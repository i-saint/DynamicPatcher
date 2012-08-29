#include "stdafx.h"
#include <windows.h>
#include "DynamicObjLoader.h"
#include "Test.h"
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


DOL_ObjFunc(float, FloatAdd, float, float);
DOL_ObjFunc(void, CallExternalFunc);
DOL_ObjFunc(void, CallExeFunc);
DOL_ObjFunc(void, IHogeReceiver, IHoge*);
DOL_ObjFunc(IHoge*, CreateObjHoge);

int main(int argc, _TCHAR* argv[])
{
    DOL_Load("TestObj.obj");
    DOL_Link();

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

    DOL_UnloadAll();
    return 0;
}
