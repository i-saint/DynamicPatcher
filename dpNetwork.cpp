#include "dpFoundation.h"
#include "dpNetwork.h"


void dpInitializeNetwork()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,2), &wsadata);
}

void dpFinalizeNetwork()
{
    WSACleanup();
}


dpTCPSocket::dpTCPSocket()
    : m_socket(INVALID_SOCKET)
    , m_needs_shutdown(true)
{
}

dpTCPSocket::dpTCPSocket(SOCKET s, bool needs_shutdown)
    : m_socket(s)
    , m_needs_shutdown(needs_shutdown)
{
}

dpTCPSocket::dpTCPSocket(const char *host, uint16_t port)
    : m_socket(INVALID_SOCKET)
    , m_needs_shutdown(true)
{
    open(host, port);
}

dpTCPSocket::~dpTCPSocket()
{
    if(m_needs_shutdown) {
        close();
    }
}

SOCKET dpTCPSocket::getHandle() const { return m_socket; }

bool dpTCPSocket::open(const char *host, uint16_t port)
{
    close();

    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        return false;
    }
    int opt = 1;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt));

    sockaddr_in sv_client; 
    sv_client.sin_family = AF_INET;
    sv_client.sin_addr.s_addr = inet_addr(host);
    sv_client.sin_port = htons(port);

    int ret;
    ret = connect( m_socket, (SOCKADDR*) &sv_client, sizeof(sv_client) );
    if ( ret == SOCKET_ERROR) {
        closesocket(m_socket);
        dpPrintError("Unable to connect to server: %ld\n", WSAGetLastError());
        return false;
    }

    return true;
}

void dpTCPSocket::close()
{
    if(m_socket!=INVALID_SOCKET) {
        shutdown(m_socket, SD_BOTH);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

int dpTCPSocket::read(void *buf, uint32_t buf_size)
{
    return recv(m_socket, (char*)buf, (int)buf_size, 0);
}

int dpTCPSocket::write(const void *data, uint32_t data_size)
{
    return send( m_socket, (const char*)data, (int)data_size, 0 );
}


// handler: false を返すと server 停止
bool dpRunTCPServer(uint16_t port, const std::function<bool (dpTCPSocket &client)> &handler)
{
    addrinfo hints;
    addrinfo *result = nullptr;
    int ret = 0;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    char port_str[64];
    sprintf(port_str, "%d", (int)port);
    ret = getaddrinfo(NULL, port_str, &hints, &result);
    if ( ret != 0 ) {
        dpPrintError("getaddrinfo failed with error: %d\n", ret);
        return false;
    }

    SOCKET server = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (server == INVALID_SOCKET) {
        dpPrintError("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        return false;
    }

    ret = bind( server, result->ai_addr, (int)result->ai_addrlen);
    if (ret == SOCKET_ERROR) {
        dpPrintError("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(server);
        return false;
    }
    freeaddrinfo(result);

    ret = listen(server, SOMAXCONN);
    if (ret == SOCKET_ERROR) {
        dpPrintError("listen failed with error: %d\n", WSAGetLastError());
        closesocket(server);
        return false;
    }

    for(;;) {
        sockaddr client_addr;
        int addrlen = sizeof(client_addr);
        SOCKET client_sock = accept(server, &client_addr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            dpPrintError("accept failed with error: %d\n", WSAGetLastError());
            break;
        }
        int opt = 1;
        setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt));
        dpTCPSocket client(client_sock);
        if(!handler(client)) {
            break;
        }
    }
    closesocket(server);
    return true;
}



dpProtocolSocket::dpProtocolSocket() {}
dpProtocolSocket::dpProtocolSocket(SOCKET s, bool needs_shutdown) : m_socket(s, needs_shutdown) {}
dpProtocolSocket::dpProtocolSocket(const char *host, uint16_t port) : m_socket(host, port) {}
dpProtocolSocket::~dpProtocolSocket() {}
bool dpProtocolSocket::open(const char *host, uint16_t port) { return m_socket.open(host, port); }
void dpProtocolSocket::close() { m_socket.close(); }

bool dpProtocolSocket::read(std::string &o_str)
{
    uint32_t len;
    if(m_socket.read(&len, sizeof(len))==sizeof(len)) {
        o_str.resize(len);
        uint32_t pos = 0;
        do {
            int r = m_socket.read(&o_str[pos], len-pos);
            if(r<0) { return false; }
            pos += r;
        } while(pos<len);
        return true;
    }
    return false;
}

bool dpProtocolSocket::write(const void *data, uint32_t data_size)
{
    if(m_socket.write(&data_size, sizeof(data_size))==sizeof(data_size)) {
        return m_socket.write(data, data_size)==data_size;
    }
    return false;
}
