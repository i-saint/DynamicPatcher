// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#ifndef dpNetwork_h
#define dpNetwork_h

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <cstdint>
#include "DynamicPatcher.h"
#include "dpFeatures.h"

class dpTCPSocket;
class dpCommunicator;
void dpInitializeNetwork();
void dpFinalizeNetwork();
bool dpRunTCPServer(uint16_t port, const std::function<bool (dpTCPSocket &client)> &handler);

class dpTCPSocket
{
public:
    dpTCPSocket();
    dpTCPSocket(SOCKET s, bool needs_shutdown=true);
    dpTCPSocket(const char *host, uint16_t port);
    ~dpTCPSocket();
    SOCKET getHandle() const;
    bool open(const char *host, uint16_t port);
    void close();
    int read(void *buf, uint32_t buf_size);
    int write(const void *data, uint32_t data_size);

private:
    SOCKET m_socket;
    bool m_needs_shutdown;

private:
    // forbidden
    dpTCPSocket(const dpTCPSocket &);
    dpTCPSocket& operator=(const dpTCPSocket &);
};

class dpProtocolSocket
{
public:
    dpProtocolSocket();
    dpProtocolSocket(SOCKET s, bool needs_shutdown=true);
    dpProtocolSocket(const char *host, uint16_t port);
    ~dpProtocolSocket();
    bool open(const char *host, uint16_t port);
    void close();
    bool read(std::string &o_str);
    bool write(const void *data, uint32_t data_size);
private:
    dpTCPSocket m_socket;
};

#endif // dpNetwork_h
