// .obj ファイルを実行時にロード＆リンクして実行可能にします。
// 具体的な使い方はテストを参照。
// DOL_StaticLink を define すると全て通常通り static link されるので、
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
// ・obj <-> exe 間で参照されるシンボルは、inline 展開や最適化で消えないように注意が必要
//      DOL_Fixate で対処可能。
// ・virtual 関数を使う場合、RTTI を無効にしておく必要がある
//      RTTI の有無で vtable の内容が変わってしまうため。
//      .obj だけ RTTI 無効でコンパイルされていればよく、.exe は RTTI 有効でビルドされていても問題ない。
// ・.obj から関数を引っ張ってくる際、mangling 後の関数名を指定する必要がある
//      とりあえず DOL_Export をつければ解決できる。(C linkage にする対応方法。これだと名前の頭に "_" をつけるだけで済む)
// ・.obj 側のコードはデバッガでは逆アセンブルモードでしか追えない
//      デバッグ情報はロードしていないため。これは解決困難で諦めモード。
// ・.obj 内の global オブジェクトのコンストラクタ/デストラクタは呼ばれない
//      デストラクタは atexit() に登録されるため、.obj リロードで終了時クラッシュを招く。なので意図的に対応していない。
//      DOL_OnLoad / DOL_OnUnload で代替する想定。

#ifndef __DynamicObjLoader_h__
#define __DynamicObjLoader_h__

#include <intrin.h>

//// 全部 static link するオプション。Master ビルド用。
//#define DOL_StaticLink

// 一応非 Windows でもビルドエラーにはならないようにしておく
#if !defined(_WIN32) && !defined(DOL_StaticLink)
#   define DOL_StaticLink
#endif


#ifndef DOL_StaticLink

#ifdef _WIN64
#   define DOL_Symbol_Prefix
#else // _WIN64
#   define DOL_Symbol_Prefix "_"
#endif // _WIN64

#if defined(_CPPRTTI) && !defined(DOL_DisableWarning_RTTI)
#   pragma message("DOL warning: RTTI が有効なため .obj 側の virtual 関数を正常に呼べません。この警告を無効にするには DOL_DisableWarning_RTTI を define します。\n")
#endif // _CPPRTTI


// obj 側で使います。親 process から参照されるシンボルにつけます。(mangling 問題解決のため)
#define DOL_Export  extern "C"

// obj, exe 両方で使います。
// obj から exe の関数などを参照する場合、それが inline 展開や最適化で消えてたりするとクラッシュします。
// 以下のマクロをつけておくと消えるのが抑止され、安全になります。
// また、obj で実装する class にもつけないと必要なシンボルが欠けることがあるようです。(詳細不詳。デストラクタ ("vector deleting destructor") で現象を確認)
#define DOL_Fixate  __declspec(dllexport)

// obj 側で使います。
// 動的にロードされる .obj だと明示します。DOL_Load() でディレクトリ指定した場合、これが定義されている .obj モジュールをディレクトリから探してロードします。
#define DOL_Module          DOL_Export __declspec(selectany) int DOL_ModuleMarker=0;

// obj 側で使います。引数には処理内容を書きます。このブロックはロード時に自動的に実行されます。
// DOL_OnUnload() と併せて、serialize/deserialize などに用います。
#define DOL_OnLoad(...)     DOL_Export static void DOL_OnLoadHandler()   { __VA_ARGS__ }; __declspec(selectany) void *_DOL_OnLoadHandler=DOL_OnLoadHandler;

// obj 側で使います。引数には処理内容を書きます。このブロックはアンロード時に自動的に実行されます。
#define DOL_OnUnload(...)   DOL_Export static void DOL_OnUnloadHandler() { __VA_ARGS__ }; __declspec(selectany) void *_DOL_OnUnloadHandler=DOL_OnUnloadHandler;

// exe 側で使います。obj から import する関数/メンバ関数/変数を宣言します。
// obj から import してくるものは exe 側では実際にはポインタなので、複数 cpp に定義があるとリンクエラーになってしまいます。
// 定義 (DOL_ImportFunction/Variable) は一箇所だけにして、他は宣言を見るだけにする必要があります。
// DOL_DeclareMemberFunction は class 宣言の中に書きます。
#define DOL_DeclareFunction(ret, name, arg) extern ret (*name)arg
#define DOL_DeclareVariable(type, name)     extern DOL_Variable<type> name
#define DOL_DeclareMemberFunction(Ret, Name, Args)\
    Ret Name##Impl Args;\
    Ret Name Args;

// obj 側で使います。メンバ関数を定義します。
#define DOL_DefineMemberFunction(Ret, Class, Name, Args)\
    DOL_Export Ret (Class::*g_##Class##_##Name)Args = &Class::Name##Impl;\
    Ret Class::Name##Impl Args

// exe 側で使います。.obj から import する関数/変数を定義します。
// 変数は実際には void* を cast operator をかまして返すオブジェクトなので若干注意が必要です。
// 例えば int の変数を printf で出力する場合、明示的に int に cast しないと意図した結果になりません。
#define DOL_ImportFunction(ret, name, arg)  ret (*name)arg=NULL; DOL_FunctionLink g_dol_link_##name##(name, DOL_Symbol_Prefix #name)
#define DOL_ImportVariable(type, name)      DOL_Variable<type> name; DOL_VariableLink g_dol_link_##name##(name, DOL_Symbol_Prefix #name)
#define DOL_ImportMemberFunction(Ret, Class, Name, Args, ArgNames)\
    DOL_ImportVariable(Ret (Class::*)Args, g_##Class##_##Name);\
    Ret Class::Name Args { return (this->*g_##Class##_##Name)ArgNames; }




// 以下 exe 側で使う API

// .obj をロードします。
// path はファイル (.obj) でもディレクトリでもよく、ディレクトリの場合 .obj を探してロードします (サブディレクトリ含む)。
// ディレクトリ指定の場合、DOL_Module が定義されている .obj しか読み込みませんが、ファイル指定の場合そうでなくても読み込みます。
// ロードが終わった後は DOL_Link() でリンクする必要があります。
void DOL_Load(const char *path);

// リンクを行います。 必要なものを全てロードした後に 1 度これを呼ぶ必要があります。
// .obj に DOL_OnLoad() のブロックがある場合、このタイミングで呼ばれます。
void DOL_Link();

// 更新された .obj があればそれをリロードし、再リンクします。
// .cpp の再コンパイルは別のスレッドで自動的に行われますが、.obj のリロードは勝手にやると問題が起きるので、
// これを呼んでユーザー側で適切なタイミングで行う必要があります。
// 再コンパイルが行われていなかった場合なにもしないので、毎フレーム呼んでも大丈夫です。
void DOL_Update();

// .obj をアンロードします。
// 対象 .obj に DOL_OnUnload() のブロックがある場合、このタイミングで呼ばれます。
void DOL_Unload(const char *path);
// 全 .obj をアンロードします。
void DOL_UnloadAll();

// 自動ビルド (.cpp の変更を検出したら自動的にビルド開始) を開始します。
void DOL_StartAutoRecompile(const char *build_options, bool create_console_window);
// 自動ビルド用の監視ディレクトリ。このディレクトリ以下のファイルの変更を検出したらビルドが始まります。
void DOL_AddSourceDirectory(const char *path);


void DOL_EvalSetCompileOption(const char *includes);
void DOL_EvalSetGlobalContext(const char *source);

__declspec(noinline) void* _DOL_GetEsp();
void _DOL_Eval(const char *file, int line, const char *function, void *esp, const char *source);
#ifdef _WIN64
#   define DOL_Eval(src) _DOL_Eval(__FILE__, __LINE__, __FUNCTION__, _DOL_GetEsp(), src)
#else // _WIN64
#   define DOL_Eval(src) _DOL_Eval(__FILE__, __LINE__, __FUNCTION__, _AddressOfReturnAddress(), src)
#endif // _WIN64


// 以下内部実装用

template<class T>
class DOL_Variable
{
public:
    DOL_Variable() : m_sym(NULL) {}
    DOL_Variable& operator=(const DOL_Variable &o) { (T&)(*this)=(T&)o; }
    void*& get() { return m_sym; }
    operator T&() const { return *reinterpret_cast<T*>(m_sym); }
    T* operator&() const { return reinterpret_cast<T*>(m_sym); }
private:
    mutable void *m_sym;
};

void DOL_LinkSymbol(const char *name, void *&target);

class DOL_FunctionLink
{
public:
    template<class FuncPtr>
    DOL_FunctionLink(FuncPtr &v, const char *name) { DOL_LinkSymbol(name, (void*&)v); }
};

class DOL_VariableLink
{
public:
    template<class T>
    DOL_VariableLink(DOL_Variable<T> &v, const char *name) { DOL_LinkSymbol(name, v.get()); }
};


#else // DOL_StaticLink


#define DOL_Export
#define DOL_Fixate
#define DOL_Module
#define DOL_OnLoad(...)
#define DOL_OnUnload(...)
#define DOL_DeclareFunction(ret, name, arg)         ret name arg
#define DOL_DeclareVariable(type, name)             extern type name
#define DOL_DeclareMemberFunction(Ret, Name, Args)  Ret Name Args
#define DOL_DefineMemberFunction(Ret, Class, Name, Args)    Ret Class::Name Args
#define DOL_ImportFunction(ret, name, arg)  ret name arg
#define DOL_ImportVariable(type, name)      extern type name
#define DOL_ImportMemberFunction(Ret, Class, Name, Args, ArgNames)

#define DOL_Load(path)
#define DOL_Update()
#define DOL_Unload(path)
#define DOL_UnloadAll()
#define DOL_Link()

#define DOL_StartAutoRecompile(...)
#define DOL_AddSourceDirectory(...)

#define DOL_EvalSetGlobalContext(...)
#define DOL_Eval(...)


#endif // DOL_StaticLink

#endif // __DynamicObjLoader_h__
