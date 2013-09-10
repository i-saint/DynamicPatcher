DynamicPatcher
================
Runtime C++ Editing library.  
compile .cpps, load and link .objs (or .libs, .dlls), and update functions at runtime.  

##demo movie  
[![DynamicPatcher demo](http://img.youtube.com/vi/rL1LZjrhJbw/0.jpg)](http://www.youtube.com/watch?v=rL1LZjrhJbw)


##detailed description
(japanese) http://www.slideshare.net/i-saint/runtime-cediting  
(japanese) http://i-saint.hatenablog.com/entry/2013/06/06/212515  

##example
```c++
#include <windows.h> // Sleep()
#include <cstdio>
#include "DynamicPatcher.h"

// dpPatch をつけておくとロード時に同名の関数を自動的に更新する。
// (dpPatch は単なる dllexport。.obj に情報が残るいい指定方法が他に見当たらなかったので仕方なく…。
//  この自動ロードは dpInitialize() のオプションで切ることも可能)
dpPatch void MaybeOverridden()
{
    // ここを書き換えるとリアルタイムに挙動が変わる
    puts("MaybeOverridden()\n");
}

// CRT の関数を差し替える例。今回の犠牲者は puts()
int puts_hook(const char *s)
{
    typedef int (*puts_t)(const char *s);
    puts_t orig_puts = (puts_t)dpGetUnpatched(&puts); // hook 前の関数を取得
    orig_puts("puts_hook()");
    return orig_puts(s);
}

// dpOnLoad() の中身はロード時に自動的に実行される。アンロード時に実行される dpOnUnload() も記述可能
dpOnLoad(
    // dpPatch による自動差し替えを使わない場合、on load 時とかに手動で差し替える必要がある。
    // 元関数と新しい関数の名前が違うケースでは手動指定するしかない。
    dpPatchAddressToAddress(&puts, &puts_hook); // puts() を puts_hook() に差し替える
)

int main()
{
    dpInitialize();
    dpAddSourcePath("."); // このディレクトリのファイルに変更があったらビルドコマンドを呼ぶ
    dpAddModulePath("example.obj"); // ビルドが終わったらこのファイルをロードする
    // cl.exe でコンパイル。msbuild や任意のコマンドも指定可能。
    // 実運用の際は msbuild を使うか、自動ビルドは使用せすユーザー制御になると思われる。
    // (dpStartAutoBuild() を呼ばなかった場合、dpUpdate() はロード済み or module path にあるモジュールに更新があればそれをリロードする)
    dpAddCLBuildCommand("example.cpp /c /Zi");
    dpStartAutoBuild(); // 自動ビルド開始

    for(;;) {
        MaybeOverridden();
        ::Sleep(2000);
        dpUpdate();
    }

    dpFinalize();
}

// cl /Zi example.cpp && ./example
```

##license
<img src="http://mirrors.creativecommons.org/presskit/buttons/88x31/png/by.png" alt="CC BY" width="200" height="70" />

##thanks
[Runtime-Compiled C++](http://runtimecompiledcplusplus.blogspot.com/)  
[Mhook](http://codefromthe70s.org/mhook23.aspx)  
[PE/COFF file format](http://www.skyfree.org/linux/references/coff.pdf)  
[lib file format](http://hp.vector.co.jp/authors/VA050396/tech_04.html)  
