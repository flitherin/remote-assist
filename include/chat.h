#ifndef CHAT_H
#define CHAT_H
#include <windows.h>
#include "assist_session.h"
#define CHAT_EVENT (WM_APP+1)
int chat_start(HWND window, int server, const wchar_t *address, unsigned short port);
void chat_event(WPARAM socket, LPARAM event);
void chat_stop(void);
int chat_send(const wchar_t *text);
void chat_status(const wchar_t *text, int active);
void chat_line(const wchar_t *text);
int chat_ready(void);
unsigned long chat_session_id(void);
int chat_started(void);
void chat_timeout(void);
int chat_frame_send(const char *frame);
void screen_stop(void);
int screen_toggle(HWND owner);
void screen_tick(void);
void screen_receive(const char *frame);
HWND screen_create(HWND parent);
void screen_clear(void);
int chat_is_host(void);
int chat_peer_local(void);
int chat_frame_idle(void);
int chat_control_send(const char *record);
void control_init(HWND owner);
void control_stop(void);
void control_toggle(void);
int control_allowed(void);
void control_receive(const char *record);
void control_mouse(int x,int y,int button,int down,int wheel);
void control_key(unsigned int scan,int extended,int down);
void control_release(void);
void control_emergency_release(void);
void control_changed(int allowed);
void control_dimensions(int *width,int *height);
int screen_sharing(void);
#define FRAME_RECORD_LIMIT (1280 * 720 * 8 + 131200)
#endif
