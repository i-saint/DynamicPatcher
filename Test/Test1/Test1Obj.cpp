#include "stdafx.h"
#include <cstdio>
#include <windows.h>
#include "DynamicObjLoader.h"
#include "Test1.h"

DOL_Export const int g_test = 100;

DOL_Export float FloatAdd(float a, float b)
{
    return a+b;
}

DOL_Export void CallExternalFunc()
{
    OutputDebugStringA("CallExternalFunc()\n");
}

void FuncInExe();
DOL_Export void CallExeFunc()
{
    return FuncInExe();
}


class DOL_Fixate ObjHoge : public IHoge
{
public:
    ObjHoge()
    {
        OutputDebugStringA("ObjHoge::ObjHoge()\n");
    }

    virtual ~ObjHoge()
    {
        OutputDebugStringA("ObjHoge::~ObjHoge()\n");
    }

    virtual void DoSomething()
    {
        OutputDebugStringA("ObjHoge::DoSomething()\n");
    }
};

DOL_Export void IHogeReceiver(IHoge *hoge)
{
    hoge->DoSomething();
}

DOL_Export IHoge* CreateObjHoge()
{
    return new ObjHoge();
}



DOL_OnLoad(
    OutputDebugStringA("DOL_OnLoad Test\n");
)

DOL_OnUnload(
    OutputDebugStringA("DOL_OnUnload Test\n");
)
