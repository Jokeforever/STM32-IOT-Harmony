from pathlib import Path
import argparse, shutil, subprocess, sys, tempfile

parser=argparse.ArgumentParser(description='Sensor service differential regression; no hardware or network.')
parser.add_argument('--firmware-root',type=Path,default=Path(__file__).resolve().parents[1])
parser.add_argument('--overlay',type=Path)
parser.add_argument('--clang',default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
args=parser.parse_args()

def read(rel):
    p=(args.overlay/rel) if args.overlay else args.firmware_root/rel
    return (p if p.exists() else args.firmware_root/rel).read_text(encoding='utf-8')

# Frozen pre-refactor acquisition, used only as an independent behavioral reference.
BASELINE='            (void)TS_GetData(&moist);\n\n            uint16_t raw_ppm = 0U;\n            if (CO2GetData(&raw_ppm) != 0U)\n            {\n                ppm = Filter_Median3_U16(s_co2_filter_history, &s_co2_filter_count, raw_ppm);\n                sensor_valid |= SENSOR_VALID_CO2;\n            }\n            else\n            {\n                sensor_valid &= (uint8_t)~SENSOR_VALID_CO2;\n            }\n\n            uint8_t raw_temp = 0U;\n            uint8_t raw_humi = 0U;\n            if (DHT11_Read_Data(&raw_temp,&raw_humi) == DHT11_OK)\n            {\n                temp = Filter_Median3_U8(s_temp_filter_history, &s_temp_filter_count, raw_temp);\n                humi = Filter_Median3_U8(s_humi_filter_history, &s_humi_filter_count, raw_humi);\n                sensor_valid |= (SENSOR_VALID_TEMP | SENSOR_VALID_HUMI);\n            }\n            else\n            {\n                sensor_valid &= (uint8_t)~(SENSOR_VALID_TEMP | SENSOR_VALID_HUMI);\n            }\n\n            (void)LDR_LuxData(&light);'
with tempfile.TemporaryDirectory(prefix='stm32-sensors-') as td:
    out=Path(td)
    def header(name,text): (out/name).write_text(text,encoding='utf-8')
    header('sensor_data.h',read('modules/middleware/inc/sensor_data.h'))
    header('app_sensors.h',read('modules/app/inc/app_sensors.h'))
    header('mw_sensor_port.h',read('modules/middleware/inc/mw_sensor_port.h'))
    header('mw_filter.h',read('modules/middleware/inc/mw_filter.h'))
    header('dht11.h','#include <stdint.h>\n#define DHT11_OK 0U\nuint8_t DHT11_Read_Data(uint8_t *,uint8_t *);\n')
    for name,fn in [('TS.h','TS_GetData'),('usart3.h','CO2GetData'),('LDR.h','LDR_LuxData')]:
        header(name,'#include <stdint.h>\nuint8_t '+fn+'(uint16_t *);\n')
    harness=r"""
#include "app_sensors.h"
#include "mw_filter.h"
#include "dht11.h"
#include "TS.h"
#include "LDR.h"
#include "usart3.h"
static uint8_t temp=20,humi=40,sensor_valid;
static uint16_t light=200,moist=20,ppm=350;
static uint8_t s_temp_filter_history[3],s_humi_filter_history[3];
static uint16_t s_co2_filter_history[3];
static uint8_t s_temp_filter_count,s_humi_filter_count,s_co2_filter_count;
static uint8_t dht_ok=1,co2_ok=1,light_ok=1,soil_ok=1,in_temp=25,in_humi=50;
static uint16_t in_light=300,in_soil=45,in_co2=700;
static unsigned order,checks;
uint8_t TS_GetData(uint16_t *p) {order=order*10+1;if(soil_ok)*p=in_soil;return soil_ok;}
uint8_t CO2GetData(uint16_t *p) {order=order*10+2;if(co2_ok)*p=in_co2;return co2_ok;}
uint8_t DHT11_Read_Data(uint8_t *t,uint8_t *h) {order=order*10+3;if(dht_ok){*t=in_temp;*h=in_humi;}return dht_ok?0:1;}
uint8_t LDR_LuxData(uint16_t *p) {order=order*10+4;if(light_ok)*p=in_light;return light_ok;}
static void legacy_poll(void) {
__BASELINE_CODE__
}
#define CHECK(x) do{checks++;if(!(x))return __LINE__;}while(0)
int main(void) {
    const SensorData *s=SensorService_GetData();
    unsigned i;unsigned rng=0x12345678U;
    CHECK(s->temperature_c==20);CHECK(s->humidity_pct==40);CHECK(s->co2_ppm==350);
    CHECK(s->light_lux==200);CHECK(s->soil_moisture_pct==20);CHECK(s->valid_mask==0);
    SensorService_Init();
    for(i=0;i<1000;i++) {
        rng=rng*1664525U+1013904223U;
        in_temp=(uint8_t)(rng%51);in_humi=(uint8_t)((rng>>6)%101);
        in_co2=(uint16_t)((rng>>13)%5001);in_light=(uint16_t)((rng>>15)%1000);
        in_soil=(uint16_t)((rng>>21)%101);
        dht_ok=(i%7!=0);co2_ok=(i%5!=0);light_ok=(i%3!=0);soil_ok=(i%11!=0);
        order=0;legacy_poll();CHECK(order==1234);
        order=0;SensorService_Poll();CHECK(order==1234);
        CHECK(s->temperature_c==temp);CHECK(s->humidity_pct==humi);CHECK(s->co2_ppm==ppm);
        CHECK(s->light_lux==light);CHECK(s->soil_moisture_pct==moist);CHECK(s->valid_mask==sensor_valid);
        CHECK((s->valid_mask & (uint8_t)~11U)==0);
    }
    /* 光照单独采样：成功/失败均不得改变其他读数及有效位。 */
    {SensorData before=*s;
     in_light=999;light_ok=1;order=0;CHECK(SensorService_ReadLight()==1);CHECK(order==4);
     CHECK(s->light_lux==999);CHECK(s->temperature_c==before.temperature_c);
     CHECK(s->humidity_pct==before.humidity_pct);CHECK(s->co2_ppm==before.co2_ppm);
     CHECK(s->soil_moisture_pct==before.soil_moisture_pct);CHECK(s->valid_mask==before.valid_mask);
     light_ok=0;CHECK(SensorService_ReadLight()==0);CHECK(s->light_lux==999);
    }
    /* 复位清除滤波历史；首个新值必须直接输出而不是残留旧中值。 */
    SensorService_Init();CHECK(s->valid_mask==0);CHECK(s->temperature_c==20);
    CHECK(s->humidity_pct==40);CHECK(s->co2_ppm==350);CHECK(s->light_lux==200);CHECK(s->soil_moisture_pct==20);
    dht_ok=co2_ok=soil_ok=light_ok=1;in_temp=50;in_humi=90;in_co2=5000;
    SensorService_Poll();CHECK(s->temperature_c==50);CHECK(s->humidity_pct==90);
    CHECK(s->co2_ppm==5000);CHECK(s->valid_mask==11);
    dht_ok=co2_ok=0;SensorService_Poll();CHECK(s->valid_mask==0);
    CHECK(s->temperature_c==50);CHECK(s->humidity_pct==90);CHECK(s->co2_ppm==5000);
    /* 9030 assertions: initial state,1000 scenarios,light-only,reset and disconnect. */
    return checks==9030?0:32000;
}
""".replace('__BASELINE_CODE__',BASELINE)
    (out/'test.c').write_text(harness,encoding='utf-8')
    sources=[]
    for rel in ['modules/app/src/app_sensors.c','modules/app/src/app_sensor_port.c','modules/middleware/src/mw_filter.c']:
        p=out/Path(rel).name;p.write_text(read(rel),encoding='utf-8');sources.append(str(p))
    cmd=[args.clang,'--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-fuse-ld=lld','-nostdlib','-O0',
         '-Wall','-Wextra','-Werror','-Wl,/entry:main,/subsystem:console','-I'+str(out),str(out/'test.c'),*sources,'-o',str(out/'test.exe')]
    built=subprocess.run(cmd,capture_output=True,text=True)
    if built.returncode: print(built.stdout+built.stderr);sys.exit(built.returncode)
    tested=subprocess.run([str(out/'test.exe')])
    if tested.returncode:
        print('FAIL at test.c line',tested.returncode)
        if 0<tested.returncode<=len(harness.splitlines()):print(harness.splitlines()[tested.returncode-1])
        sys.exit(1)
print('PASS: 1000 differential acquisition scenarios; 9030 assertions; actual service/filter C with driver mocks.')
