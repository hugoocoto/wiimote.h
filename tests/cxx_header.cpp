// The declarations must be usable from C++ (extern "C").
#include "../wiimote.h"

int
main()
{
        wr_state st;
        wr_state_init(&st); // never linked: only compiled
        return 0;
}
