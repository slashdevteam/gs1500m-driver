/* GS1500M Example
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
 */

#include "GS1500M.h"

// mbed target header should provide resetWifi() function
extern "C"
{
    extern void resetWifi();
}

const char HOST_APP_ESC_CHAR = 0x1B;
static const char BULKDATAIN[] = {0x1b};
static const char DATOK[] = {0xa, 0xd, 0xa, 0x1b, 0x4f};

GS1500M::GS1500M(PinName tx,
                 PinName rx,
                 int baud,
                 bool debug)
    : serial(tx, rx, 1024),
      parser(serial),
      mode(0),
      packets(0),
      packetsEnd(&packets),
      disconnectedId(-1)
{
    serial.baud(baud);
    parser.debugOn(debug);
    parser.oob(BULKDATAIN, this, &GS1500M::_packet_handler);
}

bool GS1500M::setMode(int _mode)
{
    bool ret = false;
    if(mode > 0 && mode < 3)
    {
         mode = _mode;
         ret = true;
    }

    return ret;
}

void GS1500M::aterror()
{
    char buffer[256] = {};
    parser.recv(" %10s", &buffer);

    return;
}

void GS1500M::socketDisconnected()
{
    parser.recv("%d", &disconnectedId);
    return;
}

bool GS1500M::startup()
{
    bool success = reset()
        && parser.send("ATV1")
        && parser.recv("OK")
        && parser.send("ATE0")
        && parser.recv("OK")
        && parser.send("AT+WM=%d", mode)
        && parser.recv("OK")
        && parser.send("AT+BDATA=1")
        && parser.recv("OK")
        ;

    parser.send("AT+NSTAT=?");
    parser.recv("OK");

    return success;
}

bool GS1500M::reset(void)
{
    resetWifi();
    for (int i = 0; i < 2; i++)
    {
        // if (parser.send("AT+RESET") // AT+RESET _does not work_
        if (parser.send("AT")
            && parser.recv("OK"))
        {
            return true;
        }
    }

    return false;
}

bool GS1500M::dhcp(bool enabled)
{
    return parser.send("AT+NDHCP=%d", enabled ? 1 : 0)
        && parser.recv("OK");
}

bool GS1500M::connect(const char *ap, const char *passPhrase)
{
    bool ret = parser.send("AT+WPAPSK=%s,%s", ap, passPhrase)
                && parser.recv("OK");

    for (uint32_t i = 0; i < 2; i++)
    {
        ret = parser.send("AT+WA=%s", ap)
                && parser.recv("OK");
        if(ret)
        {
            break;
        }
    }
    return ret;
}

bool GS1500M::disconnect(void)
{
    return parser.send("ATH") && parser.recv("OK");
}

const char *GS1500M::getIPAddress(void)
{
    if (!(parser.send("AT+NSTAT=?")
        && parser.recv("OK")))
    {
        return 0;
    }

    return ipBuffer;
}

const char *GS1500M::getMACAddress(void)
{
    if (!(parser.send("AT+NSTAT=?")
        && parser.recv("OK")))
    {
        return 0;
    }

    return macBuffer;
}

const char *GS1500M::getGateway()
{
    if (!(parser.send("AT+NSTAT=?")
        && parser.recv("OK")))
    {
        return 0;
    }

    return gatewayBuffer;
}

const char *GS1500M::getNetmask()
{
    if (!(parser.send("AT+NSTAT=?")
        && parser.recv("OK")))
    {
        return 0;
    }

    return netmaskBuffer;
}

int GS1500M::dnslookup(const char *name, char* address)
{
    parser.send("AT+DNSLOOKUP=%s", name);
    int ret = parser.scanf("\nIP:%17s\n", address);
    return parser.recv("OK");
}

int8_t GS1500M::getRSSI()
{
    int8_t rssi = 0;

    //@TODO: parse NSTAT output
    if (!(parser.send("AT+NSTAT=?")
        && parser.recv("OK")))
    {
        return 0;
    }

    return rssi;
}

bool GS1500M::isConnected(void)
{
    return getIPAddress() != 0;
}

int GS1500M::scan(WiFiAccessPoint *res, unsigned limit)
{
    unsigned cnt = 0;
    nsapi_wifi_ap_t ap;

    if (!parser.send("AT+WS"))
    {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    while (recv_ap(&ap))
    {
        if (cnt < limit)
        {
            res[cnt] = WiFiAccessPoint(ap);
        }

        cnt++;
        if (limit != 0 && cnt >= limit)
        {
            break;
        }
    }

    return cnt;
}

bool GS1500M::open(const char *type, int& id, const char* addr, int port)
{
    parser.send("AT+NC%s=%s,%d", type, addr, port);
    parser.recv("CONNECT %d", &id);
    return parser.recv("OK");
}

bool GS1500M::bind(const char *type, int& id, int port)
{
    parser.send("AT+NS%s=%d", type, port);
    parser.recv("CONNECT %d", &id);
    return parser.recv("OK");
}

bool GS1500M::sendTcp(int id, const void *data, uint32_t amount)
{
    for (unsigned i = 0; i < 1; i++)
    {
        if (parser.sendNoNewline("%c%c%.1d%.4d", HOST_APP_ESC_CHAR, 'Z', id, amount)
            && parser.write((char*)data, (int)amount)
            && parser.send("%c%c", HOST_APP_ESC_CHAR, 'E'))
        {
            return true;
        }
    }

    return false;
}

bool GS1500M::sendUdp(int id, const char* addr, int port, const void *data, uint32_t amount)
{

    for (unsigned i = 0; i < 2; i++)
    {
        if (parser.sendNoNewline("%c%c%.1d%.4d", HOST_APP_ESC_CHAR, 'Z', id, amount)
            && parser.write((char*)data, (int)amount)
            && parser.send("%c%c", HOST_APP_ESC_CHAR, 'E'))
        {
            return true;
        }
    }

    return false;
}
void GS1500M::_oobconnect_handler()
{
    int id;
    int client;
    char* address[100];

    if (!parser.recv("%1d %1d %1d%1d%1d", &id, &client, &address))
    {
        return;
    }
}

void GS1500M::_packet_handler()
{
    int id;
    uint32_t amount1000;
    uint32_t amount100;
    uint32_t amount10;
    uint32_t amount1;
    uint32_t amount;

    // parse out the packet
    if (!parser.recv("Z%1d%1d%1d%1d%1d", &id, &amount1000, &amount100, &amount10, &amount1))
    {
        return;
    }

    amount = 1000 * amount1000 + 100 * amount100 + 10 * amount10 + amount1;
    struct packet *packet = (struct packet*)malloc(sizeof(struct packet) + amount);
    if (!packet)
    {
        return;
    }

    packet->id = id;
    packet->len = amount;
    packet->next = 0;

    if (!(parser.read((char*)(packet + 1), amount)))
    {
        free(packet);
        return;
    }

    // append to packet list
    *packetsEnd = packet;
    packetsEnd = &packet->next;
}

bool GS1500M::accept(int id, int& clientId, char* addr)
{
    //@TODO: accept should be blocking
    int localServSocketId;
    parser.recv("CONNECT %1d %1d", &localServSocketId, &clientId, addr);
    return (localServSocketId == id);
}


int32_t GS1500M::recv(int id, void *data, uint32_t amount)
{
    while (true)
    {
        // check if any packets are ready for us
        for (struct packet **p = &packets; *p; p = &(*p)->next)
        {
            if ((*p)->id == id) {
                struct packet *q = *p;

                if (q->len <= amount)
                { // Return and remove full packet
                    uint8_t* realDataPtr = reinterpret_cast<uint8_t*>(q+1);
                    memcpy(data, realDataPtr, q->len);

                    if (packetsEnd == &(*p)->next)
                    {
                        packetsEnd = p;
                    }
                    *p = (*p)->next;

                    uint32_t len = q->len;
                    free(q);
                    return len;
                }
                else
                { // return only partial packet
                    memcpy(data, q+1, amount);

                    q->len -= amount;
                    memmove(q+1, (uint8_t*)(q+1) + amount, q->len);

                    return amount;
                }
            }
        }

        // Wait for inbound packet
        if (!parser.recv("\033O\033O"))
        {
            return -1;
        }
    }
}

bool GS1500M::close(int id)
{
    //May take a second try if device is busy
    for (unsigned i = 0; i < 2; i++)
    {
        if (parser.send("AT+NCLOSE=%d", id)
            && parser.recv("OK"))
        {
            return true;
        }
    }

    return false;
}

void GS1500M::setTimeout(uint32_t timeout_ms)
{
    parser.setTimeout(timeout_ms);
}

bool GS1500M::readable()
{
    return serial.readable();
}

bool GS1500M::writeable()
{
    return serial.writeable();
}

void GS1500M::attach(Callback<void()> func)
{
    serial.attach(func);
}

bool GS1500M::recv_ap(nsapi_wifi_ap_t *ap)
{
    int sec;
    char type[32];
    //@TODO: Parse GS1500M output - code below is from original esp8266 driver
    bool ret = parser.recv("\"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx\",\"%32[^\"]\", %d,\"%32[^\"]\",%d,%hhd",
                            &ap->bssid[0], &ap->bssid[1], &ap->bssid[2], &ap->bssid[3], &ap->bssid[4], &ap->bssid[5],
                            ap->ssid,
                            &ap->channel,
                            &type,
                            &ap->rssi,
                            &sec);

    ap->security = sec < 5 ? (nsapi_security_t)sec : NSAPI_SECURITY_UNKNOWN;

    return ret;
}
