/* wiimote.h - v1.0 - public domain Wii Remote over bluetooth - Hugo Coto Florez 2026

   To use this library, do this in *one* C file:
      #define INCLUDE_WIIMOTE_IMPLEMENTATION
      #include "wiimote.h"

   In other source files, just include the header normally:
      #include "wiimote.h"

   Link with -lbluetooth.

TABLE OF CONTENTS

  Table of Contents
  License
  Documentation
  Notes
  Credits

LICENSE

  Placed in the public domain and also MIT licensed.
  See end of file for detailed license information.

DOCUMENTATION

  The library talks to a Wii Remote directly over two L2CAP channels:
  control (PSM 0x11) and data (PSM 0x13). It does not need the remote to
  be paired; it must be discoverable (press 1+2 or the red sync button)
  while scanning and connecting.

  Typical use:

    wr_found found[WR_SCAN_MAX];
    int n = wr_scan(found, WR_SCAN_MAX);

    wr_socks socks;
    wr_init(found[0].mac, &socks);

    wr_state st;
    wr_state_init(&st);
    while (wr_read(socks, &st) >= 0) { ... st.keys, st.ax, st.ay ... }

    wr_fini(socks);

  Functions:

    wr_scan
      Runs a classic inquiry (~WR_SCAN_LEN * 1.28 s) and keeps what looks
      like a Wiimote: name starting with "Nintendo RVL-CNT-01" (this also
      matches the Wii Remote Plus, "-TR"), or, when the name request
      fails, the Wiimote class of device. Up to WR_INQUIRY_MAX nearby
      devices are checked, and up to max matches are stored in found[].
      Returns how many were stored, -1 on error.

    wr_init
      Connects to the remote at mac ("XX:XX:XX:XX:XX:XX"), sets reporting
      mode 0x31 (buttons + accelerometer, continuous), turns all the LEDs
      off (they blink while the remote is waiting for a host) and requests
      the accelerometer calibration into socks. Returns 0 on success, -1 on
      failure (both sockets are closed and set to -1).

    wr_fini
      Closes both channels.

    wr_set_report_mode
      Sets reporting mode 0x31 again. wr_read() already does this when the
      remote sends a status report.

    wr_set_leds
      Lights the LEDs given as WR_LED_1 | ... | WR_LED_4 (0 turns them all
      off). Animations are up to the caller: call it once per frame, e.g.
      from its own thread while another one is in wr_read().

    wr_state_init
      Fills a wr_state with the default calibration, used until the real
      one arrives (or if reading it fails).

    wr_read
      Reads one report into st. Returns -1 if the connection is closed,
      0 if it carried nothing for us (st untouched), 1 if st was updated.
      Blocks until a report arrives.

    wr_accel_g
      Converts an accelerometer reading to g using the calibration in st.
      axis is 0 for X, 1 for Y.

  Keys (wr_key) are bit flags in wr_state.keys, so several can be held at
  once: WR_KEY_A, WR_KEY_B, WR_KEY_MINUS, WR_KEY_PLUS, WR_KEY_HOME,
  WR_KEY_UP, WR_KEY_DOWN, WR_KEY_LEFT, WR_KEY_RIGHT, WR_KEY_1, WR_KEY_2.

NOTES

  * wr_scan may need root or CAP_NET_RAW for the name requests. If they
    all fail, only the class of device is used to match.

  * The class of device values (0x002504 Wii Remote, 0x000508 Wii Remote
    Plus) are not verified against real hardware.

  * Only the accelerometer X and Y axes are decoded.

CREDITS

  Hugo Coto Florez -- design and implementation

*/

#ifndef WIIMOTE_H_
#define WIIMOTE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

// bit flags, so several keys can be held at once
typedef enum wr_key {
        WR_KEY_NONE  = 0,
        WR_KEY_A     = 1 << 0,
        WR_KEY_B     = 1 << 1,
        WR_KEY_MINUS = 1 << 2,
        WR_KEY_PLUS  = 1 << 3,
        WR_KEY_HOME  = 1 << 4,
        WR_KEY_UP    = 1 << 5,
        WR_KEY_DOWN  = 1 << 6,
        WR_KEY_LEFT  = 1 << 7,
        WR_KEY_RIGHT = 1 << 8,
        WR_KEY_1     = 1 << 9,
        WR_KEY_2     = 1 << 10,
} wr_key;

#define WR_LED_1 0x10
#define WR_LED_2 0x20
#define WR_LED_3 0x40
#define WR_LED_4 0x80

#define WR_SCAN_MAX 16
// devices asked from the inquiry, Wiimotes or not (filtered afterwards)
#define WR_INQUIRY_MAX 255
// inquiry length, in units of 1.28 s
#define WR_SCAN_LEN 8

typedef int wr_sock; // an L2CAP socket: the control or data channel

// a connection: every call after wr_init() takes it and picks the channel
typedef struct wr_socks {
        wr_sock ctl;  // control channel, PSM 0x11
        wr_sock data; // data channel, PSM 0x13: reports in and out
} wr_socks;

typedef struct wr_found {
        char mac[18];
        char name[248];
} wr_found;

typedef struct wr_state {
        unsigned keys;          // wr_key flags currently held
        unsigned char ax, ay;   // accelerometer X and Y, upper 8 bits
        unsigned char zero[2];  // X and Y reading at 0 g
        unsigned char one_g[2]; // X and Y reading at 1 g
} wr_state;

/* API */

int wr_scan(wr_found *found, int max);                // Find discoverable Wiimotes
int wr_init(const char *mac, wr_socks *socks);        // Connect and start reporting
void wr_fini(wr_socks socks);                         // Close the connection
int wr_set_report_mode(wr_socks socks);               // Buttons + accelerometer, continuous
void wr_set_leds(wr_socks socks, unsigned char leds); // WR_LED_* flags
void wr_state_init(wr_state *st);                     // Default calibration
int wr_read(wr_socks socks, wr_state *st);            // -1 closed, 0 nothing, 1 st updated
float wr_accel_g(const wr_state *st, int axis);       // Reading in g, axis 0 X, 1 Y

#if defined(INCLUDE_WIIMOTE_IMPLEMENTATION)

// accelerometer calibration lives in the EEPROM, the answer is a 0x21 report
#define WR_ACCEL_CAL_ADDR 0x0016

// used until the calibration arrives (or if reading it fails)
#define WR_ACCEL_ZERO_DEFAULT 0x80
#define WR_ACCEL_ONE_G_DEFAULT 0x9A

// output reports go on the data channel, 0xA2 header
static int
wr_write(wr_sock data, const unsigned char *report, size_t len, const char *what)
{
        if (write(data, report, len) < 0) {
                perror(what);
                return -1;
        }
        return 0;
}

// dev_class is little endian: minor, major, services
static int
wr_is_wiimote_class(const uint8_t cod[3])
{
        uint32_t c = cod[0] | cod[1] << 8 | cod[2] << 16;
        return c == 0x002504     // Wii Remote
               || c == 0x000508; // Wii Remote Plus (RVL-CNT-01-TR)
}

int
wr_scan(wr_found *found, int max)
{
        int dev_id = hci_get_route(NULL);
        if (dev_id < 0) {
                perror("no bluetooth adapter");
                return -1;
        }
        int sock = hci_open_dev(dev_id);
        if (sock < 0) {
                perror("hci_open_dev");
                return -1;
        }

        inquiry_info *ii = NULL;
        int n            = hci_inquiry(dev_id, WR_SCAN_LEN, WR_INQUIRY_MAX, NULL, &ii, IREQ_CACHE_FLUSH);
        if (n < 0) {
                perror("hci_inquiry");
                close(sock);
                return -1;
        }

        int count = 0;
        for (int i = 0; i < n && count < max; i++) {
                wr_found *f = &found[count];
                memset(f->name, 0, sizeof(f->name));
                int named = hci_read_remote_name(sock, &ii[i].bdaddr, sizeof(f->name), f->name, 5000) == 0;

                if (named ? strncmp(f->name, "Nintendo RVL-CNT-01", 19) != 0 : !wr_is_wiimote_class(ii[i].dev_class))
                        continue;
                if (!named) strcpy(f->name, "(no name)");
                ba2str(&ii[i].bdaddr, f->mac);
                count++;
        }

        bt_free(ii);
        close(sock);
        return count;
}

/*
 * 0x31: core buttons + accelerometer. Continuous, so the Wiimote keeps
 * sending reports even when nothing changes, which gives a steady tick
 * to act on while the remote is held tilted.
 */
int
wr_set_report_mode(wr_socks socks)
{
        unsigned char report[] = { 0xA2, 0x12, 0x04, 0x31 };
        return wr_write(socks.data, report, sizeof(report), "write report mode");
}

static int
wr_request_accel_cal(wr_sock data)
{
        unsigned char report[] = { 0xA2, 0x17, 0x00, 0x00, 0x00, WR_ACCEL_CAL_ADDR, 0x00, 10 };
        return wr_write(data, report, sizeof(report), "read accel calibration");
}

int
wr_init(const char *mac, wr_socks *socks)
{
        struct sockaddr_l2 addr = { 0 };
        addr.l2_family          = AF_BLUETOOTH;
        str2ba(mac, &addr.l2_bdaddr);

        socks->ctl  = -1;
        socks->data = -1;

        socks->ctl = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
        if (socks->ctl < 0) {
                perror("socket ctl");
                goto err;
        }
        addr.l2_psm = htobs(0x11);
        if (connect(socks->ctl, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                perror("connect ctl");
                goto err;
        }

        socks->data = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
        if (socks->data < 0) {
                perror("socket data");
                goto err;
        }
        addr.l2_psm = htobs(0x13);
        if (connect(socks->data, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                perror("connect data");
                goto err;
        }

        if (wr_set_report_mode(*socks) < 0) goto err;
        wr_set_leds(*socks, 0);
        if (wr_request_accel_cal(socks->data) < 0) goto err;
        return 0;
err:
        if (socks->ctl >= 0) close(socks->ctl);
        if (socks->data >= 0) close(socks->data);
        socks->ctl  = -1;
        socks->data = -1;
        return -1;
}

void
wr_fini(wr_socks socks)
{
        close(socks.ctl);
        close(socks.data);
}

void
wr_set_leds(wr_socks socks, unsigned char leds)
{
        // bit 0 is rumble, keep it clear
        unsigned char report[] = { 0xA2, 0x11, leds & 0xF0 };
        if (write(socks.data, report, sizeof(report)) < 0) perror("write leds");
}

void
wr_state_init(wr_state *st)
{
        st->keys     = WR_KEY_NONE;
        st->ax       = WR_ACCEL_ZERO_DEFAULT;
        st->ay       = WR_ACCEL_ZERO_DEFAULT;
        st->zero[0]  = WR_ACCEL_ZERO_DEFAULT;
        st->zero[1]  = WR_ACCEL_ZERO_DEFAULT;
        st->one_g[0] = WR_ACCEL_ONE_G_DEFAULT;
        st->one_g[1] = WR_ACCEL_ONE_G_DEFAULT;
}

static unsigned
wr_decode_keys(const unsigned char *b)
{
        unsigned short buttons = (b[0] << 8) | b[1];
        unsigned keys          = WR_KEY_NONE;

        if (buttons & 0x0008) keys |= WR_KEY_A;
        if (buttons & 0x0004) keys |= WR_KEY_B;
        if (buttons & 0x0010) keys |= WR_KEY_MINUS;
        if (buttons & 0x1000) keys |= WR_KEY_PLUS;
        if (buttons & 0x0080) keys |= WR_KEY_HOME;
        if (buttons & 0x0800) keys |= WR_KEY_UP;
        if (buttons & 0x0400) keys |= WR_KEY_DOWN;
        if (buttons & 0x0100) keys |= WR_KEY_LEFT;
        if (buttons & 0x0200) keys |= WR_KEY_RIGHT;
        if (buttons & 0x0002) keys |= WR_KEY_1;
        if (buttons & 0x0001) keys |= WR_KEY_2;
        return keys;
}

int
wr_read(wr_socks socks, wr_state *st)
{
        unsigned char buf[256];
        int n = read(socks.data, buf, sizeof(buf));
        if (n <= 0) return -1;
        if (n < 4) return 0;

        switch (buf[1]) {
        case 0x20:
                // status (e.g. something plugged into the extension port):
                // reporting stops until the mode is set again
                st->keys = wr_decode_keys(buf + 2);
                wr_set_report_mode(socks);
                return 1;

        case 0x21:
                // read answer: size-1 << 4 | error, offset (low 16 bits), data
                if (n < 15 || buf[5] != 0x00 || buf[6] != WR_ACCEL_CAL_ADDR) return 0;
                if (buf[4] & 0x0F) {
                        fprintf(stderr, "accel calibration: error %02x, using defaults\n",
                                buf[4] & 0x0F);
                        return 0;
                }
                // 0x16..0x18 zero X Y Z, 0x1A..0x1C 1 g X Y Z
                for (int i = 0; i < 2; i++) {
                        if (buf[11 + i] <= buf[7 + i]) continue; // nonsense, keep defaults
                        st->zero[i]  = buf[7 + i];
                        st->one_g[i] = buf[11 + i];
                }
                return 0;

        case 0x30:
                st->keys = wr_decode_keys(buf + 2);
                return 1;

        case 0x31:
                if (n < 7) return 0;
                st->keys = wr_decode_keys(buf + 2);
                st->ax   = buf[4];
                st->ay   = buf[5];
                return 1;
        }
        return 0;
}

float
wr_accel_g(const wr_state *st, int axis)
{
        unsigned char raw = axis == 0 ? st->ax : st->ay;
        return (float) (raw - st->zero[axis]) / (st->one_g[axis] - st->zero[axis]);
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* WIIMOTE_H_ */

/*
-----------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
-----------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Hugo Coto Florez
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
-----------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
-----------------------------------------------------------------------------
*/
