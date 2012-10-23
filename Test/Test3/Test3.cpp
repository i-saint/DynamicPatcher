#include <cstdio>
#include <windows.h>
#include "DynamicObjLoader.h"


int main(int argc, char* argv[])
{
    AllocConsole();

    printf(""); // これがないと exe 内に printf() が含まれなくなるので eval の中から呼べなくなる

    volatile int hoge = 10; // volatile: 最適化で消えるの防止
    volatile double hage = 1.0;

    DOL_EvalSetGlobalContext("#include <cstdio>\n");
    DOL_Eval("printf(\"%d %lf\\n\", hoge, hage);");
    DOL_Eval("hoge+=10; hage*=2.0;");
    DOL_Eval("printf(\"%d %lf\\n\", hoge, hage);");

    return 0;
}
