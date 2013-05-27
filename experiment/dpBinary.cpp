// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"


// F: [](size_t size) -> void* : alloc func
template<class F>
bool dpMapFile(const char *path, void *&o_data, size_t &o_size, const F &alloc)
{
    o_data = NULL;
    o_size = 0;
    if(FILE *f=fopen(path, "rb")) {
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

// 位置指定版 VirtualAlloc()
// location より後ろの最寄りの位置にメモリを確保する。
void* dpAllocate(size_t size, void *location)
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
void* dpAllocateModule(size_t size)
{
    return dpAllocate(size, GetModuleHandleA(nullptr));
}




void dpSymbolTable::addSymbol(const char *name, void *address)
{
    dpSymbol tmp = {name, address};
    m_symbols.push_back(tmp);
}

void dpSymbolTable::merge(const dpSymbolTable &v)
{
    std::for_each(v.m_symbols.begin(), v.m_symbols.end(), [&](const dpSymbol &sym){
        m_symbols.push_back(sym);
    });
    sort();
    m_symbols.erase(std::unique(m_symbols.begin(), m_symbols.end()), m_symbols.end());
}

void dpSymbolTable::sort()
{
    std::sort(m_symbols.begin(), m_symbols.end());
}

void dpSymbolTable::clear()
{
    m_symbols.clear();
}

size_t dpSymbolTable::getNumSymbols() const
{
    return m_symbols.size();
}

const dpSymbol* dpSymbolTable::getSymbol(size_t i) const
{
    return &m_symbols[i];
}

void* dpSymbolTable::findSymbol(const char *name) const
{
    dpSymbol tmp = {name, nullptr};
    auto p = std::lower_bound(m_symbols.begin(), m_symbols.end(), tmp);
    return p==m_symbols.end() ? nullptr : p->address;
}



dpObjFile::dpObjFile()
    : m_data(nullptr), m_size(0)
    , m_aligned_data(nullptr), m_aligned_datasize(0)
    , m_path(), m_mtime(0)
    , m_reloc_bases(), m_symbols()
{
}

dpObjFile::~dpObjFile()
{
    unload();
}

bool dpObjFile::loadFile(const char *path)
{
    unload();
    // todo
    return false;
}

bool dpObjFile::loadMemory(const char *name, void *data, size_t datasize, dpTime filetime)
{
    unload();
    // todo
    return false;
}

void dpObjFile::unload()
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
    m_path.clear();
    m_symbols.clear();
}

bool dpObjFile::link(dpLoader *linker)
{
    return false;
}

const dpSymbolTable& dpObjFile::getSymbolTable() const      { return m_symbols; }
const char*          dpObjFile::getPath() const             { return m_path.c_str(); }
dpTime               dpObjFile::getLastModifiedTime() const { return m_mtime; }
dpFileType           dpObjFile::getFileType() const         { return dpE_Obj; }
void*                dpObjFile::getBaseAddress() const      { return m_data; }



dpLibFile::dpLibFile()
    : m_mtime(0)
{
}

dpLibFile::~dpLibFile()
{
    unload();
}

bool dpLibFile::loadFile(const char *path)
{
    void *lib_data;
    size_t lib_size;
    if(!dpMapFile(path, lib_data, lib_size, malloc)) {
        return false;
    }
    bool ret = loadMemory(path, lib_data, lib_size, 0);
    free(lib_data);
    return ret;
}

bool dpLibFile::loadMemory(const char *name, void *lib_data, size_t lib_size, dpTime filetime)
{
    // .lib の構成は以下を参照
    // http://hp.vector.co.jp/authors/VA050396/tech_04.html

    char *base = (char*)lib_data;
    if(strncmp(base, IMAGE_ARCHIVE_START, IMAGE_ARCHIVE_START_SIZE)!=0) {
        return false;
    }
    base += IMAGE_ARCHIVE_START_SIZE;

    char *name_section = NULL;
    char *first_linker_member = NULL;
    char *second_linker_member = NULL;
    for(; base<(char*)lib_data+lib_size; ) {
        PIMAGE_ARCHIVE_MEMBER_HEADER header = (PIMAGE_ARCHIVE_MEMBER_HEADER)base;
        base += sizeof(IMAGE_ARCHIVE_MEMBER_HEADER);

        std::string name;
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
                char *s = std::find((char*)header->Name, (char*)header->Name+sizeof(header->Name), '/');
                name = std::string((char*)header->Name, s);
            }

            dpObjFile *obj = findObjFile(name.c_str());
            if(obj) {
                if(obj->getLastModifiedTime()<=time) {
                    goto GO_NEXT;
                }
                else {
                    if(obj->loadMemory(name.c_str(), data, size, time)) {
                        dpGetLoader()->addOnLoadList(obj);
                        m_symbols.merge(obj->getSymbolTable());
                    }
                }
            }
            else {
                data = dpAllocateModule(size);
                memcpy(data, base, size);
                dpObjFile *obj = new dpObjFile();
                if(obj->loadMemory(name.c_str(), data, size, time)) {
                    dpGetLoader()->addOnLoadList(obj);
                    m_symbols.merge(obj->getSymbolTable());
                    m_objs.push_back(obj);
                }
                else {
                    delete obj;
                }
            }
        }

GO_NEXT:
        base += size;
        base = (char*)((size_t)base+1 & ~1); // 2 byte align
    }

    return true;
}

void dpLibFile::unload()
{
    eachObjs([](dpObjFile *o){ delete o; });
    m_objs.clear();
    m_symbols.clear();
}

bool dpLibFile::link(dpLoader *linker)
{
    return false;
}

const dpSymbolTable& dpLibFile::getSymbolTable() const      { return m_symbols; }
const char*          dpLibFile::getPath() const             { return m_path.c_str(); }
dpTime               dpLibFile::getLastModifiedTime() const { return m_mtime; }
dpFileType           dpLibFile::getFileType() const         { return dpE_Lib; }
size_t               dpLibFile::getNumObjFiles() const      { return m_objs.size(); }
dpObjFile*           dpLibFile::getObjFile(size_t i)        { return m_objs[i]; }
dpObjFile* dpLibFile::findObjFile( const char *name )
{
    dpObjFile *ret = nullptr;
    eachObjs([&](dpObjFile *o){
        if(_stricmp(o->getPath(), name)==0) {
            ret = o;
        }
    });
    return ret;
}



dpDllFile::dpDllFile()
    : m_module(nullptr), m_needs_freelibrary(false)
    , m_mtime(0)
{
}

dpDllFile::~dpDllFile()
{
}

// F: functor(const char *funcname, void *funcptr)
template<class F>
inline void EnumerateDLLExports(HMODULE module, const F &f)
{
    if(module==NULL) { return; }

    size_t ImageBase = (size_t)module;
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)ImageBase;
    if(pDosHeader->e_magic!=IMAGE_DOS_SIGNATURE) { return; }

    PIMAGE_NT_HEADERS pNTHeader = (PIMAGE_NT_HEADERS)(ImageBase + pDosHeader->e_lfanew);
    DWORD RVAExports = pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if(RVAExports==0) { return; }

    IMAGE_EXPORT_DIRECTORY *pExportDirectory = (IMAGE_EXPORT_DIRECTORY *)(ImageBase + RVAExports);
    DWORD *RVANames = (DWORD*)(ImageBase+pExportDirectory->AddressOfNames);
    WORD *RVANameOrdinals = (WORD*)(ImageBase+pExportDirectory->AddressOfNameOrdinals);
    DWORD *RVAFunctions = (DWORD*)(ImageBase+pExportDirectory->AddressOfFunctions);
    for(DWORD i=0; i<pExportDirectory->NumberOfFunctions; ++i) {
        char *pName = (char*)(ImageBase+RVANames[i]);
        void *pFunc = (void*)(ImageBase+RVAFunctions[RVANameOrdinals[i]]);
        f(pName, pFunc);
    }
}

bool dpDllFile::loadFile(const char *path)
{
    unload();

    m_path = path;
    m_module = ::LoadLibraryA(path);
    if(m_module!=nullptr) {
        m_needs_freelibrary = true;
        EnumerateDLLExports(m_module, [&](const char *funcname, void *funcptr){
            m_symbols.addSymbol(funcname, funcptr);
        });
        m_symbols.sort();
        return true;
    }
    return false;
}

bool dpDllFile::loadMemory(const char *name, void *data, size_t datasize, dpTime filetime)
{
    unload();

    if(data==nullptr) { return false; }
    m_path = name;
    m_module = (HMODULE)data;
    EnumerateDLLExports(m_module, [&](const char *funcname, void *funcptr){
        m_symbols.addSymbol(funcname, funcptr);
    });
    m_symbols.sort();
    return true;
}

void dpDllFile::unload()
{
    if(m_module && m_needs_freelibrary) {
        ::FreeLibrary(m_module);
    }
    m_needs_freelibrary = false;
    m_symbols.clear();
}

bool dpDllFile::link(dpLoader *linker)
{
    return m_module!=nullptr;
}

const dpSymbolTable& dpDllFile::getSymbolTable() const      { return m_symbols; }
const char*          dpDllFile::getPath() const             { return m_path.c_str(); }
dpTime               dpDllFile::getLastModifiedTime() const { return m_mtime; }
dpFileType           dpDllFile::getFileType() const         { return dpE_Dll; }
