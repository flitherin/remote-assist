#include <windows.h>
#include <stdio.h>
static unsigned int injected;
static UINT mock_input(UINT count,LPINPUT inputs,int size) {
    (void)inputs; (void)size; injected+=count; return count;
}
#define SendInput mock_input
#include "../src/control.c"
#undef SendInput
static int test_host=1,test_sharing=1,test_local=0;
int chat_peer_local(void) { return test_local; }
int chat_ready(void) { return 1; }
int chat_is_host(void) { return test_host; }
unsigned long chat_session_id(void) { return 1; }
int screen_sharing(void) { return test_sharing; }
int chat_control_send(const char *record) { (void)record; return 1; }
void chat_line(const wchar_t *text) { (void)text; }
void chat_stop(void) { control_stop(); }
void control_changed(int enabled) { (void)enabled; }
#define CHECK(condition) do { if(!(condition)) { fprintf(stderr,"Failed at line %d: %s\n",__LINE__,#condition); return 1; } } while(0)
int main(void) {
    control_receive("K 7 30 0 1"); CHECK(injected==0);
    allowed=1; epoch=7;
    test_local=1;
    control_receive("K 7 30 0 1");
    control_receive("M 7 100 200 1 1 0"); CHECK(injected==0);
    test_local=0;
    control_receive("K 6 30 0 1"); CHECK(injected==0);
    control_receive("K 7 30 0 1 trailing"); CHECK(injected==0);
    control_receive("K 7 30 0 2"); CHECK(injected==0);
    control_receive("M 7 -1 0 1 1 0"); CHECK(injected==0);
    control_receive("K 7 88 0 1"); CHECK(injected==0);
    control_receive("K 7 30 0 1"); CHECK(injected==1 && keys[30]);
    control_receive("M 7 100 200 1 1 0"); CHECK(injected==2 && buttons[0]);
    control_receive("RELEASE 6"); CHECK(injected==2);
    control_receive("RELEASE 7"); CHECK(injected==4 && !keys[30] && !buttons[0]);
    test_sharing=0; control_receive("K 7 30 0 1"); CHECK(injected==4);
    test_sharing=1; control_receive("K 7 30 0 1"); CHECK(injected==5);
    control_stop(); CHECK(injected==6 && !allowed);
    control_receive("K 7 30 0 1"); CHECK(injected==6);
    test_host=0; control_receive("ON 9 1920 1080"); CHECK(allowed && remote_epoch==9);
    control_receive("OFF"); CHECK(!allowed && !remote_epoch);
    puts("Control permission, stale commands, validation and release checks passed.");
    return 0;
}
