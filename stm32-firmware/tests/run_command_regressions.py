from pathlib import Path
import argparse,json,re,shutil,subprocess,sys,tempfile
p=argparse.ArgumentParser(description='Production command core, app adapter, real cJSON/OneJSON and extracted MQTT dispatch; mocked hardware only.')
p.add_argument('--firmware-root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--overlay',type=Path)
p.add_argument('--clang',default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
p.add_argument('--windows-sdk',type=Path,default=Path(r'C:\Program Files (x86)\Windows Kits\10'))
a=p.parse_args()
def read(rel):
    file=a.overlay/rel if a.overlay else a.firmware_root/rel
    return (file if file.exists() else a.firmware_root/rel).read_text(encoding='utf-8')
versions=sorted((a.windows_sdk/'Lib').glob('*/ucrt/x64/ucrt.lib'))
if not versions:sys.exit('Windows SDK ucrt.lib is required; set --windows-sdk.')
ucrt=versions[-1];kernel=ucrt.parents[2]/'um/x64/kernel32.lib'
assert kernel.exists(),kernel
with tempfile.TemporaryDirectory(prefix='stm32-commands-') as temp:
    out=Path(temp);inc=out/'include';inc.mkdir()
    def header(name,text):(inc/name).write_text(text,encoding='utf-8')
    for name,rel in [('app_config.h','modules/app/inc/app_config.h'),('app_commands.h','modules/app/inc/app_commands.h'),('mw_command.h','modules/middleware/inc/mw_command.h'),('onenet_command.h','onenet/onenet/inc/onenet_command.h'),('cJSON.h','onenet/CJSON/cJSON.h')]:header(name,read(rel))
    header('main.h','''#include <stdint.h>
uint32_t __get_PRIMASK(void);void __disable_irq(void);void __set_PRIMASK(uint32_t value);
''')
    header('LED.h','void LED_PID_Reset(void);void Mode_Switch(void);\n')
    header('mw_config.h','void Config_MarkDirty(void);\n')
    header('string.h','''#include <stddef.h>
void *memset(void*,int,size_t);void *memcpy(void*,const void*,size_t);void *memchr(const void*,int,size_t);
size_t strlen(const char*);char *strcpy(char*,const char*);char *strcat(char*,const char*);int strcmp(const char*,const char*);int strncmp(const char*,const char*,size_t);char *strchr(const char*,int);
''')
    header('stdlib.h','#include <stddef.h>\nvoid *malloc(size_t);void free(void*);\n')
    header('stdio.h','int sprintf(char*,const char*,...);\n')
    header('math.h','double pow(double,double);double floor(double);double fabs(double);\n')
    header('ctype.h','int tolower(int);\n')
    header('limits.h','#define INT_MAX 2147483647\n#define INT_MIN (-2147483647-1)\n')
    header('float.h','#define DBL_EPSILON 2.2204460492503131e-16\n#define DBL_MAX 1.7976931348623157e308\n')
    raw=read('onenet/onenet/src/onenet.c')
    start=raw.index('void OneNet_RevPro(unsigned char *cmd)');body=raw.index('{',start);end=body+1;depth=1
    while depth:
        if raw[end]=='{':depth+=1
        elif raw[end]=='}':depth-=1
        end+=1
    dispatch=raw[start:end]
    start=raw.index('_Bool OneNet_SendData(void)');body=raw.index('{',start);end=body+1;depth=1
    while depth:
        if raw[end]=='{':depth+=1
        elif raw[end]=='}':depth-=1
        end+=1
    send_function=raw[start:end]
    main=read('Core/Src/main.c');start=main.index('                now_tick = HAL_GetTick();');body=main.index('{',start);end=body+1;depth=1
    while depth:
        if main[end]=='{':depth+=1
        elif main[end]=='}':depth-=1
        end+=1
    report_step=main[start:end]
    # Only this dispatch function is extracted; credentials/authentication are never compiled or copied.
    harness=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "app_commands.h"
#include "app_config.h"
#include "onenet_command.h"
#include "cJSON.h"
#define PROID "test-product"
#define DEVICE_NAME "test-device"
#define MQTT_PKT_PUBLISH 3
#define MQTT_PKT_PUBACK 4
#define MQTT_PKT_SUBACK 9
#define MQTT_PKT_CMD 6
int _fltused=0;
static unsigned checks,dirty,mode_switches,pid_resets,unexpected_print;
/* Memory intrinsics normally live in the MSVC runtime; provide portable test shims.
 * cJSON printing is not used by this test, and any accidental call fails the final assertion. */
void *memcpy(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
void *memset(void *d,int c,size_t n){unsigned char *a=d;while(n--)*a++=(unsigned char)c;return d;}
void *memchr(const void *s,int c,size_t n){const unsigned char *a=s;while(n--){if(*a==(unsigned char)c)return (void*)a;++a;}return NULL;}
char *strchr(const char *s,int c){do{if(*s==(char)c)return (char*)s;}while(*s++);return NULL;}
int sprintf(char *out,const char *format,...){(void)out;(void)format;++unexpected_print;return -1;}
static uint32_t irq;
uint32_t __get_PRIMASK(void){return irq;}void __disable_irq(void){irq=1;}void __set_PRIMASK(uint32_t v){irq=v;}
void Config_MarkDirty(void){++dirty;}void LED_PID_Reset(void){++pid_resets;}void Mode_Switch(void){++mode_switches;}
__declspec(dllimport) void *GetStdHandle(uint32_t);
__declspec(dllimport) int WriteFile(void*,const void*,uint32_t,uint32_t*,void*);
static void text(const char *s){uint32_t wrote;WriteFile(GetStdHandle((uint32_t)-11),s,(uint32_t)strlen(s),&wrote,NULL);}
static void number(unsigned n){char b[16];unsigned i=15;b[i]=0;do{b[--i]=(char)('0'+n%10);n/=10;}while(n);text(b+i);}
#define CHECK(c) do{++checks;if(!(c)){text("FAIL command test line ");number(__LINE__);text("\n");return __LINE__;}}while(0)
static unsigned allocations,live,fail_at,failed;
static void *allocate(size_t size){void *p;++allocations;if(fail_at&&allocations==fail_at){failed=1;return NULL;}p=malloc(size);if(p)++live;return p;}
static void release(void *p){if(p){--live;free(p);}}
static void reset(void){AppConfig_Init();AppCommands_ReportSent();dirty=mode_switches=pid_resets=0;}
static const char *topic,*packet;static unsigned packet_length,topic_length,published,freed,cleared;
static char last_reply[160],last_topic[100];
static unsigned char MQTT_UnPacketRecv(unsigned char *cmd){(void)cmd;return MQTT_PKT_PUBLISH;}
static short MQTT_UnPacketPublish(unsigned char *cmd,char **t,unsigned short *tl,char **p,unsigned short *pl,unsigned char *q,unsigned short *id)
{(void)cmd;*t=(char*)topic;*tl=(unsigned short)topic_length;*p=(char*)packet;*pl=(unsigned short)packet_length;*q=0;*id=1;return 0;}
static int MQTT_UnPacketPublishAck(unsigned char *cmd){(void)cmd;return 0;}
static int MQTT_UnPacketSubscribe(unsigned char *cmd){(void)cmd;return 0;}
static void MQTT_FreeBuffer(char *p){if(p)++freed;}
static void ESP8266_Clear(void){++cleared;}
static void OneNET_Publish(const char *t,const char *r){++published;strcpy(last_topic,t);strcpy(last_reply,r);}
DISPATCH
#define ONENET_JSON_BUF_SIZE 512
typedef struct {unsigned char *_data;unsigned short _len;unsigned short _size;unsigned char flag;} MQTT_PACKET_STRUCTURE;
static unsigned char tx_buffer[32];static unsigned tx_calls,tx_deletes;
static short fill_length=3;static unsigned char packet_status,send_status;static _Bool one_net_connected;
static uint32_t test_tick;static uint32_t last_report_attempt_tick;static uint8_t force_periodic;
static uint32_t HAL_GetTick(void){return test_tick;}
static short OneNet_FillBuf(char *buf){strcpy(buf,"abc");return fill_length;}
static int MQTT_PacketSaveData(const char *p,const char *d,short length,void *unused,MQTT_PACKET_STRUCTURE *packet)
{(void)p;(void)d;(void)length;(void)unused;packet->_data=tx_buffer;packet->_len=0;return packet_status;}
static unsigned char ESP8266_SendData(unsigned char *data,unsigned short length){(void)data;(void)length;++tx_calls;return send_status;}
static void MQTT_DeleteBuffer(MQTT_PACKET_STRUCTURE *packet){(void)packet;++tx_deletes;}
static _Bool OneNet_IsConnected(void){return one_net_connected;}
SEND_FUNCTION
static void main_report_step(void){uint32_t now_tick;uint8_t periodic_report_due=force_periodic;MAIN_REPORT}
static unsigned reply_code(const char *reply){cJSON *r=cJSON_Parse(reply),*code;unsigned result=0;if(!r)return 0;code=cJSON_GetObjectItem(r,"code");if(code)result=(unsigned)code->valueint;cJSON_Delete(r);return result;}
static unsigned invoke(const char *json){char reply[160];return OneNetCommand_Handle(json,reply,sizeof(reply))?reply_code(reply):0;}
int main(void)
{
    cJSON_Hooks hooks={allocate,release};CommandEntry e[6];AppCommandResult result;CommandState before,desired;
    unsigned i,j;char reply[160];cJSON *n;
    cJSON_InitHooks(&hooks);reset();
    CHECK(Command_FindKey("led")==COMMAND_LED);CHECK(Command_FindKey("LED")==COMMAND_KEY_COUNT);CHECK(Command_FindKey(NULL)==COMMAND_KEY_COUNT);
    CHECK(AppCommands_Execute(NULL,0).status==COMMAND_BAD_REQUEST);CHECK(dirty==0);
    e[0].key=COMMAND_TEMP;e[0].value=26;result=AppCommands_Execute(e,1);
    CHECK(result.status==COMMAND_OK&&result.changed);CHECK(AppConfig_Get()->temp_threshold==26);CHECK(dirty==1);CHECK(AppCommands_ReportPending());
    CHECK(!AppCommands_ReportDue(999,0,1));CHECK(AppCommands_ReportDue(1000,0,1));CHECK(!AppCommands_ReportDue(1000,0,0));
    CHECK(AppCommands_ReportDue(0x3E8U,0xFFFFFFF0U,1));
    AppCommands_ReportSent();CHECK(!AppCommands_ReportPending());
    result=AppCommands_Execute(e,1);CHECK(result.status==COMMAND_OK&&!result.changed);CHECK(dirty==1);CHECK(AppCommands_ReportPending());
    e[0].key=COMMAND_LED;e[0].value=1;result=AppCommands_Execute(e,1);CHECK(result.status==COMMAND_OK);CHECK(mode_switches==1);
    i=pid_resets;result=AppCommands_Execute(e,1);CHECK(!result.changed);CHECK(mode_switches==1);CHECK(pid_resets==i);
    e[0].key=COMMAND_PUMP;e[0].value=1;irq=1;result=AppCommands_Execute(e,1);CHECK(result.status==COMMAND_OK);CHECK(irq==1);CHECK(AppConfig_Get()->bump_mode==1);
    AppConfig_CompletePumpFromISR();CHECK(AppConfig_Get()->bump_mode==0);irq=0;
    reset();e[0].key=COMMAND_LED;e[0].value=1;e[1].key=COMMAND_TEMP;e[1].value=-1;
    result=AppCommands_Execute(e,2);CHECK(result.status==COMMAND_OUT_OF_RANGE);CHECK(AppConfig_Get()->led_mode==2);CHECK(dirty==0);CHECK(!AppCommands_ReportPending());
    e[1]=e[0];CHECK(AppCommands_Execute(e,2).status==COMMAND_DUPLICATE);CHECK(dirty==0);
    e[0].key=COMMAND_SOIL_LOW;e[0].value=90;CHECK(AppCommands_Execute(e,1).status==COMMAND_CONFLICT);CHECK(AppConfig_Get()->soil_low==30);
    e[0].value=80;e[1].key=COMMAND_SOIL_HIGH;e[1].value=90;
    CHECK(AppCommands_Execute(e,2).status==COMMAND_OK);CHECK(AppConfig_Get()->soil_low==80&&AppConfig_Get()->soil_high==90);
    e[0].value=10;e[1].value=20;CHECK(AppCommands_Execute(e,2).status==COMMAND_OK);CHECK(AppConfig_Get()->soil_low==10&&AppConfig_Get()->soil_high==20);
    for(i=0;i<6;i++){before.values[i]=0;desired.values[i]=123;}before.values[COMMAND_SOIL_HIGH]=100;
    e[0].key=(CommandKey)-1;e[0].value=0;CHECK(Command_Plan(e,1,&before,&desired)==COMMAND_UNKNOWN_KEY);CHECK(desired.values[0]==123);
    CHECK(Command_Plan(e,7,&before,&desired)==COMMAND_BAD_REQUEST);
    /* Each generated JSON case resets all business state. */
    JSON_CASES
    reset();CHECK(invoke("{\"params\":{\"led\":1},\"id\":\"9\"}")==200);CHECK(AppConfig_Get()->led_mode==1);
    CHECK(invoke("{\"id\":\"10\",\"params\":{\"led\":1}}")==200);CHECK(mode_switches==1);
    CHECK(invoke("{\"id\":\"11\",\"params\":{\"droughtthrehold\":80,\"moistthrehold\":90}}")==200);
    CHECK(AppConfig_Get()->soil_low==80&&AppConfig_Get()->soil_high==90);
    CHECK(invoke("{\"id\":\"12\",\"params\":{\"moistthrehold\":20,\"droughtthrehold\":10}}")==200);
    CHECK(AppConfig_Get()->soil_low==10&&AppConfig_Get()->soil_high==20);
    reset();CHECK(invoke("{\"id\":\"13\",\"params\":{\"led\":1,\"bump\":2,\"CO2threhold\":350,\"droughtthrehold\":0,\"moistthrehold\":100,\"tempthrehold\":100}}")==200);
    CHECK(AppConfig_Get()->led_mode==1&&AppConfig_Get()->bump_mode==2&&AppConfig_Get()->temp_threshold==100);CHECK(AppConfig_Get()->soil_high==100);
    reset();CHECK(!OneNetCommand_Handle("{\"id\":\"1\",\"params\":{\"led\":1}}",reply,1));CHECK(dirty==0);CHECK(reply[0]==0);
    CHECK(!OneNetCommand_Handle("{}",NULL,160));
    /* Execute the actual sender and the actual main-loop reporting block together.
     * In this codebase the sender returns ZERO on success. */
    reset();e[0].key=COMMAND_TEMP;e[0].value=26;CHECK(AppCommands_Execute(e,1).status==COMMAND_OK);
    one_net_connected=1;send_status=0;packet_status=0;fill_length=3;test_tick=1000;last_report_attempt_tick=0;force_periodic=0;tx_calls=tx_deletes=0;
    main_report_step();CHECK(tx_calls==1&&tx_deletes==1);CHECK(!AppCommands_ReportPending());CHECK(last_report_attempt_tick==1000);
    e[0].value=27;CHECK(AppCommands_Execute(e,1).status==COMMAND_OK);send_status=1;test_tick=2000;main_report_step();
    CHECK(AppCommands_ReportPending());CHECK(!one_net_connected);CHECK(tx_calls==2);
    test_tick=3000;main_report_step();CHECK(AppCommands_ReportPending());CHECK(tx_calls==2);
    one_net_connected=1;send_status=0;test_tick=3001;main_report_step();CHECK(!AppCommands_ReportPending());CHECK(tx_calls==3);
    e[0].value=28;CHECK(AppCommands_Execute(e,1).status==COMMAND_OK);packet_status=1;force_periodic=1;test_tick=5000;main_report_step();
    CHECK(AppCommands_ReportPending());CHECK(!one_net_connected);CHECK(tx_calls==3);packet_status=0;force_periodic=0;
    /* Numeric parser hardening: actual library, not a mocked JSON tree. */
    n=cJSON_Parse("1e999999999999999999999999999");CHECK(n!=NULL&&n->valueint==2147483647);cJSON_Delete(n);
    n=cJSON_Parse("-1e999999999999999999999999999");CHECK(n!=NULL&&n->valueint==(-2147483647-1));cJSON_Delete(n);
    for(i=1;i<=24;i++){
        reset();allocations=0;fail_at=i;failed=0;
        (void)OneNetCommand_Handle("{\"id\":\"1\",\"params\":{\"led\":1,\"tempthrehold\":26}}",reply,sizeof(reply));
        CHECK(live==0);if(failed){CHECK(dirty==0);CHECK(AppConfig_Get()->led_mode==2);}
        fail_at=0;
    }
    /* Real MQTT dispatch function with only its packet/network boundary mocked. */
    reset();topic="$sys/test-product/test-device/thing/property/set";packet="{\"id\":\"21\",\"params\":{\"led\":1}}";
    topic_length=(unsigned)strlen(topic);packet_length=(unsigned)strlen(packet);published=freed=cleared=0;
    OneNet_RevPro(NULL);CHECK(published==1);CHECK(reply_code(last_reply)==200);CHECK(AppConfig_Get()->led_mode==1);CHECK(freed==2&&cleared==1);
    CHECK(strcmp(last_topic,"$sys/test-product/test-device/thing/property/set_reply")==0);
    reset();topic="$sys/other/test-device/thing/property/set";topic_length=(unsigned)strlen(topic);published=0;OneNet_RevPro(NULL);CHECK(published==0);CHECK(dirty==0);
    topic="$sys/test-product/test-device/thing/property/set_extra";topic_length=(unsigned)strlen(topic);OneNet_RevPro(NULL);CHECK(published==0);CHECK(dirty==0);
    topic="$sys/test-product/test-device/thing/property/set";topic_length=(unsigned)strlen(topic);packet_length=256;OneNet_RevPro(NULL);CHECK(published==0);CHECK(dirty==0);
    packet="{\"id\":\"21\",\"params\":{\"led\":1}}\0junk";packet_length=(unsigned)strlen(packet)+5;OneNet_RevPro(NULL);CHECK(published==0);CHECK(dirty==0);
    /* Sweep command planning independently over scalar limits and soil relationships. */
    for(i=0;i<6;i++)for(j=0;j<101;j++){
        CommandState snapshot={{2,0,2000,30,75,25}};
        e[0].key=(CommandKey)i;e[0].value=(int32_t)j;
        result.status=Command_Plan(e,1,&snapshot,&desired);
        if(result.status==COMMAND_OK){CHECK(desired.values[i]==(int32_t)j);CHECK(desired.values[3]<=desired.values[4]);}
        else {CHECK(snapshot.values[i]==(i==0?2:i==1?0:i==2?2000:i==3?30:i==4?75:25));}
    }
    CHECK(live==0);CHECK(unexpected_print==0);text("PASS: production command/OneJSON/MQTT checks: ");number(checks);text("\n");return 0;
}
'''
    invalid_cases=[
      ('{"id":"1","params":{"led":-1}}',400),('{"id":"1","params":{"led":3}}',400),
      ('{"id":"1","params":{"bump":3}}',400),('{"id":"1","params":{"tempthrehold":101}}',400),
      ('{"id":"1","params":{"CO2threhold":349}}',400),('{"id":"1","params":{"CO2threhold":4001}}',400),
      ('{"id":"1","params":{"led":1.5}}',400),('{"id":"1","params":{"led":"1"}}',400),
      ('{"id":"1","params":{"led":-}}',0),('{"id":"1","params":{"led":01}}',0),
      ('{"id":"1","params":{"led":1e}}',0),('{"id":"1","params":{"led":1e+}}',0),
      ('{"id":"1","params":{"led":1e-}}',0),
      ('{"id":"1","params":{"led":true}}',400),('{"id":"1","params":{"led":null}}',400),
      ('{"id":"1","params":{"led":[]}}',400),('{"id":"1","params":{"led":{}}}',400),
      ('{"id":"1","params":{"LED":1}}',400),('{"id":"1","params":{"unknown":1}}',400),
      ('{"id":"1","params":{"led":1,"led":0}}',400),('{"id":"1","params":{}}',400),
      ('{"id":"1","params":[]}',400),('{"id":"1","params":null}',400),('{"id":"1"}',400),
      ('{"id":"1","params":{},"params":{"led":1}}',400),
      ('{"id":"1","params":{"led":1,"tempthrehold":-1}}',400),
      ('{"id":"1","params":{"led":1,"droughtthrehold":90}}',400),
      ('{"id":"1","params":{"led":1e99999999999999999999999999}}',400),
      ('{"id":"1","params":{"led":-1e999}}',400),('{"id":"1","params":{"led":0e999}}',400),
      ('{"id":"1","params":{"led":2147483648}}',400),
      ('{"id":"1","params":{"led":1,"bump":0,"CO2threhold":2000,"droughtthrehold":30,"moistthrehold":75,"tempthrehold":25,"extra":1}}',400),
      ('{"params":{"led":1}}',0),('{"id":1,"params":{"led":1}}',0),('{"id":"","params":{"led":1}}',0),
      ('{"id":"1\\u0000x","params":{"led":1}}',0),('{"id":"x","params":{"led":1}}',0),
      ('{"id":"1","id":"2","params":{"led":1}}',0),
      ('{"id":"1","params":{"led":1}} trailing',0),('{bad',0),('[]',0),('null',0),
      ('{"id":"1","params":{"led":[[[[1]]]]}}',0),
      ('{"id":"1","params":{"led":1},"extra":"'+'x'*256+'"}',0),
      ('{"id":"'+'1'*64+'","params":{"led":1}}',0)
    ]
    cases=[]
    for payload,code in invalid_cases:
        cases.append('reset();CHECK(invoke('+json.dumps(payload)+')=='+str(code)+');CHECK(dirty==0);CHECK(AppConfig_Get()->led_mode==2);')
    harness=harness.replace('DISPATCH',dispatch).replace('SEND_FUNCTION',send_function).replace('MAIN_REPORT',report_step).replace('JSON_CASES','\n'.join(cases))
    (out/'test.c').write_text(harness,encoding='utf-8')
    sources=[]
    for rel in ['modules/middleware/src/mw_command.c','modules/app/src/app_config.c','modules/app/src/app_commands.c','onenet/onenet/src/onenet_command.c','onenet/CJSON/cJSON.c']:
        file=out/Path(rel).name;file.write_text(read(rel),encoding='utf-8');sources.append(str(file))
    cmd=[a.clang,'--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections','-fuse-ld=lld','-nostdlib','-O0','-Wall','-Wextra','-Werror','-Wno-logical-op-parentheses','-Wno-unused-parameter','-Wno-misleading-indentation','-Wl,/entry:main,/subsystem:console,/opt:ref','-I'+str(inc),str(out/'test.c'),*sources,str(ucrt),str(kernel),'-o',str(out/'test.exe')]
    r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if r.returncode:print(r.stdout+r.stderr);sys.exit(r.returncode)
    r=subprocess.run([str(out/'test.exe')],capture_output=True,text=True,encoding='utf-8',errors='replace')
    print(r.stdout+r.stderr,end='')
    if r.returncode:sys.exit(1)
    print('PASS: real JSON parser, allocation-failure cleanup, batch validation, idempotence, report timing/wrap and exact MQTT routing. No network/keys/HIL.')
