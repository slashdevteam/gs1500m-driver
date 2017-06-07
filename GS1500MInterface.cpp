/* GS1500M implementation of NetworkInterfaceAPI
 * Copyright (c) 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * This file has been adapted from ESP8266 mbed OS driver to support
 * GS1500M WiFi module over UART
 *
 * Due to huge differences between ESP and GS AT interfaces this driver
 * does not use ATParser from mbed.
 */

#include <string.h>
#include "GS1500MInterface.h"

const uint32_t GS1500M_CONNECT_TIMEOUT = 15000;
const uint32_t GS1500M_SEND_TIMEOUT    = 500;
const uint32_t GS1500M_RECV_TIMEOUT    = 500;
const uint32_t GS1500M_MISC_TIMEOUT    = 500;

using namespace std::placeholders;

GS1500MInterface::GS1500MInterface(PinName tx,
                                   PinName rx,
                                   int baud)
    : gsat(tx, rx, baud)
{
    memset(_ids, 0, sizeof(_ids));
    memset(_cbs, 0, sizeof(_cbs));
    socketSend[0] = std::bind(&GS1500MInterface::tcp_socket_send, this, _1, _2, _3);
    socketSend[1] = std::bind(&GS1500MInterface::udp_socket_send, this, _1, _2, _3);
}

int GS1500MInterface::connect(const char *ssid,
                              const char *pass,
                              nsapi_security_t security,
                              uint8_t channel)
{
    if(channel != 0)
    {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    set_credentials(ssid, pass, security);
    return connect();
}

int GS1500MInterface::connect()
{
    gsat.setTimeout(GS1500M_CONNECT_TIMEOUT);

    if(!gsat.startup())
    {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    if(!gsat.dhcp(true))
    {
        return NSAPI_ERROR_DHCP_FAILURE;
    }

    if(!gsat.connect(ap_ssid, ap_pass))
    {
        return NSAPI_ERROR_NO_CONNECTION;
    }

    if(!gsat.getIPAddress())
    {
        return NSAPI_ERROR_DHCP_FAILURE;
    }

    return NSAPI_ERROR_OK;
}

int GS1500MInterface::set_credentials(const char *ssid, const char *pass, nsapi_security_t security)
{
    memset(ap_ssid, 0, sizeof(ap_ssid));
    strncpy(ap_ssid, ssid, sizeof(ap_ssid));

    memset(ap_pass, 0, sizeof(ap_pass));
    strncpy(ap_pass, pass, sizeof(ap_pass));

    ap_sec = security;

    return 0;
}

nsapi_error_t GS1500MInterface::gethostbyname(const char *name, SocketAddress *address, nsapi_version_t version)
{
    gsat.setTimeout(GS1500M_MISC_TIMEOUT);
    char ipBuffer[16] = {0};
    int ret = gsat.dnslookup(name, ipBuffer);
    address->set_ip_address(ipBuffer);
    return (ret != 1);
}

int GS1500MInterface::set_channel(uint8_t channel)
{
    return NSAPI_ERROR_UNSUPPORTED;
}


int GS1500MInterface::disconnect()
{
    gsat.setTimeout(GS1500M_MISC_TIMEOUT);

    if (!gsat.disconnect()) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    return NSAPI_ERROR_OK;
}

const char *GS1500MInterface::get_ip_address()
{
    return gsat.getIPAddress();
}

const char *GS1500MInterface::get_mac_address()
{
    return gsat.getMACAddress();
}

const char *GS1500MInterface::get_gateway()
{
    return gsat.getGateway();
}

const char *GS1500MInterface::get_netmask()
{
    return gsat.getNetmask();
}

int8_t GS1500MInterface::get_rssi()
{
    return gsat.getRSSI();
}

int GS1500MInterface::scan(WiFiAccessPoint *res, unsigned count)
{
    return gsat.scan(res, count);
}

struct GS1500M_socket
{
    int id;
    int idgs;
    nsapi_protocol_t proto;
    bool connected;
    SocketAddress addr;
};

int GS1500MInterface::socket_open(void **handle, nsapi_protocol_t proto)
{
    return init_local_socket(handle, proto, 0);
}

int GS1500MInterface::init_local_socket(void **handle, nsapi_protocol_t proto, int _idgs)
{
    int id = -1;

    for (int i = 0; i < GS1500M_SOCKET_COUNT; i++)
    {
        if (!_ids[i])
        {
            id = i;
            _ids[i] = true;
            break;
        }
    }

    if (id == -1)
    {
        return NSAPI_ERROR_NO_SOCKET;
    }

    struct GS1500M_socket *socket = new struct GS1500M_socket;
    if (!socket)
    {
        return NSAPI_ERROR_NO_SOCKET;
    }

    socket->id = id;
    socket->idgs = _idgs;
    socket->proto = proto;
    socket->connected = false;
    *handle = socket;
    return 0;
}

int GS1500MInterface::socket_close(void *handle)
{
    struct GS1500M_socket *socket = (struct GS1500M_socket *)handle;
    int err = 0;
    gsat.setTimeout(GS1500M_MISC_TIMEOUT);

    if (!gsat.close(socket->id))
    {
        err = NSAPI_ERROR_DEVICE_ERROR;
    }

    _ids[socket->id] = false;
    delete socket;
    return err;
}

int GS1500MInterface::socket_bind(void *handle, const SocketAddress &addr)
{
    struct GS1500M_socket *socket = (struct GS1500M_socket *)handle;
    gsat.setTimeout(GS1500M_MISC_TIMEOUT);

    const char *proto = (socket->proto == NSAPI_UDP) ? "UDP" : "TCP";
    if(!gsat.bind(proto, socket->idgs, addr.get_port()))
    {
        return NSAPI_ERROR_DEVICE_ERROR;
    }
    return 0;
}

int GS1500MInterface::socket_listen(void *handle, int backlog)
{
    return 0;
}

int GS1500MInterface::socket_connect(void *handle, const SocketAddress &addr)
{
    struct GS1500M_socket *socket = (struct GS1500M_socket *)handle;
    gsat.setTimeout(GS1500M_MISC_TIMEOUT);

    const char *proto = (socket->proto == NSAPI_UDP) ? "UDP" : "TCP";
    if (!gsat.open(proto, socket->idgs, addr.get_ip_address(), addr.get_port())) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    socket->connected = true;
    socket->addr = addr;
    return 0;
}

int GS1500MInterface::socket_accept(void *server, void **socket, SocketAddress *addr)
{
    struct GS1500M_socket* servSocket = (struct GS1500M_socket *)server;

    char clientAddress[100] = {};
    int clientSocketId;
    gsat.setTimeout(6553);
    if (!gsat.accept(servSocket->idgs, clientSocketId, clientAddress))
    {
        return NSAPI_ERROR_WOULD_BLOCK;
    }

    *addr = SocketAddress(clientAddress);
    struct GS1500M_socket* clientSocket = (struct GS1500M_socket *)*socket;
    init_local_socket(socket, servSocket->proto, clientSocketId);
    clientSocket->addr = *addr;
    return 0;
}

int GS1500MInterface::socket_send(void *handle, const void *data, unsigned size)
{
    struct GS1500M_socket *socket = (struct GS1500M_socket *)handle;

    gsat.setTimeout(GS1500M_SEND_TIMEOUT);

    if (!socketSend[socket->proto](handle, data, size))
    {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    return size;
}

int GS1500MInterface::udp_socket_send(void *handle, const void *data, unsigned size)
{
    struct GS1500M_socket *socket = (struct GS1500M_socket *)handle;
    gsat.setTimeout(GS1500M_SEND_TIMEOUT);

    return gsat.sendUdp(socket->idgs, socket->addr.get_ip_address(), socket->addr.get_port(), data, size);
}

int GS1500MInterface::tcp_socket_send(void *handle, const void *data, unsigned size)
{
    struct GS1500M_socket *socket = (struct GS1500M_socket *)handle;
    gsat.setTimeout(GS1500M_SEND_TIMEOUT);

    return gsat.sendTcp(socket->idgs, data, size);
}

int GS1500MInterface::socket_recv(void *handle, void *data, unsigned size)
{
    struct GS1500M_socket *socket = (struct GS1500M_socket *)handle;
    gsat.setTimeout(GS1500M_RECV_TIMEOUT);

    int32_t recv = gsat.recv(socket->idgs, data, size);
    if (recv < 0)
    {
        return NSAPI_ERROR_WOULD_BLOCK;
    }

    return recv;
}

int GS1500MInterface::socket_sendto(void *handle, const SocketAddress &addr, const void *data, unsigned size)
{
    struct GS1500M_socket *socket = (struct GS1500M_socket *)handle;
    return socket_send(socket, data, size);
}

int GS1500MInterface::socket_recvfrom(void *handle, SocketAddress *addr, void *data, unsigned size)
{
    struct GS1500M_socket *socket = (struct GS1500M_socket *)handle;
    int ret = socket_recv(socket, data, size);
    if (ret >= 0 && addr)
    {
        *addr = socket->addr;
    }

    return ret;
}

void GS1500MInterface::socket_attach(void *handle, void (*callback)(void *), void *data)
{
    struct GS1500M_socket *socket = (struct GS1500M_socket *)handle;
    _cbs[socket->id].callback = callback;
    _cbs[socket->id].data = data;
}

void GS1500MInterface::event()
{
    for (int i = 0; i < GS1500M_SOCKET_COUNT; i++)
    {
        if (_cbs[i].callback)
        {
            _cbs[i].callback(_cbs[i].data);
        }
    }
}
