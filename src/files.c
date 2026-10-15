#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "chat.h"
#include "media_codec.h"
#define FILE_CHUNK 16384
#define FILE_MAX_SIZE (16ULL*1024*1024*1024)
static HANDLE sending=INVALID_HANDLE_VALUE,receiving=INVALID_HANDLE_VALUE;
static unsigned long next_id,send_id,receive_id;
static unsigned long long send_size,send_offset,receive_size,receive_offset;
static int accepted,pending,dialog_open,awaiting_done;
static wchar_t incoming_name[256],destination[MAX_PATH],temporary[MAX_PATH];
static ULONGLONG send_activity,receive_activity;
static int command(const char *verb,unsigned long id) {
    char record[80]; sprintf_s(record,sizeof(record),"FILE %s %lu\n",verb,id);
    return chat_control_send(record);
}
static void close_send(void) {
    if(sending!=INVALID_HANDLE_VALUE) CloseHandle(sending);
    sending=INVALID_HANDLE_VALUE; send_id=0; accepted=awaiting_done=0;
}
static void close_receive(void) {
    if(receiving!=INVALID_HANDLE_VALUE) CloseHandle(receiving);
    receiving=INVALID_HANDLE_VALUE;
    if(*temporary) DeleteFileW(temporary);
    *temporary=0; receive_id=0; pending=0;
}
void files_stop(void) { close_send(); close_receive(); }
void files_choose(HWND owner) {
    if(!chat_ready() || dialog_open) return;
    if(send_id) {
        if(MessageBoxW(owner,L"Отменить текущую отправку файла?",L"Передача файла",MB_YESNO|MB_DEFBUTTON2)==IDYES) {
            command("CANCEL",send_id); close_send(); chat_line(L"Отправка файла отменена.");
        }
        return;
    }
    wchar_t path[MAX_PATH]=L""; OPENFILENAMEW dialog={0};
    unsigned long session=chat_session_id();
    dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=owner; dialog.lpstrFile=path; dialog.nMaxFile=MAX_PATH;
    dialog.lpstrTitle=L"Выберите файл для отправки";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
    dialog_open=1; BOOL chosen=GetOpenFileNameW(&dialog); dialog_open=0;
    if(!chosen || !chat_ready() || session!=chat_session_id()) return;
    sending=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    LARGE_INTEGER size;
    if(sending==INVALID_HANDLE_VALUE || !GetFileSizeEx(sending,&size) || size.QuadPart<0 || (unsigned long long)size.QuadPart>FILE_MAX_SIZE) {
        close_send(); chat_line(L"Не удалось открыть файл. Максимальный размер — 16 ГиБ."); return;
    }
    wchar_t *name=wcsrchr(path,L'\\'); name=name?name+1:path;
    char utf8[1024],encoded[2049],record[2200];
    int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name,-1,utf8,sizeof(utf8),NULL,NULL);
    if(!bytes) { close_send(); return; }
    media_hex(encoded,(unsigned char *)utf8,(size_t)bytes-1);
    send_size=(unsigned long long)size.QuadPart; send_offset=0;
    if(!++next_id) ++next_id;
    send_id=next_id; send_activity=GetTickCount64();
    sprintf_s(record,sizeof(record),"FILE OFFER %lu %llu %s\n",send_id,send_size,encoded);
    if(!chat_control_send(record)) { close_send(); chat_line(L"Не удалось отправить предложение файла."); }
    else chat_line(L"Файл предложен. Ожидаем выбор получателя. Повторная кнопка отменяет отправку.");
}
static int safe_name(const wchar_t *name) {
    size_t n=wcslen(name);
    if(!n || n>240 || name[n-1]==L'.' || name[n-1]==L' ') return 0;
    for(size_t i=0;i<n;i++) if(name[i]<32 || wcschr(L"\\/:*?\"<>|",name[i])) return 0;
    return 1;
}
void files_receive(const char *record) {
    unsigned long id=0; unsigned long long size=0; int end=0;
    if(sscanf_s(record,"OFFER %lu %llu %n",&id,&size,&end)==2 && end && id) {
        unsigned char utf8[1024]; size_t bytes=0; wchar_t name[256];
        if(receive_id || size>FILE_MAX_SIZE || !media_unhex(utf8,sizeof(utf8)-1,record+end,&bytes) || !bytes || memchr(utf8,0,bytes)) { command("REJECT",id); return; }
        utf8[bytes]=0;
        if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(char *)utf8,-1,name,256) || !safe_name(name)) { command("REJECT",id); return; }
        wcscpy_s(incoming_name,256,name); receive_id=id; receive_size=size; receive_offset=0;
        pending=1; receive_activity=GetTickCount64(); return;
    }
    end=0;
    if(sscanf_s(record,"DATA %lu %n",&id,&end)==1 && end) {
        unsigned char data[FILE_CHUNK]; size_t bytes=0; DWORD written=0;
        if(id!=receive_id || receiving==INVALID_HANDLE_VALUE) return;
        if(!media_unhex(data,sizeof(data),record+end,&bytes) || !bytes || bytes>receive_size-receive_offset ||
           !WriteFile(receiving,data,(DWORD)bytes,&written,NULL) || written!=bytes) {
            command("REJECT",id); close_receive(); chat_line(L"Приём файла прерван: неверные данные или ошибка записи."); return;
        }
        receive_offset+=bytes; receive_activity=GetTickCount64(); return;
    }
    char verb[16],extra;
    if(sscanf_s(record,"%15s %lu %c",verb,(unsigned)sizeof(verb),&id,&extra,1U)!=2) return;
    if(!strcmp(verb,"ACCEPT") && id==send_id && !awaiting_done) { accepted=1; send_activity=GetTickCount64(); }
    else if(!strcmp(verb,"REJECT") && id==send_id) { close_send(); chat_line(L"Получатель отклонил или прервал приём файла."); }
    else if(!strcmp(verb,"CANCEL") && id==receive_id) { close_receive(); chat_line(L"Собеседник отменил отправку файла."); }
    else if(!strcmp(verb,"DONE") && id==send_id && awaiting_done) { close_send(); chat_line(L"Файл доставлен и сохранён получателем."); }
    else if(!strcmp(verb,"END") && id==receive_id && receiving!=INVALID_HANDLE_VALUE) {
        int ok=receive_offset==receive_size && FlushFileBuffers(receiving);
        CloseHandle(receiving); receiving=INVALID_HANDLE_VALUE;
        /* Never overwrite an existing destination, including a file created during transfer. */
        if(ok) ok=MoveFileExW(temporary,destination,MOVEFILE_WRITE_THROUGH)!=0;
        command(ok?"DONE":"REJECT",id);
        if(ok) { *temporary=0; chat_line(L"Файл получен и сохранён."); }
        else chat_line(L"Не удалось сохранить файл: проверьте свободное место и имя назначения.");
        close_receive();
    }
}
void files_tick(HWND owner) {
    if(dialog_open || !chat_ready()) return;
    if(pending) {
        unsigned long session=chat_session_id(),id=receive_id;
        wchar_t question[512];
        swprintf_s(question,512,L"Принять файл «%ls»?\nРазмер: %llu байт.\n\nВыберите новое имя для сохранения.",incoming_name,receive_size);
        dialog_open=1; int answer=MessageBoxW(owner,question,L"Входящий файл",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
        BOOL chosen=FALSE;
        if(answer==IDYES && session==chat_session_id() && receive_id==id) {
            OPENFILENAMEW dialog={0}; wcscpy_s(destination,MAX_PATH,incoming_name);
            dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=owner; dialog.lpstrFile=destination; dialog.nMaxFile=MAX_PATH;
            dialog.lpstrTitle=L"Сохранить полученный файл (новое имя)";
            dialog.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
            chosen=GetSaveFileNameW(&dialog);
        }
        dialog_open=0;
        if(session!=chat_session_id() || receive_id!=id) return;
        pending=0;
        if(!chosen || GetFileAttributesW(destination)!=INVALID_FILE_ATTRIBUTES || wcslen(destination)>MAX_PATH-48) { command("REJECT",id); close_receive(); return; }
        swprintf_s(temporary,MAX_PATH,L"%ls.ra-%lu-%lu.part",destination,GetCurrentProcessId(),id);
        receiving=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(receiving==INVALID_HANDLE_VALUE) {
            *temporary=0; command("REJECT",id); close_receive(); chat_line(L"Не удалось создать файл для приёма."); return;
        }
        receive_activity=GetTickCount64();
        if(!command("ACCEPT",id)) close_receive();
    }
    ULONGLONG now=GetTickCount64();
    if(send_id && now-send_activity>300000) { command("CANCEL",send_id); close_send(); chat_line(L"Время ожидания передачи файла истекло."); }
    if(receive_id && now-receive_activity>300000) { command("REJECT",receive_id); close_receive(); chat_line(L"Приём файла остановлен по тайм-ауту."); }
    if(!send_id || !accepted || awaiting_done || !chat_media_idle()) return;
    if(send_offset==send_size) {
        awaiting_done=1; send_activity=now;
        if(!command("END",send_id)) close_send();
    } else {
        unsigned char data[FILE_CHUNK]; char record[FILE_CHUNK*2+80]; DWORD bytes=0;
        DWORD wanted=(DWORD)((send_size-send_offset)>FILE_CHUNK?FILE_CHUNK:send_size-send_offset);
        if(!ReadFile(sending,data,wanted,&bytes,NULL) || bytes!=wanted) { command("CANCEL",send_id); close_send(); chat_line(L"Ошибка чтения отправляемого файла."); return; }
        int prefix=sprintf_s(record,sizeof(record),"FILE DATA %lu ",send_id);
        media_hex(record+prefix,data,bytes); strcat_s(record,sizeof(record),"\n");
        if(chat_control_send(record)) { send_offset+=bytes; send_activity=now; }
        else { command("CANCEL",send_id); close_send(); }
    }
}
