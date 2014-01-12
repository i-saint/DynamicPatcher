// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include "dpInternal.h"
#include "dpLibFile.h"


#ifdef dpWithLibFile

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
    dpTime mtime = dpGetMTime(path);
    if(!m_objs.empty() && mtime<=m_mtime) { return true; }

    void *lib_data;
    size_t lib_size;
    if(!dpMapFile(path, lib_data, lib_size, malloc)) {
        dpPrintError("file not found %s\n", path);
        return false;
    }
    bool ret = loadMemory(path, lib_data, lib_size, mtime);
    free(lib_data);
    return ret;
}

bool dpLibFile::loadMemory(const char *path, void *lib_data, size_t lib_size, dpTime mtime)
{
    if(!m_objs.empty() && mtime<=m_mtime) { return true; }
    m_path = path; dpSanitizePath(m_path);
    m_mtime = mtime;

    // .lib の構成は以下を参照
    // http://hp.vector.co.jp/authors/VA050396/tech_04.html

    char *base = (char*)lib_data;
    if(strncmp(base, IMAGE_ARCHIVE_START, IMAGE_ARCHIVE_START_SIZE)!=0) {
        dpPrintError("unknown file format %s\n", path);
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

bool dpLibFile::partialLink(size_t section)
{
    return true;
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

#endif // dpWithLibFile
