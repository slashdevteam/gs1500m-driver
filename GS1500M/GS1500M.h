/* GS1500MInterface Example
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

#include "bufferedat.h"
#include "WiFiAccessPoint.h"
#include "Queue.h"

constexpr int GS1500M_SOCKET_COUNT = 16;

struct Packet
{
    Packet(uint32_t _len, char* _data)
        : len(_len), data(_data), offset(0)
        {};
    uint32_t len;
    char* data;
    uint32_t offset;
};

class GS1500M
{
public:
    GS1500M(PinName tx,
            PinName rx,
            int baud);

    void aterror();

    bool setMode(int _mode);
    bool startup();
    bool reset();
    bool dhcp(bool enabled);
    bool connect(const char* ap, const char* passPhrase);
    bool disconnect();
    bool isConnected();

    const char* getIPAddress();
    const char* getMACAddress();
    const char* getGateway();
    const char* getNetmask();
    int8_t getRSSI();

    int dnslookup(const char *name, char* address);

    int scan(WiFiAccessPoint* res, unsigned limit);

    bool open(const char* type, int& id, const char* addr, int port);
    bool bind(const char* type, int& id, int port);
    bool send(int id, const void* data, uint32_t amount);
    int32_t recv(int id, void* data, uint32_t amount);
    bool accept(int id, int& clientId, char* addr);
    bool close(int id);
    void setTimeout(uint32_t timeout_ms);
    bool readable();
    bool writeable();
    void attach(Callback<void()> func);
    template <typename T, typename M>
    void attach(T *obj, M method)
    {
        attach(Callback<void()>(obj, method));
    }

private:
    void _packet_handler();
    void _oobconnect_handler();
    bool recv_ap(nsapi_wifi_ap_t *ap);
    void socketDisconnected();

private:
    BufferedAT parser;
    int mode;
    int disconnectedId;

    char ipBuffer[16];
    char gatewayBuffer[16];
    char netmaskBuffer[16];
    char macBuffer[18];
    Callback<void()> stackCallback;
    rtos::Queue<Packet, 5> socketQueue[GS1500M_SOCKET_COUNT];
};
