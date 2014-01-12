// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include "DynamicPatcher.h"
#include "dpInternal.h"

static dpConfig g_dpConfig;
dpConfig& dpGetConfig() { return g_dpConfig; }

dpConfigFile::dpConfigFile()
    : log_flags(-1), sys_flags(-1), vc_ver(-1)
{
}

bool dpConfigFile::copy(const char *name)
{
    return ::CopyFileA(config_path.c_str(), name, FALSE)==TRUE;
}

bool dpConfigFile::load()
{
    char dll_path[MAX_PATH];
    char exe_path[MAX_PATH];
    std::string dll_dir, exe_file, exe;
    dpGetMainModulePath(exe_path, _countof(exe_path));
    dpGetCurrentModulePath(dll_path, _countof(dll_path));
    dpSeparateDirFile(dll_path, &dll_dir, nullptr);
    dpSeparateDirFile(exe_path, nullptr, &exe_file);
    dpSeparateFileExt(exe_file.c_str(), &exe, nullptr);

    std::string conf = dll_dir;
    conf += exe;
    conf += "dpconf";
    return load(conf.c_str());
}

bool dpConfigFile::load(const wchar_t *path)
{
    size_t len = wcstombs(nullptr, path, 0);
    char *mbs = (char*)alloca(len+1);
    wcstombs(mbs, path, len);
    mbs[len] = '\0';
    return load(mbs);
}


bool dpConfigFile::load(const char *path)
{
    if(FILE *f=fopen(path, "rb")) {
        char line[1024];
        char opt[MAX_PATH];
        int iv;
        while(fgets(line, _countof(line), f)) {
            if(line[0]=='/') { continue; }
            else if(sscanf(line, "log flags: %x", &iv))                 { log_flags=iv; }
            else if(sscanf(line, "sys flags: %x", &iv))                 { sys_flags=iv; }
            else if(sscanf(line, "vc ver: %d", &iv))                    { vc_ver=iv; }
            else if(sscanf(line, "load: \"%[^\"]\"", opt))              { loads.push_back(opt); }
            else if(sscanf(line, "source path: \"%[^\"]\"", opt))       { source_paths.push_back(opt); }
            else if(sscanf(line, "module path: \"%[^\"]\"", opt))       { module_paths.push_back(opt); }
            else if(sscanf(line, "preload path: \"%[^\"]\"", opt))      { preload_paths.push_back(opt); }
            else if(sscanf(line, "msbuild command: \"%[^\"]\"", opt))   { msbuild_commands.push_back(opt); }
            else if(sscanf(line, "build command: \"%[^\"]\"", opt))     { build_commands.push_back(opt); }
            else if(sscanf(line, "force host symbol pattern: \"%[^\"]\"", opt)) { force_host_symbol_patterns.push_back(opt); }
        }
        fclose(f);
        config_path = path;
        return true;
    }
    return false;
}
