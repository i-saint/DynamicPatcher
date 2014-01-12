// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpConfigFile_h
#define dpConfigFile_h

#include "DynamicPatcher.h"
#include "dpFoundation.h"


struct dpConfigFile
{
    int log_flags;
    int sys_flags;
    int vc_ver;
    std::vector<std::string> loads;
    std::vector<std::string> source_paths;
    std::vector<std::string> module_paths;
    std::vector<std::string> preload_paths;
    std::vector<std::string> msbuild_commands;
    std::vector<std::string> build_commands;
    std::vector<std::string> force_host_symbol_patterns;
    std::string config_path;

    dpConfigFile();
    bool copy(const char *name);
    bool load();
    bool load(const char *path);
    bool load(const wchar_t *path);
};
dpConfig& dpGetConfig();

#endif // dpConfigFile_h
