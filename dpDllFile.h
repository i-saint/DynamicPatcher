// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpDLlFile_h
#define dpDLlFile_h

#include "DynamicPatcher.h"
#include "dpBinary.h"

#ifdef dpWithDllFile

// ロード中の dll は上書き不可能で、そのままだと実行時リビルドできない。
// そのため、指定のファイルをコピーしてそれを扱う。(関連する .pdb もコピーする)
// また、.dll だけでなく .exe も扱える (export された symbol がないと意味がないが)
class dpDllFile : public dpBinary
{
public:
    static const dpFileType FileType = dpE_DllFile;
    dpDllFile(dpContext *ctx);
    ~dpDllFile();
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

private:
    HMODULE m_module;
    bool m_needs_freelibrary;
    std::string m_path;
    std::string m_actual_file;
    std::string m_pdb_path;
    dpTime m_mtime;
    dpSymbolTable m_symbols;
};

#endif // dpWithDllFile
#endif // dpDLlFile_h
