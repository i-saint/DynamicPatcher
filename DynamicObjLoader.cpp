#include "DynamicObjLoader.h"
#ifndef DOL_Static_Link

#include <windows.h>
#include <imagehlp.h>
#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include <process.h>
#pragma comment(lib, "imagehlp.lib")
#pragma warning(disable: 4996) // _s じゃない CRT 関数使うとでるやつ

#pragma warning(disable: 4073) // init_seg(lib) は普通は使っちゃダメ的な warning
#pragma init_seg(lib) // global オブジェクトの初期化の優先順位上げる

namespace dol {

namespace stl = std;

const char g_symname_onload[] = DOL_Symbol_Prefix "DOL_OnLoadHandler";
const char g_symname_onunload[] = DOL_Symbol_Prefix "DOL_OnUnloadHandler";


#define istPrint(...) DebugPrint(__VA_ARGS__)

template<size_t N>
inline int istvsprintf(char (&buf)[N], const char *format, va_list vl)
{
    return _vsnprintf(buf, N, format, vl);
}

static const int DPRINTF_MES_LENGTH  = 4096;
void DebugPrintV(const char* fmt, va_list vl)
{
    char buf[DPRINTF_MES_LENGTH];
    istvsprintf(buf, fmt, vl);
    ::OutputDebugStringA(buf);
}

void DebugPrint(const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    DebugPrintV(fmt, vl);
    va_end(vl);
}

bool InitializeDebugSymbol(HANDLE proc=::GetCurrentProcess())
{
    if(!::SymInitialize(proc, NULL, TRUE)) {
        return false;
    }
    ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    return true;
}

void GetExeModuleName(char *o_name)
{
    size_t i = 0;
    const char *exefullpath = __argv[0];
    for(;;) {
        if(exefullpath[i]==0) { break; }
        if(exefullpath[i]=='\\') {
            exefullpath += i+1;
            i = 0;
            continue;
        }
        o_name[i] = exefullpath[i];
        ++i;
    }
    o_name[i] = 0;
}

bool MapFile(const stl::string &path, void *&o_data, size_t &o_size)
{
    // jmp 命令などの移動量は x64 でも 32bit なため、32bit に収まらない距離を飛ぼうとした場合あらぬところに着地して死ぬ。
    // new や malloc() だと 32bit に収まらない遥か彼方にメモリが確保されてしまうため、
    // .exe がマップされている領域を調べ、その近くに .obj をマップする領域を VirtualAlloc() してやる必要がある。
    char exefilename[MAX_PATH];
    GetExeModuleName(exefilename);
    size_t exe_base = (size_t)GetModuleHandleA(exefilename);

    o_data = NULL;
    o_size = 0;
    if(FILE *f=fopen(path.c_str(), "rb")) {
        fseek(f, 0, SEEK_END);
        o_size = ftell(f);
        if(o_size > 0) {
            // ドキュメントには、アドレス指定の VirtualAlloc() は指定先が既に予約されている場合最寄りの領域を返す、
            // と書いてるように見えるが、実際には NULL が返ってくるようにしか見えない。
            // なので成功するまでアドレスを進めつつリトライ…。
            const size_t step = 0x10000; // 64kb
            for(size_t i=0; o_data==NULL; ++i) {
                o_data = ::VirtualAlloc((void*)((size_t)exe_base+(step*i)), o_size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
            }
            fseek(f, 0, SEEK_SET);
            fread(o_data, 1, o_size, f);
        }
        fclose(f);
        return true;
    }
    return false;
}

FILETIME GetFileModifiedTime(const stl::string &path)
{
    FILETIME writetime = {0,0};
    HANDLE h = ::CreateFileA(path.c_str(), 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ::GetFileTime(h, NULL, NULL, &writetime);
    ::CloseHandle(h);
    return writetime;
}
inline bool operator==(const FILETIME &l, const FILETIME &r)
{
    return l.dwHighDateTime==r.dwHighDateTime && l.dwLowDateTime==r.dwLowDateTime;
}

void* FindSymbolInExe(const char *name)
{
    char buf[sizeof(SYMBOL_INFO)+MAX_PATH];
    PSYMBOL_INFO sinfo = (PSYMBOL_INFO)buf;
    sinfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    sinfo->MaxNameLen = MAX_PATH;
    if(SymFromName(::GetCurrentProcess(), name, sinfo)==FALSE) {
        return NULL;
    }
    return (void*)sinfo->Address;
}



class DynamicObjLoader;

class ObjFile
{
public:
    ObjFile(DynamicObjLoader *loader) : m_loader(loader) {}
    ~ObjFile() { unload(); }

    bool load(const stl::string &path);
    void unload();

    // 外部シンボルのリンケージ解決
    void link();

    void* findSymbol(const char *name);

    // f: functor [](const stl::string &symbol_name, const void *data)
    template<class F>
    void eachSymbol(const F &f)
    {
        for(SymbolTable::iterator i=m_symbols.begin(); i!=m_symbols.end(); ++i) {
            f(i->first, i->second);
        }
    }

    FILETIME getFileTime() const { return m_filetime; }

private:
    typedef stl::map<stl::string, void*> SymbolTable;
    void *m_data;
    size_t m_datasize;
    FILETIME m_filetime;
    stl::string m_filepath;
    SymbolTable m_symbols;
    DynamicObjLoader *m_loader;
};


class DynamicObjLoader
{
public:
    DynamicObjLoader();
    ~DynamicObjLoader();

    // .obj のロードを行う。
    // 既に読まれているファイルを指定した場合リロード処理を行う。
    bool load(const stl::string &path);
    void unload(const stl::string &path);
    void unloadAll();
    void reloadAndLink();

    // 依存関係の解決処理。ロード後実行前に必ず呼ぶ必要がある。
    // load の中で link までやってもいいが、.obj の数が増えるほど無駄が多くなる上、
    // 本当に未解決なシンボルを判別しづらくなるので手順を分割した。
    void link();

    // ロード済み obj 検索
    ObjFile* findObj(const stl::string &path);

    // 全ロード済み obj からシンボルを検索
    void* findSymbol(const stl::string &name);

    // exe 側 obj 側問わずシンボルを探す。link 処理用
    void* resolveExternalSymbol(const stl::string &name);

    // シンボル name が .obj リロードなどで更新された場合、target も自動的に更新する
    void addSymbolLink(const stl::string &name, void *&target);

    // f: functor [](const stl::string &symbol_name, const void *data)
    template<class F>
    void eachSymbol(const F &f)
    {
        for(SymbolTable::iterator i=m_symbols.begin(); i!=m_symbols.end(); ++i) {
            f(i->first, i->second);
        }
    }

private:
    typedef stl::vector<ObjFile*>           ObjVector;
    typedef stl::map<stl::string, ObjFile*> ObjTable;
    typedef stl::map<stl::string, void*>    SymbolTable;
    typedef stl::map<stl::string, void**>   SymbolLinkTable;

    ObjTable m_objs;
    SymbolTable m_symbols;
    SymbolLinkTable m_links;
    ObjVector m_handle_onload;
};



void ObjFile::unload()
{
    if(m_data!=NULL) {
        ::VirtualFree(m_data, 0, MEM_RELEASE);
        m_data = NULL;
        m_datasize = 0;
    }
    m_filepath.clear();
    m_symbols.clear();
}

inline const char* GetSymbolName(PSTR StringTable, PIMAGE_SYMBOL Sym)
{
    return Sym->N.Name.Short!=0 ? (const char*)&Sym->N.ShortName : (const char*)(StringTable + Sym->N.Name.Long);
}

bool ObjFile::load(const stl::string &path)
{
    m_filepath = path;
    if(!MapFile(path, m_data, m_datasize)) {
        return false;
    }
    m_filetime = GetFileModifiedTime(path);

    size_t ImageBase = (size_t)(m_data);
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)ImageBase;
#ifdef _WIN64
    if( pDosHeader->e_magic!=IMAGE_FILE_MACHINE_AMD64 || pDosHeader->e_sp!=0 ) {
#else
    if( pDosHeader->e_magic!=IMAGE_FILE_MACHINE_I386 || pDosHeader->e_sp!=0 ) {
#endif
        return false;
    }

    DWORD old_protect_flag;
    ::VirtualProtect((LPVOID)m_data, m_datasize, PAGE_READONLY, &old_protect_flag);

    // 以下 symbol 収集処理
    PIMAGE_FILE_HEADER pImageHeader = (PIMAGE_FILE_HEADER)ImageBase;
    PIMAGE_OPTIONAL_HEADER *pOptionalHeader = (PIMAGE_OPTIONAL_HEADER*)(pImageHeader+1);

    PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)(ImageBase + sizeof(IMAGE_FILE_HEADER) + pImageHeader->SizeOfOptionalHeader);
    PIMAGE_SYMBOL pSymbolTable = (PIMAGE_SYMBOL)((size_t)pImageHeader + pImageHeader->PointerToSymbolTable);
    DWORD SymbolCount = pImageHeader->NumberOfSymbols;

    PSTR StringTable = (PSTR)&pSymbolTable[SymbolCount];

    for( size_t i=0; i < SymbolCount; ++i ) {
        PIMAGE_SYMBOL sym = pSymbolTable + i;
        const char *name = GetSymbolName(StringTable, sym);
        if(sym->SectionNumber>0) {
            IMAGE_SECTION_HEADER &sect = pSectionHeader[sym->SectionNumber-1];
            void *data = (void*)(ImageBase + sect.PointerToRawData + sym->Value);
            if(sym->SectionNumber!=IMAGE_SYM_UNDEFINED) {
                m_symbols[name] = data;
            }
        }
        i += pSymbolTable[i].NumberOfAuxSymbols;
    }

    return true;
}

// 外部シンボルのリンケージ解決
void ObjFile::link()
{
    DWORD old_protect_flag;
    ::VirtualProtect((LPVOID)m_data, m_datasize, PAGE_READWRITE, &old_protect_flag);

    size_t ImageBase = (size_t)(m_data);
    PIMAGE_FILE_HEADER pImageHeader = (PIMAGE_FILE_HEADER)ImageBase;
    PIMAGE_OPTIONAL_HEADER *pOptionalHeader = (PIMAGE_OPTIONAL_HEADER*)(pImageHeader+1);

    PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)(ImageBase + sizeof(IMAGE_FILE_HEADER) + pImageHeader->SizeOfOptionalHeader);
    PIMAGE_SYMBOL pSymbolTable = (PIMAGE_SYMBOL)((size_t)pImageHeader + pImageHeader->PointerToSymbolTable);
    DWORD SymbolCount = pImageHeader->NumberOfSymbols;

    PSTR StringTable = (PSTR)(pSymbolTable+SymbolCount);

    for( size_t i=0; i < SymbolCount; ++i ) {
        PIMAGE_SYMBOL sym = pSymbolTable + i;
        const char *name = GetSymbolName(StringTable, sym);

        if(sym->SectionNumber>0) {
            IMAGE_SECTION_HEADER &sect = pSectionHeader[sym->SectionNumber-1];
            size_t SectionBase = (size_t)(ImageBase + sect.PointerToRawData + sym->Value);

            DWORD NumRelocations = sect.NumberOfRelocations;
            PIMAGE_RELOCATION pRelocation = (PIMAGE_RELOCATION)(ImageBase + sect.PointerToRelocations);
            for(size_t ri=0; ri<NumRelocations; ++ri) {
                PIMAGE_RELOCATION pReloc = pRelocation + ri;
                PIMAGE_SYMBOL rsym = pSymbolTable + pReloc->SymbolTableIndex;
                const char *rname = GetSymbolName(StringTable, rsym);
                size_t rdata = (size_t)m_loader->resolveExternalSymbol(rname);
                if(rdata==NULL) {
                    istPrint("DOL error: %s が参照するシンボル %s を解決できませんでした。\n", m_filepath.c_str(), rname);
                    DebugBreak();
                    continue;
                }
                // 相対アドレス指定の場合相対アドレスに変換
#ifdef _WIN64
                if( pReloc->Type==IMAGE_REL_AMD64_REL32 ) {
#else
                if( pReloc->Type==IMAGE_REL_I386_REL32 ) {
#endif
                    DWORD rel = (DWORD)(rdata - SectionBase - pReloc->VirtualAddress - 4);
                    *(DWORD*)(SectionBase + pReloc->VirtualAddress) = rel;
                }
                else {
                    *(size_t*)(SectionBase + pReloc->VirtualAddress) = rdata;
                }
            }
        }
        i += pSymbolTable[i].NumberOfAuxSymbols;
    }

    // 安全のため書き込み禁止にしておく
    ::VirtualProtect((LPVOID)m_data, m_datasize, PAGE_EXECUTE_READ, &old_protect_flag);
}

void* ObjFile::findSymbol(const char *name)
{
    SymbolTable::iterator i = m_symbols.find(name);
    if(i == m_symbols.end()) { return NULL; }
    return i->second;
}


typedef void (*Handler)();


DynamicObjLoader::DynamicObjLoader()
{
}

DynamicObjLoader::~DynamicObjLoader()
{
    unloadAll();
}

bool DynamicObjLoader::load( const stl::string &path )
{
    // 同名ファイルが読み込み済み && 前回からファイルの更新時刻変わってない ならばなにもしない
    {
        ObjTable::iterator i = m_objs.find(path);
        if(i!=m_objs.end()) {
            FILETIME t = GetFileModifiedTime(path);
            if(i->second->getFileTime()==t) {
                return false;
            }
        }
    }

    ObjFile *obj = new ObjFile(this);
    if(!obj->load(path)) {
        delete obj;
        return false;
    }

    // obj 内のシンボルがどこからも参照されておらず、OnLoad などのハンドラも持たないならば破棄する。
    // 該当する obj は exe を構成するものと予想され、それをロードして他の .obj から参照すると問題を起こす可能性がある。
    {
        if(obj->findSymbol(g_symname_onload)!=NULL)     { goto RELATION_CHECK_PASSED; }
        if(obj->findSymbol(g_symname_onunload)!=NULL)   { goto RELATION_CHECK_PASSED; }
        for(SymbolLinkTable::iterator i=m_links.begin(); i!=m_links.end(); ++i) {
            if(obj->findSymbol(i->first.c_str())!=NULL) { goto RELATION_CHECK_PASSED; }
        }
        {
            unload(path);
            m_objs.erase(path);
            delete obj;
            return false;
        }
RELATION_CHECK_PASSED:;
    }

    // 同名 obj が既にロード済みであれば、適切に unload 処理して新しいものに差し替える
    {
        ObjTable::iterator i = m_objs.find(path);
        if(i!=m_objs.end()) {
            i->second->eachSymbol([&](const stl::string &name, void *data){ m_symbols.erase(name); });
            delete i->second;
            i->second = obj;
        }
        else {
            m_objs[path] = obj;
        }
    }

    // OnLoad を呼ぶリストに追加。リンク処理の後に呼ぶ
    m_handle_onload.push_back(obj);

    return true;
}

inline void CallOnLoadHandler(ObjFile *obj)   { if(Handler h=(Handler)obj->findSymbol(g_symname_onload))   { h(); } }
inline void CallOnUnloadHandler(ObjFile *obj) { if(Handler h=(Handler)obj->findSymbol(g_symname_onunload)) { h(); } }

void DynamicObjLoader::link()
{
    // OnLoad が必要なやつがいない場合、新たにロードされたものはないはずなので何もしない
    if(m_handle_onload.empty()) { return; }

    // 全シンボルを 1 つの map に収集
    for(ObjTable::iterator i=m_objs.begin(); i!=m_objs.end(); ++i) {
        i->second->eachSymbol([&](const stl::string &name, void *data){ m_symbols.insert(stl::make_pair(name, data)); });
    }
    // ↑で集めた map を使うことで、obj 間シンボルもリンク
    for(ObjTable::iterator i=m_objs.begin(); i!=m_objs.end(); ++i) {
        i->second->link();
    }
    // exe 側から参照されているシンボルを適切に張り替える
    for(SymbolLinkTable::iterator i=m_links.begin(); i!=m_links.end(); ++i) {
        *i->second = findSymbol(i->first);
    }

    // 新規ロードされているオブジェクトに OnLoad があれば呼ぶ
    for(size_t i=0; i<m_handle_onload.size(); ++i) {
        CallOnLoadHandler(m_handle_onload[i]);
    }
    m_handle_onload.clear();
}

void DynamicObjLoader::unload( const stl::string &path )
{
    ObjTable::iterator i = m_objs.find(path);
    if(i!=m_objs.end()) {
        ObjFile *obj = i->second;
        CallOnUnloadHandler(obj);
        obj->eachSymbol([&](const stl::string &name, void *data){ m_symbols.erase(name); });
        delete obj;
        m_objs.erase(i);
    }
}

void DynamicObjLoader::unloadAll()
{
    while(!m_objs.empty()) {
        ObjTable::iterator i=m_objs.begin();
        unload(i->first);
    }
}

void DynamicObjLoader::reloadAndLink()
{
    for(ObjTable::iterator i=m_objs.begin(); i!=m_objs.end(); ++i) {
        load(i->first);
    }
    link();
}

ObjFile* DynamicObjLoader::findObj( const stl::string &path )
{
    ObjTable::iterator i = m_objs.find(path);
    if(i==m_objs.end()) { return NULL; }
    return i->second;
}

void* DynamicObjLoader::findSymbol( const stl::string &name )
{
    SymbolTable::iterator i = m_symbols.find(name);
    if(i==m_symbols.end()) { return NULL; }
    return i->second;
}

void* DynamicObjLoader::resolveExternalSymbol( const stl::string &name )
{
    void *sym = findSymbol(name);
    if(sym==NULL) { sym=FindSymbolInExe(name.c_str()); }
    return sym;
}

void DynamicObjLoader::addSymbolLink( const stl::string &name, void *&target )
{
    m_links.insert(stl::make_pair(name, &target));
}




class Builder
{
public:
    struct SourceDir
    {
        stl::string path;
        HANDLE notifier;
    };

public:
    Builder()
        : m_msbuild_option()
        , m_create_console(false)
        , m_build_has_just_completed(false)
        , m_flag_exit(false)
        , m_thread_watchfile(NULL)
    {
        stl::string VCVersion;
        {
            //switch around prefered compiler to the one we've used to compile this file
            const unsigned int MSCVERSION = _MSC_VER;
            switch( MSCVERSION )
            {
            case 1500: VCVersion="9.0";  break; //VS 2008
            case 1600: VCVersion="10.0"; break; //VS 2010
            case 1700: VCVersion="11.0"; break; //VS 2011
            default: ::DebugBreak(); //shouldn't happen.
            }
        }

        //HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\<version>\Setup\VS\<edition>
        stl::string keyName = "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VC7";
        char value[MAX_PATH];
        DWORD size = MAX_PATH;
        HKEY key;
        LONG retKey = ::RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, KEY_READ|KEY_WOW64_32KEY, &key);
        LONG retVal = ::RegQueryValueExA(key, VCVersion.c_str(), NULL, NULL, (LPBYTE)value, &size );
        if( retVal==ERROR_SUCCESS  ) {
            m_msbuild += '"';
            m_msbuild += value;
            m_msbuild += "vcvarsall.bat";
            m_msbuild += '"';
#ifdef _WIN64
            m_msbuild += " amd64";
#else // _WIN64
            m_msbuild += " x86";
#endif // _WIN64
            m_msbuild += " && ";
            m_msbuild += "msbuild";
        }
        ::RegCloseKey( key );
    }

    ~Builder()
    {
        m_flag_exit = true;
        if(m_thread_watchfile!=NULL) {
            ::WaitForSingleObject(m_thread_watchfile, INFINITE);
        }
        if(m_create_console) {
            ::FreeConsole();
        }
    }

    void start(const char *build_options, bool create_console)
    {
        m_msbuild_option = build_options;
        m_create_console = create_console;
        if(create_console) {
            ::AllocConsole();
        }
        m_thread_watchfile = (HANDLE)_beginthread( threadWatchFile, 0, this );
    }

    static void threadWatchFile( LPVOID arg )
    {
        ((Builder*)arg)->execWatchFile();
    }

    void execWatchFile()
    {
        // ファイルの変更を監視、変更が検出されたらビルド開始
        for(size_t i=0; i<m_srcdirs.size(); ++i) {
            m_srcdirs[i].notifier = ::FindFirstChangeNotificationA(m_srcdirs[i].path.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
        }

        while(!m_flag_exit) {
            bool needs_build = false;
            for(size_t i=0; i<m_srcdirs.size(); ++i) {
                if(::WaitForSingleObject(m_srcdirs[i].notifier, 10)==WAIT_OBJECT_0) {
                    needs_build = true;
                }
            }
            if(needs_build) {
                execBuild();
                for(size_t i=0; i<m_srcdirs.size(); ++i) {
                    // ビルドで大量のファイルに変更が加わっていることがあり、以後の FindFirstChangeNotificationA() はそれを検出してしまう。
                    // そうなると永遠にビルドし続けてしまうため、HANDLE ごと作りなおして一度リセットする。
                    FindCloseChangeNotification(m_srcdirs[i].notifier);
                    m_srcdirs[i].notifier = ::FindFirstChangeNotificationA(m_srcdirs[i].path.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
                }
            }
            else {
                ::Sleep(1000);
            }
        }

        for(size_t i=0; i<m_srcdirs.size(); ++i) {
            FindCloseChangeNotification(m_srcdirs[i].notifier);
        }
    }

    void execBuild()
    {
        stl::string command = m_msbuild;
        command+=' ';
        command+=m_msbuild_option;

        STARTUPINFOA si; 
        PROCESS_INFORMATION pi; 
        memset(&si, 0, sizeof(si)); 
        memset(&pi, 0, sizeof(pi)); 
        si.cb = sizeof(si);
        if(::CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)==TRUE) {
            ::WaitForSingleObject(pi.hProcess, INFINITE);
            m_build_has_just_completed = true;
        }
    }

    void addSourceDirectory(const char *path)
    {
        SourceDir sd = {path, NULL};
        m_srcdirs.push_back(sd);
    }

    bool needsReloadAndLink() const
    {
        if(m_build_has_just_completed) {
            m_build_has_just_completed = false;
            return true;
        }
        return false;
    }

private:
    stl::string m_msbuild;
    stl::string m_msbuild_option;
    stl::vector<SourceDir> m_srcdirs;
    bool m_create_console;
    mutable bool m_build_has_just_completed;
    bool m_flag_exit;
    HANDLE m_thread_watchfile;
};

class AutoLoader
{

};

DynamicObjLoader *g_objloader = NULL;
Builder *g_builder = NULL;

class DOL_Initializer
{
public:
    DOL_Initializer()
    {
        InitializeDebugSymbol();
        g_objloader = new DynamicObjLoader();
        g_builder = new Builder();
    }

    ~DOL_Initializer()
    {
        delete g_builder;  g_builder=NULL;
        delete g_objloader; g_objloader=NULL;
    }
} g_dol_init;

} // namespace dol

using namespace dol;

void  DOL_LoadObj(const char *path)
{
    g_objloader->load(path);
}

void DOL_Unload(const char *path)
{
    g_objloader->unload(path);
}

void DOL_UnloadAll()
{
    g_objloader->unloadAll();
}

void  DOL_Link()
{
    g_objloader->link();
}

void DOL_ReloadAndLink()
{
    if(g_builder->needsReloadAndLink()) {
        g_objloader->reloadAndLink();
    }
}

void DOL_LinkSymbol(const char *name, void *&target)
{
    g_objloader->addSymbolLink(name, target);
}


void DOL_StartAutoRecompile(const char *build_options, bool create_console_window)
{
    g_builder->start(build_options, create_console_window);
}

void DOL_AddSourceDirectory(const char *path)
{
    g_builder->addSourceDirectory(path);
};

void DOL_LoadObjDirectory(const char *path)
{
    stl::string tmppath;
    stl::string key = path;
    key += "\\*.obj";

    WIN32_FIND_DATAA wfdata;
    HANDLE handle = ::FindFirstFileA(key.c_str(), &wfdata);
    if(handle!=INVALID_HANDLE_VALUE) {
        do {
            tmppath = path;
            tmppath += "\\";
            tmppath += wfdata.cFileName;
            g_objloader->load(tmppath.c_str());
        } while(::FindNextFileA(handle, &wfdata));
        ::FindClose(handle);
    }
}


#endif // DOL_Static_Link
