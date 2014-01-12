// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpCommunicator_h
#define dpCommunicator_h

#include "DynamicPatcher.h"
#include "dpInternal.h"
#include "dpNetwork.h"

#ifdef dpWithCommunicator



class dpCommunicator
{
public:
    dpCommunicator(dpContext *ctx);
    ~dpCommunicator();
    bool run(uint16_t port, bool auto_flush);
    void stop();

private:
    bool onAccept(dpProtocolSocket &client);
    bool processQuery(std::vector<std::string> &request, std::string &out_res);

    dpContext  *m_context;
    bool        m_running;
    bool        m_auto_flush;
    uint16_t    m_port;
};

#endif // dpWithCommunicator
#endif // dpCommunicator_h
