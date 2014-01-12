// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpObjFile_h
#define dpObjFile_h

#include "DynamicPatcher.h"
#include "dpBinary.h"

#ifdef dpWithObjFile

class dpObjFile : public dpBinary
{
public:
    static const dpFileType FileType= dpE_ObjFile;
    dpObjFile(dpContext *ctx);
    ~dpObjFile();
    void unload();
    virtual bool loadFile(const char *path);
    virtual bool loadMemory(const char *name, void *data, size_t datasize, dpTime filetime);
    virtual bool link();
    virtual bool partialLink(size_t section);
    virtual bool callHandler(dpEventType e);

    virtual dpSymbolTable& getSymbolTable();
    virtual const char*    getPath() const;
    virtual dpTime         getLastModifiedTime() const;
    virtual dpFileType     getFileType() const;

    void* getBaseAddress() const;

private:
    struct RelocationData
    {
        uint32_t base;
        RelocationData() : base(0) {}
    };
    struct LinkData // section と対になるデータ
    {
        uint32_t flags;
        LinkData() : flags(dpE_NeedsLink|dpE_NeedsBase) {}
    };
    typedef std::vector<LinkData>       link_cont;
    typedef std::vector<RelocationData> reloc_cont;
    void  *m_data;
    size_t m_size;
    void  *m_aligned_data;
    size_t m_aligned_datasize;
    std::string m_path;
    dpTime m_mtime;
    dpSymbolTable m_symbols;
    link_cont m_linkdata;
    reloc_cont m_relocdata;

    void* resolveSymbol(const char *name);
    void* resolveSymbolFromLocal(const char *name);
};

#endif // dpWithObjFile
#endif // dpObjFile_h
