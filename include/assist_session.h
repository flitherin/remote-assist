#ifndef ASSIST_SESSION_H
#define ASSIST_SESSION_H
#ifdef __cplusplus
extern "C" {
#endif
/* Fixed int ABI, suitable for a small assembly caller. Local UI state only. */
int assist_confirm_local(void);
void assist_stop_local(void);
int assist_is_confirmed(void);
#ifdef __cplusplus
}
#endif
#endif
