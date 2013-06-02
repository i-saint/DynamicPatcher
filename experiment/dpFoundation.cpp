// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"

template<size_t N>
inline int dpVSprintf(char (&buf)[N], const char *format, va_list vl)
{
    return _vsnprintf(buf, N, format, vl);
}

static const int DPRINTF_MES_LENGTH  = 4096;
void dpPrintV(const char* fmt, va_list vl)
{
    char buf[DPRINTF_MES_LENGTH];
    dpVSprintf(buf, fmt, vl);
    ::OutputDebugStringA(buf);
}

dpAPI void dpPrint(const char* fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    dpPrintV(fmt, vl);
    va_end(vl);
}


// 位置指定版 VirtualAlloc()
// location より大きいアドレスの最寄りの位置にメモリを確保する。
void* dpAllocateForward(size_t size, void *location)
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

// 位置指定版 VirtualAlloc()
// location より小さいアドレスの最寄りの位置にメモリを確保する。
void* dpAllocateBackward(size_t size, void *location)
{
    if(size==0) { return NULL; }
    static size_t base = (size_t)location;

    void *ret = NULL;
    const size_t step = 0x10000; // 64kb
    for(size_t i=0; ret==NULL; ++i) {
        ret = ::VirtualAlloc((void*)((size_t)base-(step*i)), size, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    return ret;
}

// exe がマップされている領域の後ろの最寄りの場所にメモリを確保する。
// jmp 命令などの移動量は x64 でも 32bit なため、32bit に収まらない距離を飛ぼうとした場合あらぬところに着地して死ぬ。
// そして new や malloc() だと 32bit に収まらない遥か彼方にメモリが確保されてしまうため、これが必要になる。
// .exe がマップされている領域を調べ、その近くに VirtualAlloc() するという内容。
void* dpAllocateModule(size_t size)
{
    return dpAllocateBackward(size, GetModuleHandleA(nullptr));
}

void dpDeallocate(void *location, size_t size)
{
    ::VirtualFree(location, size, MEM_RELEASE);
}

dpTime dpGetFileModifiedTime(const char *path)
{
    union RetT
    {
        FILETIME filetime;
        dpTime qword;
    } ret;
    HANDLE h = ::CreateFileA(path, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ::GetFileTime(h, NULL, NULL, &ret.filetime);
    ::CloseHandle(h);
    return ret.qword;
}

bool dpDemangle(const char *mangled, char *demangled, size_t buflen)
{
    return ::UnDecorateSymbolName(mangled, demangled, (DWORD)buflen, UNDNAME_COMPLETE)!=0;
}

// fill_gap: .dll ファイルをそのままメモリに移した場合はこれを true にする必要があります。
// LoadLibrary() で正しくロードしたものは section の再配置が行われ、元ファイルとはデータの配置にズレが生じます。
// fill_gap==true の場合このズレを補正します。
char* dpGetPDBPathFromModule(void *pModule, bool fill_gap)
{
    if(!pModule) { return nullptr; }

    struct CV_INFO_PDB70
    {
        DWORD  CvSignature;
        GUID Signature;
        DWORD Age;
        BYTE PdbFileName[1];
    };

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
                return (char*)pCVI->PdbFileName;
            }
        }
    }
    return nullptr;
}

bool dpCopyFile(const char *srcpath, const char *dstpath)
{
    return ::CopyFileA(srcpath, dstpath, FALSE)==TRUE;
}

bool dpWriteFile(const char *path, const void *data, size_t size)
{
    if(FILE *f=fopen(path, "wb")) {
        fwrite((const char*)data, 1, size, f);
        fclose(f);
        return true;
    }
    return false;
}

bool dpDeleteFile(const char *path)
{
    return ::DeleteFileA(path)==TRUE;
}

bool dpFileExists( const char *path )
{
    return ::GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES;
}

size_t dpSeparateDirFile(const char *path, std::string *dir, std::string *file)
{
    size_t f_len=0;
    size_t l = strlen(path);
    for(size_t i=0; i<l; ++i) {
        if(path[i]=='\\' || path[i]=='/') { f_len=i+1; }
    }
    if(dir)  { dir->insert(dir->end(), path, path+f_len); }
    if(file) { file->insert(file->end(), path+f_len, path+l); }
    return f_len;
}

size_t dpSeparateFileExt(const char *filename, std::string *file, std::string *ext)
{
    size_t dir_len=0;
    size_t l = strlen(filename);
    for(size_t i=0; i<l; ++i) {
        if(filename[i]=='.') { dir_len=i+1; }
    }
    if(file){ file->insert(file->end(), filename, filename+dir_len); }
    if(ext) { ext->insert(ext->end(), filename+dir_len, filename+l); }
    return dir_len;
}


dpSectionAllocator::dpSectionAllocator(void *data, size_t size)
    : m_data(data), m_size(size), m_used(0)
{}

void* dpSectionAllocator::allocate(size_t size, size_t align)
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

size_t dpSectionAllocator::getUsed() const { return m_used; }



class dpPatchAllocator::Page
{
public:
    struct Block {
        union {
            char data[block_size];
            Block *next;
        };
    };
    Page(void *base);
    ~Page();
    void* allocate();
    bool deallocate(void *v);
    bool isInsideMemory(void *p) const;
    bool isInsideJumpRange(void *p) const;

private:
    void *m_data;
    Block *m_freelist;
};

dpPatchAllocator::Page::Page(void *base)
    : m_data(nullptr), m_freelist(nullptr)
{
    m_data = dpAllocateBackward(page_size, base);
    m_freelist = (Block*)m_data;
    size_t n = page_size / block_size;
    for(size_t i=0; i<n-1; ++i) {
        m_freelist[i].next = m_freelist+i+1;
    }
    m_freelist[n-1].next = nullptr;
}

dpPatchAllocator::Page::~Page()
{
    dpDeallocate(m_data, page_size);
}

void* dpPatchAllocator::Page::allocate()
{
    void *ret = nullptr;
    if(m_freelist) {
        ret = m_freelist;
        m_freelist = m_freelist->next;
    }
    return ret;
}

bool dpPatchAllocator::Page::deallocate(void *v)
{
    if(v==nullptr) { return false; }
    bool ret = false;
    if(isInsideMemory(v)) {
        Block *b = (Block*)v;
        b->next = m_freelist;
        m_freelist = b;
        ret = true;
    }
    return ret;
}

bool dpPatchAllocator::Page::isInsideMemory(void *p) const
{
    size_t loc = (size_t)p;
    size_t base = (size_t)m_data;
    return loc>=base && loc<base+page_size;
}

bool dpPatchAllocator::Page::isInsideJumpRange( void *p ) const
{
    size_t loc = (size_t)p;
    size_t base = (size_t)m_data;
    size_t dist = base<loc ? loc-base : base-loc;
    return dist < 0x7fff0000;
}


dpPatchAllocator::dpPatchAllocator()
{
}

dpPatchAllocator::~dpPatchAllocator()
{
    dpEach(m_pages, [](Page *p){ delete p; });
    m_pages.clear();
}

void* dpPatchAllocator::allocate(void *location)
{
    void *ret = nullptr;
    if(Page *page=findCandidatePage(location)) {
        ret = page->allocate();
    }
    if(!ret) {
        Page *page = createPage(location);
        ret = page->allocate();
    }
    return ret;
}

bool dpPatchAllocator::deallocate(void *v)
{
    if(Page *page=findOwnerPage(v)) {
        return page->deallocate(v);
    }
    return false;
}

dpPatchAllocator::Page* dpPatchAllocator::createPage(void *location)
{
    Page *p = new Page(location);
    m_pages.push_back(p);
    return p;
}

dpPatchAllocator::Page* dpPatchAllocator::findOwnerPage(void *location)
{
    auto p = dpFind(m_pages, [=](const Page *p){ return p->isInsideMemory(location); });
    return p==m_pages.end() ? nullptr : *p;
}

dpPatchAllocator::Page* dpPatchAllocator::findCandidatePage(void *location)
{
    auto p = dpFind(m_pages, [=](const Page *p){ return p->isInsideJumpRange(location); });
    return p==m_pages.end() ? nullptr : *p;
}




class dpSymbolAllocator::Page
{
public:
    struct Block {
        union {
            char data[block_size];
            Block *next;
        };
    };
    Page();
    ~Page();
    void* allocate();
    bool deallocate(void *v);
    bool isInsideMemory(void *p) const;
    bool isInsideJumpRange(void *p) const;

private:
    void *m_data;
    Block *m_freelist;
};

dpSymbolAllocator::Page::Page()
    : m_data(nullptr), m_freelist(nullptr)
{
    m_data = malloc(page_size);
    m_freelist = (Block*)m_data;
    size_t n = page_size / block_size;
    for(size_t i=0; i<n-1; ++i) {
        m_freelist[i].next = m_freelist+i+1;
    }
    m_freelist[n-1].next = nullptr;
}

dpSymbolAllocator::Page::~Page()
{
    free(m_data);
}

void* dpSymbolAllocator::Page::allocate()
{
    void *ret = nullptr;
    if(m_freelist) {
        ret = m_freelist;
        m_freelist = m_freelist->next;
    }
    return ret;
}

bool dpSymbolAllocator::Page::deallocate(void *v)
{
    if(v==nullptr) { return false; }
    bool ret = false;
    if(isInsideMemory(v)) {
        Block *b = (Block*)v;
        b->next = m_freelist;
        m_freelist = b;
        ret = true;
    }
    return ret;
}

bool dpSymbolAllocator::Page::isInsideMemory(void *p) const
{
    size_t loc = (size_t)p;
    size_t base = (size_t)m_data;
    return loc>=base && loc<base+page_size;
}


dpSymbolAllocator::dpSymbolAllocator()
{
}

dpSymbolAllocator::~dpSymbolAllocator()
{
    dpEach(m_pages, [](Page *p){ delete p; });
    m_pages.clear();
}

void* dpSymbolAllocator::allocate()
{
    for(size_t i=0; i<m_pages.size(); ++i) {
        if(void *ret=m_pages[i]->allocate()) {
            return ret;
        }
    }

    Page *p = new Page();
    m_pages.push_back(p);
    return p->allocate();
}

bool dpSymbolAllocator::deallocate(void *v)
{
    auto p = dpFind(m_pages, [=](const Page *p){ return p->isInsideMemory(v); });
    if(p!=m_pages.end()) {
        (*p)->deallocate(v);
        return true;
    }
    return false;
}



void dpSymbolTable::addSymbol(dpSymbol *v)
{
    m_symbols.push_back(v);
}

void dpSymbolTable::merge(const dpSymbolTable &v)
{
    dpEach(v.m_symbols, [&](const dpSymbol *sym){
        m_symbols.push_back( const_cast<dpSymbol*>(sym) );
    });
    sort();
}

void dpSymbolTable::sort()
{
    std::sort(m_symbols.begin(), m_symbols.end(), dpLTPtr<dpSymbol>());
    m_symbols.erase(
        std::unique(m_symbols.begin(), m_symbols.end(), dpEQPtr<dpSymbol>()),
        m_symbols.end());
}

void dpSymbolTable::clear()
{
    m_symbols.clear();
}

size_t dpSymbolTable::getNumSymbols() const
{
    return m_symbols.size();
}

dpSymbol* dpSymbolTable::getSymbol(size_t i)
{
    return m_symbols[i];
}

dpSymbol* dpSymbolTable::findSymbolByName(const char *name)
{
    dpSymbol tmp(name, nullptr, 0, 0, nullptr);
    auto p = std::lower_bound(m_symbols.begin(), m_symbols.end(), &tmp, dpLTPtr<dpSymbol>());
    if(p!=m_symbols.end() && **p==tmp) {
        return *p;
    }
    return nullptr;
}

dpSymbol* dpSymbolTable::findSymbolByAddress( void *addr )
{
    auto p = dpFind(m_symbols, [=](const dpSymbol *sym){ return sym->address==addr; });
    return p==m_symbols.end() ? nullptr : *p;
}

const dpSymbol* dpSymbolTable::getSymbol(size_t i) const
{
    return const_cast<dpSymbolTable*>(this)->getSymbol(i);
}

const dpSymbol* dpSymbolTable::findSymbolByName(const char *name) const
{
    return const_cast<dpSymbolTable*>(this)->findSymbolByName(name);
}

const dpSymbol* dpSymbolTable::findSymbolByAddress( void *sym ) const
{
    return const_cast<dpSymbolTable*>(this)->findSymbolByAddress(sym);
}
