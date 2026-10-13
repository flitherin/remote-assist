#include <windows.h>
#include <stdlib.h>
#include "chat.h"
#define W 320
#define H 180
static unsigned char pixels[W*H*4];
static HWND viewer;
static int sharing;
static BITMAPINFO info;
static LRESULT CALLBACK view_proc(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_PAINT) {
        PAINTSTRUCT ps; RECT r; HDC dc=BeginPaint(window,&ps); GetClientRect(window,&r);
        FillRect(dc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));
        int width=r.right,height=width*H/W;
        if(height>r.bottom) { height=r.bottom; width=height*W/H; }
        SetStretchBltMode(dc,COLORONCOLOR);
        StretchDIBits(dc,(r.right-width)/2,(r.bottom-height)/2,width,height,0,0,W,H,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
        EndPaint(window,&ps); return 0;
    }
    if(msg==WM_CLOSE) { ShowWindow(window,SW_HIDE); return 0; }
    return DefWindowProcW(window,msg,wp,lp);
}
static void setup(void) {
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=W;
    info.bmiHeader.biHeight=-H; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
}
void screen_stop(void) { sharing=0; if(viewer) { DestroyWindow(viewer); viewer=NULL; } }
int screen_toggle(HWND owner) {
    if(sharing) { sharing=0; return 0; }
    if(MessageBoxW(owner,L"Показать основной монитор собеседнику?\nВсе видимые данные будут передаваться без шифрования.\n\nОстановить показ можно кнопкой «Остановить показ» или завершением сеанса.",L"Разрешение на показ экрана",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES) return 0;
    sharing=1; return 1;
}
void screen_tick(void) {
    if(!sharing || !chat_ready()) return;
    setup(); void *bits=NULL;
    HDC desktop=GetDC(NULL),memory=CreateCompatibleDC(desktop);
    HBITMAP bitmap=CreateDIBSection(desktop,&info,DIB_RGB_COLORS,&bits,NULL,0);
    if(!bitmap || !memory) { if(bitmap) DeleteObject(bitmap); if(memory) DeleteDC(memory); ReleaseDC(NULL,desktop); return; }
    HGDIOBJ old=SelectObject(memory,bitmap);
    if(StretchBlt(memory,0,0,W,H,desktop,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),SRCCOPY|CAPTUREBLT)) {
        static const char hex[]="0123456789abcdef";
        char *record=malloc(W*H*8+8);
        if(record) {
            memcpy(record,"FRAME ",6);
            unsigned char *source=bits;
            for(size_t i=0;i<W*H*4;i++) { record[6+i*2]=hex[source[i]>>4]; record[7+i*2]=hex[source[i]&15]; }
            record[6+W*H*8]='\n'; record[7+W*H*8]=0;
            chat_frame_send(record); free(record);
        }
    }
    SelectObject(memory,old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(NULL,desktop);
}
static int nibble(char c) { return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:-1; }
void screen_receive(const char *record) {
    if(strlen(record)!=W*H*8) return;
    for(size_t i=0;i<W*H*4;i++) { int a=nibble(record[i*2]),b=nibble(record[i*2+1]); if(a<0||b<0) return; }
    for(size_t i=0;i<W*H*4;i++) pixels[i]=(unsigned char)((nibble(record[i*2])<<4)|nibble(record[i*2+1]));
    setup();
    if(!viewer) {
        WNDCLASSW cls={0}; cls.hInstance=GetModuleHandleW(NULL); cls.lpfnWndProc=view_proc;
        cls.lpszClassName=L"AssistReadOnlyView"; cls.hCursor=LoadCursorW(NULL,IDC_ARROW);
        RegisterClassW(&cls);
        viewer=CreateWindowW(cls.lpszClassName,L"Экран собеседника · только просмотр",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,960,580,NULL,NULL,cls.hInstance,NULL);
    }
    ShowWindow(viewer,SW_SHOWNOACTIVATE); InvalidateRect(viewer,NULL,FALSE);
}
