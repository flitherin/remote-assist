#include <winsock2.h>
#include <ws2tcpip.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include <iphlpapi.h>
#include <stdlib.h>
#include <time.h>
#include "chat.h"
static SOCKET listener=INVALID_SOCKET, peer=INVALID_SOCKET;
static HWND owner;
static int host, active, deciding, initialized;
static char input[FRAME_RECORD_LIMIT], output[FRAME_RECORD_LIMIT];
static size_t used, queued;
static unsigned long session_id;
static void audit(const wchar_t *event) {
    FILE *file=_wfopen(L"RemoteAssist-session.log",L"a, ccs=UTF-8");
    if(!file) return;
    time_t now=time(NULL); struct tm local_time;
    localtime_s(&local_time,&now);
    fwprintf(file,L"%04d-%02d-%02d %02d:%02d:%02d %ls\n",local_time.tm_year+1900,local_time.tm_mon+1,local_time.tm_mday,local_time.tm_hour,local_time.tm_min,local_time.tm_sec,event);
    fclose(file);
}
int chat_peer_local(void) {
    struct sockaddr_in remote={0}; int length=sizeof(remote);
    /* Fail closed if we cannot establish that the peer is another machine. */
    if(peer==INVALID_SOCKET || getpeername(peer,(struct sockaddr *)&remote,&length)) return 1;
    if((ntohl(remote.sin_addr.s_addr)>>24)==127) return 1;
    ULONG size=0;
    if(GetIpAddrTable(NULL,&size,FALSE)!=ERROR_INSUFFICIENT_BUFFER) return 1;
    PMIB_IPADDRTABLE addresses=malloc(size);
    if(!addresses) return 1;
    int local=1;
    if(GetIpAddrTable(addresses,&size,FALSE)==NO_ERROR) {
        local=0;
        for(DWORD i=0;i<addresses->dwNumEntries;i++)
            if(addresses->table[i].dwAddr==remote.sin_addr.s_addr) { local=1; break; }
    }
    free(addresses); return local;
}
static void disconnect(void) {
    if(active || peer!=INVALID_SOCKET) audit(L"Сеанс отключён");
    ++session_id;
    control_stop();
    KillTimer(owner,42);
    if(peer!=INVALID_SOCKET) { WSAAsyncSelect(peer,owner,0,0); closesocket(peer); }
    peer=INVALID_SOCKET; active=0; used=queued=0; screen_stop();
    assist_stop_local();
}
static void flush(void) {
    while(peer!=INVALID_SOCKET && queued) {
        int n=send(peer,output,(int)queued,0);
        if(n==SOCKET_ERROR) {
            if(WSAGetLastError()==WSAEWOULDBLOCK) return;
            disconnect(); chat_status(L"Соединение потеряно",0); return;
        }
        if(!n) { disconnect(); chat_status(L"Соединение закрыто",0); return; }
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
int chat_start(HWND window,int server,const wchar_t *address,unsigned short port) {
    WSADATA data; struct sockaddr_in endpoint;
    chat_stop(); owner=window; host=server;
    audit(server?L"Запущено ожидание подключения":L"Запущено подключение");
    if(!window || !address || !port) return 0;
    if(WSAStartup(MAKEWORD(2,2),&data)) return 0;
    initialized=1;
    memset(&endpoint,0,sizeof(endpoint)); endpoint.sin_family=AF_INET; endpoint.sin_port=htons(port);
    if(InetPtonW(AF_INET,address,&endpoint.sin_addr)!=1) { chat_stop(); return 0; }
    SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(s==INVALID_SOCKET) { chat_stop(); return 0; }
    if(server) {
        listener=s;
        BOOL exclusive=TRUE;
        setsockopt(s,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char *)&exclusive,sizeof(exclusive));
        if(bind(s,(const struct sockaddr *)&endpoint,sizeof(endpoint)) || listen(s,1) ||
           WSAAsyncSelect(s,window,CHAT_EVENT,FD_ACCEPT)) { chat_stop(); return 0; }
        chat_status(L"Ожидание запроса на чат",0);
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
        if(host && !strcmp(s,"CHAT/3")) {
            struct sockaddr_in remote={0}; int len=sizeof(remote); wchar_t ip[64]=L"?",question[400];
            getpeername(peer,(struct sockaddr *)&remote,&len);
            InetNtopW(AF_INET,&remote.sin_addr,ip,64);
            swprintf(question,400,L"Компьютер %ls запрашивает подключение.\n\nПринять соединение? Показ экрана и управление требуют отдельного согласия.\nЛичность участника не проверена.",ip);
            KillTimer(owner,42);
            deciding=1;
            int answer=MessageBoxW(owner,question,L"Входящий запрос на чат",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
            deciding=0;
            if(peer==INVALID_SOCKET) return;
            if(answer!=IDYES) { audit(L"Входящий запрос отклонён"); queue("REJECT\n"); disconnect(); chat_status(L"Запрос отклонён · ожидание нового запроса",0); return; }
            active=1;
            if(!queue("ACCEPT\n")) return;
        } else if(!host && !strcmp(s,"ACCEPT")) active=1;
        else { disconnect(); chat_status(L"Соединение отклонено или неверный протокол",0); return; }
        assist_confirm_local();
        KillTimer(owner,42); chat_status(L"Чат активен · управление по отдельному разрешению",1);
        audit(L"Подключение подтверждено"); chat_line(L"Сеанс текстового чата начат.");
    } else if(!host && !strncmp(s,"FRAME2 ",7)) { screen_receive(s+7);
    } else if(!host && !strcmp(s,"SCREEN_STOP")) { control_release(); screen_clear();
    } else if(!strncmp(s,"CTRL ",5)) { control_receive(s+5);
    } else if(!strncmp(s,"MSG ",4)) {
        wchar_t text[2048],display[2100];
        int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s+4,-1,text,2048);
        if(!n || n>1501 || n==1 || wcschr(text,L'\r')) { disconnect(); chat_status(L"Некорректное сообщение",0); return; }
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
        if(peer!=INVALID_SOCKET || deciding) { closesocket(incoming); return; }
        peer=incoming; used=queued=0; active=0;
        if(WSAAsyncSelect(peer,owner,CHAT_EVENT,FD_READ|FD_WRITE|FD_CLOSE)) {
            disconnect(); chat_status(L"Не удалось принять соединение",0); return;
        }
        SetTimer(owner,42,30000,NULL); break;
    }
    case FD_CONNECT: if(queue("CHAT/3\n")) chat_status(L"Ожидание согласия собеседника…",0); break;
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
                static char record[FRAME_RECORD_LIMIT]; size_t length=(size_t)(end-input);
                if(memchr(input,0,length)) { disconnect(); chat_status(L"Некорректное сообщение",0); break; }
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
    if(!active || !text || !*text || wcslen(text)>1500 || wcschr(text,L'\n') || wcschr(text,L'\r')) return 0;
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,record+4,sizeof(record)-6,NULL,NULL);
    if(!n) return 0;
    record[4+n-1]='\n'; record[4+n]=0;
    return queue(record);
}

int chat_ready(void) { return active; }
unsigned long chat_session_id(void) { return session_id; }
int chat_started(void) { return peer!=INVALID_SOCKET || listener!=INVALID_SOCKET; }
int chat_is_host(void) { return host; }
int chat_frame_idle(void) { return active && host && !queued; }
int chat_control_send(const char *record) { return active && record ? queue(record) : 0; }
void chat_timeout(void) {
    if(active || deciding || peer==INVALID_SOCKET) return;
    disconnect();
    chat_status(listener!=INVALID_SOCKET?L"Время ожидания истекло · ожидание нового запроса":L"Время ожидания истекло",0);
}
int chat_frame_send(const char *frame) {
    if(!active || !host || !frame) return 0;
    if(strncmp(frame,"FRAME2 ",7)==0 && (queued || strlen(frame)>sizeof(output)-128)) return 0;
    return queue(frame);
}
