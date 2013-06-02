// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <cstdio>
#include <string>
#include <vector>

struct Options
{
    std::string target_process;
    std::vector<std::string> load_module;

    Options()
    {
        const char *conf = "DynamicPatcherInjector.txt";
        if(__argc>2) { conf=__argv[1] ;}

        char line[1024];
        char opt[1024];
        if(FILE *f=fopen(conf, "rb")) {
            while(fgets(line, _countof(line), f)) {
                if     (sscanf(line, "target_process: \"%[^\"]\"", opt)) { target_process=opt; }
                else if(sscanf(line, "load_module: \"%[^\"]\"", opt))    { load_module.push_back(opt); }
            }
            fclose(f);
        }
    }
};
