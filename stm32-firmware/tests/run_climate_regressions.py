from pathlib import Path
import argparse,re,shutil,subprocess,sys,tempfile,zlib
p=argparse.ArgumentParser(description='Climate hysteresis, production GPIO adapter, paired commands and v1/v2/v3 persistence; host mocks, no network/hardware.')
p.add_argument('--firmware-root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--overlay',type=Path)
p.add_argument('--clang',default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
a=p.parse_args()
def read(rel):
    q=a.overlay/rel if a.overlay else a.firmware_root/rel
    return (q if q.exists() else a.firmware_root/rel).read_text(encoding='utf-8')
def run(cmd):
    r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if r.returncode:print(r.stdout+r.stderr);sys.exit(r.returncode)
    return r
kernel=sorted(Path(r'C:\Program Files (x86)\Windows Kits\10\Lib').glob('*/um/x64/kernel32.lib'))[-1]
legacy_v2=bytes([49,71,70,67,2,0,48,0,7,0,2,0,65,33,252,8,120,0,8,7,25,0,85,0,217,4,255,15,244,1,17,0,219,5,0,0,123,0,0,0,200,1,0,0,211,10,200,96])
legacy_v1=bytes([49,71,70,67,1,0,32,0,5,0,2,0,60,31,152,8,100,0,220,5,25,0,80,0,217,4,255,15,95,251,98,80])
v3=bytearray(legacy_v2[:44]);v3[4]=3;v3[6]=52;v3[13]=35;v3+=bytes([15,0,0,0]);v3+=zlib.crc32(v3).to_bytes(4,'little')
arrays='\n'.join('static const uint8_t '+name+'[]={'+','.join(map(str,data))+'};' for name,data in [('old_v1',legacy_v1),('old_v2',legacy_v2),('current_v3',v3)])
harness=r'''
#include "mw_climate.h"
#include "app_config.h"
#include "mw_config.h"
#include "app_commands.h"
#include "jdq.h"
#include "fan.h"
#include "gpio.h"
#include <stddef.h>
ARRAYS
static uint8_t memory[256];static uint32_t tick;static unsigned read_fail,write_fail,corrupt,checks,writes,reads,resets;
static SensorData data;
typedef struct {
    unsigned port;
    unsigned pins;
    unsigned level;
} GpioWrite;
static GpioWrite gpio_writes[8];
static unsigned used;
static unsigned climate_mode, writes_at_climate_mode;

static int wrote(unsigned index, unsigned pins, unsigned level)
{
    const GpioWrite *write = &gpio_writes[index];
    return write->port == GPIOB && write->pins == pins && write->level == level;
}
int _fltused=0;
void *memset(void *p,int c,size_t n){unsigned char *d=p;while(n--)*d++=(unsigned char)c;return p;}
void *memcpy(void *p,const void *q,size_t n){unsigned char *d=p;const unsigned char *s=q;while(n--)*d++=*s++;return p;}
int strcmp(const char *a,const char *b){while(*a&&*a==*b){++a;++b;}return (unsigned char)*a-(unsigned char)*b;}
void LED_PID_Reset(void){++resets;}void Mode_Switch(void){}
uint32_t __get_PRIMASK(void){return 0;}void __disable_irq(void){}void __set_PRIMASK(uint32_t v){(void)v;}
const SensorData *SensorService_GetData(void){return &data;}
void HAL_GPIO_WritePin(unsigned port, unsigned pins, unsigned level)
{
    if (used < sizeof(gpio_writes) / sizeof(gpio_writes[0])) {
        gpio_writes[used] = (GpioWrite){port, pins, level};
    }
    ++used;
}

void HAL_GPIO_Init(unsigned port, const GPIO_InitTypeDef *config)
{
    if (port == GPIOB && (config->Pin & (GPIO_PIN_5 | GPIO_PIN_6)) != 0U) {
        climate_mode = config->Mode;
        writes_at_climate_mode = used;
    }
}
uint32_t ConfigPort_NowMs(void){return tick;}
uint8_t ConfigPort_Read(uint16_t offset,uint8_t *out,uint16_t count){++reads;if(read_fail||(unsigned)offset+count>256)return 0;memcpy(out,memory+offset,count);return 1;}
uint8_t ConfigPort_Write(uint16_t offset,const uint8_t *in,uint16_t count){++writes;if(write_fail||(unsigned)offset+count>256)return 0;memcpy(memory+offset,in,count);if(corrupt)memory[offset]^=1;return 1;}
__declspec(dllimport) void *GetStdHandle(uint32_t);
__declspec(dllimport) int WriteFile(void*,const void*,uint32_t,uint32_t*,void*);
static void text(const char*s){uint32_t n=0,w;while(s[n])++n;WriteFile(GetStdHandle((uint32_t)-11),s,n,&w,0);}
static void number(unsigned n){char b[16];unsigned i=15;b[i]=0;do{b[--i]=(char)('0'+n%10);n/=10;}while(n);text(b+i);}
#define CHECK(x) do{++checks;if(!(x)){text("FAIL climate line ");number(__LINE__);text("\n");return __LINE__;}}while(0)
static uint32_t crc(const uint8_t *p,unsigned n){uint32_t v=0xffffffffU;unsigned j;while(n--){v^=*p++;for(j=0;j<8;++j)v=(v>>1)^((v&1)?0xedb88320U:0);}return v^0xffffffffU;}
static void load_fixture(const uint8_t *p,unsigned n){memset(memory,255,sizeof(memory));memcpy(memory,p,n);AppConfig_Init();Config_Load();}
int main(void){
    ClimateLimits limits={30,20,2000};ClimateState state;ClimateDemand out;
    unsigned i,j,k,v,c,cases=0;static const unsigned pairs[][2]={{1,0},{30,20},{100,99},{21,20}};
    /* Independent truth-table oracle, including previous states at exact equality. */
    for(i=0;i<4;++i)for(j=0;j<4;++j)for(k=0;k<101;++k)for(v=0;v<4;++v)for(c=0;c<3;++c){
        unsigned cool,heat,co2;
        limits.temp_high_c=(uint8_t)pairs[i][0];limits.temp_low_c=(uint8_t)pairs[i][1];
        state.cooling=(uint8_t)(j&1);state.heating=(uint8_t)((j>>1)&1);state.configured=1;
        state.last_high_c=limits.temp_high_c;state.last_low_c=limits.temp_low_c;
        data.temperature_c=(uint8_t)k;data.co2_ppm=(uint16_t)(1999+c);data.valid_mask=(uint8_t)((v&1)|((v&2)<<2));
        cool=(v&1)?(k>pairs[i][0]?1:k<pairs[i][0]?0:(j&1)):0;
        heat=(v&1)?(k<pairs[i][1]?1:k>pairs[i][1]?0:((j>>1)&1)):0;
        co2=(v&2)&&c==2;
        CHECK(Climate_Evaluate(&data,&limits,&state,&out));CHECK(state.cooling==cool);CHECK(state.heating==heat);
        CHECK(out.fan_on==(cool||co2));CHECK(out.heater_on==heat);++cases;
    }
    limits.temp_high_c=30;limits.temp_low_c=20;data.valid_mask=9;data.co2_ppm=350;
    Climate_Reset(&state);data.temperature_c=31;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&out.fan_on);
    data.temperature_c=30;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&out.fan_on);
    data.temperature_c=29;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&!out.fan_on);
    data.temperature_c=19;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&out.heater_on);
    data.temperature_c=20;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&out.heater_on);
    data.temperature_c=21;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&!out.heater_on);
    /* CO2 must not latch the temperature request after its own recovery. */
    Climate_Reset(&state);data.temperature_c=30;data.co2_ppm=2001;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&out.fan_on&&!state.cooling);
    data.co2_ppm=2000;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&!out.fan_on);
    data.temperature_c=19;data.co2_ppm=2001;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&out.fan_on&&out.heater_on);
    data.valid_mask=8;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&out.fan_on&&!out.heater_on);
    data.valid_mask=1;data.temperature_c=20;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&!out.fan_on&&!out.heater_on);
    data.temperature_c=31;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&out.fan_on);
    limits.temp_high_c=31;CHECK(Climate_Evaluate(&data,&limits,&state,&out)&&!out.fan_on);
    CHECK(!Climate_Evaluate(0,&limits,&state,&out)&&!out.fan_on&&!out.heater_on&&!state.configured);
    CHECK(!Climate_Evaluate(&data,0,&state,&out));CHECK(!Climate_Evaluate(&data,&limits,0,&out));
    CHECK(!Climate_Evaluate(&data,&limits,&state,0));limits.temp_low_c=31;CHECK(!Climate_Evaluate(&data,&limits,&state,&out));
    limits.temp_low_c=20;limits.temp_high_c=101;CHECK(!Climate_Evaluate(&data,&limits,&state,&out));
    /* GPIO initialization must preload both inactive levels before push-pull. */
    used = 0;
    MX_GPIO_Init();
    CHECK(climate_mode == GPIO_MODE_OUTPUT_PP);
    CHECK(writes_at_climate_mode > 3U && wrote(3, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SET));

    /* Hardware_Init calls these in heater-then-fan order. */
    AppConfig_Init();
    used = 0;
    JDQ_Init();
    FAN_Init();
    CHECK(used == 2U && wrote(0, GPIO_PIN_6, GPIO_PIN_SET) && wrote(1, GPIO_PIN_5, GPIO_PIN_SET));

    /* Runtime writes fan first, heater second. The four rows exercise both pins. */
    {
        static const struct {
            uint8_t temperature;
            uint16_t co2;
            unsigned fan_level;
            unsigned heater_level;
        } outputs[] = {
            {25, 350, GPIO_PIN_SET, GPIO_PIN_SET},
            {31, 350, GPIO_PIN_RESET, GPIO_PIN_SET},
            {19, 350, GPIO_PIN_SET, GPIO_PIN_RESET},
            {19, 2001, GPIO_PIN_RESET, GPIO_PIN_RESET},
        };
        data.valid_mask = SENSOR_VALID_TEMP | SENSOR_VALID_CO2;
        for (i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i) {
            used = 0;
            data.temperature_c = outputs[i].temperature;
            data.co2_ppm = outputs[i].co2;
            Monitor_Temp();
            CHECK(used == 2U && wrote(0, GPIO_PIN_5, outputs[i].fan_level));
            CHECK(wrote(1, GPIO_PIN_6, outputs[i].heater_level));
        }
    }
    used = 0;
    data.valid_mask = SENSOR_VALID_CO2;
    Monitor_Temp();
    CHECK(used == 2U && wrote(0, GPIO_PIN_5, GPIO_PIN_RESET) && wrote(1, GPIO_PIN_6, GPIO_PIN_SET));
    used = 0;
    data.valid_mask = 0;
    Monitor_Temp();
    CHECK(used == 2U && wrote(0, GPIO_PIN_5, GPIO_PIN_SET) && wrote(1, GPIO_PIN_6, GPIO_PIN_SET));
    /* Paired config boundaries, restore rejection, same-value behavior and PID independence. */
    CHECK(AppConfig_Get()->temp_threshold==30&&AppConfig_Get()->temp_low_threshold==20);
    CHECK(AppConfig_Set(APP_CFG_TEMP_THRESHOLD,20)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,30)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,100)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_TEMP_THRESHOLD,101)==APP_CONFIG_REJECTED);
    i=resets;CHECK(AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,19)==APP_CONFIG_CHANGED);CHECK(resets==i);
    CHECK(AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,19)==APP_CONFIG_UNCHANGED);
    CHECK(AppConfig_Adjust(APP_CFG_TEMP_LOW_THRESHOLD,2147483647)==APP_CONFIG_CHANGED);CHECK(AppConfig_Get()->temp_low_threshold==29);
    CHECK(AppConfig_Adjust(APP_CFG_TEMP_THRESHOLD,(-2147483647-1))==APP_CONFIG_UNCHANGED);
    {RuntimeConfig bad=*AppConfig_Get();bad.temp_low_threshold=bad.temp_threshold;CHECK(!AppConfig_Restore(&bad));}
    /* Final pair validated before any side effect; both upward/downward batch orderings. */
    {CommandEntry e[3]={{COMMAND_TEMP,50},{COMMAND_TEMP_LOW,40},{COMMAND_LED,1}};AppCommandResult r;
     r=AppCommands_Execute(e,2);CHECK(r.status==COMMAND_OK);CHECK(AppConfig_Get()->temp_threshold==50&&AppConfig_Get()->temp_low_threshold==40);
     e[0].value=20;e[1].value=10;CHECK(AppCommands_Execute(e,2).status==COMMAND_OK);
     CHECK(AppConfig_Get()->temp_threshold==20&&AppConfig_Get()->temp_low_threshold==10);
     e[0].value=15;e[1].value=15;e[2].value=0;CHECK(AppCommands_Execute(e,3).status==COMMAND_CONFLICT);
     CHECK(AppConfig_Get()->temp_threshold==20&&AppConfig_Get()->temp_low_threshold==10&&AppConfig_Get()->led_mode==2);
     CHECK(Command_FindKey("temp_low_threshold")==COMMAND_TEMP_LOW);}
    /* Old records have independent known CRCs; retain non-temperature settings. */
    CHECK(sizeof(DeviceConfig)==52&&offsetof(DeviceConfig,crc32)==48);
    load_fixture(old_v2,sizeof(old_v2));CHECK(AppConfig_Get()->temp_threshold==30&&AppConfig_Get()->temp_low_threshold==20);
    CHECK(AppConfig_Get()->pid_target_lux==500&&AppConfig_Get()->led_manual_pwm==65&&AppConfig_Get()->co2_threshold==2300);
    CHECK(AppConfig_Get()->pid_kp_x1000==1499&&AppConfig_Get()->pid_ki_x1000==123&&AppConfig_Get()->pid_kd_x1000==456);
    CHECK(AppConfig_Get()->soil_low==25&&AppConfig_Get()->soil_high==85&&Config_IsDirty());
    tick=1000;Config_Process();CHECK(!Config_IsDirty());CHECK(memory[64+4]==3&&memory[64+6]==52&&memory[64+44]==20);
    {DeviceConfig saved;memcpy(&saved,memory+64,sizeof(saved));CHECK(saved.crc32==crc(memory+64,48));}
    AppConfig_Init();Config_Load();CHECK(AppConfig_Get()->temp_threshold==30&&AppConfig_Get()->pid_target_lux==500);
    load_fixture(old_v1,sizeof(old_v1));CHECK(AppConfig_Get()->temp_threshold==30&&AppConfig_Get()->temp_low_threshold==20);
    CHECK(AppConfig_Get()->led_manual_pwm==60&&AppConfig_Get()->co2_threshold==2200&&AppConfig_Get()->pid_target_lux==200&&Config_IsDirty());
    load_fixture(current_v3,sizeof(current_v3));CHECK(AppConfig_Get()->temp_threshold==35&&AppConfig_Get()->temp_low_threshold==15);
    CHECK(AppConfig_Get()->pid_target_lux==500);
    /* Alternating slots, CRC corruption, failure/retry/debounce and uint32 tick wrap. */
    CHECK(AppConfig_Set(APP_CFG_TEMP_THRESHOLD,36)==APP_CONFIG_CHANGED);tick+=1000;Config_Process();CHECK(!Config_IsDirty());
    CHECK(memory[64+13]==36&&memory[64+44]==15);Config_Load();CHECK(AppConfig_Get()->temp_threshold==36);
    memory[64+13]^=1;Config_Load();CHECK(AppConfig_Get()->temp_threshold==35);
    CHECK(AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,16)==APP_CONFIG_CHANGED);write_fail=1;CHECK(!Config_Save());write_fail=0;
    Config_Load();CHECK(AppConfig_Get()->temp_low_threshold==15);
    CHECK(AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,16)==APP_CONFIG_CHANGED);read_fail=1;CHECK(!Config_Save());read_fail=0;
    load_fixture(current_v3,sizeof(current_v3));corrupt=1;CHECK(!Config_Save());corrupt=0;Config_Load();CHECK(AppConfig_Get()->temp_low_threshold==15);
    CHECK(AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,16)==APP_CONFIG_CHANGED);tick=0xffffff00U;Config_MarkDirty();i=writes;
    tick=743;Config_Process();CHECK(writes==i&&Config_IsDirty());tick=744;Config_Process();CHECK(writes==i+1&&!Config_IsDirty());
    load_fixture(current_v3,sizeof(current_v3));memory[4]=99;Config_Load();CHECK(AppConfig_Get()->temp_threshold==30&&AppConfig_Get()->temp_low_threshold==20);
    memset(memory,255,sizeof(memory));Config_Load();CHECK(Config_IsDirty());i=writes;tick+=999;Config_Process();CHECK(writes==i);
    ++tick;write_fail=1;Config_Process();CHECK(Config_IsDirty());write_fail=0;tick+=1000;Config_Process();CHECK(!Config_IsDirty());
    text("PASS: climate/config integration assertions: ");number(checks);text("; independent climate cases: ");number(cases);text("\n");return 0;
}
'''.replace('ARRAYS',arrays)
with tempfile.TemporaryDirectory(prefix='stm32-climate-') as td:
    out=Path(td)
    def put(name,t):
        f=out/name;f.write_text(t,encoding='utf-8');return str(f)
    for name,rel in [('mw_climate.h','modules/middleware/inc/mw_climate.h'),('sensor_data.h','modules/middleware/inc/sensor_data.h'),('mw_config.h','modules/middleware/inc/mw_config.h'),('mw_config_port.h','modules/middleware/inc/mw_config_port.h'),('app_config.h','modules/app/inc/app_config.h'),('app_commands.h','modules/app/inc/app_commands.h'),('mw_command.h','modules/middleware/inc/mw_command.h'),('app_sensors.h','modules/app/inc/app_sensors.h'),('jdq.h','Core/Inc/jdq.h'),('fan.h','Core/Inc/fan.h')]:put(name,read(rel))
    put('main.h','#include <stdint.h>\nuint32_t __get_PRIMASK(void);void __disable_irq(void);void __set_PRIMASK(uint32_t);\n#define GPIOB 1U\n#define GPIO_PIN_5 (1U<<5)\n#define GPIO_PIN_6 (1U<<6)\n#define GPIO_PIN_RESET 0U\n#define GPIO_PIN_SET 1U\nvoid HAL_GPIO_WritePin(unsigned,unsigned,unsigned);\n')
    put('mw_climate_port.h', read('modules/middleware/inc/mw_climate_port.h'))
    put('gpio.h', r'''#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H
#include "main.h"
#define GPIOA 0U
#define GPIOC 2U
#define GPIO_PIN_0 (1U << 0)
#define GPIO_PIN_1 (1U << 1)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_7 (1U << 7)
#define GPIO_PIN_12 (1U << 12)
#define GPIO_PIN_13 (1U << 13)
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_INPUT 2U
#define GPIO_NOPULL 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_SPEED_FREQ_HIGH 1U
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOD_CLK_ENABLE() ((void)0)
typedef struct { unsigned Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
void HAL_GPIO_Init(unsigned port, const GPIO_InitTypeDef *config);
void MX_GPIO_Init(void);
#endif
''')
    put('LED.h','void LED_PID_Reset(void);void Mode_Switch(void);\n')
    put('string.h','#include <stddef.h>\nvoid *memcpy(void*,const void*,size_t);void *memset(void*,int,size_t);int strcmp(const char*,const char*);\n')
    sources=[put('test.c',harness)]+[put(Path(rel).name,read(rel)) for rel in ['modules/middleware/src/mw_climate.c','modules/app/src/app_config.c','modules/middleware/src/mw_config.c','modules/middleware/src/mw_command.c','modules/app/src/app_commands.c','Core/Src/jdq.c', 'Core/Src/fan.c', 'Core/Src/gpio.c', 'modules/app/src/app_climate_port.c']]
    flags=['--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-O0','-Wall','-Wextra','-Werror','-I'+str(out)]
    # Pure module compiles with only its contract and SensorData, also check C++ API.
    public=out/'public';public.mkdir()
    for name in ['mw_climate.h','sensor_data.h']:(public/name).write_text((out/name).read_text(encoding='utf-8'),encoding='utf-8')
    api=public/'api.cpp';api.write_text('#include "mw_climate.h"\nvoid api(ClimateState*s){Climate_Reset(s);}\n',encoding='utf-8')
    run([a.clang,'--target=x86_64-pc-windows-msvc','-Wall','-Wextra','-Werror','-fsyntax-only','-I'+str(public),str(api)])
    run([a.clang,'--target=x86_64-pc-windows-msvc','-ffreestanding','-fsyntax-only','-I'+str(public),str(out/'mw_climate.c')])
    exe=out/'test.exe';run([a.clang,*flags,'-fuse-ld=lld','-nostdlib','-Wl,/entry:main,/subsystem:console',*sources,str(kernel),'-o',str(exe)])
    print(run([str(exe)]).stdout,end='')
print('PASS: stateful hysteresis, CO2 independence, GPIO initialization and runtime writes, atomic paired validation, independent v1/v2/v3 CRC fixtures and persistence failure recovery. No hardware/network.')
