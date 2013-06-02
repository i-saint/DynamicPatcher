// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <cstdio>
#include <string>
#include <vector>

inline size_t GetModulePath(char *out_path, size_t len)
{
    HMODULE mod = 0;
    ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)&GetModulePath, &mod);
    DWORD size = ::GetModuleFileNameA(mod, out_path, len);
    return size;
}

inline bool GetModuleDirectory(char *out_path, size_t len)
{
    size_t size = ::GetModulePath(out_path, len);
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
    std::string target_process;
    std::vector<std::string> load_module;

    Options()
    {
        std::string config_path;
        {
            char module_path[MAX_PATH];
            GetModuleDirectory(module_path, _countof(module_path));
            config_path += module_path;

            // todo:
            // 引数で設定ファイル変えられるようにしたいところだが、別プロセス内で動く dll にもそれを伝えないといけないのでめんどい。
            // とりあえず後回し。
            //if(__argc>2) { config_path += __argv[1] ;}
            config_path += "DynamicPatcherInjector.txt";
        }

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
