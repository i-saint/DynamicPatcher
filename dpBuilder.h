// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpBuilder_h
#define dpBuilder_h

#include "DynamicPatcher.h"
#include "dpBinary.h"


class dpBuilder
{
public:
    class ScopedPreloadLock
    {
    public:
        ScopedPreloadLock(dpBuilder *v) : m_builder(v) { m_builder->lockPreload(); }
        ~ScopedPreloadLock() { m_builder->unlockPreload(); }
        dpBuilder *m_builder;
    };

    dpBuilder(dpContext *ctx);
    ~dpBuilder();
    void addModulePath(const char *path);
    void addSourcePath(const char *path);
    void addPreloadPath(const char *path);
    void addMSBuildCommand(const char *msbuild_options);
    void addCLBuildCommand(const char *cl_options);
    void addBuildCommand(const char *any_command);
    bool startAutoBuild();
    bool stopAutoBuild();
    bool startPreload();
    bool stopPreload();
    void lockPreload();
    void unlockPreload();

    size_t reload();
    bool   reload(const char *path);
    void   watchFiles();
    bool   build();
    bool   build(const char *command, const char *obj=nullptr);
    void   preload();

    const char* getVCVarsPath() const;

private:
    struct SourcePath
    {
        std::string path;
        HANDLE notifier;
    };

    dpContext *m_context;
    std::string m_vcvars;
    std::vector<std::string> m_build_commands;
    std::vector<SourcePath>  m_srcpathes;
    std::vector<std::string> m_loadpathes;
    std::vector<std::string> m_preloadpathes;
    bool m_watchfile_stop;
    HANDLE m_thread_autobuild;
    HANDLE m_thread_preload;
    bool m_preload_stop;
    dpMutex m_mtx_preload;
};


#endif // dpBuilder_h
