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

#pragma once

#include "mbed.h"
#include "GS1500M.h"
#include <functional>

using SocketSend = std::function<int(void* handle, const void* data, unsigned size)>;
using SocketCallback = std::function<void(const void* data, unsigned size)>;


class GS1500MInterface : public NetworkStack, public WiFiInterface
{
public:
    GS1500MInterface(PinName tx, PinName rx, int baud);
    virtual ~GS1500MInterface() = default;

    // Interface implementations
    virtual int set_credentials(const char *ssid,
                                const char *pass,
                                nsapi_security_t security);
    virtual int set_channel(uint8_t channel);

    virtual int connect(const char* ssid,
                        const char* pass,
                        nsapi_security_t security,
                        uint8_t channel);

    virtual int connect();
    virtual int disconnect();

    virtual const char* get_ip_address();
    virtual const char* get_mac_address();
    virtual const char* get_gateway();
    virtual const char* get_netmask();

    virtual int8_t get_rssi();

    virtual int scan(WiFiAccessPoint* res, unsigned count);

    // override NetworkStack to use GS1500M DNS
    nsapi_error_t gethostbyname(const char *name, SocketAddress *address, nsapi_version_t version = NSAPI_UNSPEC);


protected:

    virtual int socket_open(void** handle, nsapi_protocol_t proto);
    virtual int socket_close(void* handle);
    virtual int socket_bind(void* handle, const SocketAddress& address);
    virtual int socket_listen(void* handle, int backlog);
    virtual int socket_connect(void* handle, const SocketAddress& address);
    virtual int socket_accept(void* handle, void** socket, SocketAddress* address);
    virtual int socket_send(void* handle, const void* data, unsigned size);
    virtual int socket_recv(void* handle, void* data, unsigned size);
    virtual int socket_sendto(void* handle, const SocketAddress& address, const void* data, unsigned size);
    virtual int socket_recvfrom(void* handle, SocketAddress* address, void* buffer, unsigned size);
    virtual void socket_attach(void* handle, void (*callback)(void*), void* data);

    int udp_socket_send(void *handle, const void *data, unsigned size);
    int tcp_socket_send(void *handle, const void *data, unsigned size);
    int init_local_socket(void **handle, nsapi_protocol_t proto, int _idgs);

    virtual NetworkStack *get_stack()
    {
        return this;
    }

private:
    GS1500M gsat;
    bool _ids[GS1500M_SOCKET_COUNT];
    SocketSend socketSend[2];

    char ap_ssid[33]; /* 32 is what 802.11 defines as longest possible name; +1 for the \0 */
    nsapi_security_t ap_sec;
    uint8_t ap_ch;
    char ap_pass[64]; /* The longest allowed passphrase */

    void event();

    struct
    {
        void (*callback)(void *);
        void *data;
    } _cbs[GS1500M_SOCKET_COUNT];
};
