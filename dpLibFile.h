// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpLibFile_h
#define dpLibFile_h

#include "DynamicPatcher.h"
#include "dpBinary.h"

#ifdef dpWithLibFile

class dpLibFile : public dpBinary
{
public:
    static const dpFileType FileType = dpE_LibFile;
    dpLibFile(dpContext *ctx);
    ~dpLibFile();
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
    size_t                 getNumObjFiles() const;
    dpObjFile*             getObjFile(size_t index);
    dpObjFile*             findObjFile(const char *name);

    template<class F>
    void eachObjs(const F &f) { dpEach(m_objs, f); }

private:
    typedef std::vector<dpObjFile*> obj_cont;
    obj_cont m_objs;
    dpSymbolTable m_symbols;
    std::string m_path;
    dpTime m_mtime;
};

#endif // dpWithLibFile
#endif // dpLibFile_h
