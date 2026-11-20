/* two_tu_a.c, two_tu_b.c - the header included from two translation units,
   the implementation in only one. Linking fails on duplicate symbols. */

#define INCLUDE_WIIMOTE_IMPLEMENTATION
#include "../wiimote.h"

float two_tu_b(void);

int
main(void)
{
        return two_tu_b() == 0.0f ? 0 : 1;
}
