/* demo.c - scan for a Wii Remote, connect and print what it sends.

   Build: cc demo.c -o demo -lbluetooth
   Press 1+2 on the remote, then run ./demo (root may be needed to scan).
*/

#define INCLUDE_WIIMOTE_IMPLEMENTATION
#include "../wiimote.h"

int
main(void)
{
        wr_found found[WR_SCAN_MAX];

        printf("scanning, press 1+2 on the remote...\n");
        int n = wr_scan(found, WR_SCAN_MAX);
        if (n <= 0) {
                fprintf(stderr, "no Wiimote found\n");
                return 1;
        }
        printf("found %s (%s)\n", found[0].mac, found[0].name);

        wr_socks socks;
        if (wr_init(found[0].mac, &socks) < 0) return 1;
        wr_set_leds(socks, WR_LED_1);

        wr_state st;
        wr_state_init(&st);

        // HOME quits
        int r;
        while ((r = wr_read(socks, &st)) >= 0) {
                if (r == 0) continue;
                printf("\rkeys %04x  x %+.2f g  y %+.2f g ", st.keys,
                       wr_accel_g(&st, 0), wr_accel_g(&st, 1));
                fflush(stdout);
                if (st.keys & WR_KEY_HOME) break;
        }
        printf("\n");

        wr_fini(socks);
        return 0;
}
