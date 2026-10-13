#include "assist_session.h"
/* Access only on the UI thread. This flag is not authentication. */
static int confirmed;
int assist_confirm_local(void) { confirmed = 1; return confirmed; }
void assist_stop_local(void) { confirmed = 0; }
int assist_is_confirmed(void) { return confirmed; }
