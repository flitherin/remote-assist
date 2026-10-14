#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include <stdio.h>
#include "chat.h"
static HWND heading, role, address, port, status, start, stop, history, message_edit, send_button, share, view,control_button;
static HFONT font;
static UINT dpi=96;
static int px(int value) { return MulDiv(value,(int)dpi,96); }
static int hosting(void) { return SendMessageW(role,CB_GETCURSEL,0,0)==1; }
static HWND control(HWND parent,const wchar_t *type,const wchar_t *text,DWORD style,int id) {
    HWND child=CreateWindowW(type,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    SendMessageW(child,WM_SETFONT,(WPARAM)font,TRUE); return child;
}
static void layout(HWND window) {
    RECT r; GetClientRect(window,&r); int sidebar=px(350),margin=px(16),gap=px(12);
    MoveWindow(heading,margin,margin,sidebar,px(30),TRUE);
    MoveWindow(role,margin,px(56),sidebar,px(160),TRUE);
    MoveWindow(address,margin,px(100),px(230),px(32),TRUE);
    MoveWindow(port,margin+px(242),px(100),px(108),px(32),TRUE);
    MoveWindow(start,margin,px(144),px(168),px(36),TRUE);
    MoveWindow(stop,margin+px(180),px(144),px(170),px(36),TRUE);
    MoveWindow(status,margin,px(194),sidebar,px(100),TRUE);
    MoveWindow(share,margin,px(302),sidebar,px(38),TRUE);
    MoveWindow(control_button,margin,px(348),sidebar,px(38),TRUE);
    int chatHeight=r.bottom-px(466); if(chatHeight<px(70)) chatHeight=px(70);
    MoveWindow(history,margin,px(402),sidebar,chatHeight,TRUE);
    MoveWindow(message_edit,margin,r.bottom-px(48),px(242),px(32),TRUE);
    MoveWindow(send_button,margin+px(254),r.bottom-px(48),px(96),px(32),TRUE);
    MoveWindow(view,sidebar+margin+gap,margin,r.right-sidebar-margin*2-gap,r.bottom-margin*2,TRUE);
}
static void update_controls(void) {
    int started=chat_started(),active=chat_ready();
    EnableWindow(role,!started); EnableWindow(address,!started); EnableWindow(port,!started);
    EnableWindow(start,!started); EnableWindow(stop,started);
    EnableWindow(message_edit,active); EnableWindow(send_button,active);
    EnableWindow(share,active && hosting());
    ShowWindow(share,hosting()?SW_SHOW:SW_HIDE);
    EnableWindow(control_button,active && hosting());
    ShowWindow(control_button,hosting()?SW_SHOW:SW_HIDE);
}
void control_changed(int allowed) {
    if(!control_button) return;
    SetWindowTextW(control_button,allowed?L"Запретить управление · Ctrl+Alt+F12":L"Разрешить управление");
    if(allowed) {
        SetWindowTextW(status,chat_is_host()?L"ОПЕРАТОР УПРАВЛЯЕТ КОМПЬЮТЕРОМ\nОтзыв доступа: Ctrl+Alt+F12":L"Управление разрешено владельцем.\nНажмите на экран для мыши и клавиатуры.");
    } else if(chat_ready()) SetWindowTextW(status,L"Чат активен · управление отключено");
}
void chat_status(const wchar_t *text,int active) {
    SetWindowTextW(status,text);
    if(!active) SetWindowTextW(share,L"Разрешить показ экрана");
    update_controls();
}
void chat_line(const wchar_t *text) {
    int length=GetWindowTextLengthW(history);
    if(length>56000) {
        SendMessageW(history,EM_SETSEL,0,12000);
        SendMessageW(history,EM_REPLACESEL,FALSE,(LPARAM)L"");
    }
    SendMessageW(history,EM_SETSEL,(WPARAM)-1,(LPARAM)-1);
    SendMessageW(history,EM_REPLACESEL,FALSE,(LPARAM)text);
    SendMessageW(history,EM_REPLACESEL,FALSE,(LPARAM)L"\r\n");
    SendMessageW(history,EM_SCROLLCARET,0,0);
}
static void send_message(void) {
    wchar_t text[1501],display[1540]; GetWindowTextW(message_edit,text,1501);
    if(chat_send(text)) {
        swprintf_s(display,1540,L"Вы: %ls",text); chat_line(display);
        SetWindowTextW(message_edit,L""); SetFocus(message_edit);
    } else if(*text && chat_ready()) chat_line(L"Не удалось отправить сообщение. Повторите отправку.");
}
static LRESULT CALLBACK edit_proc(HWND window,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
    (void)id; (void)data;
    if(msg==WM_KEYDOWN && wp==VK_RETURN) { send_message(); return 0; }
    if(msg==WM_CHAR && wp==VK_RETURN) return 0;
    if(msg==WM_GETDLGCODE && lp && ((MSG *)lp)->message==WM_KEYDOWN && ((MSG *)lp)->wParam==VK_RETURN) return DLGC_WANTMESSAGE;
    return DefSubclassProc(window,msg,wp,lp);
}
static LRESULT CALLBACK window_proc(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_CREATE:
        dpi=GetDpiForWindow(window);
        font=CreateFontW(-px(15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        heading=control(window,L"STATIC",L"Remote Assist · чат и экран",0,0);
        role=control(window,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,10);
        SendMessageW(role,CB_ADDSTRING,0,(LPARAM)L"Оператор · подключиться");
        SendMessageW(role,CB_ADDSTRING,0,(LPARAM)L"Владелец · принять подключение");
        SendMessageW(role,CB_SETCURSEL,0,0);
        address=control(window,L"EDIT",L"127.0.0.1",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,3);
        port=control(window,L"EDIT",L"45873",WS_BORDER|ES_NUMBER|WS_TABSTOP,4);
        SendMessageW(address,EM_SETLIMITTEXT,15,0); SendMessageW(port,EM_SETLIMITTEXT,5,0);
        start=control(window,L"BUTTON",L"Начать сеанс",WS_TABSTOP,1);
        stop=control(window,L"BUTTON",L"Завершить",WS_TABSTOP,2);
        status=control(window,L"STATIC",L"Отключено. Укажите IPv4 и порт.\nТранспорт без шифрования.\nУправление требует отдельного разрешения.\nЖурнал: RemoteAssist-session.log",0,0);
        share=control(window,L"BUTTON",L"Разрешить показ экрана",WS_TABSTOP,8);
        control_button=control(window,L"BUTTON",L"Разрешить управление",WS_TABSTOP,9);
        control_init(window);
        history=control(window,L"EDIT",L"",WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY,5);
        SendMessageW(history,EM_SETLIMITTEXT,65535,0);
        message_edit=control(window,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,6);
        SendMessageW(message_edit,EM_SETLIMITTEXT,1500,0);
        SetWindowSubclass(message_edit,edit_proc,1,0);
        send_button=control(window,L"BUTTON",L"Отправить",WS_TABSTOP,7);
        view=screen_create(window);
        SetTimer(window,43,250,NULL); update_controls(); layout(window); return 0;
    case WM_SIZE: layout(window); return 0;
    case WM_HOTKEY: if(wp==44 && chat_is_host() && control_allowed()) control_toggle(); return 0;
    case WM_ACTIVATE: if(LOWORD(wp)==WA_INACTIVE) control_release(); return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize.x=px(850);
        ((MINMAXINFO *)lp)->ptMinTrackSize.y=px(620); return 0;
    case WM_DPICHANGED: {
        RECT *r=(RECT *)lp; dpi=HIWORD(wp);
        HFONT old=font; font=CreateFontW(-px(15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        HWND children[]={heading,role,address,port,status,start,stop,share,control_button,history,message_edit,send_button};
        for(unsigned i=0;i<sizeof(children)/sizeof(children[0]);i++) SendMessageW(children[i],WM_SETFONT,(WPARAM)font,TRUE);
        DeleteObject(old);
        SetWindowPos(window,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
        layout(window); return 0;
    }
    case CHAT_EVENT: chat_event(wp,lp); return 0;
    case WM_TIMER: if(wp==43) screen_tick(); if(wp==42) { KillTimer(window,42); chat_timeout(); } return 0;
    case WM_COMMAND:
        if(LOWORD(wp)==10 && HIWORD(wp)==CBN_SELCHANGE) { update_controls(); return 0; }
        if(LOWORD(wp)==1) {
            wchar_t ip[16],portText[8],*end=NULL;
            GetWindowTextW(address,ip,16); GetWindowTextW(port,portText,8);
            long number=wcstol(portText,&end,10);
            if(!*portText || *end || number<1 || number>65535) { SetWindowTextW(status,L"Порт должен быть от 1 до 65535."); return 0; }
            if(!chat_start(window,hosting(),ip,(unsigned short)number)) chat_status(L"Ошибка подключения: проверьте IPv4, порт и занятость адреса.",0);
        } else if(LOWORD(wp)==2) { chat_stop(); chat_status(L"Сеанс завершён",0); }
        else if(LOWORD(wp)==7) send_message();
        else if(LOWORD(wp)==8 && chat_ready()) SetWindowTextW(share,screen_toggle(window)?L"Остановить показ":L"Разрешить показ экрана");
        else if(LOWORD(wp)==9) control_toggle();
        return 0;
    case WM_DESTROY:
        chat_stop(); KillTimer(window,42); KillTimer(window,43); DeleteObject(font); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command,int show) {
    (void)previous; (void)command;
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&controls);
    WNDCLASSW cls={0}; cls.lpfnWndProc=window_proc; cls.hInstance=instance; cls.lpszClassName=L"RemoteAssistUnified";
    cls.hCursor=LoadCursorW(NULL,IDC_ARROW); cls.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
    if(!RegisterClassW(&cls)) return 1;
    dpi=GetDpiForSystem(); RECT r={0,0,px(1180),px(720)};
    AdjustWindowRectExForDpi(&r,WS_OVERLAPPEDWINDOW,FALSE,0,dpi);
    HWND window=CreateWindowW(cls.lpszClassName,L"Remote Assist",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,NULL,NULL,instance,NULL);
    if(!window) return 2; ShowWindow(window,show);
    MSG msg; BOOL result;
    while((result=GetMessageW(&msg,NULL,0,0))>0) { if(!IsDialogMessageW(window,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    return result==-1?3:(int)msg.wParam;
}
