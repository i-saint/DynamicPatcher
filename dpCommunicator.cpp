// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include "dpCommunicator.h"

#ifdef dpWithCommunicator
#pragma comment (lib, "Ws2_32.lib")





dpCommunicator::dpCommunicator(dpContext *ctx)
    : m_context(ctx)
    , m_running(false)
    , m_auto_flush(false)
    , m_port(0)
{
}

dpCommunicator::~dpCommunicator()
{
    stop();
}

bool dpCommunicator::run(uint16_t port, bool auto_flush)
{
    if(m_running) { return false; }

    m_auto_flush = auto_flush;
    m_port = port;
    m_running = true;
    dpRunThread([&](){
        bool r = dpRunTCPServer(m_port, [&](dpTCPSocket &client){
            dpProtocolSocket s(client.getHandle(), false);
            return onAccept(s);
        });
        if(r) {
            dpPrintInfo("communicator started. port: %d\n", (int)port);
        }
        m_running = false;
    });
    return true;
}

void dpCommunicator::stop()
{
    if(!m_running) { return; }

    dpProtocolSocket stopper;
    if(stopper.open("127.0.0.1", m_port)) {
        static const char s_dpShutdown[] = "dpShutdown\n\n";
        static const int s_dpShutdown_len = strlen(s_dpShutdown);
        if(stopper.write(s_dpShutdown, s_dpShutdown_len)) {
            std::string recvbuf;
            stopper.read(recvbuf);
        }
        while(m_running) { ::SwitchToThread(); }
    }
}

bool dpCommunicator::onAccept(dpProtocolSocket &client)
{
    // command
    static const char   s_dpShutdown[]          = "dpShutdown\n";
    static const size_t s_dpShutdown_len        = strlen(s_dpShutdown);
    static const char   s_dpLoadBinary[]        = "dpLoadBinary\n";
    static const size_t s_dpLoadBinary_len      = strlen(s_dpLoadBinary);
    static const char   s_dpLoadSymbols[]       = "dpLoadSymbols\n";
    static const size_t s_dpLoadSymbols_len     = strlen(s_dpLoadSymbols);
    static const char   s_dpSetSymbolFilter[]   = "dpSetSymbolFilter\n";
    static const size_t s_dpSetSymbolFilter_len = strlen(s_dpSetSymbolFilter);
    static const char   s_dpToggleSuspend[]     = "dpToggleSuspend\n";
    static const size_t s_dpToggleSuspend_len   = strlen(s_dpToggleSuspend);
    static const char   s_dpQuery[]             = "dpQuery\n";
    static const size_t s_dpQuery_len           = strlen(s_dpQuery);

    // response
    static const char s_ok[]       = "dpOK\n";
    static const char s_error[]    = "dpError\n";
    std::string response = "";

    bool does_continue = true;
    bool handled = false;
    std::string recvbuf;
    if (client.read(recvbuf)) {
        char *buf = &recvbuf[0];
        uint32_t len = recvbuf.size();
        if(strncmp(buf, s_dpShutdown, s_dpShutdown_len)==0) {
            handled = true;
            does_continue = false;
        }
        else if(strncmp(buf, s_dpLoadBinary, s_dpLoadBinary_len)==0) {
            std::string command = std::string(buf+s_dpLoadBinary_len, len-s_dpLoadBinary_len);
            m_context->addCommand(dpCommand(dpE_CmdLoadBinary, command));
            handled = true;
        }
        else if(strncmp(buf, s_dpSetSymbolFilter, s_dpSetSymbolFilter_len)==0) {
            std::string command = std::string(buf+s_dpSetSymbolFilter_len, len-s_dpSetSymbolFilter_len);
            m_context->addCommand(dpCommand(dpE_CmdSetSymbolFilter, command));
            handled = true;
        }
        else if(strncmp(buf, s_dpLoadSymbols, s_dpLoadSymbols_len)==0) {
            std::string command = std::string(buf+s_dpLoadSymbols_len, len-s_dpLoadSymbols_len);
            m_context->addCommand(dpCommand(dpE_CmdLoadSymbols, command));
            handled = true;
        }
        else if(strncmp(buf, s_dpToggleSuspend, s_dpToggleSuspend_len)==0) {
            std::string command = std::string(buf+s_dpToggleSuspend_len, len-s_dpToggleSuspend_len);
            m_context->addCommand(dpCommand(dpE_CmdToggleSuspend, command));
            handled = true;
        }
        else if(strncmp(buf, s_dpQuery, s_dpQuery_len)==0) {
            std::vector<std::string> parameters;
            dpEachLines(buf, len, [&](char *l){
                parameters.push_back(std::string(l));
            });
            if(parameters.size()>=2) {
                handled = processQuery(parameters, response);
            }
        }
    }

    if(handled) {
        response = s_ok + response + "\n";
        client.write(response.c_str(), response.size());
        if(m_auto_flush) {
            m_context->update();
        }
    }
    else {
        response = s_error + response + "\n";
        client.write(response.c_str(), response.size());
    }
    return does_continue;
}

bool dpCommunicator::processQuery(std::vector<std::string> &request, std::string &response)
{
    if(request[1]=="dpGetFilter") {
        if(request.size()>=3) {
            response.clear();
            if(dpSymbolFilter *filt = m_context->getSymbolFilters()->getFilter(request[2].c_str())) {
                filt->eachPatterns([&](const dpSymbolPattern &pat){
                    char tmp[4096+64];
                    sprintf(tmp, "%d %s\n", pat.flags, pat.name.c_str());
                    response += tmp;
                });
                response += "\n";
                return true;
            }
        }
    }
    return false;
}

#endif // dpWithCommunicator
