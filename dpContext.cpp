// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include "DynamicPatcher.h"
#include "dpInternal.h"
#include "dpCommunicator.h"

dpContext::dpContext()
    : m_suspended(false)
    , m_toggle_suspend(false)
{
    m_builder      = new dpBuilder(this);
    m_patcher      = new dpPatcher(this);
    m_loader       = new dpLoader(this);
    m_filters       = new dpSymbolFilters();
#ifdef dpWithCommunicator
    m_communicator = new dpCommunicator(this);
#endif // dpWithCommunicator
}

dpContext::~dpContext()
{
    m_builder->stopAutoBuild();
    m_builder->stopPreload();
    // バイナリ unload 時に適切に unpatch するには patcher より先に loader を破棄する必要がある

#ifdef dpWithCommunicator
    delete m_communicator;  m_communicator=nullptr;
#endif // dpWithCommunicator
    delete m_filters;        m_filters=nullptr;
    delete m_loader;        m_loader=nullptr;
    delete m_patcher;       m_patcher=nullptr;
    delete m_builder;       m_builder=nullptr;
}

dpBuilder*       dpContext::getBuilder()       { return m_builder; }
dpPatcher*       dpContext::getPatcher()       { return m_patcher; }
dpLoader*        dpContext::getLoader()        { return m_loader; }
dpSymbolFilters* dpContext::getSymbolFilters() { return m_filters; }
dpCommunicator*  dpContext::getCommunicator()  { return m_communicator; }

bool dpContext::runCommunicator( uint16_t port, bool auto_flush )
{
#ifdef dpWithCommunicator
    return m_communicator->run(port, auto_flush);
#else
    return false;
#endif // dpWithCommunicator
}

size_t dpContext::loadBinaries(const char *path)
{
    size_t ret = 0;
    dpGlob(path, [&](const std::string &p){
        if(m_loader->load(p.c_str())) { ++ret; }
    });
    return ret;
}

size_t dpContext::patchByFile(const char *filename, const char *filter_regex)
{
    if(dpBinary *bin=m_loader->findBinary(filename)) {
        std::regex reg(filter_regex);
        m_patcher->patchByBinary(bin,
            [&](const dpSymbolS &sym){
                return std::regex_search(sym.name, reg);
        });
        return true;
    }
    return false;
}

size_t dpContext::patchByFile(const char *filename, const std::function<bool (const dpSymbolS&)> &condition)
{
    if(dpBinary *bin=m_loader->findBinary(filename)) {
        m_patcher->patchByBinary(bin, condition);
        return true;
    }
    return false;
}

bool dpContext::patchNameToName(const char *target_name, const char *hook_name)
{
    dpSymbol *target = m_loader->findHostSymbolByName(target_name);
    dpSymbol *hook   = m_loader->findSymbolByName(hook_name);
    if(target && hook) {
        return m_patcher->patch(target, hook)!=nullptr;
    }
    return false;
}

bool dpContext::patchAddressToName(const char *target_name, void *hook_addr)
{
    dpSymbol *target = m_loader->findHostSymbolByName(target_name);
    dpSymbol *hook   = m_loader->findSymbolByAddress(hook_addr);
    if(target && hook) {
            return m_patcher->patch(target, hook)!=nullptr;
    }
    return false;
}

bool dpContext::patchAddressToAddress(void *target_addr, void *hook_addr)
{
    dpSymbol *target = m_loader->findHostSymbolByAddress(target_addr);
    dpSymbol *hook   = m_loader->findSymbolByAddress(hook_addr);
    if(target && hook) {
        return m_patcher->patch(target, hook)!=nullptr;
    }
    return false;
}

bool dpContext::patchByAddress(void *hook_addr)
{
    if(dpSymbol *hook=m_loader->findSymbolByAddress(hook_addr)) {
        if(dpSymbol *target=m_loader->findHostSymbolByName(hook->name)) {
            return m_patcher->patch(target, hook)!=nullptr;
        }
    }
    return false;
}

bool dpContext::unpatchByAddress(void *target_or_hook_addr)
{
    return m_patcher->unpatchByAddress(target_or_hook_addr);
}

void dpContext::unpatchAll()
{
    return m_patcher->unpatchAll();
}

void* dpContext::getUnpatched(void *target)
{
#ifdef dpWithTDisasm
    if(dpPatchData *pd = m_patcher->findPatchByAddress(target)) {
        return pd->unpatched;
    }
#endif // dpWithTDisasm
    return nullptr;
}

void dpContext::addForceHostSymbolPattern(const char *pattern)
{
    m_loader->addForceHostSymbolPattern(pattern);
}

void dpContext::addCommand(const dpCommand &cmd)
{
    dpMutex::ScopedLock lock(m_mtx_commands);
    m_commands.push_back(cmd);
}

void dpContext::update()
{
    dpCommands tmp_commands;
    for(;;) {
        {
            dpMutex::ScopedLock lock(m_mtx_commands);
            tmp_commands = m_commands;
            m_commands.clear();
        }
        if(tmp_commands.empty()) { break; }

        if(m_suspended) {
            dpEach(tmp_commands, [&](dpCommand& cmd){ handleCommand(cmd); });
        }
        else {
            dpExecExclusive([&](){
                dpEach(tmp_commands, [&](dpCommand& cmd){ handleCommand(cmd); });
            });
        }
        tmp_commands.clear();

        if(m_toggle_suspend) {
            dpEnumerateThreads([&](DWORD tid){
                if(tid==::GetCurrentThreadId()) { return; }
                if(HANDLE thread=::OpenThread(THREAD_ALL_ACCESS, FALSE, tid)) {
                    if(m_suspended) { ::ResumeThread(thread);  }
                    else            { ::SuspendThread(thread); }
                    ::CloseHandle(thread);
                }
            });
            m_suspended = !m_suspended;
            dpPrintInfo(m_suspended ? "suspended\n" : "resumed\n");
        }
        m_toggle_suspend = false;
    }
}

void dpContext::handleCommand( dpCommand &cmd )
{
    switch(cmd.code) {
    case dpE_CmdLoadBinary:
        if(!cmd.param.empty()) {
            dpEachLines(&cmd.param[0], cmd.param.size(), [&](char *file){
                m_builder->reload(file);
            });
        }
        else {
            m_builder->reload();
        }
        dpPrintInfo("patching started...\n");
        m_loader->link();
        dpPrintInfo("patching finished.\n");
        break;

    case dpE_CmdSetSymbolFilter:
        if(!cmd.param.empty()) {
            dpSymbolFilter *filter = nullptr;
            dpEachLines(&cmd.param[0], cmd.param.size(), [&](char *l){
                if(!filter) {
                    filter = m_filters->getFilter(l);
                    filter->clear();
                }
                else {
                    uint32_t flags = 0;
                    if(sscanf(l, "%d ", &flags)==1) {
                        while(*l!=' ') { ++l; } ++l;
                        dpSymbolPattern ptn(flags, l);
                        filter->addPattern(ptn);
                    }
                }
            });
        }
        break;

    case dpE_CmdLoadSymbols:
        dpPrintInfo("LoadSymbol\n");
        dpLoadMapFiles();
        dpEachModules([&](HMODULE mod){
            char path[MAX_PATH];
            ::GetModuleFileNameA(mod, path, sizeof(path));
            if(!::SymLoadModuleEx(::GetCurrentProcess(), nullptr, path, nullptr, (DWORD64)mod, 0, nullptr, 0)) {
                //dpPrintError("SymLoadModuleEx() failed. %s\n", path);
            }
            else {
                dpPrintInfo("SymLoadModuleEx() succeeded. %s\n", path);
            }
        });
        break;

    case dpE_CmdToggleSuspend:
        m_toggle_suspend = true;
        break;
    }
}
