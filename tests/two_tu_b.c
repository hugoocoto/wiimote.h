#include "../wiimote.h"

float two_tu_b(void);

float
two_tu_b(void)
{
        wr_state st;
        wr_state_init(&st);
        return wr_accel_g(&st, 0);
}
