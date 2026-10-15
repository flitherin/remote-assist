#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "chat.h"
#include "media_codec.h"
static IAudioClient *client;
static IAudioCaptureClient *capture;
static WAVEFORMATEX *mix;
static int sharing,com_owned,float_samples;
static HWAVEOUT playback;
static WAVEHDR headers[8];
static unsigned char playback_data[8][32768];
static DWORD playback_rate;
static WORD playback_channels;
static void playback_stop(void) {
    if(playback) {
        waveOutReset(playback);
        for(unsigned i=0;i<8;i++) if(headers[i].dwFlags&WHDR_PREPARED) waveOutUnprepareHeader(playback,&headers[i],sizeof(WAVEHDR));
        waveOutClose(playback); playback=NULL;
    }
    memset(headers,0,sizeof(headers));
}
extern "C" int audio_sharing(void) { return sharing; }
extern "C" void audio_stop(void) {
    sharing=0;
    if(client) client->Stop();
    if(capture) { capture->Release(); capture=NULL; }
    if(client) { client->Release(); client=NULL; }
    if(mix) { CoTaskMemFree(mix); mix=NULL; }
    if(com_owned) { CoUninitialize(); com_owned=0; }
    playback_stop(); audio_changed();
}
static int start_capture(void) {
    IMMDeviceEnumerator *enumerator=NULL; IMMDevice *device=NULL;
    HRESULT hr=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    com_owned=SUCCEEDED(hr);
    if(FAILED(hr) && hr!=RPC_E_CHANGED_MODE) return 0;
    hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),NULL,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void **)&enumerator);
    if(SUCCEEDED(hr)) hr=enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&device);
    if(SUCCEEDED(hr)) hr=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,NULL,(void **)&client);
    if(device) device->Release();
    if(enumerator) enumerator->Release();
    if(SUCCEEDED(hr)) hr=client->GetMixFormat(&mix);
    if(FAILED(hr)) return 0;
    WORD tag=mix->wFormatTag;
    if(tag==WAVE_FORMAT_EXTENSIBLE && mix->cbSize>=22) {
        WAVEFORMATEXTENSIBLE *extended=(WAVEFORMATEXTENSIBLE *)mix;
        if(IsEqualGUID(extended->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) tag=WAVE_FORMAT_IEEE_FLOAT;
        else if(IsEqualGUID(extended->SubFormat,KSDATAFORMAT_SUBTYPE_PCM)) tag=WAVE_FORMAT_PCM;
    }
    float_samples=tag==WAVE_FORMAT_IEEE_FLOAT && mix->wBitsPerSample==32;
    if((!float_samples && !(tag==WAVE_FORMAT_PCM && (mix->wBitsPerSample==16 || mix->wBitsPerSample==24 || mix->wBitsPerSample==32))) ||
       !mix->nChannels || mix->nChannels>8 || mix->nSamplesPerSec<8000 || mix->nSamplesPerSec>192000 ||
       mix->nBlockAlign!=mix->nChannels*(mix->wBitsPerSample/8)) return 0;
    hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,1000000,0,mix,NULL);
    if(SUCCEEDED(hr)) hr=client->GetService(__uuidof(IAudioCaptureClient),(void **)&capture);
    if(SUCCEEDED(hr)) hr=client->Start();
    return SUCCEEDED(hr);
}
extern "C" void audio_toggle(void) {
    if(!chat_ready() || !chat_is_host()) return;
    if(sharing) { audio_stop(); chat_control_send("AUDIO STOP\n"); chat_line(L"Трансляция звука остановлена."); return; }
    if(chat_peer_local()) { chat_line(L"Проверяйте трансляцию звука на двух компьютерах, чтобы избежать звуковой петли."); return; }
    if(!start_capture()) { audio_stop(); chat_line(L"Не удалось захватить системный звук. Проверьте устройство вывода Windows."); return; }
    sharing=1; audio_changed(); chat_line(L"Системный звук передаётся собеседнику. Повторная кнопка останавливает трансляцию.");
}
extern "C" void audio_tick(void) {
    if(!sharing || !capture) return;
    UINT32 frames=0;
    HRESULT hr=capture->GetNextPacketSize(&frames);
    /* Drain capture even on a congested connection, discarding old sound. */
    unsigned packets=0;
    while(SUCCEEDED(hr) && frames && packets++<32) {
        BYTE *data=NULL; DWORD flags=0;
        hr=capture->GetBuffer(&data,&frames,&flags,NULL,NULL);
        if(FAILED(hr)) break;
        if(chat_media_idle() && !(flags&AUDCLNT_BUFFERFLAGS_SILENT)) {
            unsigned char pcm[32768];
            size_t samples=(size_t)frames*mix->nChannels;
            if(samples<=sizeof(pcm)/2) {
                unsigned step=mix->wBitsPerSample/8;
                for(size_t i=0;i<samples;i++) {
                    short value;
                    if(float_samples) {
                        float f; memcpy(&f,data+i*step,4);
                        if(!isfinite(f)) f=0;
                        if(f>1) f=1; if(f< -1) f=-1;
                        value=(short)(f*32767);
                    } else memcpy(&value,data+i*step+step-2,2);
                    memcpy(pcm+i*2,&value,2);
                }
                char record[65632];
                int prefix=sprintf_s(record,sizeof(record),"AUDIO PCM %lu %u ",mix->nSamplesPerSec,(unsigned)mix->nChannels);
                media_hex(record+prefix,pcm,samples*2); strcat_s(record,sizeof(record),"\n");
                chat_control_send(record);
                if(!sharing || !capture) return;
            }
        }
        capture->ReleaseBuffer(frames);
        hr=capture->GetNextPacketSize(&frames);
    }
    if(FAILED(hr)) { audio_stop(); chat_control_send("AUDIO STOP\n"); chat_line(L"Трансляция звука остановлена: устройство вывода изменилось или недоступно."); }
}
extern "C" void audio_receive(const char *record) {
    if(chat_is_host()) return;
    if(!strcmp(record,"STOP")) { playback_stop(); return; }
    unsigned long rate=0; unsigned int channels=0; int end=0;
    if(sscanf_s(record,"PCM %lu %u %n",&rate,&channels,&end)!=2 || !end || rate<8000 || rate>192000 || !channels || channels>8) return;
    unsigned char pcm[32768]; size_t bytes=0;
    if(!media_unhex(pcm,sizeof(pcm),record+end,&bytes) || !bytes || bytes%(channels*2)) return;
    if(playback && (rate!=playback_rate || channels!=playback_channels)) playback_stop();
    if(!playback) {
        WAVEFORMATEX format={0}; format.wFormatTag=WAVE_FORMAT_PCM; format.nChannels=(WORD)channels;
        format.nSamplesPerSec=rate; format.wBitsPerSample=16; format.nBlockAlign=(WORD)(channels*2);
        format.nAvgBytesPerSec=rate*format.nBlockAlign;
        if(waveOutOpen(&playback,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR) { playback=NULL; return; }
        playback_rate=rate; playback_channels=(WORD)channels;
    }
    for(unsigned i=0;i<8;i++) {
        WAVEHDR *header=&headers[i];
        if((header->dwFlags&WHDR_PREPARED) && !(header->dwFlags&WHDR_DONE)) continue;
        if(header->dwFlags&WHDR_PREPARED) {
            if(waveOutUnprepareHeader(playback,header,sizeof(*header))!=MMSYSERR_NOERROR) continue;
        }
        memset(header,0,sizeof(*header)); memcpy(playback_data[i],pcm,bytes);
        header->lpData=(LPSTR)playback_data[i]; header->dwBufferLength=(DWORD)bytes;
        if(waveOutPrepareHeader(playback,header,sizeof(*header))==MMSYSERR_NOERROR) waveOutWrite(playback,header,sizeof(*header));
        return;
    }
}
