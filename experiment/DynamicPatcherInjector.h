// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <cstdio>
#include <string>
#include <vector>

inline size_t GetCurrentModulePath(char *out_path, size_t len)
{
    HMODULE mod = 0;
    ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)&GetCurrentModulePath, &mod);
    DWORD size = ::GetModuleFileNameA(mod, out_path, len);
    return size;
}

inline bool GetMainModuleFilename(char *out_path, size_t len)
{
    char tmp[MAX_PATH];
    size_t size = ::GetModuleFileNameA(nullptr, tmp, _countof(tmp));
    while(size>0) {
        if(tmp[size]=='\\') {
            strcpy_s(out_path, len, tmp+size+1);
            return true;
        }
        --size;
    }
    return false;
}

inline bool GetCurrentModuleDirectory(char *out_path, size_t len)
{
    size_t size = ::GetCurrentModulePath(out_path, len);
    while(size>0) {
        if(out_path[size]=='\\') {
            out_path[size+1] = '\0';
            return true;
        }
        --size;
    }
    return false;
}

struct Options
{
    std::string config_dir;
    std::string config_path;
    std::string target_process;
    std::vector<std::string> load_module;

    Options()
    {
        char module_path[MAX_PATH];
        GetCurrentModuleDirectory(module_path, _countof(module_path));
        config_dir = module_path;
    }

    bool copyShared(const char *name)
    {
        char dst[MAX_PATH];
        sprintf(dst, "%s%s.inj", config_dir.c_str(), name);
        ::CopyFileA(config_path.c_str(), dst, FALSE);
        return true;
    }

    void loadHost()
    {
        if(__argc>2) { config_path = config_dir+__argv[1] ;}
        else         { config_path = config_dir+"DynamicPatcherInjector.txt"; }
        _load();

    }

    void loadDLL()
    {
        char process_name[MAX_PATH];
        char src[MAX_PATH];
        GetMainModuleFilename(process_name, _countof(process_name));
        sprintf(src, "%s%s.inj", config_dir.c_str(), process_name);
        config_path = src;
        _load();
        ::DeleteFileA(config_path.c_str());
    }

    void _load()
    {
        char line[1024];
        char opt[MAX_PATH];
        if(FILE *f=fopen(config_path.c_str(), "rb")) {
            while(fgets(line, _countof(line), f)) {
                if     (sscanf(line, "target_process: \"%[^\"]\"", opt)) { target_process=opt; }
                else if(sscanf(line, "load_module: \"%[^\"]\"", opt))    { load_module.push_back(opt); }
            }
            fclose(f);
        }
    }
};
