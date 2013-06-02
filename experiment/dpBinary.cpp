// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"
#include <regex>


typedef unsigned long long QWORD;
const char g_symname_onload[]   = dpSymPrefix "dpOnLoadHandler";
const char g_symname_onunload[] = dpSymPrefix "dpOnUnloadHandler";



typedef void (*dpEventHandler)();
static inline void dpCallOnLoadHandler(dpBinary *v)
{
    if(const dpSymbol *sym = v->getSymbolTable().findSymbolByName(g_symname_onload)) {
        ((dpEventHandler)sym->address)();
    }
}
static inline void dpCallOnUnloadHandler(dpBinary *v)
{
    if(const dpSymbol *sym = v->getSymbolTable().findSymbolByName(g_symname_onunload)) {
        ((dpEventHandler)sym->address)();
    }
}




dpBinary::dpBinary(dpContext *ctx) : m_context(ctx)
{
}

dpBinary::~dpBinary()
{
}


dpObjFile::dpObjFile(dpContext *ctx)
    : dpBinary(ctx)
    , m_data(nullptr), m_size(0)
    , m_aligned_data(nullptr), m_aligned_datasize(0)
    , m_path(), m_mtime(0)
    , m_reloc_bases(), m_symbols()
{
}

dpObjFile::~dpObjFile()
{
    unload();
}

void dpObjFile::unload()
{
    dpGetPatcher()->unpatchByBinary(this);
    eachSymbols([&](dpSymbol *sym){ dpGetLoader()->deleteSymbol(sym); });
    if(m_data!=NULL) {
        dpDeallocate(m_data, m_size);
        m_data = NULL;
        m_size = 0;
    }
    if(m_aligned_data!=NULL) {
        dpDeallocate(m_aligned_data, m_aligned_datasize);
        m_aligned_data = NULL;
        m_aligned_datasize = 0;
    }
    m_path.clear();
    m_symbols.clear();
}


static inline const char* dpGetSymbolName(PSTR pStringTable, PIMAGE_SYMBOL pSym)
{
    return pSym->N.Name.Short!=0 ? (const char*)&pSym->N.ShortName : (const char*)(pStringTable + pSym->N.Name.Long);
}

bool dpObjFile::loadFile(const char *path)
{
    dpTime mtime = dpGetFileModifiedTime(path);
    if(m_symbols.getNumSymbols()>0 && mtime<=m_mtime) { return true; }

    void *data;
    size_t size;
    if(!dpMapFile(path, data, size, dpAllocateModule)) {
        dpPrint("dp error: file not found %s\n", path);
        return false;
    }
    return loadMemory(path, data, size, mtime);
}

bool dpObjFile::loadMemory(const char *path, void *data, size_t size, dpTime mtime)
{
    if(m_symbols.getNumSymbols()>0 && mtime<=m_mtime) { return true; }
    m_path = path;
    m_data = data;
    m_size = size;
    m_mtime = mtime;

    size_t ImageBase = (size_t)(m_data);
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)ImageBase;
#ifdef _WIN64
    if( pDosHeader->e_magic!=IMAGE_FILE_MACHINE_AMD64 || pDosHeader->e_sp!=0 ) {
#else
    if( pDosHeader->e_magic!=IMAGE_FILE_MACHINE_I386 || pDosHeader->e_sp!=0 ) {
#endif
        dpPrint("dp fatal: %s unknown file format. it might be compiled with /GL option.\n"
            , m_path.c_str());
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
        dpSectionAllocator salloc(m_aligned_data, m_aligned_datasize);

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
            m_aligned_data = dpAllocateForward(m_aligned_datasize, m_data);
        }
    }

    // symbol 収集処理
    for( size_t i=0; i < SymbolCount; ++i ) {
        PIMAGE_SYMBOL sym = pSymbolTable + i;
        //const char *name = GetSymbolName(StringTable, sym);
        if(sym->SectionNumber>0) {
            IMAGE_SECTION_HEADER &sect = pSectionHeader[sym->SectionNumber-1];
            void *data = (void*)(ImageBase + (int)sect.PointerToRawData + sym->Value);
            if(sym->SectionNumber==IMAGE_SYM_UNDEFINED) { continue; }
            const char *name = dpGetSymbolName(StringTable, sym);
            if(name[0]!='.') {
                DWORD flags = 0;
                if((sect.Characteristics&IMAGE_SCN_CNT_CODE))               { flags|=dpE_Code; }
                if((sect.Characteristics&IMAGE_SCN_CNT_INITIALIZED_DATA))   { flags|=dpE_IData; }
                if((sect.Characteristics&IMAGE_SCN_CNT_UNINITIALIZED_DATA)) { flags|=dpE_UData; }
                if((sect.Characteristics&IMAGE_SCN_MEM_READ))    { flags|=dpE_Read; }
                if((sect.Characteristics&IMAGE_SCN_MEM_WRITE))   { flags|=dpE_Write; }
                if((sect.Characteristics&IMAGE_SCN_MEM_EXECUTE)) { flags|=dpE_Execute; }
                if((sect.Characteristics&IMAGE_SCN_MEM_SHARED))  { flags|=dpE_Shared; }
                m_symbols.addSymbol(dpGetLoader()->newSymbol(name, data, flags, sym->SectionNumber, this));
            }
        }
        i += pSymbolTable[i].NumberOfAuxSymbols;
    }
    m_symbols.sort();

    for(size_t si=0; si<pImageHeader->NumberOfSections; ++si) {
        IMAGE_SECTION_HEADER &sect = pSectionHeader[si];
        // .drectve section には linker directive が入っており、dllexport 付き symbol のリストがここに含まれる
        if(strncmp((char*)sect.Name, ".drectve", 8)==0) {
            char *data = (char*)(ImageBase + sect.PointerToRawData);
            data[sect.SizeOfRawData] = '\0';
            std::regex reg("/EXPORT:([^ ,]+)");
            std::cmatch m;
            size_t pos = 0;
            for(;;) {
                if(std::regex_search(data+pos, m, reg)) {
                    char *name = data+pos+m.position(1);
                    name[m.length(1)] = '\0';
                    if(dpSymbol *s = m_symbols.findSymbolByName(name)) {
                        s->flags |= dpE_Export;
                    }
                    pos += m.position()+m.length()+1;
                    if(pos>=sect.SizeOfRawData) { break; }
                }
                else {
                    break;
                }
            }
        }
    }
    if(dpSymbol *s=getSymbolTable().findSymbolByName(g_symname_onload))   { s->flags |= dpE_Handler; }
    if(dpSymbol *s=getSymbolTable().findSymbolByName(g_symname_onunload)) { s->flags |= dpE_Handler; }

    dpGetLoader()->addOnLoadList(this);
    return true;
}

// 外部シンボルのリンケージ解決
bool dpObjFile::link()
{
    bool ret = true;
    std::string mes;

    size_t ImageBase = (size_t)(m_data);
    PIMAGE_FILE_HEADER pImageHeader = (PIMAGE_FILE_HEADER)ImageBase;
    PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)(ImageBase + sizeof(IMAGE_FILE_HEADER) + pImageHeader->SizeOfOptionalHeader);
    PIMAGE_SYMBOL pSymbolTable = (PIMAGE_SYMBOL)((size_t)pImageHeader + pImageHeader->PointerToSymbolTable);
    DWORD SymbolCount = pImageHeader->NumberOfSymbols;
    PSTR StringTable = (PSTR)(pSymbolTable+SymbolCount);

    for(size_t si=0; si<pImageHeader->NumberOfSections; ++si) {
        IMAGE_SECTION_HEADER &sect = pSectionHeader[si];
        size_t SectionBase = (size_t)(ImageBase + (int)sect.PointerToRawData);

        DWORD NumRelocations = sect.NumberOfRelocations;
        DWORD FirstRelocation = 0;
        // NumberOfRelocations==0xffff の場合、最初の IMAGE_RELOCATION に実際の値が入っている。(NumberOfRelocations は 16bit のため)
        if(sect.NumberOfRelocations==0xffff && (sect.Characteristics&IMAGE_SCN_LNK_NRELOC_OVFL)!=0) {
            NumRelocations = ((PIMAGE_RELOCATION)(ImageBase + (int)sect.PointerToRelocations))[0].RelocCount;
            FirstRelocation = 1;
        }

        PIMAGE_RELOCATION pRelocation = (PIMAGE_RELOCATION)(ImageBase + (int)sect.PointerToRelocations);
        for(size_t ri=FirstRelocation; ri<NumRelocations; ++ri) {
            PIMAGE_RELOCATION pReloc = pRelocation + ri;
            PIMAGE_SYMBOL rsym = pSymbolTable + pReloc->SymbolTableIndex;
            const char *rname = dpGetSymbolName(StringTable, rsym);
            size_t rdata = (size_t)resolveSymbol(rname);
            if(rdata==NULL) {
                char buf[1024];
                _snprintf(buf, _countof(buf), "dp fatal: symbol %s referenced by %s could not be resolved.\n", rname, m_path.c_str());
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
                dpPrint("dp warning: unknown IMAGE_RELOCATION::Type 0x%x\n", pReloc->Type);
                break;
            }
        }
    }

    // 安全のため VirtualProtect() で write protect をかけたいところだが、
    // const ではない global 変数を含む可能性があるのでフルアクセス可能にしておく必要がある。
    //::VirtualProtect((LPVOID)m_data, m_datasize, PAGE_EXECUTE_READ, &old_protect_flag);
    if(!ret) {
        dpPrint(mes.c_str());
    }
    return ret;
}

bool dpObjFile::callHandler( dpEventType e )
{
    switch(e) {
    case dpE_OnLoad:   dpCallOnLoadHandler(this);   return true;
    case dpE_OnUnload: dpCallOnUnloadHandler(this); return true;
    }
    return false;
}

dpSymbolTable& dpObjFile::getSymbolTable()            { return m_symbols; }
const char*    dpObjFile::getPath() const             { return m_path.c_str(); }
dpTime         dpObjFile::getLastModifiedTime() const { return m_mtime; }
dpFileType     dpObjFile::getFileType() const         { return FileType; }
void*          dpObjFile::getBaseAddress() const      { return m_data; }

void* dpObjFile::resolveSymbol( const char *name )
{
    void *sym = nullptr;
    {
        if(const dpSymbol *s=getSymbolTable().findSymbolByName(name)) {
            sym = s->address;
        }
    }
    if(!sym) {
        if(const dpSymbol *s=dpGetLoader()->findSymbolByName(name)) {
            sym = s->address;
        }
    }
    if(!sym) {
        if(const dpSymbol *s=dpGetLoader()->findHostSymbolByName(name)) {
            sym = s->address;
        }
    }
    return sym;
}



dpLibFile::dpLibFile(dpContext *ctx)
    : dpBinary(ctx)
    , m_mtime(0)
{
}

dpLibFile::~dpLibFile()
{
    unload();
}

void dpLibFile::unload()
{
    eachObjs([](dpObjFile *o){ delete o; });
    m_objs.clear();
    m_symbols.clear();
}

bool dpLibFile::loadFile(const char *path)
{
    dpTime mtime = dpGetFileModifiedTime(path);
    if(!m_objs.empty() && mtime<=m_mtime) { return true; }

    void *lib_data;
    size_t lib_size;
    if(!dpMapFile(path, lib_data, lib_size, malloc)) {
        dpPrint("dp error: file not found %s\n", path);
        return false;
    }
    bool ret = loadMemory(path, lib_data, lib_size, mtime);
    free(lib_data);
    return ret;
}

bool dpLibFile::loadMemory(const char *path, void *lib_data, size_t lib_size, dpTime mtime)
{
    if(!m_objs.empty() && mtime<=m_mtime) { return true; }
    m_path = path;
    m_mtime = mtime;

    // .lib の構成は以下を参照
    // http://hp.vector.co.jp/authors/VA050396/tech_04.html

    char *base = (char*)lib_data;
    if(strncmp(base, IMAGE_ARCHIVE_START, IMAGE_ARCHIVE_START_SIZE)!=0) {
        dpPrint("do error: unknown file format %s\n", path);
        return false;
    }
    base += IMAGE_ARCHIVE_START_SIZE;

    size_t num_loaded = 0;
    char *name_section = NULL;
    char *first_linker_member = NULL;
    char *second_linker_member = NULL;
    for(; base<(char*)lib_data+lib_size; ) {
        PIMAGE_ARCHIVE_MEMBER_HEADER header = (PIMAGE_ARCHIVE_MEMBER_HEADER)base;
        base += sizeof(IMAGE_ARCHIVE_MEMBER_HEADER);

        std::string name;
        void *data = nullptr;
        DWORD32 mtime, size;
        sscanf((char*)header->Date, "%d", &mtime);
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

            dpObjFile *old = findObjFile(name.c_str());
            if(old && mtime<=old->getLastModifiedTime()) {
                goto GO_NEXT;
            }
            else {
                data = dpAllocateModule(size);
                memcpy(data, base, size);
                dpObjFile *obj = new dpObjFile(m_context);
                if(obj->loadMemory(name.c_str(), data, size, mtime)) {
                    if(old) {
                        m_objs.erase(std::find(m_objs.begin(), m_objs.end(), old));
                        delete old;
                    }
                    m_objs.push_back(obj);
                    ++num_loaded;
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

    if(num_loaded) {
        m_symbols.clear();
        eachObjs([&](dpObjFile *o){
            m_symbols.merge(o->getSymbolTable());
        });
    }

    return true;
}

bool dpLibFile::link()
{
    bool ret = true;
    eachObjs([&](dpObjFile *o){ if(!o->link()){ ret=false; } });
    return ret;
}

bool dpLibFile::callHandler( dpEventType e )
{
    return false;
}

dpSymbolTable& dpLibFile::getSymbolTable()            { return m_symbols; }
const char*    dpLibFile::getPath() const             { return m_path.c_str(); }
dpTime         dpLibFile::getLastModifiedTime() const { return m_mtime; }
dpFileType     dpLibFile::getFileType() const         { return FileType; }
size_t         dpLibFile::getNumObjFiles() const      { return m_objs.size(); }
dpObjFile*     dpLibFile::getObjFile(size_t i)        { return m_objs[i]; }
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



dpDllFile::dpDllFile(dpContext *ctx)
    : dpBinary(ctx)
    , m_module(nullptr), m_needs_freelibrary(false)
    , m_mtime(0)
{
}

dpDllFile::~dpDllFile()
{
    unload();
}

void dpDllFile::unload()
{
    dpGetPatcher()->unpatchByBinary(this);
    eachSymbols([&](dpSymbol *sym){ dpGetLoader()->deleteSymbol(sym); });
    if(m_module && m_needs_freelibrary) {
        ::FreeLibrary(m_module);
        dpDeleteFile(m_actual_file.c_str()); m_actual_file.clear();
        dpDeleteFile(m_pdb_path.c_str()); m_pdb_path.clear();
    }
    m_needs_freelibrary = false;
    m_symbols.clear();
}

// F: functor(const char *name, void *sym)
template<class F>
inline void dpEnumerateDLLExports(HMODULE module, const F &f)
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
    dpTime mtime = dpGetFileModifiedTime(path);
    if(m_module && m_path==path && mtime<=m_mtime) { return true; }

    // ロード中の dll と関連する pdb はロックがかかってしまい、リビルドに失敗するようになるため、
    // 一時ファイルにコピーする処理をここで行う。
    // dll に含まれる .pdb のパスを書き換えてコピーしないといけない。

    // dll をメモリに map
    void *data = nullptr;
    size_t datasize = 0;
    if(!dpMapFile(path, data, datasize, malloc)) {
        dpPrint("dp error: file not found %s\n", path);
        return false;
    }

    // 一時ファイル名を算出
    char rev[8] = {0};
    for(int i=0; i<0xfff; ++i) {
        _snprintf(rev, _countof(rev), "%x", i);
        m_path = path;
        m_actual_file.clear();
        std::string ext;
        dpSeparateFileExt(path, &m_actual_file, &ext);
        m_actual_file+=rev;
        m_actual_file+=".";
        m_actual_file+=ext;
        if(!dpFileExists(m_actual_file.c_str())) { break; }
    }

    // pdb のパスを抽出。あればパスを書き換え、書き換え後のパスに元ファイルをコピー
    if(char *pdb=dpGetPDBPathFromModule(data, true)) {
        std::string before;
        before = pdb;
        strncpy(pdb+before.size()-3, rev, 3);
        m_pdb_path = pdb;
        dpCopyFile(before.c_str(), m_pdb_path.c_str());
    }
    // dll を一時ファイルにコピー
    dpWriteFile(m_actual_file.c_str(), data, datasize);
    free(data);

    HMODULE module = ::LoadLibraryA(m_actual_file.c_str());
    if(loadMemory(path, module, 0, mtime)) {
        m_needs_freelibrary = true;
        return true;
    }
    else {
        dpPrint("dp error: LoadLibraryA() failed. %s\n", path);
    }
    return false;
}

bool dpDllFile::loadMemory(const char *path, void *data, size_t /*datasize*/, dpTime mtime)
{
    if(data==nullptr) { return false; }
    if(m_module && m_path==path && mtime<=m_mtime) { return true; }

    m_path = path;
    m_mtime = mtime;
    m_module = (HMODULE)data;
    dpEnumerateDLLExports(m_module, [&](const char *name, void *sym){
        m_symbols.addSymbol(dpGetLoader()->newSymbol(name, sym, dpE_Code|dpE_Read|dpE_Execute|dpE_Export, 0, this));
    });
    m_symbols.sort();
    dpGetLoader()->addOnLoadList(this);
    return true;
}

bool dpDllFile::link()
{
    return m_module!=nullptr;
}

bool dpDllFile::callHandler( dpEventType e )
{
    switch(e) {
    case dpE_OnLoad:   dpCallOnLoadHandler(this);   return true;
    case dpE_OnUnload: dpCallOnUnloadHandler(this); return true;
    }
    return false;
}

dpSymbolTable& dpDllFile::getSymbolTable()            { return m_symbols; }
const char*    dpDllFile::getPath() const             { return m_path.c_str(); }
dpTime         dpDllFile::getLastModifiedTime() const { return m_mtime; }
dpFileType     dpDllFile::getFileType() const         { return FileType; }
