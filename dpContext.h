// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpContext_h
#define dpContext_h

#include "DynamicPatcher.h"
#include "dpFoundation.h"
#include "dpBinary.h"


enum dpCommandType {
    dpE_CmdLoadBinary,
    dpE_CmdSetSymbolFilter,
    dpE_CmdLoadSymbols,
    dpE_CmdToggleSuspend,
    dpE_CmdQuery,
};
struct dpCommand
{
    int code;
    std::string param;

    dpCommand(int c=0, const std::string &p="") : code(c), param(p) {}
};
typedef std::vector<dpCommand> dpCommands;


class dpContext
{
public:
    dpContext();
    ~dpContext();
    dpBuilder*       getBuilder();
    dpPatcher*       getPatcher();
    dpLoader*        getLoader();
    dpSymbolFilters* getSymbolFilters();
    dpCommunicator*  getCommunicator();

    bool        runCommunicator(uint16_t port, bool auto_flush);
    size_t      loadBinaries(const char *path);

    size_t      patchByFile(const char *filename, const char *filter_regex);
    size_t      patchByFile(const char *filename, const std::function<bool (const dpSymbolS&)> &condition);
    bool        patchNameToName(const char *target_name, const char *hook_name);
    bool        patchAddressToName(const char *target_name, void *hook);
    bool        patchAddressToAddress(void *target, void *hook);
    bool        patchByAddress(void *hook);
    bool        unpatchByAddress(void *target_or_hook_addr);
    void        unpatchAll();
    void*       getUnpatched(void *target_or_hook_addr);
    void        addForceHostSymbolPattern(const char *pattern);

    void        addCommand(const dpCommand &cmd);
    void        update();

private:
    void        handleCommand(dpCommand &cmd);

private:
    dpBuilder       *m_builder;
    dpPatcher       *m_patcher;
    dpLoader        *m_loader;
    dpSymbolFilters *m_filters;
    dpCommunicator  *m_communicator;

    dpMutex         m_mtx_commands;
    dpCommands      m_commands;
    bool            m_suspended;
    bool            m_toggle_suspend;
};

#endif // dpContext_h
