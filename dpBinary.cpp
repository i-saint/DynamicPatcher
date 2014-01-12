// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include "DynamicPatcher.h"
#include "dpInternal.h"




dpSymbol::dpSymbol(const char *nam, void *addr, int fla, int sect, dpBinary *bin)
    : name(nam), address(addr), flags(fla), section(sect), binary(bin)
{}
dpSymbol::~dpSymbol()
{
    if((flags&dpE_NameNeedsDelete)!=0) {
        delete[] name;
    }
}
const dpSymbolS& dpSymbol::simplify() const { return (const dpSymbolS&)*this; }
bool dpSymbol::partialLink() { return binary->partialLink(section); }


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



class dpTrampolineAllocator::Page
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

dpTrampolineAllocator::Page::Page(void *base)
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

dpTrampolineAllocator::Page::~Page()
{
    dpDeallocate(m_data);
}

void* dpTrampolineAllocator::Page::allocate()
{
    void *ret = nullptr;
    if(m_freelist) {
        ret = m_freelist;
        m_freelist = m_freelist->next;
    }
    return ret;
}

bool dpTrampolineAllocator::Page::deallocate(void *v)
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

bool dpTrampolineAllocator::Page::isInsideMemory(void *p) const
{
    size_t loc = (size_t)p;
    size_t base = (size_t)m_data;
    return loc>=base && loc<base+page_size;
}

bool dpTrampolineAllocator::Page::isInsideJumpRange( void *p ) const
{
    size_t loc = (size_t)p;
    size_t base = (size_t)m_data;
    size_t dist = base<loc ? loc-base : base-loc;
    return dist < 0x7fff0000;
}


dpTrampolineAllocator::dpTrampolineAllocator()
{
}

dpTrampolineAllocator::~dpTrampolineAllocator()
{
    dpEach(m_pages, [](Page *p){ delete p; });
    m_pages.clear();
}

void* dpTrampolineAllocator::allocate(void *location)
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

bool dpTrampolineAllocator::deallocate(void *v)
{
    if(Page *page=findOwnerPage(v)) {
        return page->deallocate(v);
    }
    return false;
}

dpTrampolineAllocator::Page* dpTrampolineAllocator::createPage(void *location)
{
    Page *p = new Page(location);
    m_pages.push_back(p);
    return p;
}

dpTrampolineAllocator::Page* dpTrampolineAllocator::findOwnerPage(void *location)
{
    auto p = dpFind(m_pages, [=](const Page *p){ return p->isInsideMemory(location); });
    return p==m_pages.end() ? nullptr : *p;
}

dpTrampolineAllocator::Page* dpTrampolineAllocator::findCandidatePage(void *location)
{
    auto p = dpFind(m_pages, [=](const Page *p){ return p->isInsideJumpRange(location); });
    return p==m_pages.end() ? nullptr : *p;
}




template<size_t PageSize, size_t BlockSize>
class dpBlockAllocator<PageSize, BlockSize>::Page
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

private:
    void *m_data;
    Block *m_freelist;
};

template<size_t PageSize, size_t BlockSize>
dpBlockAllocator<PageSize, BlockSize>::Page::Page()
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

template<size_t PageSize, size_t BlockSize>
dpBlockAllocator<PageSize, BlockSize>::Page::~Page()
{
    free(m_data);
}

template<size_t PageSize, size_t BlockSize>
void* dpBlockAllocator<PageSize, BlockSize>::Page::allocate()
{
    void *ret = nullptr;
    if(m_freelist) {
        ret = m_freelist;
        m_freelist = m_freelist->next;
    }
    return ret;
}

template<size_t PageSize, size_t BlockSize>
bool dpBlockAllocator<PageSize, BlockSize>::Page::deallocate(void *v)
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

template<size_t PageSize, size_t BlockSize>
bool dpBlockAllocator<PageSize, BlockSize>::Page::isInsideMemory(void *p) const
{
    size_t loc = (size_t)p;
    size_t base = (size_t)m_data;
    return loc>=base && loc<base+page_size;
}


template<size_t PageSize, size_t BlockSize>
dpBlockAllocator<PageSize, BlockSize>::dpBlockAllocator()
{
}

template<size_t PageSize, size_t BlockSize>
dpBlockAllocator<PageSize, BlockSize>::~dpBlockAllocator()
{
    dpEach(m_pages, [](Page *p){ delete p; });
    m_pages.clear();
}

template<size_t PageSize, size_t BlockSize>
void* dpBlockAllocator<PageSize, BlockSize>::allocate()
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

template<size_t PageSize, size_t BlockSize>
bool dpBlockAllocator<PageSize, BlockSize>::deallocate(void *v)
{
    auto p = dpFind(m_pages, [=](const Page *p){ return p->isInsideMemory(v); });
    if(p!=m_pages.end()) {
        (*p)->deallocate(v);
        return true;
    }
    return false;
}
template dpBlockAllocator<1024*256, sizeof(dpSymbol)>;



dpSymbolTable::dpSymbolTable() : m_partial_link(false)
{
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

void dpSymbolTable::enablePartialLink(bool v)
{
    m_partial_link = v;
}

size_t dpSymbolTable::getNumSymbols() const
{
    return m_symbols.size();
}

dpSymbol* dpSymbolTable::getSymbol(size_t i)
{
    dpSymbol *sym = m_symbols[i];
    return sym;
}

dpSymbol* dpSymbolTable::findSymbolByName(const char *name)
{
    auto p = std::lower_bound(m_symbols.begin(), m_symbols.end(), name,
        [](const dpSymbol *sym, const char *name){ return *sym<name; }
    );
    if(p!=m_symbols.end() && **p==name) {
        dpSymbol *sym = *p;
        if(m_partial_link) { sym->partialLink(); }
        return sym;
    }
    return nullptr;
}

dpSymbol* dpSymbolTable::findSymbolByAddress( void *addr )
{
    auto p = dpFind(m_symbols, [=](const dpSymbol *sym){ return sym->address==addr; });
    if(p!=m_symbols.end()) {
        dpSymbol *sym = *p;
        if(m_partial_link) { sym->partialLink(); }
        return sym;
    }
    return nullptr;
}





dpSymbolFilter::dpSymbolFilter()  {}
dpSymbolFilter::~dpSymbolFilter() {}

void dpSymbolFilter::clear()                                    { m_patterns.clear(); }
void dpSymbolFilter::addPattern(const dpSymbolPattern &pattern) { m_patterns.insert(pattern); }

const dpSymbolPattern* dpSymbolFilter::findPattern( const char *sym_name ) const
{
    auto it = std::lower_bound(m_patterns.begin(), m_patterns.end(), sym_name,
        [&](const dpSymbolPattern &a, const char *b){
            return strcmp(a.name.c_str(), b)<0;
    });
    if(it==m_patterns.end() || it->name!=sym_name) {
        return nullptr;
    }
    return &(*it);
}

bool dpSymbolFilter::matchUpdate(const char *sym_name) const
{
    if(const dpSymbolPattern *pattern = findPattern(sym_name)) {
        return (pattern->flags & dpE_IncludeUpdate)!=0;
    }
    return false;
}

bool dpSymbolFilter::matchLinkToLocal(const char *sym_name) const
{
    if(const dpSymbolPattern *pattern = findPattern(sym_name)) {
        return (pattern->flags & dpE_IncludeLinkToLocal)!=0;
    }
    return false;
}

bool dpSymbolFilter::matchOnLoad(const char *sym_name) const
{
    if(const dpSymbolPattern *pattern = findPattern(sym_name)) {
        return (pattern->flags & dpE_IncludeOnLoad)!=0;
    }
    return false;
}

bool dpSymbolFilter::matchOnUnload(const char *sym_name) const
{
    if(const dpSymbolPattern *pattern = findPattern(sym_name)) {
        return (pattern->flags & dpE_IncludeOnUnload)!=0;
    }
    return false;
}


dpSymbolFilters::dpSymbolFilters()
{
}

dpSymbolFilters::~dpSymbolFilters()
{
    dpEach(m_filters, [](FilterCont::value_type &pair){
        delete pair.second;
    });
    m_filters.clear();
}

dpSymbolFilter* dpSymbolFilters::getFilter(const char *module_name)
{
    dpSymbolFilter *&filter = m_filters[module_name];
    if(!filter) {
        filter = new dpSymbolFilter();
    }
    return filter;
}

bool dpSymbolFilters::eraseFilter(const char *module_name)
{
    auto i = m_filters.find(module_name);
    if(i!=m_filters.end()) {
        delete i->second;
        m_filters.erase(i);
        return true;
    }
    return false;
}



dpBinary::dpBinary(dpContext *ctx) : m_context(ctx)
{
}

dpBinary::~dpBinary()
{
}



const char *g_dpSymName_OnLoad   = dpSymPrefix "dpOnLoadHandler";
const char *g_dpSymName_OnUnload = dpSymPrefix "dpOnUnloadHandler";

void dpCallOnLoadHandler(dpBinary *v)
{
    if(const dpSymbol *sym = v->getSymbolTable().findSymbolByName(g_dpSymName_OnLoad)) {
        if(!dpIsLinkFailed(sym->flags)) {
            ((dpEventHandler)sym->address)();
        }
    }
}

void dpCallOnUnloadHandler(dpBinary *v)
{
    if(const dpSymbol *sym = v->getSymbolTable().findSymbolByName(g_dpSymName_OnUnload)) {
        if(!dpIsLinkFailed(sym->flags)) {
            ((dpEventHandler)sym->address)();
        }
    }
}

