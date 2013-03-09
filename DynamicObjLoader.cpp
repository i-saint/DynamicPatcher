#include "DynamicObjLoader.h"
#ifndef DOL_StaticLink

#define _NO_CVCONST_H
#include <windows.h>
#include <dbghelp.h>
#include <process.h>
#include <stdint.h>
#include <cstdio>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#pragma comment(lib, "dbghelp.lib")
#pragma warning(disable: 4996) // _s じゃない CRT 関数使うとでるやつ


namespace dol {

namespace stl = std;
typedef unsigned __int64 QWORD, *PQWORD;

const char g_symname_modulemarker[] = DOL_Symbol_Prefix "DOL_ModuleMarker";
const char g_symname_onload[]       = DOL_Symbol_Prefix "DOL_OnLoadHandler";
const char g_symname_onunload[]     = DOL_Symbol_Prefix "DOL_OnUnloadHandler";


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
    const char *exefullpath = NULL;
    char _exefullpath[MAX_PATH];
    // argv から .exe のフルパスを取得
    // エントリポイントが main/WinMain の時は __argv が有効、wmain か wWinMain のときは __wargv が有効
    if(__argv!=NULL) {
        exefullpath = __argv[0];
    }
    else if(__wargv!=NULL) {
        wcstombs(_exefullpath, __wargv[0], MAX_PATH);
        exefullpath = _exefullpath;
    }

    // フルパスからファイル名を抽出
    size_t i = 0;
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

// 位置指定版 VirtualAlloc()
// location より後ろの最寄りの位置にメモリを確保する。
void* VirtualAllocLocated(size_t size, void *location)
{
    if(size==0) { return NULL; }
    static size_t base = (size_t)location;

    // ドキュメントには、アドレス指定の VirtualAlloc() は指定先が既に予約されている場合最寄りの領域を返す、
    // と書いてるように見えるが、実際には NULL が返ってくるようにしか見えない。
    // なので成功するまでアドレスを進めつつリトライ…。
    void *ret = NULL;
    const size_t step = 0x10000; // 64kb
    for(size_t i=0; ret==NULL; ++i) {
        ret = ::VirtualAlloc((void*)((size_t)base+(step*i)), size, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    return ret;
}

// exe がマップされている領域の後ろの最寄りの場所にメモリを確保する。
// jmp 命令などの移動量は x64 でも 32bit なため、32bit に収まらない距離を飛ぼうとした場合あらぬところに着地して死ぬ。
// そして new や malloc() だと 32bit に収まらない遥か彼方にメモリが確保されてしまうため、これが必要になる。
// .exe がマップされている領域を調べ、その近くに VirtualAlloc() するという内容。
void* VirtualAllocModule(size_t size)
{
    static void *exe_base = NULL;
    if(exe_base==NULL) {
        char exefilename[MAX_PATH];
        GetExeModuleName(exefilename);
        exe_base = (void*)GetModuleHandleA(exefilename);
    }
    return VirtualAllocLocated(size, exe_base);
}

typedef void *(AllocFunc)(size_t size);
bool MapFile(const stl::string &path, void *&o_data, size_t &o_size, AllocFunc alloc=VirtualAllocModule)
{
    o_data = NULL;
    o_size = 0;
    if(FILE *f=fopen(path.c_str(), "rb")) {
        fseek(f, 0, SEEK_END);
        o_size = ftell(f);
        if(o_size > 0) {
            o_data = alloc(o_size);
            fseek(f, 0, SEEK_SET);
            fread(o_data, 1, o_size, f);
        }
        fclose(f);
        return true;
    }
    return false;
}

// アラインが必要な section データを再配置するための単純なアロケータ
class SectionAllocator
{
public:
    // data=NULL, size_t size=0xffffffff で初期化した場合、必要な容量を調べるのに使える
    SectionAllocator(void *data=NULL, size_t size=0xffffffff) : m_data(data), m_size(size), m_used(0)
    {}

    // align: 2 の n 乗である必要がある
    void* allocate(size_t size, size_t align)
    {
        size_t base = (size_t)m_data;
        size_t mask = align - 1;
        size_t aligned = (base + m_used + mask) & ~mask;
        if(aligned+size <= base+m_size) {
            m_used = (aligned+size) - base;
            return m_data==NULL ? NULL : (void*)aligned;
        }
        return NULL;
    }

    size_t getUsed() const { return m_used; }

private:
    void *m_data;
    size_t m_size;
    size_t m_used;
};

QWORD GetFileModifiedTime(const stl::string &path)
{
    union RetT
    {
        FILETIME filetime;
        QWORD qword;
    } ret;
    HANDLE h = ::CreateFileA(path.c_str(), 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ::GetFileTime(h, NULL, NULL, &ret.filetime);
    ::CloseHandle(h);
    return ret.qword;
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

bool IsDirectory(const char *path)
{
    DWORD attr = ::GetFileAttributesA(path);
    return (attr & FILE_ATTRIBUTE_DIRECTORY)!=0;
}



enum BasicType
{
    btNoType = 0,
    btVoid = 1,
    btChar = 2,
    btWChar = 3,
    btInt = 6,
    btUInt = 7,
    btFloat = 8,
    btBCD = 9,
    btBool = 10,
    btLong = 13,
    btULong = 14,
    btCurrency = 25,
    btDate = 26,
    btVariant = 27,
    btComplex = 28,
    btBit = 29,
    btBSTR = 30,
    btHresult = 31
};
void BasetypeToString(DWORD t, ULONG64 size, stl::string &ret)
{
    ret.clear();
    switch(t) {
    case btVoid:  ret += "void"; break;
    case btChar:  ret += "char"; break;
    case btWChar: ret += "wchar_t"; break;
    case btInt:
    case btLong:
        switch(size) {
        case 1: ret+="char"; break;
        case 2: ret+="short"; break;
        case 4: ret+="int"; break;
        case 8: ret+="long long"; break;
        }
        break;
    case btUInt:
    case btULong:
        switch(size) {
        case 1: ret+="unsigned char"; break;
        case 2: ret+="unsigned short"; break;
        case 4: ret+="unsigned int"; break;
        case 8: ret+="unsigned long long"; break;
        }
        break;
    case btFloat:
        switch(size) {
        case 4: ret+="float"; break;
        case 8: ret+="double"; break;
        }
        break;
    }
}
void TypeToString(SYMBOL_INFO *si, DWORD t, stl::string &ret)
{
    DWORD tag;
    DWORD type;
    ::SymGetTypeInfo(::GetCurrentProcess(), si->ModBase, t, TI_GET_SYMTAG, &tag );

    if(tag==SymTagBaseType) {
        if( ::SymGetTypeInfo(::GetCurrentProcess(), si->ModBase, t, TI_GET_BASETYPE, &type ) ) {
            ULONG64 len;
            ::SymGetTypeInfo(::GetCurrentProcess(), si->ModBase, t, TI_GET_LENGTH, &len );
            BasetypeToString(type, len, ret);
        }
    }
    else if(tag==SymTagData) {
        if( ::SymGetTypeInfo(::GetCurrentProcess(), si->ModBase, t, TI_GET_TYPEID, &type ) ) {
            TypeToString(si, type, ret);
        }
    }
    else if(tag==SymTagPointerType) {
        if( ::SymGetTypeInfo(::GetCurrentProcess(), si->ModBase, t, TI_GET_TYPEID, &type ) ) {
            TypeToString(si, type, ret);
            ret += "*";
        }
    }
    else if(tag==SymTagUDT) {
        WCHAR *wname = NULL;
        ::SymGetTypeInfo(::GetCurrentProcess(), si->ModBase, t, TI_GET_SYMNAME, &wname );
        char name[1024];
        size_t num = 0;
        ::wcstombs_s(&num, name, wname, _countof(name));
        ::LocalFree(wname);
        ret += name;
    }
}



class DynamicObjLoader;

class ObjFile
{
public:
    ObjFile(DynamicObjLoader *loader)
        : m_data(NULL), m_size(0), m_aligned_data(NULL), m_aligned_datasize(0), m_loader(loader) {}
    ~ObjFile() { unload(); }

    bool load(const stl::string &path);
    bool load(const stl::string &name, void *data, size_t datasize, QWORD filetime);
    void unload();
    bool link();
    void* findSymbol(const stl::string &name);
    void addSymbol(const stl::string &name, void *sym) { m_symbols[name]=sym; }

    // f: functor [](const stl::string &symbol_name, const void *data)
    template<class F>
    void eachSymbol(const F &f)
    {
        for(SymbolTable::iterator i=m_symbols.begin(); i!=m_symbols.end(); ++i) {
            f(i->first, i->second);
        }
    }

    QWORD getFileTime() const { return m_time; }

private:
    typedef stl::map<stl::string, void*> SymbolTable;
    typedef stl::map<size_t, size_t> RelocBaseMap;
    void  *m_data;
    size_t m_size;
    void  *m_aligned_data;
    size_t m_aligned_datasize;
    QWORD m_time;
    stl::string m_name;
    SymbolTable m_symbols;
    RelocBaseMap m_reloc_bases;
    DynamicObjLoader *m_loader;
};


class DynamicObjLoader
{
public:
    DynamicObjLoader();
    ~DynamicObjLoader();

    bool needsReload(const stl::string &name, QWORD time);
    bool loadObj(const stl::string &path, bool force=false);
    void loadObj(const stl::string &name, ObjFile *obj);
    bool loadLib(const stl::string &path);
    void unload(ObjFile *obj);
    void unload(const stl::string &path);
    void unloadAll();
    bool link();
    void update();

    // 全ロード済み obj からシンボルを検索
    void* findSymbol(const stl::string &name);

    // exe 側 obj 側問わずシンボルを探す。link 処理用
    void* resolveExternalSymbol(ObjFile *from, const stl::string &name);

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
        ::VirtualFree(m_data, m_size, MEM_RELEASE);
        m_data = NULL;
        m_size = 0;
    }
    if(m_aligned_data!=NULL) {
        ::VirtualFree(m_aligned_data, m_aligned_datasize, MEM_RELEASE);
        m_aligned_data = NULL;
        m_aligned_datasize = 0;
    }
    m_name.clear();
    m_symbols.clear();
}

inline const char* GetSymbolName(PSTR StringTable, PIMAGE_SYMBOL Sym)
{
    return Sym->N.Name.Short!=0 ? (const char*)&Sym->N.ShortName : (const char*)(StringTable + Sym->N.Name.Long);
}

bool ObjFile::load(const stl::string &path)
{
    void *data;
    size_t size;
    if(!MapFile(path, data, size)) {
        return false;
    }
    QWORD time = GetFileModifiedTime(path);
    return load(path, data, size, time);
}

bool ObjFile::load(const stl::string &name, void *data, size_t size, QWORD time)
{
    m_name = name;
    m_data = data;
    m_size = size;
    m_time = time;

    size_t ImageBase = (size_t)(m_data);
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)ImageBase;
#ifdef _WIN64
    if( pDosHeader->e_magic!=IMAGE_FILE_MACHINE_AMD64 || pDosHeader->e_sp!=0 ) {
#else
    if( pDosHeader->e_magic!=IMAGE_FILE_MACHINE_I386 || pDosHeader->e_sp!=0 ) {
#endif
        istPrint("DOL fatal: %s 認識できないフォーマットです。/GL (プログラム全体の最適化) 有効でコンパイルされている可能性があります。\n"
            , m_name.c_str());
        ::DebugBreak();
        return false;
    }

    PIMAGE_FILE_HEADER pImageHeader = (PIMAGE_FILE_HEADER)ImageBase;
    PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)(ImageBase + sizeof(IMAGE_FILE_HEADER) + pImageHeader->SizeOfOptionalHeader);
    PIMAGE_SYMBOL pSymbolTable = (PIMAGE_SYMBOL)((size_t)pImageHeader + pImageHeader->PointerToSymbolTable);
    DWORD SymbolCount = pImageHeader->NumberOfSymbols;
    PSTR StringTable = (PSTR)&pSymbolTable[SymbolCount];

    // アラインが必要な section をアラインしつつ新しい領域に移す
    m_aligned_data = NULL;
    m_aligned_datasize = 0xffffffff;
    for(size_t ti=0; ti<2; ++ti) {
        // ti==0 で必要な容量を調べ、ti==1 で実際のメモリ確保と再配置を行う
        SectionAllocator salloc(m_aligned_data, m_aligned_datasize);

        for(size_t si=0; si<pImageHeader->NumberOfSections; ++si) {
            IMAGE_SECTION_HEADER &sect = pSectionHeader[si];
            // IMAGE_SECTION_HEADER::Characteristics にアライン情報が詰まっている
            DWORD align = 1 << (((sect.Characteristics & 0x00f00000) >> 20) - 1);
            if(align==1) {
                // do nothing
                continue;
            }
            else {
                if(void *rd = salloc.allocate(sect.SizeOfRawData, align)) {
                    if(sect.PointerToRawData != 0) {
                        memcpy(rd, (void*)(ImageBase + sect.PointerToRawData), sect.SizeOfRawData);
                    }
                    sect.PointerToRawData = (DWORD)((size_t)rd - ImageBase);
                }
            }
        }

        if(ti==0) {
            m_aligned_datasize = salloc.getUsed();
            m_aligned_data = VirtualAllocLocated(m_aligned_datasize, m_data);
        }
    }

    // symbol 収集処理
    for( size_t i=0; i < SymbolCount; ++i ) {
        PIMAGE_SYMBOL sym = pSymbolTable + i;
        //const char *name = GetSymbolName(StringTable, sym);
        if(sym->SectionNumber>0) {
            IMAGE_SECTION_HEADER &sect = pSectionHeader[sym->SectionNumber-1];
            void *data = (void*)(ImageBase + sect.PointerToRawData + sym->Value);
            if(sym->SectionNumber==IMAGE_SYM_UNDEFINED) { continue; }
            const char *name = GetSymbolName(StringTable, sym);
            m_symbols[name] = data;
        }
        i += pSymbolTable[i].NumberOfAuxSymbols;
    }

    return true;
}

// 外部シンボルのリンケージ解決
bool ObjFile::link()
{
    bool ret = true;
    stl::string mes;

    size_t ImageBase = (size_t)(m_data);
    PIMAGE_FILE_HEADER pImageHeader = (PIMAGE_FILE_HEADER)ImageBase;
    PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)(ImageBase + sizeof(IMAGE_FILE_HEADER) + pImageHeader->SizeOfOptionalHeader);
    PIMAGE_SYMBOL pSymbolTable = (PIMAGE_SYMBOL)((size_t)pImageHeader + pImageHeader->PointerToSymbolTable);
    DWORD SymbolCount = pImageHeader->NumberOfSymbols;
    PSTR StringTable = (PSTR)(pSymbolTable+SymbolCount);

    for(size_t si=0; si<pImageHeader->NumberOfSections; ++si) {
        IMAGE_SECTION_HEADER &sect = pSectionHeader[si];
        size_t SectionBase = (size_t)(ImageBase + sect.PointerToRawData);

        DWORD NumRelocations = sect.NumberOfRelocations;
        DWORD FirstRelocation = 0;
        // NumberOfRelocations==0xffff の場合、最初の IMAGE_RELOCATION に実際の値が入っている。(NumberOfRelocations は 16bit のため)
        if(sect.NumberOfRelocations==0xffff && (sect.Characteristics&IMAGE_SCN_LNK_NRELOC_OVFL)!=0) {
            NumRelocations = ((PIMAGE_RELOCATION)(ImageBase + sect.PointerToRelocations))[0].RelocCount;
            FirstRelocation = 1;
        }

        PIMAGE_RELOCATION pRelocation = (PIMAGE_RELOCATION)(ImageBase + sect.PointerToRelocations);
        for(size_t ri=FirstRelocation; ri<NumRelocations; ++ri) {
            PIMAGE_RELOCATION pReloc = pRelocation + ri;
            PIMAGE_SYMBOL rsym = pSymbolTable + pReloc->SymbolTableIndex;
            const char *rname = GetSymbolName(StringTable, rsym);
            size_t rdata = (size_t)m_loader->resolveExternalSymbol(this, rname);
            if(rdata==NULL) {
                char buf[1024];
                _snprintf(buf, _countof(buf), "DOL fatal: %s が参照するシンボル %s を解決できませんでした。\n", m_name.c_str(), rname);
                mes += buf;
                ret = false;
                continue;
            }

            enum {
#ifdef _WIN64
                IMAGE_SECTION   = IMAGE_REL_AMD64_SECTION,
                IMAGE_SECREL    = IMAGE_REL_AMD64_SECREL,
                IMAGE_REL32     = IMAGE_REL_AMD64_REL32,
                IMAGE_DIR32     = IMAGE_REL_AMD64_ADDR32,
                IMAGE_DIR32NB   = IMAGE_REL_AMD64_ADDR32NB,
                IMAGE_DIR64     = IMAGE_REL_AMD64_ADDR64,
#else
                IMAGE_SECTION   = IMAGE_REL_I386_SECTION,
                IMAGE_SECREL    = IMAGE_REL_I386_SECREL,
                IMAGE_REL32     = IMAGE_REL_I386_REL32,
                IMAGE_DIR32     = IMAGE_REL_I386_DIR32,
                IMAGE_DIR32NB   = IMAGE_REL_I386_DIR32NB,
#endif
            };
            size_t addr = SectionBase + pReloc->VirtualAddress;
            // 更新先に相対アドレスが入ってることがある。リンクだけ再度行われることがあるため、単純に加算するわけにはいかない。
            // 面倒だが std::map を使う。初参照のアドレスであればここで相対アドレスが記憶される。
            if(m_reloc_bases.find(addr)==m_reloc_bases.end()) {
                m_reloc_bases[addr] = *(DWORD*)(addr);
            }

            // IMAGE_RELOCATION::Type に応じて再配置
            switch(pReloc->Type) {
            case IMAGE_SECTION: break; // 
            case IMAGE_SECREL:  break; // デバッグ情報にしか出てこない (はず)
            case IMAGE_REL32:
                {
                    DWORD rel = (DWORD)(rdata - SectionBase - pReloc->VirtualAddress - 4);
                    *(DWORD*)(addr) = (DWORD)(m_reloc_bases[addr] + rel);
                }
                break;
            case IMAGE_DIR32:
                {
                    *(DWORD*)(addr) = (DWORD)(m_reloc_bases[addr] + rdata);
                }
                break;
            case IMAGE_DIR32NB:
                {
                    *(DWORD*)(addr) = (DWORD)rdata;
                }
                break;
#ifdef _WIN64
            case IMAGE_DIR64:
                {
                    *(QWORD*)(addr) = (QWORD)(m_reloc_bases[addr] + rdata);
                }
                break;
#endif // _WIN64
            default:
                istPrint("DOL warning: 未知の IMAGE_RELOCATION::Type 0x%x\n", pReloc->Type);
                break;
            }
        }
    }

    // 安全のため VirtualProtect() で write protect をかけたいところだが、
    // const ではない global 変数を含む可能性があるのでフルアクセス可能にしておく必要がある。
    //::VirtualProtect((LPVOID)m_data, m_datasize, PAGE_EXECUTE_READ, &old_protect_flag);
    if(!ret) {
        ::OutputDebugStringA(mes.c_str());
    }
    return ret;
}

void* ObjFile::findSymbol(const stl::string &name)
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

bool DynamicObjLoader::needsReload( const stl::string &name, QWORD time )
{
    // 同名ファイルが読み込み済み && 前回からファイルの更新時刻変わってない ならばリロード不要
    ObjTable::iterator i = m_objs.find(name);
    if(i!=m_objs.end()) {
        if(i->second->getFileTime()==time) {
            return false;
        }
    }
    return true;
}

bool DynamicObjLoader::loadObj( const stl::string &path, bool force )
{
    if(!needsReload(path, GetFileModifiedTime(path))) {
        return false;
    }

    ObjFile *obj = new ObjFile(this);
    if(!obj->load(path)) {
        delete obj;
        return false;
    }
    if(!force) {
        // DOL_Module がない obj は無視
        // .exe を構成する普通の .obj をロードすると、他の .obj がそちらを参照して問題を起こす可能性があるため
        if(obj->findSymbol(g_symname_modulemarker)==NULL) {
            unload(path);
            m_objs.erase(path);
            delete obj;
            return false;
        }
    }

    loadObj(path, obj);
    return true;
}

void DynamicObjLoader::loadObj( const stl::string &name, ObjFile *obj )
{
    {
        ObjTable::iterator i = m_objs.find(name);
        // 同名 obj が既にロード済みであれば unload を挟む
        if(i!=m_objs.end()) {
            unload(i->second);
            i->second = obj;
        }
        else {
            m_objs[name] = obj;
        }
    }

    // OnLoad を呼ぶリストに追加。リンク処理の後に呼ぶ
    m_handle_onload.push_back(obj);

    istPrint("DOL info: loaded %s\n", name.c_str());
}

bool DynamicObjLoader::loadLib( const stl::string &path )
{
    // .lib の構成は以下を参照
    // http://hp.vector.co.jp/authors/VA050396/tech_04.html

    void *lib_data;
    size_t lib_size;
    if(!MapFile(path, lib_data, lib_size, malloc)) {
        return false;
    }

    char *base = (char*)lib_data;
    if(strncmp(base, IMAGE_ARCHIVE_START, IMAGE_ARCHIVE_START_SIZE)!=0) {
        free(lib_data);
        return false;
    }
    base += IMAGE_ARCHIVE_START_SIZE;

    char *name_section = NULL;
    char *first_linker_member = NULL;
    char *second_linker_member = NULL;
    for(; base<(char*)lib_data+lib_size; ) {
        PIMAGE_ARCHIVE_MEMBER_HEADER header = (PIMAGE_ARCHIVE_MEMBER_HEADER)base;
        base += sizeof(IMAGE_ARCHIVE_MEMBER_HEADER);

        stl::string name;
        void *data;
        DWORD32 time, size;
        sscanf((char*)header->Date, "%d", &time);
        sscanf((char*)header->Size, "%d", &size);

        // Name の先頭 2 文字が "//" の場合 long name を保持する特殊セクション
        if(header->Name[0]=='/' && header->Name[1]=='/') {
            name_section = base;
        }
        // Name が '/' 1 文字だけの場合、リンク高速化のためのデータを保持する特殊セクション (最大 2 つある)
        else if(header->Name[0]=='/' && header->Name[1]==' ') {
            if     (first_linker_member==NULL)  { first_linker_member = base; }
            else if(second_linker_member==NULL) { second_linker_member = base; }
        }
        else {
            // Name が '/'+数字 の場合、その数字は long name セクションの offset 値
            if(header->Name[0]=='/') {
                DWORD offset;
                sscanf((char*)header->Name+1, "%d", &offset);
                name = name_section+offset;
            }
            // それ以外の場合 Name にはファイル名が入っている。null terminated ではないので注意が必要 ('/' で終わる)
            else {
                char *s = stl::find((char*)header->Name, (char*)header->Name+sizeof(header->Name), '/');
                name = stl::string((char*)header->Name, s);
            }

            if(!needsReload(name, time)) {
                goto GO_NEXT;
            }

            data = VirtualAllocModule(size);
            memcpy(data, base, size);
            ObjFile *obj = new ObjFile(this);
            if(!obj->load(name, data, size, time)) {
                delete obj;
            }
            else {
                loadObj(name, obj);
            }
        }

    GO_NEXT:
        base += size;
        base = (char*)((size_t)base+1 & ~1); // 2 byte align
    }

    free(lib_data);
    return false;
}


inline void CallOnLoadHandler(ObjFile *obj)   { if(Handler h=(Handler)obj->findSymbol(g_symname_onload))   { (h)(); } }
inline void CallOnUnloadHandler(ObjFile *obj) { if(Handler h=(Handler)obj->findSymbol(g_symname_onunload)) { (h)(); } }

void DynamicObjLoader::unload(ObjFile *obj)
{
    CallOnUnloadHandler(obj);
    obj->eachSymbol([&](const stl::string &name, void*){ m_symbols.erase(name); });
    delete obj;
}

void DynamicObjLoader::unload( const stl::string &path )
{
    ObjTable::iterator i = m_objs.find(path);
    if(i!=m_objs.end()) {
        unload(i->second);
        m_objs.erase(i);
    }
}

void DynamicObjLoader::unloadAll()
{
    for(ObjTable::iterator i=m_objs.begin(); i!=m_objs.end(); ++i) {
        unload(i->second);
    }
    m_objs.clear();
}

bool DynamicObjLoader::link()
{
    // わかりやすいようにエラーハンドリング
    if(m_objs.empty() && !m_links.empty()) {
        istPrint("DOL fatal: シンボルを解決できませんでした。.obj がロードされていません。DOL_Module がついた .cpp がないようです。\n");
        ::DebugBreak();
        return false;
    }

    // OnLoad が必要なやつがいない場合、新たにロードされたものはないはずなので何もしない
    if(m_handle_onload.empty()) {
        return true;
    }

    bool ret = true;
    // 全シンボルを 1 つの map に収集
    for(ObjTable::iterator i=m_objs.begin(); i!=m_objs.end(); ++i) {
        i->second->eachSymbol([&](const stl::string &name, void *data){ m_symbols.insert(stl::make_pair(name, data)); });
    }
    // ↑で集めた map を使うことで、obj 間シンボルもリンク
    for(ObjTable::iterator i=m_objs.begin(); i!=m_objs.end(); ++i) {
        if(!i->second->link()) {
            ret = false;
        }
    }
    // exe 側から参照されているシンボルを適切に張り替える
    for(SymbolLinkTable::iterator i=m_links.begin(); i!=m_links.end(); ++i) {
        void *sym = findSymbol(i->first);
        if(sym==NULL) {
            istPrint("DOL fatal: シンボル %s を解決できませんでした。\n", i->first.c_str());
            ret = false;
        }
        *i->second = sym;
    }

    if(ret==true) {
        istPrint("DOL info: link completed\n");
    }
    else {
        istPrint("DOL fatal: link error\n");
        ::DebugBreak();
    }

    // 新規ロードされているオブジェクトに OnLoad があれば呼ぶ
    for(size_t i=0; i<m_handle_onload.size(); ++i) {
        CallOnLoadHandler(m_handle_onload[i]);
    }
    m_handle_onload.clear();
    return true;
}

void DynamicObjLoader::update()
{
    size_t n = 0;
    for(ObjTable::iterator i=m_objs.begin(); i!=m_objs.end(); ++i) {
        if(loadObj(i->first, true)) { ++n; }
    }
    if(n > 0) { link(); }
}

void* DynamicObjLoader::findSymbol( const stl::string &name )
{
    SymbolTable::iterator i = m_symbols.find(name);
    if(i==m_symbols.end()) { return NULL; }
    return i->second;
}

void* DynamicObjLoader::resolveExternalSymbol( ObjFile *obj, const stl::string &name )
{
    void *sym = obj->findSymbol(name);
    if(sym==NULL) { sym=findSymbol(name); }
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
        , m_eval_id(0)
    {
        stl::string VCVersion;
#if     _MSC_VER==1500
        VCVersion = "9.0";
#elif   _MSC_VER==1600
        VCVersion = "10.0";
#elif   _MSC_VER==1700
        VCVersion = "11.0";
#else
#   error
#endif

        //HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\<version>\Setup\VS\<edition>
        stl::string keyName = "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VC7";
        char value[MAX_PATH];
        DWORD size = MAX_PATH;
        HKEY key;
        LONG retKey = ::RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, KEY_READ|KEY_WOW64_32KEY, &key);
        LONG retVal = ::RegQueryValueExA(key, VCVersion.c_str(), NULL, NULL, (LPBYTE)value, &size );
        if( retKey==ERROR_SUCCESS && retVal==ERROR_SUCCESS  ) {
            m_vcvars += '"';
            m_vcvars += value;
            m_vcvars += "vcvarsall.bat";
            m_vcvars += '"';
#ifdef _WIN64
            m_vcvars += " amd64";
#else // _WIN64
            m_vcvars += " x86";
#endif // _WIN64
            m_msbuild = m_vcvars;
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
        istPrint("DOL info: recompile thread started\n");
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
                    ::FindCloseChangeNotification(m_srcdirs[i].notifier);
                    m_srcdirs[i].notifier = ::FindFirstChangeNotificationA(m_srcdirs[i].path.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
                }
            }
            else {
                ::Sleep(1000);
            }
        }

        for(size_t i=0; i<m_srcdirs.size(); ++i) {
            ::FindCloseChangeNotification(m_srcdirs[i].notifier);
        }
    }

    bool execBuild()
    {
        istPrint("DOL info: recompile begin\n");
        stl::string command = m_msbuild;
        command+=' ';
        command+=m_msbuild_option;

        STARTUPINFOA si; 
        PROCESS_INFORMATION pi; 
        memset(&si, 0, sizeof(si)); 
        memset(&pi, 0, sizeof(pi)); 
        si.cb = sizeof(si);
        if(::CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)==TRUE) {
            DWORD exit_code = 0;
            ::WaitForSingleObject(pi.hThread, INFINITE);
            ::WaitForSingleObject(pi.hProcess, INFINITE);
            ::GetExitCodeProcess(pi.hProcess, &exit_code);
            ::CloseHandle(pi.hThread);
            ::CloseHandle(pi.hProcess);
            ::Sleep(500); // 終了直後だとファイルの書き込みが終わってないことがあるっぽい？ので少し待つ…
            m_build_has_just_completed = true;
            if(exit_code!=0) {
                istPrint("DOL error: build error.\n");
                return false;
            }
        }
        istPrint("DOL info: recompile end\n");
        return true;
    }

    void addSourceDirectory(const char *path)
    {
        SourceDir sd = {path, NULL};
        m_srcdirs.push_back(sd);
    }

    bool needsUpdate() const
    {
        if(m_build_has_just_completed) {
            m_build_has_just_completed = false;
            return true;
        }
        return false;
    }

    bool execCompile(const char *filename)
    {
        istPrint("DOL info: eval begin\n");
        stl::string command = m_vcvars;
        command+=" && cl ";
        command+=m_cl_option + " /c /O2 /GR- /nologo ";
        command+=filename;

        STARTUPINFOA si; 
        PROCESS_INFORMATION pi; 
        memset(&si, 0, sizeof(si)); 
        memset(&pi, 0, sizeof(pi)); 
        si.cb = sizeof(si);
        if(::CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)==TRUE) {
            DWORD exit_code = 0;
            ::WaitForSingleObject(pi.hThread, INFINITE);
            ::WaitForSingleObject(pi.hProcess, INFINITE);
            ::GetExitCodeProcess(pi.hProcess, &exit_code);
            ::CloseHandle(pi.hThread);
            ::CloseHandle(pi.hProcess);
            if(exit_code!=0) {
                istPrint("DOL error: compile error.\n");
                return false;
            }
        }
        else {
            istPrint("DOL fatal: CreateProcessA() failed.\n");
            return false;
        }
        istPrint("DOL info: eval end\n");
        return true;
    }

    struct EnumSymbolsCallbackContext {
        void *esp;
        stl::string *src_sym;
        stl::string *src_lk;
    };
    static BOOL CALLBACK EnumSymbolsCallback( SYMBOL_INFO* si, ULONG SymbolSize, PVOID UserContext ) 
    {
        EnumSymbolsCallbackContext *context = (EnumSymbolsCallbackContext*)UserContext;
        stl::string &src_sym = *context->src_sym;
        stl::string &src_lk = *context->src_lk;
        if( si != 0 ) {
#ifdef _WIN64
            ULONG64 Address = (ULONG64)context->esp + si->Address;
#else // _WIN64
            ULONG64 Address = (ULONG64)context->esp + si->Address - 4;
#endif // _WIN64

            stl::string type;
            stl::string name(si->Name, si->NameLen);
            TypeToString(si, si->TypeIndex, type);

            char buf[256];
            sprintf(buf, "0x%lx;\n", Address);
            src_sym += type + "* sym_" + name + "=(" + type + "*)" + buf;
            src_lk += type + "& " + name + "= *sym_"+name+";\n";
        }

        return TRUE; // Continue enumeration 
    }

    void setCompileOption(const char *opt)
    {
        m_cl_option = opt;
    }

    void setGlobalEvalContext(const char *context)
    {
        m_eval_global_context = context;
    }

    std::string compileEvalBlock(const char *file, int line, const char *function, void *esp, const char *source)
    {
        std::string ret;
        ::InterlockedIncrement(&m_eval_id);
        char filename[128];
        sprintf(filename, "evalsource%d.cpp", m_eval_id);
        {
            FILE *sourcefile = fopen(filename, "wb");
            {
                stl::string src = m_eval_global_context;
                src += stl::string("#include \"") + file + "\"\n";
                src += "\n";

                stl::string lk;
                EnumSymbolsCallbackContext c = {esp, &src, &lk};

                DWORD64 SymAddr = (DWORD64)FindSymbolInExe(function);
                IMAGEHLP_STACK_FRAME sf; 
                sf.InstructionOffset = SymAddr;
                ::SymSetContext(GetCurrentProcess(), &sf, 0 );
                ::SymEnumSymbols(GetCurrentProcess(), 0, 0, EnumSymbolsCallback, &c); 

                src += "\nextern \"C\" void evalblock()\n{\n";
                src += lk;
                src += source;
                src += "\n}\n";
                fwrite(src.c_str(), 1, src.size(), sourcefile);
            }
            fclose(sourcefile);
        }
        if(execCompile(filename)) {
            ret = filename;
            size_t pos = ret.find(".cpp");
            ret.erase(ret.begin()+pos, ret.end());
            ret += ".obj";
        }
        unlink(filename);
        return ret;
    }

private:
    stl::string m_vcvars;
    stl::string m_msbuild;
    stl::string m_msbuild_option;
    stl::vector<SourceDir> m_srcdirs;
    bool m_create_console;
    mutable bool m_build_has_just_completed;
    bool m_flag_exit;
    HANDLE m_thread_watchfile;

    volatile LONG m_eval_id;
    stl::string m_cl_option;
    stl::string m_eval_global_context;
};


DynamicObjLoader *g_objloader = NULL;
Builder *g_builder = NULL;

void _DOL_Initialize()
{
    if(!g_objloader) {
        InitializeDebugSymbol();
        g_objloader = new DynamicObjLoader();
        g_builder = new Builder();
    }
}

void _DOL_Finalize()
{
    delete g_builder;   g_builder=NULL;
    delete g_objloader; g_objloader=NULL;
};

struct _DOL_Finalizer
{
    _DOL_Finalizer() { _DOL_Initialize(); }
    ~_DOL_Finalizer() { _DOL_Finalize(); }
} g_dol_finalizer;

DynamicObjLoader*   _DOL_GetLoader()  { _DOL_Initialize(); return g_objloader; }
Builder*            _DOL_GetBuilder() { _DOL_Initialize(); return g_builder; }


} // namespace dol

using namespace dol;

void  DOL_Load(const char *path)
{
    DynamicObjLoader *loader = _DOL_GetLoader();
    if(IsDirectory(path)) {
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
                loader->loadObj(tmppath.c_str());
            } while(::FindNextFileA(handle, &wfdata));
            ::FindClose(handle);
        }
    }
    else {
        if(strstr(path, ".lib\0")!=NULL) {
            loader->loadLib(path);
        }
        else {
            loader->loadObj(path, true);
        }
    }
}

void DOL_Unload(const char *path)
{
    _DOL_GetLoader()->unload(path);
}

void DOL_UnloadAll()
{
    _DOL_GetLoader()->unloadAll();
}

void DOL_Link()
{
    _DOL_GetLoader()->link();
}

void DOL_Update()
{
    if(_DOL_GetBuilder()->needsUpdate()) {
        _DOL_GetLoader()->update();
    }
}

void DOL_LinkSymbol(const char *name, void *&target)
{
    _DOL_GetLoader()->addSymbolLink(name, target);
}


void DOL_StartAutoRecompile(const char *build_options, bool create_console_window)
{
    _DOL_GetBuilder()->start(build_options, create_console_window);
}

void DOL_AddSourceDirectory(const char *path)
{
    _DOL_GetBuilder()->addSourceDirectory(path);
};


void DOL_EvalSetCompileOption( const char *options )
{
    _DOL_GetBuilder()->setCompileOption(options);
}

void DOL_EvalSetGlobalContext(const char *source)
{
    _DOL_GetBuilder()->setGlobalEvalContext(source);
};

void* _DOL_GetEsp()
{
    return (void*)((size_t)_AddressOfReturnAddress()+sizeof(void*));
}

void _DOL_Eval(const char *file, int line, const char *function, void *esp, const char *source)
{
    std::string objfile = _DOL_GetBuilder()->compileEvalBlock(file, line, function, esp, source);
    if(objfile.empty()) {
        istPrint("DOL warning: DOL_Eval() failed.\n");
        return;
    }

    typedef void (*evalf)();
    ObjFile *obj = new ObjFile(_DOL_GetLoader());
    obj->load(objfile.c_str());
    unlink(objfile.c_str());
    obj->link();
    if(evalf f = (evalf)obj->findSymbol(DOL_Symbol_Prefix "evalblock")) {
        f();
    }
    delete obj;
}


#endif // DOL_StaticLink
