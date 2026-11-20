/* test_read.c - wr_read() and friends without a Wii Remote.

   The data channel is replaced by one end of an AF_UNIX SOCK_SEQPACKET
   socketpair: the test writes input reports into the other end (keeping
   packet boundaries, like L2CAP) and reads back what the library sends.
*/

#define INCLUDE_WIIMOTE_IMPLEMENTATION
#include "../wiimote.h"

#include <sys/socket.h>

static int failures;

#define CHECK(cond)                                                          \
        do {                                                                 \
                if (!(cond)) {                                               \
                        fprintf(stderr, "%s:%d: CHECK(%s) failed\n",         \
                                __FILE__, __LINE__, #cond);                  \
                        failures++;                                          \
                }                                                            \
        } while (0)

static wr_socks socks; // what the library uses
static int remote;     // the fake Wiimote's end

static void
setup(void)
{
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) < 0) {
                perror("socketpair");
                _exit(2);
        }
        socks.ctl  = -1;
        socks.data = sv[0];
        remote     = sv[1];
}

static void
teardown(void)
{
        close(socks.data);
        close(remote);
}

static void
send_report(const unsigned char *r, size_t len)
{
        if (write(remote, r, len) != (ssize_t) len) {
                perror("send_report");
                _exit(2);
        }
}

// buttons as the remote sends them: two bytes, big endian
static void
report_31(unsigned short buttons, unsigned char ax, unsigned char ay)
{
        unsigned char r[] = { 0xA1, 0x31, buttons >> 8, buttons & 0xFF, ax, ay, 0x80 };
        send_report(r, sizeof(r));
}

// 0x21 read answer for 10 bytes at addr, error nibble err
static void
report_21(unsigned char addr, unsigned char err, const unsigned char data[10])
{
        unsigned char r[23] = { 0xA1, 0x21, 0x00, 0x00, (unsigned char) ((10 - 1) << 4 | err), 0x00, addr };
        memcpy(r + 7, data, 10);
        send_report(r, sizeof(r));
}

static void
test_keys(void)
{
        static const struct {
                unsigned short bit;
                unsigned key;
        } map[] = {
                { 0x0008, WR_KEY_A },     { 0x0004, WR_KEY_B },    { 0x0010, WR_KEY_MINUS },
                { 0x1000, WR_KEY_PLUS },  { 0x0080, WR_KEY_HOME }, { 0x0800, WR_KEY_UP },
                { 0x0400, WR_KEY_DOWN },  { 0x0100, WR_KEY_LEFT }, { 0x0200, WR_KEY_RIGHT },
                { 0x0002, WR_KEY_1 },     { 0x0001, WR_KEY_2 },
        };
        wr_state st;
        setup();
        wr_state_init(&st);

        unsigned all_bits = 0, all_keys = 0;
        for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
                report_31(map[i].bit, 0x80, 0x80);
                CHECK(wr_read(socks, &st) == 1);
                CHECK(st.keys == map[i].key);
                all_bits |= map[i].bit;
                all_keys |= map[i].key;
        }

        report_31(all_bits, 0x80, 0x80);
        CHECK(wr_read(socks, &st) == 1);
        CHECK(st.keys == all_keys);

        // bits that are not buttons (accelerometer LSBs) are ignored
        report_31(0x6060, 0x80, 0x80);
        CHECK(wr_read(socks, &st) == 1);
        CHECK(st.keys == WR_KEY_NONE);

        // 0x30: buttons only
        unsigned char r30[] = { 0xA1, 0x30, 0x00, 0x08 };
        send_report(r30, sizeof(r30));
        CHECK(wr_read(socks, &st) == 1);
        CHECK(st.keys == WR_KEY_A);
        teardown();
}

static void
test_accel(void)
{
        wr_state st;
        setup();
        wr_state_init(&st);

        report_31(0, 0x9A, 0x66);
        CHECK(wr_read(socks, &st) == 1);
        CHECK(st.ax == 0x9A);
        CHECK(st.ay == 0x66);
        // default calibration: zero 0x80, 1 g 0x9A
        CHECK(wr_accel_g(&st, 0) == 1.0f);
        CHECK(wr_accel_g(&st, 1) == -1.0f);

        report_31(0, 0x80, 0x80);
        CHECK(wr_read(socks, &st) == 1);
        CHECK(wr_accel_g(&st, 0) == 0.0f);
        CHECK(wr_accel_g(&st, 1) == 0.0f);
        teardown();
}

static void
test_calibration(void)
{
        // zero X Y Z, LSBs, 1 g X Y Z, ...
        const unsigned char good[10] = { 0x70, 0x72, 0x74, 0x00, 0x90, 0x94, 0x96, 0, 0, 0 };
        wr_state st;
        setup();

        wr_state_init(&st);
        report_21(WR_ACCEL_CAL_ADDR, 0, good);
        CHECK(wr_read(socks, &st) == 0);
        CHECK(st.zero[0] == 0x70 && st.zero[1] == 0x72);
        CHECK(st.one_g[0] == 0x90 && st.one_g[1] == 0x94);
        report_31(0, 0x90, 0x72);
        CHECK(wr_read(socks, &st) == 1);
        CHECK(wr_accel_g(&st, 0) == 1.0f);
        CHECK(wr_accel_g(&st, 1) == 0.0f);

        // answer for another address: ignored
        wr_state_init(&st);
        report_21(0x20, 0, good);
        CHECK(wr_read(socks, &st) == 0);
        CHECK(st.zero[0] == WR_ACCEL_ZERO_DEFAULT && st.one_g[0] == WR_ACCEL_ONE_G_DEFAULT);

        // error nibble set: defaults kept
        wr_state_init(&st);
        report_21(WR_ACCEL_CAL_ADDR, 0x8, good);
        CHECK(wr_read(socks, &st) == 0);
        CHECK(st.zero[0] == WR_ACCEL_ZERO_DEFAULT && st.one_g[0] == WR_ACCEL_ONE_G_DEFAULT);

        // 1 g not above zero on X: X keeps defaults, Y is taken
        const unsigned char bad_x[10] = { 0x90, 0x72, 0x74, 0x00, 0x90, 0x94, 0x96, 0, 0, 0 };
        wr_state_init(&st);
        report_21(WR_ACCEL_CAL_ADDR, 0, bad_x);
        CHECK(wr_read(socks, &st) == 0);
        CHECK(st.zero[0] == WR_ACCEL_ZERO_DEFAULT && st.one_g[0] == WR_ACCEL_ONE_G_DEFAULT);
        CHECK(st.zero[1] == 0x72 && st.one_g[1] == 0x94);
        teardown();
}

static void
test_status(void)
{
        wr_state st;
        setup();
        wr_state_init(&st);

        // status report: battery, flags... the library only reads the buttons
        unsigned char r20[] = { 0xA1, 0x20, 0x00, 0x80, 0x02, 0x00, 0x00, 0xC0 };
        send_report(r20, sizeof(r20));
        CHECK(wr_read(socks, &st) == 1);
        CHECK(st.keys == WR_KEY_HOME);

        // and it must ask for mode 0x31 again
        unsigned char out[16];
        ssize_t n = read(remote, out, sizeof(out));
        const unsigned char want[] = { 0xA2, 0x12, 0x04, 0x31 };
        CHECK(n == (ssize_t) sizeof(want));
        CHECK(n == (ssize_t) sizeof(want) && memcmp(out, want, sizeof(want)) == 0);
        teardown();
}

static void
test_short_and_unknown(void)
{
        wr_state st;
        setup();
        wr_state_init(&st);
        st.keys = WR_KEY_A;

        unsigned char tiny[] = { 0xA1, 0x31, 0x00 };
        send_report(tiny, sizeof(tiny));
        CHECK(wr_read(socks, &st) == 0);

        unsigned char short31[] = { 0xA1, 0x31, 0x00, 0x04, 0x80, 0x80 };
        send_report(short31, sizeof(short31));
        CHECK(wr_read(socks, &st) == 0);

        unsigned char short21[] = { 0xA1, 0x21, 0x00, 0x00, 0x90, 0x00, WR_ACCEL_CAL_ADDR };
        send_report(short21, sizeof(short21));
        CHECK(wr_read(socks, &st) == 0);

        unsigned char unknown[] = { 0xA1, 0x3D, 0x00, 0x04, 0, 0, 0, 0 };
        send_report(unknown, sizeof(unknown));
        CHECK(wr_read(socks, &st) == 0);

        // none of the above touched st
        CHECK(st.keys == WR_KEY_A);
        CHECK(st.ax == WR_ACCEL_ZERO_DEFAULT && st.zero[0] == WR_ACCEL_ZERO_DEFAULT);
        teardown();
}

static void
test_closed(void)
{
        wr_state st;
        setup();
        wr_state_init(&st);
        close(remote);
        CHECK(wr_read(socks, &st) == -1);
        close(socks.data);
}

static void
test_output_reports(void)
{
        unsigned char out[16];
        ssize_t n;
        setup();

        CHECK(wr_set_report_mode(socks) == 0);
        const unsigned char mode[] = { 0xA2, 0x12, 0x04, 0x31 };
        n = read(remote, out, sizeof(out));
        CHECK(n == (ssize_t) sizeof(mode) && memcmp(out, mode, sizeof(mode)) == 0);

        // rumble bit (and the rest of the low nibble) must stay clear
        wr_set_leds(socks, WR_LED_1 | WR_LED_4 | 0x0F);
        const unsigned char leds[] = { 0xA2, 0x11, 0x90 };
        n = read(remote, out, sizeof(out));
        CHECK(n == (ssize_t) sizeof(leds) && memcmp(out, leds, sizeof(leds)) == 0);
        teardown();
}

int
main(void)
{
        test_keys();
        test_accel();
        test_calibration();
        test_status();
        test_short_and_unknown();
        test_closed();
        test_output_reports();

        if (failures) {
                fprintf(stderr, "%d check(s) failed\n", failures);
                return 1;
        }
        printf("all tests passed\n");
        return 0;
}
