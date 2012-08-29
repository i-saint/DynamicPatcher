#include <cstdio>
#include <windows.h>
#include "DynamicObjLoader.h"
#include "Test.h"



DOL_ObjExport float FloatAdd(float a, float b)
{
    return a+b;
}

DOL_ObjExport void CallExternalFunc()
{
    OutputDebugStringA("CallExternalFunc()\n");
}

void FuncInExe();
DOL_ObjExport void CallExeFunc()
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

DOL_ObjExport void IHogeReceiver(IHoge *hoge)
{
    hoge->DoSomething();
}

DOL_ObjExport IHoge* CreateObjHoge()
{
    return new ObjHoge();
}



DOL_OnLoad(
    OutputDebugStringA("DOL_OnLoad Test\n");
)

DOL_OnUnload(
    OutputDebugStringA("DOL_OnUnload Test\n");
)
