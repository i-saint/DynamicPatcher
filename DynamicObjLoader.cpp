#include "DynamicObjLoader.h"
#ifndef DOL_Static_Link

#include <windows.h>
#include <imagehlp.h>
#include <cstdio>
#include <vector>
#include <string>
#include <map>
#pragma comment(lib, "imagehlp.lib")
#pragma warning(disable: 4996) // _s じゃない CRT 関数使うとでるやつ

#pragma warning(disable: 4073) // init_seg(lib) は普通は使っちゃダメ的な warning
#pragma init_seg(lib) // global オブジェクトの初期化の優先順位上げる

namespace dol {

namespace stl = std;


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
    // なので .obj と .exe のメモリ上の距離が 32bit に収まるようにする。
    // 具体的には .exe がマップされている領域を調べ、その近くに .obj のマップ領域を VirtualAlloc() する。
    // (ちなみに通常はリンカがリンク時に命令を修正して解決している様子)
    char exefilename[MAX_PATH];
    GetExeModuleName(exefilename);
    HMODULE exe = GetModuleHandleA(exefilename);

    o_data = NULL;
    o_size = 0;
    if(FILE *f=fopen(path.c_str(), "rb")) {
        fseek(f, 0, SEEK_END);
        o_size = ftell(f);
        if(o_size > 0) {
            // ドキュメントには、アドレス指定の VirtualAlloc() は指定先が既に予約されている場合最寄りの領域を返す、
            // と書いてるように見えるが、実際には NULL が返ってくるようにしか見えない。
            // なので成功するまでアドレスを進めつつリトライ…。
            for(size_t i=0; o_data==NULL; ++i) {
                o_data = ::VirtualAlloc((void*)((size_t)exe+(0x10000*i)), o_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            }
            fseek(f, 0, SEEK_SET);
            fread(o_data, 1, o_size, f);
        }
        fclose(f);
        return true;
    }
    return false;
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

private:
    typedef stl::map<stl::string, void*> SymbolTable;
    void *m_data;
    size_t m_datasize;
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
    void load(const stl::string &path);
    void unload(const stl::string &path);
    void unloadAll();

    // 依存関係の解決処理。ロード後実行前に必ず呼ぶ必要がある。
    // load の中で link までやってもいいが、.obj の数が増えるほど無駄が多くなる上、
    // 未解決シンボルを判別しづらくなるので手順を分割した。
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
    typedef stl::map<stl::string, ObjFile*> ObjTable;
    typedef stl::map<stl::string, void*>    SymbolTable;
    typedef stl::map<stl::string, void**>   SymbolLinkTable;

    ObjTable m_objs;
    SymbolTable m_symbols;
    SymbolLinkTable m_links;
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

    size_t ImageBase = (size_t)(m_data);
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)ImageBase;
#ifdef _WIN64
    if( pDosHeader->e_magic!=IMAGE_FILE_MACHINE_AMD64 || pDosHeader->e_sp!=0 ) {
#else
    if( pDosHeader->e_magic!=IMAGE_FILE_MACHINE_I386 || pDosHeader->e_sp!=0 ) {
#endif
        return false;
    }

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
    size_t ImageBase = (size_t)(m_data);
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

void DynamicObjLoader::load( const stl::string &path )
{
    ObjFile *&loader = m_objs[path];
    if(loader==NULL) {
        loader = new ObjFile(this);
    }
    loader->eachSymbol([&](const stl::string &name, void *data){ m_symbols.erase(name); });
    loader->unload();
    loader->load(path);
    loader->eachSymbol([&](const stl::string &name, void *data){ m_symbols.insert(stl::make_pair(name, data)); });
}

inline void CallOnLoadHandler(ObjFile *obj)   { if(Handler h=(Handler)obj->findSymbol("_DOL_OnLoadHandler")) { h(); } }
inline void CallOnUnloadHandler(ObjFile *obj) { if(Handler h=(Handler)obj->findSymbol("_DOL_OnUnloadHandler")) { h(); } }

void DynamicObjLoader::link()
{
    for(ObjTable::iterator i=m_objs.begin(); i!=m_objs.end(); ++i) {
        i->second->link();
    }
    for(SymbolLinkTable::iterator i=m_links.begin(); i!=m_links.end(); ++i) {
        *i->second = findSymbol(i->first);
    }
    for(ObjTable::iterator i=m_objs.begin(); i!=m_objs.end(); ++i) {
        CallOnLoadHandler(i->second);
    }
}

void DynamicObjLoader::unload( const stl::string &path )
{
    ObjTable::iterator i = m_objs.find(path);
    if(i!=m_objs.end()) {
        CallOnUnloadHandler(i->second);
        delete i->second;
        m_objs.erase(i);
    }
}

void DynamicObjLoader::unloadAll()
{
    for(ObjTable::iterator i=m_objs.begin(); i!=m_objs.end(); ++i) {
        CallOnUnloadHandler(i->second);
        delete i->second;
    }
    m_objs.clear();
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



DynamicObjLoader *m_objloader = NULL;

class DOL_Initializer
{
public:
    DOL_Initializer()
    {
        InitializeDebugSymbol();
        m_objloader = new DynamicObjLoader();
    }

    ~DOL_Initializer()
    {
        delete m_objloader;
        m_objloader = NULL;
    }
} g_dol_init;

} // namespace dol

using namespace dol;

void  DOL_Load(const char *path)
{
    m_objloader->load(path);
}

void DOL_Unload(const char *path)
{
    m_objloader->unload(path);
}

void DOL_UnloadAll()
{
    m_objloader->unloadAll();
}

void  DOL_Link()
{
    m_objloader->link();
}

void DOL_LinkSymbol(const char *name, void *&target)
{
    m_objloader->addSymbolLink(name, target);
}

#endif // DOL_Static_Link
