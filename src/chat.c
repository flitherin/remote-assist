#include <winsock2.h>
#include <ws2tcpip.h>
#include <wchar.h>
#include <stdio.h>
#include "chat.h"
static SOCKET listener=INVALID_SOCKET, peer=INVALID_SOCKET;
static HWND owner;
static int host, active, deciding, initialized;
static char input[500000], output[600000];
static size_t used, queued;
static void disconnect(void) {
    if(peer!=INVALID_SOCKET) { WSAAsyncSelect(peer,owner,0,0); closesocket(peer); }
    peer=INVALID_SOCKET; active=0; used=queued=0; screen_stop();
}
static void flush(void) {
    while(peer!=INVALID_SOCKET && queued) {
        int n=send(peer,output,(int)queued,0);
        if(n==SOCKET_ERROR) {
            if(WSAGetLastError()==WSAEWOULDBLOCK) return;
            disconnect(); chat_status(L"Соединение потеряно",0); return;
        }
        if(!n) return;
        memmove(output,output+n,queued-(size_t)n); queued-=(size_t)n;
    }
}
static int queue(const char *s) {
    size_t n=strlen(s);
    if(peer==INVALID_SOCKET || n>sizeof(output)-queued) return 0;
    memcpy(output+queued,s,n); queued+=n; flush(); return peer!=INVALID_SOCKET;
}
void chat_stop(void) {
    disconnect();
    if(listener!=INVALID_SOCKET) { WSAAsyncSelect(listener,owner,0,0); closesocket(listener); }
    listener=INVALID_SOCKET;
    if(initialized) { WSACleanup(); initialized=0; }
}
int chat_start(HWND window,int server,const wchar_t *address) {
    WSADATA data; struct sockaddr_in endpoint;
    chat_stop(); owner=window; host=server;
    if(WSAStartup(MAKEWORD(2,2),&data)) return 0;
    initialized=1;
    memset(&endpoint,0,sizeof(endpoint)); endpoint.sin_family=AF_INET; endpoint.sin_port=htons(45873);
    if(InetPtonW(AF_INET,address,&endpoint.sin_addr)!=1) { chat_stop(); return 0; }
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(s==INVALID_SOCKET) { chat_stop(); return 0; }
    if(server) {
        listener=s;
        BOOL exclusive=TRUE;
        setsockopt(s,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char *)&exclusive,sizeof(exclusive));
        if(bind(s,(const struct sockaddr *)&endpoint,sizeof(endpoint)) || listen(s,1) ||
           WSAAsyncSelect(s,window,CHAT_EVENT,FD_ACCEPT)) { chat_stop(); return 0; }
        chat_status(L"Ожидание запроса на чат · порт 45873",0);
    } else {
        peer=s;
        if(WSAAsyncSelect(s,window,CHAT_EVENT,FD_CONNECT|FD_READ|FD_WRITE|FD_CLOSE)) { chat_stop(); return 0; }
        if(connect(s,(const struct sockaddr *)&endpoint,sizeof(endpoint))==SOCKET_ERROR && WSAGetLastError()!=WSAEWOULDBLOCK) { chat_stop(); return 0; }
        chat_status(L"Подключение к чату…",0);
    }
    if(!server) SetTimer(window,42,30000,NULL);
    return 1;
}
static void line(char *s) {
    if(!active) {
        if(host && !strcmp(s,"CHAT/1")) {
            struct sockaddr_in remote; int len=sizeof(remote); wchar_t ip[64]=L"?",question[400];
            getpeername(peer,(struct sockaddr *)&remote,&len);
            InetNtopW(AF_INET,&remote.sin_addr,ip,64);
            swprintf(question,400,L"Компьютер %ls запрашивает текстовый чат.\n\nПринять соединение? Экран и управление недоступны.\nЛичность участника не проверена.",ip);
            KillTimer(owner,42);
            deciding=1;
            int answer=MessageBoxW(owner,question,L"Входящий запрос на чат",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
            deciding=0;
            if(peer==INVALID_SOCKET) return;
            if(answer!=IDYES) { queue("REJECT\n"); disconnect(); chat_status(L"Запрос отклонён · ожидание нового запроса",0); return; }
            active=1; queue("ACCEPT\n");
        } else if(!host && !strcmp(s,"ACCEPT")) active=1;
        else { disconnect(); chat_status(L"Соединение отклонено или неверный протокол",0); return; }
        KillTimer(owner,42); chat_status(L"Чат активен · управление компьютером отсутствует",1);
        chat_line(L"Сеанс текстового чата начат.");
    } else if(!host && !strncmp(s,"FRAME ",6)) { screen_receive(s+6);
    } else if(!strncmp(s,"MSG ",4)) {
        wchar_t text[2048],display[2100];
        int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s+4,-1,text,2048);
        if(!n) { disconnect(); chat_status(L"Некорректное сообщение",0); return; }
        swprintf(display,2100,L"Собеседник: %ls",text); chat_line(display);
    } else { disconnect(); chat_status(L"Ошибка протокола",0); }
}
void chat_event(WPARAM socket,LPARAM event) {
    SOCKET s=(SOCKET)socket;
    if(s!=peer && s!=listener) return;
    if(WSAGETSELECTERROR(event)) { disconnect(); chat_status(L"Ошибка соединения",0); return; }
    switch(WSAGETSELECTEVENT(event)) {
    case FD_ACCEPT: {
        SOCKET incoming=accept(listener,NULL,NULL);
        if(incoming==INVALID_SOCKET) return;
        if(peer!=INVALID_SOCKET) { closesocket(incoming); return; }
        peer=incoming; used=queued=0; active=0;
        WSAAsyncSelect(peer,owner,CHAT_EVENT,FD_READ|FD_WRITE|FD_CLOSE);
        SetTimer(owner,42,30000,NULL); break;
    }
    case FD_CONNECT: queue("CHAT/1\n"); chat_status(L"Ожидание согласия собеседника…",0); break;
    case FD_WRITE: flush(); break;
    case FD_CLOSE: disconnect(); chat_status(L"Собеседник завершил сеанс",0); KillTimer(owner,42); break;
    case FD_READ:
        if(deciding) return;
        while(peer!=INVALID_SOCKET) {
            int n=recv(peer,input+used,(int)(sizeof(input)-1-used),0);
            if(n==SOCKET_ERROR) { if(WSAGetLastError()!=WSAEWOULDBLOCK) { disconnect(); chat_status(L"Ошибка чтения",0); } break; }
            if(!n) { disconnect(); chat_status(L"Собеседник отключился",0); break; }
            used+=(size_t)n; input[used]=0;
            char *end;
            while(peer!=INVALID_SOCKET && (end=memchr(input,'\n',used))!=NULL) {
                static char record[500000]; size_t length=(size_t)(end-input);
                memcpy(record,input,length); record[length]=0;
                memmove(input,end+1,used-length-1); used-=length+1;
                line(record);
            }
            if(used==sizeof(input)-1) { disconnect(); chat_status(L"Сообщение слишком длинное",0); break; }
        }
        break;
    }
}
int chat_send(const wchar_t *text) {
    char record[8192]="MSG ";
    if(!active || !*text || wcschr(text,L'\n') || wcschr(text,L'\r')) return 0;
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,record+4,sizeof(record)-6,NULL,NULL);
    if(!n) return 0;
    record[4+n-1]='\n'; record[4+n]=0;
    return queue(record);
}

int chat_ready(void) { return active; }
int chat_frame_send(const char *frame) { if(!active || !host || queued) return 0; return queue(frame); }
