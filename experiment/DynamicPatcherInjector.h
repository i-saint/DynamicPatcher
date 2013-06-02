// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <cstdio>
#include <string>
#include <vector>

struct Options
{
    std::string target_process;
    std::string target_update_function;
    std::string hook_update_function;
    std::string msbuild_command;
    std::vector<std::string> load_list;

    Options()
    {
        const char *conf = "DynamicPatcherInjector.txt";
        if(__argc>2) { conf=__argv[1] ;}

        char line[1024];
        char opt[1024];
        if(FILE *f=fopen(conf, "rb")) {
            while(fgets(line, _countof(line), f)) {
                if     (sscanf(line, "target_process: \"%[^\"]\"", opt))         { target_process=opt; }
                else if(sscanf(line, "target_update_function: \"%[^\"]\"", opt)) { target_update_function=opt; }
                else if(sscanf(line, "hook_update_function: \"%[^\"]\"", opt))   { hook_update_function=opt; }
                else if(sscanf(line, "msbuild_command: \"%[^\"]\"", opt))        { msbuild_command=opt; }
                else if(sscanf(line, "load_list: \"%[^\"]\"", opt))              { load_list.push_back(opt); }
            }
            fclose(f);
        }
    }
};
