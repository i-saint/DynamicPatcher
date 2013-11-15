// dpVSHelper.h

#pragma once
#include <windows.h>

using namespace System;
using namespace System::Runtime::InteropServices;

public ref class dpVSHelper
{
public:
    static DWORD ExecuteSuspended(String^ path_to_exe, String^ work_dir, String^ environment);
    static bool Resume(DWORD pid);
};
