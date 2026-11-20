# wiimote.h

Single-header C library to read a Wii Remote over Bluetooth on Linux: buttons
and accelerometer, plus LEDs. No pairing, no drivers: it talks to the remote
directly over L2CAP with BlueZ's libbluetooth.

## Usage

Copy `wiimote.h` into your project. In **one** C file:

```c
#define INCLUDE_WIIMOTE_IMPLEMENTATION
#include "wiimote.h"
```

Everywhere else just `#include "wiimote.h"`. Link with `-lbluetooth`.

```c
wr_found found[WR_SCAN_MAX];
int n = wr_scan(found, WR_SCAN_MAX);   // press 1+2 on the remote first
if (n <= 0) return 1;

wr_socks socks;
if (wr_init(found[0].mac, &socks) < 0) return 1;
wr_set_leds(socks, WR_LED_1);

wr_state st;
wr_state_init(&st);
while (wr_read(socks, &st) >= 0) {
        if (st.keys & WR_KEY_A) puts("A");
        printf("x %.2f g\n", wr_accel_g(&st, 0));
}

wr_fini(socks);
```

A complete program is in [`examples/demo.c`](examples/demo.c).
The full API reference is the comment at the top of [`wiimote.h`](wiimote.h).

## Requirements

- Linux with BlueZ and a Bluetooth adapter.
- libbluetooth headers: `libbluetooth-dev` (Debian/Ubuntu), `bluez-libs` (Arch),
  `bluez-libs-devel` (Fedora).
- The remote must be discoverable (press 1+2 or the red sync button) while
  scanning and connecting.
- `wr_scan` may need root or `CAP_NET_RAW` for the name requests. Without it,
  only the Bluetooth class of device is used to recognise a Wiimote.

## Limitations

- Only reporting mode 0x31 (buttons + accelerometer).
- Only the accelerometer X and Y axes are decoded.
- No extensions (Nunchuk, Classic Controller, MotionPlus), IR camera, rumble or
  speaker.

## Tests

```sh
make check
```

Builds the example with strict warnings, checks the header from C++ and from two
translation units, and runs `tests/test_read.c` under ASan/UBSan. The tests feed
fake reports through a socketpair, so no Wiimote is needed.

## License

Public domain or MIT, your choice. See [LICENSE](LICENSE).
