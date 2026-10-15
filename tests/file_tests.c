#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../src/files.c"
static char last_record[512];
int chat_ready(void) { return 1; }
int chat_media_idle(void) { return 1; }
unsigned long chat_session_id(void) { return 1; }
int chat_control_send(const char *record) { strcpy_s(last_record,sizeof(last_record),record); return 1; }
void chat_line(const wchar_t *text) { (void)text; }
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"Failed at line %d: %s\n",__LINE__,#c); return 1; } } while(0)
static int prepare(const wchar_t *directory,unsigned long long size) {
    swprintf_s(destination,MAX_PATH,L"%ls\\result.bin",directory);
    swprintf_s(temporary,MAX_PATH,L"%ls\\transfer.part",directory);
    receiving=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    receive_id=7; receive_size=size; receive_offset=0;
    return receiving!=INVALID_HANDLE_VALUE;
}
int main(void) {
    unsigned char bytes[256],decoded[256]; char hex[513]; size_t count=0;
    for(unsigned i=0;i<256;i++) bytes[i]=(unsigned char)i;
    media_hex(hex,bytes,256);
    CHECK(media_unhex(decoded,256,hex,&count) && count==256 && !memcmp(bytes,decoded,256));
    CHECK(!media_unhex(decoded,256,"0",&count));
    CHECK(!media_unhex(decoded,256,"gg",&count));
    CHECK(!media_unhex(decoded,1,"0001",&count));
    files_receive("OFFER 9 3 2e2e2f6576696c"); CHECK(!receive_id);
    files_receive("OFFER 9 3 610062"); CHECK(!receive_id);
    files_receive("OFFER 9 17179869185 61"); CHECK(!receive_id);
    files_receive("OFFER 9 3 746573742e62696e"); CHECK(receive_id==9 && pending);
    files_stop(); CHECK(!receive_id);
    wchar_t root[MAX_PATH],directory[MAX_PATH];
    CHECK(GetTempPathW(MAX_PATH,root));
    CHECK(GetTempFileNameW(root,L"rat",0,directory));
    CHECK(DeleteFileW(directory) && CreateDirectoryW(directory,NULL));
    CHECK(prepare(directory,3));
    files_receive("DATA 8 000102"); CHECK(receive_offset==0);
    files_receive("DATA 7 000102"); CHECK(receive_offset==3);
    files_receive("END 7"); CHECK(!receive_id && !strcmp(last_record,"FILE DONE 7\n"));
    HANDLE result=CreateFileW(destination,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    DWORD read=0; CHECK(result!=INVALID_HANDLE_VALUE);
    CHECK(ReadFile(result,decoded,256,&read,NULL) && read==3 && !memcmp(bytes,decoded,3));
    CloseHandle(result);
    /* Existing files are never replaced, even when created after the save dialog. */
    CHECK(prepare(directory,0)); files_receive("END 7");
    CHECK(!strcmp(last_record,"FILE REJECT 7\n"));
    CHECK(DeleteFileW(destination));
    CHECK(prepare(directory,3)); files_receive("DATA 7 00010203");
    CHECK(!receive_id && GetFileAttributesW(temporary)==INVALID_FILE_ATTRIBUTES);
    CHECK(prepare(directory,3)); files_receive("END 7");
    CHECK(!receive_id && GetFileAttributesW(destination)==INVALID_FILE_ATTRIBUTES);
    CHECK(prepare(directory,0)); files_receive("END 7");
    CHECK(!strcmp(last_record,"FILE DONE 7\n")); CHECK(DeleteFileW(destination));
    CHECK(prepare(directory,3)); files_stop();
    swprintf_s(temporary,MAX_PATH,L"%ls\\transfer.part",directory);
    CHECK(GetFileAttributesW(temporary)==INVALID_FILE_ATTRIBUTES); *temporary=0;
    CHECK(RemoveDirectoryW(directory));
    puts("File validation, binary/empty transfer, stale IDs, overwrite protection and cleanup passed.");
    return 0;
}
