from pathlib import Path
import argparse,json,shutil,subprocess,sys,tempfile
p=argparse.ArgumentParser(description='Extract actual OneNet_Append/FillBuf only; host CRT formatting, no credentials/network.')
p.add_argument('--firmware-root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--clang',default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
a=p.parse_args()
def read(rel):return (a.firmware_root/rel).read_text(encoding='utf-8')
def function(t,sig):
    start=t.index(sig);end=t.index('{',start)+1;depth=1
    while depth:depth+=(t[end]=='{')-(t[end]=='}');end+=1
    return t[start:end]
raw=read('onenet/onenet/src/onenet.c')
code=function(raw,'static int OneNet_Append(')+'\n'+function(raw,'unsigned short OneNet_FillBuf(')
assert 'ACCESS_KEY' not in code and 'ESP8266_WIFI_INFO' not in code
libs=sorted(Path(r'C:\Program Files (x86)\Windows Kits\10\Lib').glob('*/ucrt/x64/ucrt.lib'))
ucrt=libs[-1];kernel=ucrt.parents[2]/'um/x64/kernel32.lib'
with tempfile.TemporaryDirectory(prefix='climate-json-') as td:
    out=Path(td)
    for name,rel in [('app_config.h','modules/app/inc/app_config.h'),('sensor_data.h','modules/middleware/inc/sensor_data.h')]:
        (out/name).write_text(read(rel),encoding='utf-8')
    harness=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "app_config.h"
#include "sensor_data.h"
#define ONENET_JSON_BUF_SIZE 512
int _fltused=0;
static RuntimeConfig config;
static SensorData data;
static uint16_t one_net_msg_id=1;
float water_vol=1.2f;
const RuntimeConfig *AppConfig_Get(void){return &config;}
const SensorData *SensorService_GetData(void){return &data;}
__declspec(dllimport) int __stdio_common_vsprintf(unsigned long long,char*,size_t,const char*,void*,va_list);
int vsnprintf(char *out,size_t n,const char *format,va_list ap){return __stdio_common_vsprintf(2,out,n,format,0,ap);}
__declspec(dllimport) void *GetStdHandle(uint32_t);
__declspec(dllimport) int WriteFile(void*,const void*,uint32_t,uint32_t*,void*);
CODE
int main(void){
    unsigned i,j;uint32_t wrote;char buffer[528];unsigned short n;
    config.temp_threshold=30;config.temp_low_threshold=20;config.co2_threshold=2000;
    config.led_mode=2;config.bump_mode=0;config.soil_low=30;config.soil_high=75;
    data.temperature_c=23;data.humidity_pct=55;data.light_lux=310;data.co2_ppm=800;data.soil_moisture_pct=50;data.valid_mask=11;
    for(i=0;i<3;++i){
        if(i==1){config.temp_threshold=100;config.temp_low_threshold=99;data.light_lux=65535;data.co2_ppm=65535;}
        if(i==2){config.temp_threshold=1;config.temp_low_threshold=0;}
        for(j=0;j<528;++j)buffer[j]='Z';
        n=OneNet_FillBuf(buffer);
        if(n==0||n>=512||buffer[n]!=0)return 10;
        for(j=512;j<528;++j)if(buffer[j]!='Z')return 11;
        WriteFile(GetStdHandle((uint32_t)-11),buffer,n,&wrote,0);
        WriteFile(GetStdHandle((uint32_t)-11),"\n",1,&wrote,0);
    }
    return 0;
}
'''.replace('CODE',code)
    src=out/'test.c';src.write_text(harness,encoding='utf-8');exe=out/'test.exe'
    cmd=[a.clang,'--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-O0','-Wall','-Wextra','-Werror','-I'+str(out),'-fuse-ld=lld','-nostdlib','-Wl,/entry:main,/subsystem:console',str(src),str(ucrt),str(kernel),'-o',str(exe)]
    r=subprocess.run(cmd,capture_output=True,text=True)
    if r.returncode:print(r.stdout+r.stderr);sys.exit(r.returncode)
    r=subprocess.run([str(exe)],capture_output=True,text=True)
    assert r.returncode==0,'Actual serializer failed length/guard checks'
    rows=[json.loads(line) for line in r.stdout.splitlines()];assert len(rows)==3
    for i,(row,pair) in enumerate(zip(rows,[(30,20),(100,99),(1,0)])):
        assert row['id']==str(i+1) and row['version']=='1.0'
        assert row['params']['tempthrehold']['value']==pair[0]
        assert row['params']['temp_low_threshold']['value']==pair[1]
        assert row['params']['sensor_status']['value']==11
        assert row['params']['CO2threhold']['value']==2000
        assert len(row['params'])==14
print('PASS: 3 actual OneJSON report vectors; independent JSON parsing, both endpoints, IDs, existing fields, buffer length and guard bytes. No credentials/network.')
