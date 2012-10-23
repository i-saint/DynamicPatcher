#include <cstdio>
#include <windows.h>
#include "DynamicObjLoader.h"


int main(int argc, char* argv[])
{
    AllocConsole();

    printf(""); // これがないと exe 内に printf() が含まれなくなるので eval の中から呼べなくなる
    volatile int hoge = 10;
    DOL_Eval("printf(\"%d\\n\", *(int*)hoge);", "#include <cstdio>\n"); // 現状 eval 内から参照されるホスト側変数は全部 void*

    return 0;
}
