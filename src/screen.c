#include <windows.h>
#include <windowsx.h>
#include <compressapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "chat.h"
#define W 1280
#define H 720
static unsigned char pixels[W*H*4];
static HWND viewer;
static int sharing, available;
static BITMAPINFO info;
static int releasing_capture;
static void pointer_event(HWND window,int x,int y,int button,int down,int wheel) {
    if(!available || !control_allowed() || chat_is_host()) return;
    RECT r; GetClientRect(window,&r);
    int w=r.right,h=MulDiv(w,H,W);
    if(h>r.bottom) { h=r.bottom; w=MulDiv(h,W,H); }
    if(w<=1 || h<=1) return;
    int rw,rh; control_dimensions(&rw,&rh);
    int contentW=W,contentH=MulDiv(rh,W,rw);
    if(contentH>H) { contentH=H; contentW=MulDiv(rw,H,rh); }
    int left=(r.right-w)/2+MulDiv((W-contentW)/2,w,W);
    int top=(r.bottom-h)/2+MulDiv((H-contentH)/2,h,H);
    int cw=MulDiv(contentW,w,W),ch=MulDiv(contentH,h,H);
    if(cw<=1 || ch<=1) return;
    x-=left; y-=top;
    if(GetCapture()!=window && (x<0 || y<0 || x>=cw || y>=ch)) return;
    if(x<0) x=0; if(y<0) y=0; if(x>=cw) x=cw-1; if(y>=ch) y=ch-1;
    control_mouse(MulDiv(x,65535,cw-1),MulDiv(y,65535,ch-1),button,down,wheel);
}
static void setup(void) {
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=W;
    info.bmiHeader.biHeight=-H; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
}
static LRESULT CALLBACK view_proc(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_GETDLGCODE && control_allowed() && !chat_is_host()) return DLGC_WANTALLKEYS;
    if((msg==WM_KEYDOWN || msg==WM_KEYUP || msg==WM_SYSKEYDOWN || msg==WM_SYSKEYUP) && control_allowed() && !chat_is_host()) {
        if(msg==WM_KEYDOWN && wp==VK_ESCAPE) { control_emergency_release(); ReleaseCapture(); return 0; }
        control_key((unsigned int)((lp>>16)&255),(lp&(1L<<24))!=0,msg==WM_KEYDOWN||msg==WM_SYSKEYDOWN); return 0;
    }
    if(msg==WM_KILLFOCUS || msg==WM_CANCELMODE || (msg==WM_CAPTURECHANGED && !releasing_capture)) control_release();
    if(msg==WM_MOUSEMOVE) { pointer_event(window,GET_X_LPARAM(lp),GET_Y_LPARAM(lp),0,0,0); return 0; }
    if(msg==WM_LBUTTONDOWN || msg==WM_RBUTTONDOWN || msg==WM_MBUTTONDOWN || msg==WM_LBUTTONUP || msg==WM_RBUTTONUP || msg==WM_MBUTTONUP) {
        int down=msg==WM_LBUTTONDOWN||msg==WM_RBUTTONDOWN||msg==WM_MBUTTONDOWN;
        int button=(msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP)?1:(msg==WM_RBUTTONDOWN||msg==WM_RBUTTONUP)?2:3;
        if(down && control_allowed() && !chat_is_host()) { SetFocus(window); SetCapture(window); }
        pointer_event(window,GET_X_LPARAM(lp),GET_Y_LPARAM(lp),button,down,0);
        if(!down && !(wp&(MK_LBUTTON|MK_RBUTTON|MK_MBUTTON)) && GetCapture()==window) {
            releasing_capture=1; ReleaseCapture(); releasing_capture=0;
        }
        return 0;
    }
    if(msg==WM_MOUSEWHEEL) {
        POINT point={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; ScreenToClient(window,&point);
        pointer_event(window,point.x,point.y,0,0,GET_WHEEL_DELTA_WPARAM(wp)); return 0;
    }
    if(msg==WM_PAINT) {
        PAINTSTRUCT ps; RECT r; HDC dc=BeginPaint(window,&ps); GetClientRect(window,&r);
        FillRect(dc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));
        if(available) {
            int width=r.right,height=width*H/W;
            if(height>r.bottom) { height=r.bottom; width=height*W/H; }
            SetStretchBltMode(dc,HALFTONE); SetBrushOrgEx(dc,0,0,NULL);
            StretchDIBits(dc,(r.right-width)/2,(r.bottom-height)/2,width,height,0,0,W,H,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
        } else {
            SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(220,220,220));
            DrawTextW(dc,L"Экран не передаётся",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
        EndPaint(window,&ps); return 0;
    }
    return DefWindowProcW(window,msg,wp,lp);
}
HWND screen_create(HWND parent) {
    WNDCLASSW cls={0}; cls.hInstance=GetModuleHandleW(NULL); cls.lpfnWndProc=view_proc;
    cls.lpszClassName=L"AssistReadOnlyView2"; cls.hCursor=LoadCursorW(NULL,IDC_ARROW);
    RegisterClassW(&cls);
    viewer=CreateWindowW(cls.lpszClassName,L"Экран собеседника",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,parent,NULL,cls.hInstance,NULL);
    setup(); return viewer;
}
void screen_clear(void) { available=0; memset(pixels,0,sizeof(pixels)); if(viewer) InvalidateRect(viewer,NULL,FALSE); }
void screen_stop(void) { sharing=0; screen_clear(); }
int screen_sharing(void) { return sharing; }
int screen_toggle(HWND owner) {
    if(sharing) { if(control_allowed()) control_toggle(); sharing=0; chat_frame_send("SCREEN_STOP\n"); return 0; }
    if(!chat_ready() || !chat_is_host()) return 0;
    unsigned long session=chat_session_id();
    if(MessageBoxW(owner,L"Показать основной монитор собеседнику?\nВсе видимые данные передаются без шифрования.\n\nОстановить показ можно кнопкой «Остановить показ» или завершением сеанса.",L"Разрешение на показ экрана",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)!=IDYES) return 0;
    if(!chat_ready() || chat_session_id()!=session) return 0;
    sharing=1; return 1;
}
void screen_tick(void) {
    if(!sharing || !chat_frame_idle()) return;
    setup(); void *bits=NULL;
    HDC desktop=GetDC(NULL); if(!desktop) return;
    HDC memory=CreateCompatibleDC(desktop);
    HBITMAP bitmap=CreateDIBSection(desktop,&info,DIB_RGB_COLORS,&bits,NULL,0);
    if(!bitmap || !memory) { if(bitmap) DeleteObject(bitmap); if(memory) DeleteDC(memory); ReleaseDC(NULL,desktop); return; }
    HGDIOBJ old=SelectObject(memory,bitmap);
    int sw=GetSystemMetrics(SM_CXSCREEN),sh=GetSystemMetrics(SM_CYSCREEN);
    if(sw<=0 || sh<=0) { SelectObject(memory,old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(NULL,desktop); return; }
    int w=W,h=MulDiv(sh,W,sw); if(h>H) { h=H; w=MulDiv(sw,H,sh); }
    PatBlt(memory,0,0,W,H,BLACKNESS);
    SetStretchBltMode(memory,HALFTONE); SetBrushOrgEx(memory,0,0,NULL);
    if(StretchBlt(memory,(W-w)/2,(H-h)/2,w,h,desktop,0,0,sw,sh,SRCCOPY|CAPTUREBLT)) {
        SIZE_T packed=0; COMPRESSOR_HANDLE compressor=NULL;
        if(CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,NULL,&compressor)) {
            Compress(compressor,bits,sizeof(pixels),NULL,0,&packed);
            if(packed && packed<=sizeof(pixels)+65536) {
                unsigned char *data=malloc(packed);
                if(data && Compress(compressor,bits,sizeof(pixels),data,packed,&packed)) {
                    char *record=malloc(packed*2+9);
                    if(record) {
                        memcpy(record,"FRAME2 ",7); static const char hex[]="0123456789abcdef";
                        for(SIZE_T i=0;i<packed;i++) { record[7+i*2]=hex[data[i]>>4]; record[8+i*2]=hex[data[i]&15]; }
                        record[7+packed*2]='\n'; record[8+packed*2]=0;
                        chat_frame_send(record); free(record);
                    }
                }
                free(data);
            }
            CloseCompressor(compressor);
        }
    }
    SelectObject(memory,old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(NULL,desktop);
}
static int nibble(char c) { return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:-1; }
void screen_receive(const char *record) {
    if(!record) return;
    size_t length=strlen(record);
    if(!length || length%2 || length>sizeof(pixels)*2+131072) return;
    unsigned char *data=malloc(length/2),*decoded=malloc(sizeof(pixels));
    if(!data || !decoded) { free(data); free(decoded); return; }
    for(size_t i=0;i<length/2;i++) {
        int a=nibble(record[i*2]),b=nibble(record[i*2+1]);
        if(a<0||b<0) { free(data); free(decoded); return; }
        data[i]=(unsigned char)((a<<4)|b);
    }
    DECOMPRESSOR_HANDLE decoder=NULL; SIZE_T actual=0;
    if(CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,NULL,&decoder)) {
        if(Decompress(decoder,data,length/2,decoded,sizeof(pixels),&actual) && actual==sizeof(pixels)) {
            memcpy(pixels,decoded,actual); available=1; setup();
            if(viewer) InvalidateRect(viewer,NULL,FALSE);
        }
        CloseDecompressor(decoder);
    }
    free(data); free(decoded);
}
