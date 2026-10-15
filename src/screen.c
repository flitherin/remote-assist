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
static HCURSOR viewer_cursor;
static int sharing, available;
static BITMAPINFO info;
static int releasing_capture;
static HDC paint_dc;
static HBITMAP paint_bitmap;
static HGDIOBJ paint_original;
static int paint_width,paint_height;
static HDC capture_dc;
static HBITMAP capture_bitmap;
static HGDIOBJ capture_original;
static void *capture_bits;
static COMPRESSOR_HANDLE capture_compressor;
static unsigned char *compressed_frame;
static char *encoded_frame;
static HDC desktop_copy_dc;
static HBITMAP desktop_copy_bitmap;
static HGDIOBJ desktop_copy_original;
static int desktop_copy_width,desktop_copy_height;
static void discard_desktop_copy(void) {
    if(desktop_copy_dc && desktop_copy_original) SelectObject(desktop_copy_dc,desktop_copy_original);
    if(desktop_copy_bitmap) DeleteObject(desktop_copy_bitmap);
    if(desktop_copy_dc) DeleteDC(desktop_copy_dc);
    desktop_copy_dc=NULL; desktop_copy_bitmap=NULL; desktop_copy_original=NULL;
    desktop_copy_width=desktop_copy_height=0;
}
static int prepare_desktop_copy(HDC desktop,int width,int height) {
    if(desktop_copy_dc && desktop_copy_width==width && desktop_copy_height==height) return 1;
    discard_desktop_copy();
    desktop_copy_dc=CreateCompatibleDC(desktop);
    desktop_copy_bitmap=CreateCompatibleBitmap(desktop,width,height);
    if(!desktop_copy_dc || !desktop_copy_bitmap) { discard_desktop_copy(); return 0; }
    desktop_copy_original=SelectObject(desktop_copy_dc,desktop_copy_bitmap);
    if(!desktop_copy_original || desktop_copy_original==HGDI_ERROR) {
        desktop_copy_original=NULL; discard_desktop_copy(); return 0;
    }
    desktop_copy_width=width; desktop_copy_height=height;
    return 1;
}
static void discard_capture(void) {
    discard_desktop_copy();
    if(capture_dc && capture_original) SelectObject(capture_dc,capture_original);
    if(capture_bitmap) DeleteObject(capture_bitmap);
    if(capture_dc) DeleteDC(capture_dc);
    if(capture_compressor) CloseCompressor(capture_compressor);
    free(compressed_frame); free(encoded_frame);
    capture_dc=NULL; capture_bitmap=NULL; capture_original=NULL; capture_bits=NULL;
    capture_compressor=NULL; compressed_frame=NULL; encoded_frame=NULL;
}
static int prepare_capture(HDC desktop) {
    if(capture_dc) return 1;
    capture_dc=CreateCompatibleDC(desktop);
    capture_bitmap=CreateDIBSection(desktop,&info,DIB_RGB_COLORS,&capture_bits,NULL,0);
    if(!capture_dc || !capture_bitmap) { discard_capture(); return 0; }
    capture_original=SelectObject(capture_dc,capture_bitmap);
    if(!capture_original || capture_original==HGDI_ERROR) { capture_original=NULL; discard_capture(); return 0; }
    compressed_frame=malloc(sizeof(pixels)+65536);
    encoded_frame=malloc((sizeof(pixels)+65536)*2+9);
    if(!compressed_frame || !encoded_frame || !CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,NULL,&capture_compressor)) {
        discard_capture(); return 0;
    }
    return 1;
}
static void discard_paint_buffer(void) {
    if(paint_dc && paint_original) SelectObject(paint_dc,paint_original);
    if(paint_bitmap) DeleteObject(paint_bitmap);
    if(paint_dc) DeleteDC(paint_dc);
    paint_dc=NULL; paint_bitmap=NULL; paint_original=NULL;
    paint_width=paint_height=0;
}
static int prepare_paint_buffer(HDC target,int width,int height) {
    if(width<=0 || height<=0) return 0;
    if(paint_dc && paint_width==width && paint_height==height) return 1;
    discard_paint_buffer();
    paint_dc=CreateCompatibleDC(target);
    paint_bitmap=CreateCompatibleBitmap(target,width,height);
    if(!paint_dc || !paint_bitmap) { discard_paint_buffer(); return 0; }
    paint_original=SelectObject(paint_dc,paint_bitmap);
    if(!paint_original || paint_original==HGDI_ERROR) { paint_original=NULL; discard_paint_buffer(); return 0; }
    paint_width=width; paint_height=height;
    return 1;
}
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
    /* Handle the client cursor locally instead of forwarding to the parent. */
    if(msg==WM_SETCURSOR && LOWORD(lp)==HTCLIENT) {
        SetCursor(viewer_cursor); return TRUE;
    }
    /* The completed image covers the entire client area. */
    if(msg==WM_ERASEBKGND) return 1;
    if(msg==WM_NCDESTROY) { discard_paint_buffer(); viewer=NULL; }
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
        if(!prepare_paint_buffer(dc,r.right,r.bottom)) { EndPaint(window,&ps); return 0; }
        HDC frame=paint_dc;
        FillRect(frame,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));
        if(available) {
            int width=r.right,height=width*H/W;
            if(height>r.bottom) { height=r.bottom; width=height*W/H; }
            SetStretchBltMode(frame,HALFTONE); SetBrushOrgEx(frame,0,0,NULL);
            StretchDIBits(frame,(r.right-width)/2,(r.bottom-height)/2,width,height,0,0,W,H,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
        } else {
            SetBkMode(frame,TRANSPARENT); SetTextColor(frame,RGB(220,220,220));
            DrawTextW(frame,L"Экран не передаётся",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
        BitBlt(dc,0,0,r.right,r.bottom,frame,0,0,SRCCOPY);
        EndPaint(window,&ps); return 0;
    }
    return DefWindowProcW(window,msg,wp,lp);
}
HWND screen_create(HWND parent) {
    WNDCLASSW cls={0}; cls.hInstance=GetModuleHandleW(NULL); cls.lpfnWndProc=view_proc;
    viewer_cursor=LoadCursorW(NULL,IDC_ARROW);
    cls.lpszClassName=L"AssistReadOnlyView2"; cls.hCursor=viewer_cursor;
    RegisterClassW(&cls);
    viewer=CreateWindowW(cls.lpszClassName,L"Экран собеседника",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,0,0,parent,NULL,cls.hInstance,NULL);
    setup(); return viewer;
}
void screen_clear(void) { available=0; memset(pixels,0,sizeof(pixels)); if(viewer) InvalidateRect(viewer,NULL,FALSE); }
void screen_stop(void) { sharing=0; discard_capture(); screen_clear(); }
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
    setup();
    HDC desktop=GetDC(NULL); if(!desktop) return;
    if(!prepare_capture(desktop)) { ReleaseDC(NULL,desktop); return; }
    int sw=GetSystemMetrics(SM_CXSCREEN),sh=GetSystemMetrics(SM_CYSCREEN);
    if(sw<=0 || sh<=0) { ReleaseDC(NULL,desktop); return; }
    if(!prepare_desktop_copy(desktop,sw,sh)) { ReleaseDC(NULL,desktop); return; }
    /* Keep the screen read short; perform expensive scaling in memory.
       CAPTUREBLT can disturb the source cursor with some display drivers. */
    BOOL copied=BitBlt(desktop_copy_dc,0,0,sw,sh,desktop,0,0,SRCCOPY);
    BOOL copy_completed=GdiFlush();
    ReleaseDC(NULL,desktop);
    if(!copied || !copy_completed) return;
    int w=W,h=MulDiv(sh,W,sw); if(h>H) { h=H; w=MulDiv(sw,H,sh); }
    PatBlt(capture_dc,0,0,W,H,BLACKNESS);
    SetStretchBltMode(capture_dc,HALFTONE); SetBrushOrgEx(capture_dc,0,0,NULL);
    BOOL captured=StretchBlt(capture_dc,(W-w)/2,(H-h)/2,w,h,desktop_copy_dc,0,0,sw,sh,SRCCOPY);
    BOOL completed=GdiFlush();
    if(!captured || !completed) return;
    SIZE_T packed=0;
    if(!Compress(capture_compressor,capture_bits,sizeof(pixels),compressed_frame,sizeof(pixels)+65536,&packed)) return;
    memcpy(encoded_frame,"FRAME2 ",7);
    static const char hex[]="0123456789abcdef";
    for(SIZE_T i=0;i<packed;i++) {
        encoded_frame[7+i*2]=hex[compressed_frame[i]>>4];
        encoded_frame[8+i*2]=hex[compressed_frame[i]&15];
    }
    encoded_frame[7+packed*2]='\n'; encoded_frame[8+packed*2]=0;
    chat_frame_send(encoded_frame);
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
