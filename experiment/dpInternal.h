// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

void dpPrint(const char* fmt, ...);
void* dpAllocate(size_t size, void *location);
void* dpAllocateModule(size_t size);
dpTime dpGetFileModifiedTime(const char *path);
bool dpDemangle(const char *mangled, char *demangled, size_t buflen);

template<class F>
void dpGlob(const std::string &path, const F &f)
{
    std::string dir;
    size_t dir_len=0;
    for(size_t l=0; l<path.size(); ++l) {
        if(path[l]=='\\' || path[l]=='/') { dir_len=l+1; }
    }
    dir.insert(dir.end(), path.begin(), path.begin()+dir_len);

    WIN32_FIND_DATAA wfdata;
    HANDLE handle = ::FindFirstFileA(path.c_str(), &wfdata);
    if(handle!=INVALID_HANDLE_VALUE) {
        do {
            f( dir+wfdata.cFileName );
        } while(::FindNextFileA(handle, &wfdata));
        ::FindClose(handle);
    }
}

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

