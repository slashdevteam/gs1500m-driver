/*
 * This file has been adapted from ESP8266 mbed OS driver to support
 * GS1500M WiFi module over UART
 *
 * Due to huge differences between ESP and GS AT interfaces this driver
 * does not use ATParser from mbed os.
 *
 * Adaptations: Copyright (c) 2018 Slashdev SDG UG
 *
 * Original copyright:
 * Copyright (c) 2015 ARM Limited
 *
 * License:
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
 */

#include "GS1500M.h"

#include "gpio_api.h"
#include "mbed_wait_api.h"
extern "C" WEAK void resetWifi()
{
    {
        gpio_t gsPD;
        gpio_init_in(&gsPD, PTD5);
        while(!gpio_read(&gsPD))
        {};
    }

    {
        gpio_t gsPD;
        gpio_init_out(&gsPD, PTD5);
        gpio_write(&gsPD, 0);
    }

    {
        gpio_t gsPD;
        gpio_init_in(&gsPD, PTD5);
        wait(1);
    }
}

Packet::Packet(uint32_t _len)
  : len(_len), data(new char[_len]), offset(0)
{

}

Packet::~Packet()
{
    delete data;
}

const size_t MAX_OUTGOING_PACKET_SIZE = 1400;
const char HOST_APP_ESC_CHAR = 0x1B;
static const char BULKDATAIN[] = {HOST_APP_ESC_CHAR, 'Z'};
static const char DATASENDOK[] = {HOST_APP_ESC_CHAR, 'O'};

GS1500M::GS1500M(PinName tx,
                 PinName rx,
                 int baud)
    : parser(tx, rx, 115200),
      mode(0),
      disconnectedId(-1)
{
    parser.registerSequence(BULKDATAIN, callback(this, &GS1500M::_packet_handler));
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

bool GS1500M::startup()
{
    return reset()
        && parser.send("ATV1\n")
        && parser.recv("OK")
        && parser.send("ATE0\n")
        && parser.recv("OK")
        && parser.send("AT+WM=%d\n", mode)
        && parser.recv("OK")
        && parser.send("AT+BDATA=1\n")
        && parser.recv("OK")
        && parser.send("AT+WST=300,2000\n")
        && parser.recv("OK");
}

bool GS1500M::reset(void)
{
    resetWifi();
    for (int i = 0; i < 2; i++)
    {
        // if(parser.send("AT+RESET") // AT+RESET _does not work_
        if(parser.send("AT\n")
           && parser.recv("OK"))
        {
            return true;
        }
    }

    return false;
}

bool GS1500M::dhcp(bool enabled)
{
    return parser.send("AT+NDHCP=%d\n", enabled ? 1 : 0)
        && parser.recv("OK");
}

bool GS1500M::connect(const char* ap, const char* passPhrase, nsapi_security_t security)
{
    bool ret = false;
    if(0 == std::strncmp(ssid, ap, sizeof(ssid))
       && 0 == std::strncmp(pass, passPhrase, sizeof(pass)))
    {
        ret = parser.send("ATZ0\n")
               && parser.recv("OK")
               && parser.send("AT+WA=%s\n", ssid)
               && parser.recv("OK")
               && parser.send("AT+WRXPS=0\n")
               && parser.recv("OK");
    }
    else
    {
        bool securityOk = false;
        // allowed modes are NONE, WEP (Open only), WPA, WPA2, WPA_WPA2 (PPP not supported)
        switch(security)
        {
            case NSAPI_SECURITY_NONE:
                securityOk = true;
                break;
            case NSAPI_SECURITY_WEP: // WEP (Open only)
                securityOk = parser.send("AT+WAUTH=1\n") // 1 = WEP (Open only)
                             && parser.recv("OK")
                             && parser.send("AT+WWEP1=%s\n", passPhrase)
                             && parser.recv("OK");
                break;
            case NSAPI_SECURITY_WPA:  // intentional fall-through
            case NSAPI_SECURITY_WPA2: // intentional fall-through
            case NSAPI_SECURITY_WPA_WPA2:
                securityOk = parser.send("AT+WPAPSK=%s,%s\n", ap, passPhrase)
                             && parser.recv("OK");
                break;
            default:
                securityOk = false;
                break;
        }

        if(securityOk)
        {
            ret = parser.send("AT+WA=%s\n", ap)
               && parser.recv("OK")
               && parser.send("AT+WRXPS=0\n")
               && parser.recv("OK");
        }

        if(ret)
        {
            parser.send("AT&W0\n");
            parser.recv("OK");
            parser.send("AT&Y0\n");
            parser.recv("OK");
            std::strncpy(ssid, ap, sizeof(ssid));
            std::strncpy(pass, passPhrase, sizeof(pass));
        }
    }

    if(ret)
    {
        parser.send("AT+DGPIO=30,1\n");
    }
    return ret;
}

bool GS1500M::disconnect(void)
{
    parser.send("AT+DGPIO=30,0\n");
    return parser.send("ATH\n") && parser.recv("OK");
}

const char* GS1500M::getIPAddress(void)
{
    //@TODO: parse output
    if(!(parser.send("AT+NSTAT=?\n")
       && parser.recv("IP addr=")
       && parser.readTill(ipBuffer, sizeof(ipBuffer)-1, "\r")
       && parser.recv("OK")))
    {
        return 0;
    }

    return ipBuffer;
}

const char* GS1500M::getMACAddress(void)
{
    if(!(parser.send("AT+NSTAT=?\n")
        && parser.recv("MAC=")
        && parser.readTill(macBuffer, sizeof(macBuffer)-1, "\r")
        && parser.recv("OK")))
    {
        return 0;
    }

    return macBuffer;
}

const char* GS1500M::getGateway()
{
    if(!(parser.send("AT+NSTAT=?\n")
        && parser.recv("Gateway=")
        && parser.readTill(gatewayBuffer, sizeof(gatewayBuffer)-1, "\r")
        && parser.recv("OK")))
    {
        return 0;
    }

    return gatewayBuffer;
}

const char* GS1500M::getNetmask()
{
    if(!(parser.send("AT+NSTAT=?\n")
        && parser.recv("SubNet=")
        && parser.readTill(netmaskBuffer, sizeof(netmaskBuffer)-1, "\r")
        && parser.recv("OK")))
    {
        return 0;
    }

    return netmaskBuffer;
}

int GS1500M::dnslookup(const char* name, char* address)
{
    return parser.send("AT+DNSLOOKUP=%s\n", name)
          && parser.recv("IP:")
          && parser.readTill(address, 15, "\r")
          && parser.recv("OK");
}

int8_t GS1500M::getRSSI()
{
    char rssiBuffer[5];
    int8_t rssi = 0;

    if(!(parser.send("AT+NSTAT=?\n")
        && parser.recv("RSSI=")
        && parser.readTill(rssiBuffer, sizeof(rssiBuffer)-1, "\r")
        && parser.recv("OK")))
    {
        return 0;
    }

    sscanf(rssiBuffer, "%d", reinterpret_cast<int*>(&rssi));
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

    if(!parser.send("AT+WS\n"))
    {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    while(recv_ap(&ap))
    {
        if(cnt < limit)
        {
            res[cnt] = WiFiAccessPoint(ap);
        }

        cnt++;
        if(limit != 0 && cnt >= limit)
        {
            break;
        }
    }

    return cnt;
}

bool GS1500M::open(const char* type, int& id, const char* addr, int port)
{
    parser.send("AT+NC%s=%s,%d\n", type, addr, port);
    parser.recv("CONNECT ");
    char idraw[2]; // CID is 1 hex digit + null/\n
    parser.readTill(idraw, sizeof(idraw), "\n");
    sscanf(idraw, "%1x", &id);
    parser.recv("OK");
    // Apparently, against GS documentation, SO_KEEPALIVE (param 8)
    // must be enabled for "default on" TCP_KEEPALIVE to really work!
    parser.send("AT+SETSOCKOPT=%x,65535,8,1,4\n", id);
    return parser.recv("OK");
}

bool GS1500M::bind(const char* type, int& id, int port)
{
    parser.send("AT+NS%s=%d\n", type, port);
    parser.recv("CONNECT ");
    char idraw[2]; // CID is 1 hex digit + null/\n
    parser.readTill(idraw, sizeof(idraw), "\n");
    sscanf(idraw, "%1x", &id);
    return parser.recv("OK");
}

size_t GS1500M::send(int id, const void *data, uint32_t amount)
{
    size_t amoutToSend = amount;
    const char* charData = reinterpret_cast<const char*>(data);

    while(amoutToSend > MAX_OUTGOING_PACKET_SIZE)
    {
        size_t part = sendPart(id, charData, MAX_OUTGOING_PACKET_SIZE);
        if(part != 0)
        {
            amoutToSend -= MAX_OUTGOING_PACKET_SIZE;
            charData += MAX_OUTGOING_PACKET_SIZE;
        }
        else
        {
            return 0;
        }
    }

    if(sendPart(id, charData, amoutToSend) != 0)
    {
        return amount;
    }

    return 0;
}

size_t GS1500M::sendPart(int id, const char* data, uint32_t amount)
{
    if(parser.send("%c%c%.1x%.4d", HOST_APP_ESC_CHAR, 'Z', id, amount)
       && parser.write(data, amount))
    {
        if(stackCallback)
        {
            stackCallback();
        }
        if(parser.recv(DATASENDOK))
        {
            return amount;
        }
    }

    return 0;
}

void GS1500M::_packet_handler()
{
    int id = -1;
    int amount1000 = 0;
    int amount100 = 0;
    int amount10 = 0;
    int amount1 = 0;
    int amount = 0;
    char idamount[6] = {0}; // <1 hex for CID><4 digits for len>+\0
    if(parser.readData(idamount, 5) == 0)
    {
        return;
    }

    sscanf(reinterpret_cast<char*>(idamount), "%1x%1d%1d%1d%1d", &id, &amount1000, &amount100, &amount10, &amount1);

    amount = 1000 * amount1000 + 100 * amount100 + 10 * amount10 + amount1;

    Packet* incoming = new Packet(amount);
    size_t readAmount = parser.readData(incoming->data, amount);

    if(readAmount == 0 || id == -1)
    {
        delete incoming;
        return;
    }

    socketQueue[id].put(incoming);
    if(stackCallback)
    {
        stackCallback();
    }
}

bool GS1500M::accept(int id, int& clientId, char* addr)
{
    //@TODO: accept should be blocking
    int localServSocketId;
    parser.recv("CONNECT ");
    char ids[5];
    parser.readTill(ids, 5, "\n");
    sscanf(ids, "%x %d", &localServSocketId, &clientId);
    return (localServSocketId == id);
}


int32_t GS1500M::recv(int id, void *data, uint32_t amount)
{
    osEvent evt = socketQueue[id].get(10);
    if(osEventMessage == evt.status)
    {
        Packet *q = reinterpret_cast<Packet*>(evt.value.p);
        if(q->len <= amount)
        {
            // Return and remove full packet
            memcpy(data, q->data + q->offset, q->len);
            uint32_t len = q->len;
            delete q;
            return len;
        }
        else
        {
            // return only partial packet and put the event back in queue with high prio
            // to be again taken from queue on next recv
            memcpy(data, q->data + q->offset, amount);

            // update length and data pointer
            q->offset += amount;
            q->len -= amount;
            socketQueue[id].put(q, 0, 255);
            return amount;
        }
    }
    else
    {
        return -1;
    }
}

bool GS1500M::close(int id)
{
    if(parser.send("AT+NCLOSE=%x\n", id)
       && parser.recv("OK"))
    {
        return true;
    }
    //@TODO: check socket queue for any remaining data
    return false;
}

void GS1500M::setTimeout(uint32_t _timeoutMs)
{
    parser.setTimeout(_timeoutMs);
}

bool GS1500M::readable()
{
    return parser.readable();
}

bool GS1500M::writeable()
{
    return parser.writeable();
}

void GS1500M::attach(Callback<void()> func)
{
    stackCallback = func;
}

bool GS1500M::recv_ap(nsapi_wifi_ap_t* ap)
{
    //@TODO: Parse GS1500M output
    bool ret = false;
    return ret;
}
