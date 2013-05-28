// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "DynamicPatcher.h"
#include "dpInternal.h"

dpBuilder::dpBuilder()
    : m_msbuild_option()
    , m_create_console(false)
    , m_build_done(false)
    , m_flag_exit(false)
    , m_thread_watchfile(NULL)
{
    std::string VCVersion;
#if     _MSC_VER==1500
    VCVersion = "9.0";
#elif   _MSC_VER==1600
    VCVersion = "10.0";
#elif   _MSC_VER==1700
    VCVersion = "11.0";
#else
#   error
#endif

    std::string keyName = "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VC7";
    char value[MAX_PATH];
    DWORD size = MAX_PATH;
    HKEY key;
    LONG retKey = ::RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyName.c_str(), 0, KEY_READ|KEY_WOW64_32KEY, &key);
    LONG retVal = ::RegQueryValueExA(key, VCVersion.c_str(), NULL, NULL, (LPBYTE)value, &size );
    if( retKey==ERROR_SUCCESS && retVal==ERROR_SUCCESS  ) {
        m_vcvars += '"';
        m_vcvars += value;
        m_vcvars += "vcvarsall.bat";
        m_vcvars += '"';
#ifdef _WIN64
        m_vcvars += " amd64";
#else // _WIN64
        m_vcvars += " x86";
#endif // _WIN64
        m_msbuild = m_vcvars;
        m_msbuild += " && ";
        m_msbuild += "msbuild";
    }
    ::RegCloseKey( key );
}

dpBuilder::~dpBuilder()
{
    m_flag_exit = true;
    if(m_thread_watchfile!=NULL) {
        ::WaitForSingleObject(m_thread_watchfile, INFINITE);
    }
    if(m_create_console) {
        ::FreeConsole();
    }
}

void dpBuilder::addLoadPath(const char *path)
{
    m_loadpathes.push_back(path);
}

void dpBuilder::addSourcePath(const char *path)
{
    char fullpath[MAX_PATH];
    ::GetFullPathNameA(path, MAX_PATH, fullpath, nullptr);
    SourcePath sd = {fullpath, NULL};
    m_srcpathes.push_back(sd);
}

static void WatchFile( LPVOID arg )
{
    ((dpBuilder*)arg)->watchFiles();
}

bool dpBuilder::startAutoCompile(const char *build_options, bool create_console)
{
    m_msbuild_option = build_options;
    m_create_console = create_console;
    if(create_console) {
        ::AllocConsole();
    }
    m_thread_watchfile = (HANDLE)_beginthread( WatchFile, 0, this );
    dpPrint("dp info: recompile thread started\n");
    return true;
}

bool dpBuilder::stopAutoCompile()
{
    return false;
}


void dpBuilder::watchFiles()
{
    // ファイルの変更を監視、変更が検出されたらビルド開始
    for(size_t i=0; i<m_srcpathes.size(); ++i) {
        m_srcpathes[i].notifier = ::FindFirstChangeNotificationA(m_srcpathes[i].path.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
    }

    while(!m_flag_exit) {
        bool needs_build = false;
        for(size_t i=0; i<m_srcpathes.size(); ++i) {
            if(::WaitForSingleObject(m_srcpathes[i].notifier, 10)==WAIT_OBJECT_0) {
                needs_build = true;
            }
        }
        if(needs_build) {
            build();
            for(size_t i=0; i<m_srcpathes.size(); ++i) {
                // ビルドで大量のファイルに変更が加わっていることがあり、以後の FindFirstChangeNotificationA() はそれを検出してしまう。
                // そうなると永遠にビルドし続けてしまうため、HANDLE ごと作りなおして一度リセットする。
                ::FindCloseChangeNotification(m_srcpathes[i].notifier);
                m_srcpathes[i].notifier = ::FindFirstChangeNotificationA(m_srcpathes[i].path.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);
            }
        }
        else {
            ::Sleep(1000);
        }
    }

    for(size_t i=0; i<m_srcpathes.size(); ++i) {
        ::FindCloseChangeNotification(m_srcpathes[i].notifier);
    }
}

bool dpBuilder::build()
{
    dpPrint("dp info: recompile begin\n");
    std::string command = m_msbuild;
    command+=' ';
    command+=m_msbuild_option;

    STARTUPINFOA si; 
    PROCESS_INFORMATION pi; 
    memset(&si, 0, sizeof(si)); 
    memset(&pi, 0, sizeof(pi)); 
    si.cb = sizeof(si);
    if(::CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)==TRUE) {
        DWORD exit_code = 0;
        ::WaitForSingleObject(pi.hThread, INFINITE);
        ::WaitForSingleObject(pi.hProcess, INFINITE);
        ::GetExitCodeProcess(pi.hProcess, &exit_code);
        ::CloseHandle(pi.hThread);
        ::CloseHandle(pi.hProcess);
        ::Sleep(500); // 終了直後だとファイルの書き込みが終わってないことがあるっぽい？ので少し待つ…
        m_build_done = true;
        if(exit_code!=0) {
            dpPrint("DOL error: build error.\n");
            return false;
        }
    }
    dpPrint("dp info: recompile end\n");
    return true;
}

void dpBuilder::update()
{
    if(m_build_done) {
        m_build_done = false;

        size_t num_loaded = 0;
        for(size_t i=0; i<m_loadpathes.size(); ++i) {
            dpGlob(m_loadpathes[i], [&](const std::string &path){
                if(dpGetLoader()->loadBinary(path.c_str())) {
                    ++num_loaded;
                }
            });
        }
        if(num_loaded) {
            dpGetLoader()->link();
        }
    }
}


dpCLinkage dpAPI dpBuilder* dpCreateBuilder()
{
    return new dpBuilder();
}
