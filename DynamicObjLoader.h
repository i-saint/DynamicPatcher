// .obj ファイルを実行時にロード＆リンクして実行可能にします。
// 具体的な使い方はテストを参照。
// DOL_Static_Link を define すると全て通常通り static link されるので、
// 開発用ビルド構成でのみ .obj ロードを使う、という使い方が可能です。
// 
// ロードされる .obj には以下のような制限があります。
// 
// ・/GL (プログラム全体の最適化あり) でコンパイルされた .obj は読めない
//      リンク時関数 inline 展開実現のためにフォーマットが変わるらしいため
// ・exe 本体のデバッグ情報 (.pdb) が必要
//      .obj から exe や dll の関数をリンクする際、文字列から関数のアドレスを取れないといけないため
// ・exe 本体がリンクしていない外部 dll の関数や .lib の関数は呼べない
//      超頑張って .lib を読めば対応できそうだが…
// ・.obj <-> exe 間で参照されるシンボルは、inline 展開や最適化で消えないように注意が必要
//      DOL_Fixate で対処可能。
// ・virtual 関数を使う場合、RTTI を無効にしておく必要がある
//      RTTI の有無で vtable の内容が変わってしまうため。
//      .obj だけ RTTI 無効でコンパイルされていればよく、.exe は RTTI 有効でビルドされていても問題ない。
// ・.obj から関数を引っ張ってくる際、mangling 後の関数名を指定する必要がある
//      とりあえず DOL_ObjExport をつければ解決できる。(C linkage にする対応方法。これだと名前の頭に "_" をつけるだけで済む)
// ・.obj 側のコードはデバッガでは逆アセンブルモードでしか追えない
//      デバッグ情報はロードしていないため。これは解決困難で諦めモード。
// ・.obj 内の global オブジェクトのコンストラクタ/デストラクタは呼ばれない
//      デストラクタは atexit() に登録されるため、.obj リロードで終了時クラッシュを招く。なので意図的に対応していない。
//      ロード/アンロード時に呼ばれる DOL_OnLoad / DOL_OnUnload で代替する想定。

#ifndef __DynamicObjLoader_h__
#define __DynamicObjLoader_h__


// 一応非 Windows でもビルドエラーにはならないようにしておく
#if !defined(_WIN32) && !defined(DOL_Static_Link)
#   define DOL_Static_Link
#endif

#ifndef DOL_Static_Link

// obj 側で使います。親 process から参照されるシンボルにつけます。mangling 問題を解決するためのもの
#define DOL_ObjExport   extern "C"

// obj 側で使います。引数には処理内容を書きます。このブロックはロード時に自動的に実行されます
#define DOL_OnLoad(...)     DOL_ObjExport void DOL_OnLoadHandler()   { __VA_ARGS__ }

// obj 側で使います。引数には処理内容を書きます。このブロックはアンロード時に自動的に実行されます
#define DOL_OnUnload(...)   DOL_ObjExport void DOL_OnUnloadHandler() { __VA_ARGS__ }

// obj, exe 両方で使います。
// 最適化で消えたり inline 展開されたりするのを防ぐためのもので、.obj から参照される exe の関数や変数につけると安全になります。
// また、.obj で実装する class にもつけないと必要なシンボルが欠けることがあるようです。(詳細不詳。デストラクタ ("vector deleting destructor") で現象を確認)
#define DOL_Fixate  __declspec(dllexport)

// exe 側で使います。.obj の関数の宣言
// .obj の関数は exe 側では実際には関数ポインタなので、複数 cpp に定義があるとリンクエラーになってしまいます。
// 定義 (DOL_ObjFunc) は一箇所だけにして、他は宣言を見るだけにする必要があります。
#define DOL_DeclareObjFunc(ret, name, ...)  extern ret (*name)(__VA_ARGS__)

// exe 側で使います。.obj の関数の定義
#define DOL_ObjFunc(ret, name, ...) ret (*name)(__VA_ARGS__)=NULL; DOL_FunctionLink g_rlcpp_link_##name##(name, "_" #name)

// exe 側で使います。.obj の変数の定義
#define DOL_ObjValue(type, name)    type *name=NULL


// 以下 exe 側で使う API

// .obj をロードします。ロードが終わった後は DOL_Link() でリンクする必要があります。
void DOL_Load(const char *path);

// リンクを行います。 必要なものを全てロードした後に 1 度これを呼ぶ必要があります。
// .obj に DOL_OnLoad() のブロックがある場合、このタイミングで呼ばれます。
void DOL_Link();
// .obj をアンロードします。
// 対象 .obj に DOL_OnUnload() のブロックがある場合、このタイミングで呼ばれます。
void DOL_Unload(const char *path);
// 全 .obj をアンロードします。
void DOL_UnloadAll();


void DOL_LinkSymbol(const char *name, void *&target);

class DOL_FunctionLink
{
public:
    template<class FuncPtr>
    DOL_FunctionLink(FuncPtr &v, const char *name)
    {
        DOL_LinkSymbol(name, (void*&)v);
    }
};


#else // DOL_Static_Link


#define DOL_ObjExport
#define DOL_OnLoad(...)
#define DOL_OnUnload(...)
#define DOL_Fixate
#define DOL_DeclareObjFunc(ret, name, ...)    ret name(__VA_ARGS__)
#define DOL_ObjFunc(ret, name, ...)           ret name(__VA_ARGS__)

#define DOL_Load(path)
#define DOL_Unload(path)
#define DOL_UnloadAll()
#define DOL_Link()


#endif // DOL_Static_Link

#endif // __DynamicObjLoader_h__
