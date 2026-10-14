#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "chat.h"
static HWND owner;
static int allowed;
static unsigned int epoch,remote_epoch;
static unsigned char keys[512],buttons[3];
static int hotkey;
static int remote_width=16,remote_height=9;
void control_dimensions(int *width,int *height) { *width=remote_width; *height=remote_height; }
static void release_local(void) {
    for(unsigned int i=1;i<512;i++) if(keys[i]) {
        INPUT input={0}; input.type=INPUT_KEYBOARD; input.ki.wScan=(WORD)(i&255);
        input.ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP|((i&256)?KEYEVENTF_EXTENDEDKEY:0);
        SendInput(1,&input,sizeof(input)); keys[i]=0;
    }
    DWORD flags[]={MOUSEEVENTF_LEFTUP,MOUSEEVENTF_RIGHTUP,MOUSEEVENTF_MIDDLEUP};
    for(int i=0;i<3;i++) if(buttons[i]) {
        INPUT input={0}; input.type=INPUT_MOUSE; input.mi.dwFlags=flags[i];
        SendInput(1,&input,sizeof(input)); buttons[i]=0;
    }
}
void control_init(HWND window) { owner=window; }
int control_allowed(void) { return allowed; }
void control_stop(void) {
    allowed=0; remote_epoch=0; release_local();
    if(hotkey) { UnregisterHotKey(owner,44); hotkey=0; }
    control_changed(0);
}
void control_toggle(void) {
    if(!chat_ready() || !chat_is_host()) return;
    if(allowed) {
        control_stop(); chat_control_send("CTRL OFF\n");
        chat_line(L"Управление отозвано владельцем."); return;
    }
    if(chat_peer_local()) {
        chat_line(L"Управление на этом же ПК отключено: оба окна используют один системный курсор. Проверяйте управление на другом компьютере; локально доступны чат и просмотр.");
        return;
    }
    if(!screen_sharing()) { chat_line(L"Сначала разрешите показ экрана."); return; }
    unsigned long session=chat_session_id();
    if(MessageBoxW(owner,L"Разрешить подключённому оператору мышь и клавиатуру?\n\nОператор сможет работать с вашими открытыми приложениями. Транспорт без шифрования: используйте доверенную сеть/VPN.\n\nОтозвать доступ: кнопка «Запретить управление», Ctrl+Alt+F12 или завершение сеанса.",L"Разрешение управления",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES) return;
    if(!chat_ready() || !chat_is_host() || !screen_sharing() || session!=chat_session_id() || chat_peer_local()) return;
    if(!RegisterHotKey(owner,44,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F12)) {
        chat_line(L"Управление не включено: Ctrl+Alt+F12 занято другой программой."); return;
    }
    hotkey=1; if(++epoch==0) ++epoch;
    char record[96]; sprintf_s(record,sizeof(record),"CTRL ON %u %d %d\n",epoch,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
    allowed=1;
    if(!chat_control_send(record)) { control_stop(); return; }
    control_changed(1); chat_line(L"Управление разрешено. Отзыв: Ctrl+Alt+F12.");
}
void control_release(void) {
    if(!chat_is_host() && allowed && chat_ready()) {
        char record[64]; sprintf_s(record,sizeof(record),"CTRL RELEASE %u\n",remote_epoch);
        if(!chat_control_send(record)) chat_stop();
    }
}
void control_emergency_release(void) {
    if(chat_is_host()) return;
    release_local();
    if(allowed && chat_ready()) {
        char record[64];
        sprintf_s(record,sizeof(record),"CTRL RELEASE %u\n",remote_epoch);
        chat_control_send(record);
    }
    control_changed(0);
}
void control_mouse(int x,int y,int button,int down,int wheel) {
    if(!allowed || chat_is_host() || !chat_ready()) return;
    char record[128]; sprintf_s(record,sizeof(record),"CTRL M %u %d %d %d %d %d\n",remote_epoch,x,y,button,down,wheel);
    if(!chat_control_send(record)) chat_stop();
}
void control_key(unsigned int scan,int extended,int down) {
    if(!allowed || chat_is_host() || !chat_ready() || !scan || scan>255) return;
    char record[96]; sprintf_s(record,sizeof(record),"CTRL K %u %u %d %d\n",remote_epoch,scan,extended,down);
    if(!chat_control_send(record)) chat_stop();
}
void control_receive(const char *record) {
    unsigned int token=0,scan=0; int x,y,button,down,wheel,extended,n=0;
    if(!chat_is_host()) {
        if(!strcmp(record,"OFF")) { control_stop(); return; }
        int width=0,height=0;
        if(sscanf_s(record,"ON %u %d %d%n",&token,&width,&height,&n)==3 && record[n]==0 && token && width>0 && width<=32768 && height>0 && height<=32768) {
            remote_width=width; remote_height=height;
            remote_epoch=token; allowed=1; control_changed(1);
            chat_line(L"Владелец разрешил управление. Нажмите на экран для ввода.");
        }
        return;
    }
    if(!allowed || !screen_sharing() || chat_peer_local()) return;
    if(sscanf_s(record,"RELEASE %u%n",&token,&n)==1 && !record[n] && token==epoch) { release_local(); return; }
    n=0;
    if(sscanf_s(record,"K %u %u %d %d%n",&token,&scan,&extended,&down,&n)==4 && !record[n] && token==epoch && scan>0 && scan<256 && (extended==0||extended==1) && (down==0||down==1)) {
        unsigned int index=scan+(extended?256:0);
        if(!down && !keys[index]) return;
        /* Remote input cannot issue the local emergency shortcut. */
        if(scan==0x58) return;
        INPUT input={0}; input.type=INPUT_KEYBOARD; input.ki.wScan=(WORD)scan;
        input.ki.dwFlags=KEYEVENTF_SCANCODE|(extended?KEYEVENTF_EXTENDEDKEY:0)|(down?0:KEYEVENTF_KEYUP);
        if(SendInput(1,&input,sizeof(input))==1) keys[index]=(unsigned char)down;
        return;
    }
    n=0;
    if(sscanf_s(record,"M %u %d %d %d %d %d%n",&token,&x,&y,&button,&down,&wheel,&n)==6 && !record[n] && token==epoch && x>=0 && x<=65535 && y>=0 && y<=65535 && button>=0 && button<=3 && (down==0||down==1) && wheel>=-1200 && wheel<=1200) {
        INPUT input={0}; input.type=INPUT_MOUSE; input.mi.dx=x; input.mi.dy=y;
        input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE;
        DWORD downs[]={MOUSEEVENTF_LEFTDOWN,MOUSEEVENTF_RIGHTDOWN,MOUSEEVENTF_MIDDLEDOWN};
        DWORD ups[]={MOUSEEVENTF_LEFTUP,MOUSEEVENTF_RIGHTUP,MOUSEEVENTF_MIDDLEUP};
        if(button) input.mi.dwFlags|=down?downs[button-1]:ups[button-1];
        if(wheel) { input.mi.dwFlags|=MOUSEEVENTF_WHEEL; input.mi.mouseData=(DWORD)wheel; }
        if(SendInput(1,&input,sizeof(input))==1 && button) buttons[button-1]=(unsigned char)down;
    }
}
