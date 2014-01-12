// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include "dpInternal.h"
#include "dpDllFile.h"

#ifdef dpWithDllFile

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

struct CV_INFO_PDB70
{
    DWORD  CvSignature;
    GUID Signature;
    DWORD Age;
    BYTE PdbFileName[1];
};

// fill_gap: .dll ファイルをそのままメモリに移した場合はこれを true にする必要があります。
// LoadLibrary() で正しくロードしたものは section の再配置が行われ、元ファイルとはデータの配置にズレが生じます。
// fill_gap==true の場合このズレを補正します。
CV_INFO_PDB70* dpGetPDBInfoFromModule(void *pModule, bool fill_gap)
{
    if(!pModule) { return nullptr; }

    PBYTE pData = (PUCHAR)pModule;
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)pData;
    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(pData + pDosHeader->e_lfanew);
    if(pDosHeader->e_magic==IMAGE_DOS_SIGNATURE && pNtHeaders->Signature==IMAGE_NT_SIGNATURE) {
        ULONG DebugRVA = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
        if(DebugRVA==0) { return nullptr; }

        PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
        for(size_t i=0; i<pNtHeaders->FileHeader.NumberOfSections; ++i) {
            PIMAGE_SECTION_HEADER s = pSectionHeader+i;
            if(DebugRVA >= s->VirtualAddress && DebugRVA < s->VirtualAddress+s->SizeOfRawData) {
                pSectionHeader = s;
                break;
            }
        }
        if(fill_gap) {
            DWORD gap = pSectionHeader->VirtualAddress - pSectionHeader->PointerToRawData;
            pData -= gap;
        }

        PIMAGE_DEBUG_DIRECTORY pDebug;
        pDebug = (PIMAGE_DEBUG_DIRECTORY)(pData + DebugRVA);
        if(DebugRVA!=0 && DebugRVA < pNtHeaders->OptionalHeader.SizeOfImage && pDebug->Type==IMAGE_DEBUG_TYPE_CODEVIEW) {
            CV_INFO_PDB70 *pCVI = (CV_INFO_PDB70*)(pData + pDebug->AddressOfRawData);
            if(pCVI->CvSignature=='SDSR') {
                return pCVI;
            }
        }
    }
    return nullptr;
}

struct PDBStream70
{
    DWORD impv;
    DWORD sig;
    DWORD age;
    GUID sig70;
};

// pdb ファイルから Age & GUID 情報を抽出します
PDBStream70* dpGetPDBSignature(void *mapped_pdb_file)
{
    // thanks to https://code.google.com/p/pdbparser/

#define ALIGN_UP(x, align)      ((x+align-1) & ~(align-1))
#define STREAM_SPAN_PAGES(size) (ALIGN_UP(size,pHeader->dwPageSize)/pHeader->dwPageSize)
#define PAGE(x)                 (pImageBase + pHeader->dwPageSize*(x))
#define PDB_STREAM_PDB    1

    struct MSF_Header
    {
        char szMagic[32];          // 0x00  Signature
        DWORD dwPageSize;          // 0x20  Number of bytes in the pages (i.e. 0x400)
        DWORD dwFpmPage;           // 0x24  FPM (free page map) page (i.e. 0x2)
        DWORD dwPageCount;         // 0x28  Page count (i.e. 0x1973)
        DWORD dwRootSize;          // 0x2c  Size of stream directory (in bytes; i.e. 0x6540)
        DWORD dwReserved;          // 0x30  Always zero.
        DWORD dwRootPointers[0x49];// 0x34  Array of pointers to root pointers stream. 
    };

    BYTE *pImageBase = (BYTE*)mapped_pdb_file;
    MSF_Header *pHeader = (MSF_Header*)pImageBase;

    DWORD RootPages = STREAM_SPAN_PAGES(pHeader->dwRootSize);
    DWORD RootPointersPages = STREAM_SPAN_PAGES(RootPages*sizeof(DWORD));

    std::string RootPointersRaw;
    RootPointersRaw.resize(RootPointersPages * pHeader->dwPageSize);
    for(DWORD i=0; i<RootPointersPages; i++) {
        PVOID Page = PAGE(pHeader->dwRootPointers[i]);
        SIZE_T Offset = pHeader->dwPageSize * i;
        memcpy(&RootPointersRaw[0]+Offset, Page, pHeader->dwPageSize);
    }
    DWORD *RootPointers = (DWORD*)&RootPointersRaw[0];

    std::string StreamInfoRaw;
    StreamInfoRaw.resize(RootPages * pHeader->dwPageSize);
    for(DWORD i=0; i<RootPages; i++) {
        PVOID Page = PAGE(RootPointers[i]);
        SIZE_T Offset = pHeader->dwPageSize * i;
        memcpy(&StreamInfoRaw[0]+Offset, Page, pHeader->dwPageSize);
    }
    DWORD StreamCount = *(DWORD*)&StreamInfoRaw[0];
    DWORD *dwStreamSizes = (DWORD*)&StreamInfoRaw[4];

    {
        DWORD *StreamPointers = &dwStreamSizes[StreamCount];
        DWORD page = 0;
        for(DWORD i=0; i<PDB_STREAM_PDB; i++) {
            DWORD nPages = STREAM_SPAN_PAGES(dwStreamSizes[i]);
            page += nPages;
        }
        DWORD *pdwStreamPointers = &StreamPointers[page];

        PVOID Page = PAGE(pdwStreamPointers[0]);
        return (PDBStream70*)Page;
    }

#undef PDB_STREAM_PDB
#undef PAGE
#undef STREAM_SPAN_PAGES
#undef ALIGN_UP
}

bool dpDllFile::loadFile(const char *path)
{
    dpTime mtime = dpGetMTime(path);
    if(m_module && m_path==path && mtime<=m_mtime) { return true; }

    // ロード中の dll と関連する pdb はロックがかかってしまい、以降のビルドが失敗するようになるため、その対策を行う。
    // .dll と .pdb を一時ファイルにコピーしてそれをロードする。
    // コピーの際、
    // ・.dll に含まれる .pdb へのパスをコピー版へのパスに書き換える
    // ・.dll と .pdb 両方に含まれる pdb の GUID を更新する
    //   (VisualC++2012 の場合、これを怠ると最初にロードした dll の pdb が以降の更新された dll でも使われ続けてしまう)

    std::string pdb_base;
    GUID uuid;
    {
        // dll をメモリに map
        void *data = nullptr;
        size_t datasize = 0;
        if(!dpMapFile(path, data, datasize, malloc)) {
            dpPrintError("file not found %s\n", path);
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

        // pdb へのパスと GUID を更新
        if(CV_INFO_PDB70 *cv=dpGetPDBInfoFromModule(data, true)) {
            char *pdb = (char*)cv->PdbFileName;
            pdb_base = pdb;
            strncpy(pdb+pdb_base.size()-3, rev, 3);
            m_pdb_path = pdb;
            cv->Signature.Data1 += ::clock();
            uuid = cv->Signature;
        }
        // dll を一時ファイルにコピー
        dpWriteFile(m_actual_file.c_str(), data, datasize);
        free(data);
    }

    {
        // pdb を GUID を更新してコピー
        void *data = nullptr;
        size_t datasize = 0;
        if(dpMapFile(pdb_base.c_str(), data, datasize, malloc)) {
            if(PDBStream70 *sig = dpGetPDBSignature(data)) {
                sig->sig70 = uuid;
            }
            dpWriteFile(m_pdb_path.c_str(), data, datasize);
            free(data);
        }
    }

    HMODULE module = ::LoadLibraryA(m_actual_file.c_str());
    if(loadMemory(path, module, 0, mtime)) {
        m_needs_freelibrary = true;
        return true;
    }
    else {
        dpPrintError("LoadLibraryA() failed. %s\n", path);
    }
    return false;
}

bool dpDllFile::loadMemory(const char *path, void *data, size_t /*datasize*/, dpTime mtime)
{
    if(data==nullptr) { return false; }
    if(m_module && m_path==path && mtime<=m_mtime) { return true; }

    m_path = path; dpSanitizePath(m_path);
    m_mtime = mtime;
    m_module = (HMODULE)data;
    dpEnumerateDLLExports(m_module, [&](const char *name, void *sym){
        m_symbols.addSymbol(dpGetLoader()->newSymbol(name, sym, dpE_Code|dpE_Read|dpE_Execute|dpE_Export, 0, this));
    });
    m_symbols.sort();
    return true;
}

bool dpDllFile::link() { return m_module!=nullptr; }
bool dpDllFile::partialLink(size_t section) { return m_module!=nullptr; }

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

#endif // dpWithDllFile
