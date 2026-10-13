#include <windows.h>
#include <commctrl.h>
#include "assist_session.h"
#include "chat.h"
static HWND heading, description, status_label, confirm_button, stop_button;
static HWND address_label, address_edit, port_label, port_edit, notice;
static HWND history, message_edit, send_button, share_button, action_button;
static int actions_allowed;
static HFONT body_font, title_font;
static UINT dpi = 96;
static int px(int n) { return MulDiv(n, (int)dpi, 96); }
static void layout(HWND window) {
    RECT r; GetClientRect(window,&r);
    MoveWindow(heading,px(28),px(24),r.right-px(56),px(38),TRUE);
    MoveWindow(description,px(28),px(72),r.right-px(56),px(58),TRUE);
    MoveWindow(address_label,px(28),px(142),px(320),px(24),TRUE);
    MoveWindow(address_edit,px(28),px(170),px(350),px(32),TRUE);
    MoveWindow(port_label,px(398),px(142),px(100),px(24),TRUE);
    MoveWindow(port_edit,px(398),px(170),px(110),px(32),TRUE);
    MoveWindow(notice,px(28),px(218),r.right-px(56),px(46),TRUE);
    MoveWindow(status_label,px(28),px(290),r.right-px(56),px(46),TRUE);
    MoveWindow(confirm_button,px(28),px(362),px(270),px(40),TRUE);
    MoveWindow(stop_button,px(314),px(362),px(194),px(40),TRUE);
    if(share_button) MoveWindow(share_button,px(28),px(670),px(270),px(38),TRUE);
    if(action_button) MoveWindow(action_button,px(28),px(720),px(480),px(38),TRUE);
    MoveWindow(history,px(28),px(422),r.right-px(56),px(180),TRUE);
    MoveWindow(message_edit,px(28),px(618),px(350),px(36),TRUE);
    MoveWindow(send_button,px(394),px(618),px(114),px(36),TRUE);
}
void chat_status(const wchar_t *text,int active) {
    SetWindowTextW(status_label,text);
    EnableWindow(message_edit,active); EnableWindow(send_button,active);
    if(share_button) { EnableWindow(share_button,active); if(!active) SetWindowTextW(share_button,L"Разрешить показ экрана"); }
    if(action_button) { EnableWindow(action_button,active); if(!active) { actions_allowed=0; SetWindowTextW(action_button,L"Разрешить действия · функция недоступна"); } }
    EnableWindow(confirm_button,FALSE); EnableWindow(stop_button,TRUE);
}
void chat_line(const wchar_t *text) {
    if(GetWindowTextLengthW(history)>60000) SetWindowTextW(history,L"");
    SendMessageW(history,EM_SETSEL,(WPARAM)-1,(LPARAM)-1);
    SendMessageW(history,EM_REPLACESEL,FALSE,(LPARAM)text);
    SendMessageW(history,EM_REPLACESEL,FALSE,(LPARAM)L"\r\n");
    SendMessageW(history,EM_SCROLLCARET,0,0);
}
static void fonts(void) {
    HWND children[]={description,status_label,confirm_button,stop_button,address_label,address_edit,port_label,port_edit,notice,history,message_edit,send_button,share_button,action_button};
    HFONT old_body=body_font, old_title=title_font;
    body_font=CreateFontW(-px(15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    title_font=CreateFontW(-px(25),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    for(unsigned i=0;i<sizeof(children)/sizeof(children[0]);++i) SendMessageW(children[i],WM_SETFONT,(WPARAM)body_font,TRUE);
    SendMessageW(heading,WM_SETFONT,(WPARAM)title_font,TRUE);
    if(old_body) DeleteObject(old_body);
    if(old_title) DeleteObject(old_title);
}
static HWND control(HWND parent,const wchar_t *type,const wchar_t *text,DWORD style,int id) {
    return CreateWindowW(type,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
}
static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    switch(message) {
    case WM_CREATE:
        dpi=GetDpiForWindow(window);
#ifdef ASSIST_HOST
        heading=control(window,L"STATIC",L"Принимающая сторона",0,0);
#else
        heading=control(window,L"STATIC",L"Рабочее место оператора",0,0);
#endif
        description=control(window,L"STATIC",L"Текстовый чат с подтверждением подключения.\nПоказ экрана включается отдельно. Только просмотр.",0,0);
#ifdef ASSIST_HOST
        address_label=control(window,L"STATIC",L"Локальный IPv4 для прослушивания",0,0);
        address_edit=control(window,L"EDIT",L"127.0.0.1",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,3);
#else
        address_label=control(window,L"STATIC",L"IPv4 собеседника",0,0);
        address_edit=control(window,L"EDIT",L"127.0.0.1",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,3);
#endif
        port_label=control(window,L"STATIC",L"Порт",0,0);
        port_edit=control(window,L"EDIT",L"45873",WS_BORDER|ES_READONLY,4);
        notice=control(window,L"STATIC",L"Чат без шифрования: используйте доверенную сеть / VPN.\nПо умолчанию доступен только на этом компьютере.",0,0);
        status_label=control(window,L"STATIC",L"Состояние: отключено",0,0);
        confirm_button=control(window,L"BUTTON",L"Начать / запросить чат",WS_TABSTOP|BS_DEFPUSHBUTTON,1);
        stop_button=control(window,L"BUTTON",L"Завершить сеанс",WS_TABSTOP,2);
        history=control(window,L"EDIT",L"",WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY,5);
        message_edit=control(window,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP,6);
        send_button=control(window,L"BUTTON",L"Отправить",WS_TABSTOP,7);
        SendMessageW(message_edit,EM_SETLIMITTEXT,1500,0);
        SendMessageW(address_edit,EM_SETLIMITTEXT,15,0);
#ifdef ASSIST_HOST
        share_button=control(window,L"BUTTON",L"Разрешить показ экрана",WS_TABSTOP,8);
        SendMessageW(share_button,WM_SETFONT,(WPARAM)body_font,TRUE);
        action_button=control(window,L"BUTTON",L"Разрешить действия · функция недоступна",WS_TABSTOP,9);
        EnableWindow(action_button,FALSE);
        EnableWindow(share_button,FALSE); SetTimer(window,43,1000,NULL);
#endif
        EnableWindow(message_edit,FALSE); EnableWindow(send_button,FALSE);
        EnableWindow(stop_button,FALSE); fonts(); layout(window); return 0;
    case WM_SIZE: layout(window); return 0;
    case WM_DPICHANGED: {
        RECT *r=(RECT *)lp; dpi=HIWORD(wp); fonts();
        SetWindowPos(window,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
        layout(window); return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)wp,RGB(32,38,48)); SetBkColor((HDC)wp,RGB(255,255,255));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    case CHAT_EVENT: chat_event(wp,lp); return 0;
    case WM_TIMER:
        if(wp==43) screen_tick();
        if(wp==42) { KillTimer(window,42); chat_stop(); chat_status(L"Время ожидания истекло",0); EnableWindow(confirm_button,TRUE); EnableWindow(stop_button,FALSE); }
        return 0;
    case WM_COMMAND:
        if(LOWORD(wp)==1) {
            wchar_t address[64]; GetWindowTextW(address_edit,address,64);
#ifdef ASSIST_HOST
            int server=1;
#else
            int server=0;
#endif
            if(!chat_start(window,server,address)) { SetWindowTextW(status_label,L"Не удалось подключиться. Проверьте IPv4 и порт."); EnableWindow(confirm_button,TRUE); EnableWindow(stop_button,FALSE); }
        } else if(LOWORD(wp)==2) {
            chat_stop(); KillTimer(window,42); chat_status(L"Сеанс завершён",0);
            EnableWindow(confirm_button,TRUE); EnableWindow(stop_button,FALSE);
        } else if(LOWORD(wp)==9 && chat_ready()) {
            if(actions_allowed) actions_allowed=0;
            else if(MessageBoxW(window,L"Сохранить локальный выбор: разрешить действия?\nУдалённый ввод не реализован: клиент не получит управление.",L"Разрешение действий · интерфейс",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES) actions_allowed=1;
            SetWindowTextW(action_button,actions_allowed?L"Запретить действия · локальный выбор":L"Разрешить действия · функция недоступна");
        } else if(LOWORD(wp)==8) {
            if(chat_ready()) SetWindowTextW(share_button,screen_toggle(window)?L"Остановить показ":L"Разрешить показ экрана");
        } else if(LOWORD(wp)==7) {
            wchar_t text[1501],display[1540]; GetWindowTextW(message_edit,text,1501);
            if(chat_send(text)) { wsprintfW(display,L"Вы: %s",text); chat_line(display); SetWindowTextW(message_edit,L""); }
        }
        return 0;
    case WM_DESTROY:
        chat_stop(); KillTimer(window,42); assist_stop_local(); DeleteObject(body_font); DeleteObject(title_font); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command,int show) {
    WNDCLASSW cls={0}; MSG message; BOOL result;
    INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_STANDARD_CLASSES};
    (void)previous; (void)command; InitCommonControlsEx(&controls);
    cls.lpfnWndProc=window_proc; cls.hInstance=instance; cls.lpszClassName=L"RemoteAssistDemo";
    cls.hCursor=LoadCursorW(NULL,IDC_ARROW); cls.hIcon=LoadIconW(NULL,IDI_APPLICATION);
    cls.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
    if(!RegisterClassW(&cls)) return 1;
    dpi=GetDpiForSystem();
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    RECT r={0,0,px(540),px(786)}; AdjustWindowRectExForDpi(&r,style,FALSE,0,dpi);
#ifdef ASSIST_HOST
    const wchar_t *title=L"Remote Assist · Сервер чата";
#else
    const wchar_t *title=L"Remote Assist · Клиент чата";
#endif
    HWND window=CreateWindowW(cls.lpszClassName,title,style,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,NULL,NULL,instance,NULL);
    if(!window) return 2;
    ShowWindow(window,show);
    while((result=GetMessageW(&message,NULL,0,0))>0) {
        if(!IsDialogMessageW(window,&message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    return result==-1?3:(int)message.wParam;
}
