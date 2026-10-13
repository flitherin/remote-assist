#ifndef CHAT_H
#define CHAT_H
#include <windows.h>
#define CHAT_EVENT (WM_APP+1)
int chat_start(HWND window, int server, const wchar_t *address);
void chat_event(WPARAM socket, LPARAM event);
void chat_stop(void);
int chat_send(const wchar_t *text);
void chat_status(const wchar_t *text, int active);
void chat_line(const wchar_t *text);
int chat_ready(void);
int chat_frame_send(const char *frame);
void screen_stop(void);
int screen_toggle(HWND owner);
void screen_tick(void);
void screen_receive(const char *frame);
#endif
