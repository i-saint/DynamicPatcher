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


//#define DOL_Static_Link

// 一応非 Windows でもビルドエラーにはならないようにしておく
#if !defined(_WIN32) && !defined(DOL_Static_Link)
#   define DOL_Static_Link
#endif

#ifndef DOL_Static_Link
#ifdef _WIN64
#   define DOL_Symbol_Prefix
#else // _WIN64
#   define DOL_Symbol_Prefix "_"
#endif // _WIN64


// obj 側で使います。親 process から参照されるシンボルにつけます。mangling 問題を解決するためのもの。
#define DOL_Export  extern "C"

// obj 側で使います。引数には処理内容を書きます。このブロックはロード時に自動的に実行されます。
// また、もうひとつ特殊な役割を持ちます。
// .exe から参照されているシンボルがなく、OnLoad/OnUnload もない .obj は安全のためロードされないようになっています。
// このため、.exe からは参照されないが他の .obj からは参照される .obj はこれをつけて省かれないようにする必要があります。(内容は空でも ok)
#define DOL_OnLoad(...)     DOL_Export void DOL_OnLoadHandler()   { __VA_ARGS__ }

// obj 側で使います。引数には処理内容を書きます。このブロックはアンロード時に自動的に実行されます。
#define DOL_OnUnload(...)   DOL_Export void DOL_OnUnloadHandler() { __VA_ARGS__ }

// obj, exe 両方で使います。
// 最適化で消えたり inline 展開されたりするのを防ぐためのもので、.obj から参照される exe の関数や変数につけると安全になります。
// また、.obj で実装する class にもつけないと必要なシンボルが欠けることがあるようです。(詳細不詳。デストラクタ ("vector deleting destructor") で現象を確認)
#define DOL_Fixate  __declspec(dllexport)

// exe 側で使います。.obj の関数の宣言
// .obj の関数は exe 側では実際には関数ポインタなので、複数 cpp に定義があるとリンクエラーになってしまいます。
// 定義 (DOL_ObjFunc) は一箇所だけにして、他は宣言を見るだけにする必要があります。
#define DOL_DeclareFunction(ret, name, arg) extern ret (*name)arg
#define DOL_DeclareVariable(type, name)     extern type *name

// exe 側で使います。.obj の関数の定義
#define DOL_ImportFunction(ret, name, arg) ret (*name)arg=NULL; DOL_FunctionLink g_dol_link_##name##(name, DOL_Symbol_Prefix #name)

// exe 側で使います。.obj の変数の定義
#define DOL_ImportVariable(type, name)    DOL_Variable<type> name; DOL_VariableLink g_dol_link_##name##(name, DOL_Symbol_Prefix #name)


// 以下 exe 側で使う API

// .obj をロードします。ロードが終わった後は DOL_Link() でリンクする必要があります。
void DOL_LoadObj(const char *path);

// 指定ディレクトリ以下の全 .obj をロードします。こちらも後に DOL_Link() が必要です。
void DOL_LoadObjDirectory(const char *path);

// リンクを行います。 必要なものを全てロードした後に 1 度これを呼ぶ必要があります。
// .obj に DOL_OnLoad() のブロックがある場合、このタイミングで呼ばれます。
void DOL_Link();

// 全 .obj をリロードし、再リンクします。
// .cpp の再コンパイルは別のスレッドで自動的に行われますが、.obj のリロードは勝手にやると問題が起きるので、
// これを呼んでユーザー側で適切なタイミングで行う必要があります。
// 再コンパイルが行われていなかった場合なにもしないので、毎フレーム呼んでも大丈夫です。
void DOL_ReloadAndLink();

// .obj をアンロードします。
// 対象 .obj に DOL_OnUnload() のブロックがある場合、このタイミングで呼ばれます。
void DOL_Unload(const char *path);
// 全 .obj をアンロードします。
void DOL_UnloadAll();

// 自動ビルド (.cpp の変更を検出したら自動的にビルド開始) を開始します。
void DOL_StartAutoRecompile(const char *build_options, bool create_console_window);
// 自動ビルド用の監視ディレクトリ。このディレクトリ以下の .cpp や .h の変更を検出したらビルドが始まります。
void DOL_AddSourceDirectory(const char *path);


// internal
void DOL_LinkSymbol(const char *name, void *&target);
class DOL_FunctionLink
{
public:
    template<class FuncPtr>
    DOL_FunctionLink(FuncPtr &v, const char *name) { DOL_LinkSymbol(name, (void*&)v); }
};

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

class DOL_VariableLink
{
public:
    template<class T>
    DOL_VariableLink(DOL_Variable<T> &v, const char *name) { DOL_LinkSymbol(name, v.get()); }
};


#else // DOL_Static_Link


#define DOL_Export
#define DOL_OnLoad(...)
#define DOL_OnUnload(...)
#define DOL_Fixate
#define DOL_DeclareFunction(ret, name, arg) ret name arg
#define DOL_DeclareVariable(type, name)     extern type name
#define DOL_ImportFunction(ret, name, arg)  ret name arg
#define DOL_ImportVariable(type, name)      extern type name

#define DOL_LoadObj(path)
#define DOL_ReloadAndLink()
#define DOL_Unload(path)
#define DOL_UnloadAll()
#define DOL_Link()

#define DOL_StartAutoRecompile(...)
#define DOL_AddSourceDirectory(...)
#define DOL_LoadObjDirectory(...)


#endif // DOL_Static_Link

#endif // __DynamicObjLoader_h__
