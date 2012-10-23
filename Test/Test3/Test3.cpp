#include <cstdio>
#include <windows.h>
#include "DynamicObjLoader.h"


int main(int argc, char* argv[])
{
    AllocConsole();

    printf(""); // これがないと exe 内に printf() が含まれなくなるので eval の中から呼べなくなる

    volatile int hoge = 10; // 最適化で消えるの防止の volatile 
    volatile double hage = 1.0;
    DOL_Eval("printf(\"%d %lf\\n\", *(int*)hoge, *(double*)hage);", "#include <cstdio>\n"); // 現状 eval 内から参照されるホスト側変数は全部 void*

    return 0;
}
