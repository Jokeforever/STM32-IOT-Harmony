from pathlib import Path
import argparse, re, shutil, subprocess, sys, tempfile

p = argparse.ArgumentParser(description='Actual sensor-port adapter against driver boundary mocks; no hardware or network.')
p.add_argument('--firmware-root', type=Path, default=Path(__file__).resolve().parents[1])
p.add_argument('--overlay', type=Path)
p.add_argument('--clang', default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
a = p.parse_args()
def read(rel):
    candidate = a.overlay / rel if a.overlay else a.firmware_root / rel
    return (candidate if candidate.exists() else a.firmware_root / rel).read_text(encoding='utf-8')
def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode:
        print(r.stdout + r.stderr)
        sys.exit(r.returncode)

with tempfile.TemporaryDirectory(prefix='stm32-sensor-port-') as td:
    out = Path(td); public = out / 'public'; public.mkdir(); inc = out / 'include'; inc.mkdir()
    for name, rel in [('mw_sensor_port.h','modules/middleware/inc/mw_sensor_port.h'), ('sensor_data.h','modules/middleware/inc/sensor_data.h'), ('app_sensors.h','modules/app/inc/app_sensors.h'), ('mw_filter.h','modules/middleware/inc/mw_filter.h')]:
        value = read(rel)
        (public / name).write_text(value, encoding='utf-8')
        (inc / name).write_text(value, encoding='utf-8')
    header_check = public / 'api.c'
    header_check.write_text('#include "mw_sensor_port.h"\n#include "app_sensors.h"\nSensorPortResult result;\nSensorData value;\n', encoding='utf-8')
    service = out / 'app_sensors.c'; service.write_text(read('modules/app/src/app_sensors.c'), encoding='utf-8')
    assert not re.search(r'\b(?:DHT11_Read_Data|CO2GetData|LDR_LuxData|TS_GetData|HAL_[A-Za-z0-9_]+)\b', service.read_text(encoding='utf-8'))
    common = [a.clang, '--target=x86_64-pc-windows-msvc', '-ffreestanding', '-fno-builtin', '-Wall', '-Wextra', '-Werror']
    # Real service and public headers must compile without HAL or driver headers.
    run(common + ['-fsyntax-only', '-I' + str(public), str(header_check), str(service)])
    run(common + ['-x', 'c++', '-fsyntax-only', '-I' + str(public), str(header_check)])
    for name, fn in [('TS.h','TS_GetData'), ('usart3.h','CO2GetData'), ('LDR.h','LDR_LuxData')]:
        (inc / name).write_text('#include <stdint.h>\nuint8_t ' + fn + '(uint16_t *);\n', encoding='utf-8')
    (inc / 'dht11.h').write_text('#include <stdint.h>\n#define DHT11_OK 0U\nuint8_t DHT11_Read_Data(uint8_t *,uint8_t *);\n', encoding='utf-8')
    adapter = out / 'app_sensor_port.c'; adapter.write_text(read('modules/app/src/app_sensor_port.c'), encoding='utf-8')
    harness = r'''
#include "mw_sensor_port.h"
static uint8_t dht_status, analog_status, input_t, input_h;
static uint16_t input_value;
static unsigned partial_write, calls, order, checks;
#define CHECK(x) do { ++checks; if (!(x)) return __LINE__; } while (0)
static void reset_calls(void) { calls=0; order=0; }
uint8_t DHT11_Read_Data(uint8_t *t,uint8_t *h) {
    ++calls; order=3;
    if (dht_status==0 || partial_write) { *h=input_h; *t=input_t; }
    return dht_status;
}
static uint8_t analog_read(uint16_t *v,unsigned kind) {
    ++calls; order=kind;
    if (analog_status!=0 || partial_write) *v=input_value;
    return analog_status;
}
uint8_t TS_GetData(uint16_t *v) { return analog_read(v,1); }
uint8_t CO2GetData(uint16_t *v) { return analog_read(v,2); }
uint8_t LDR_LuxData(uint16_t *v) { return analog_read(v,4); }
typedef SensorPortResult (*AnalogRead)(uint16_t *);
int main(void) {
    unsigned i,j,k;
    uint8_t t,h;
    uint16_t v;
    SensorPortResult result;
    const uint8_t dht_values[]={0,1,20,50,100,255};
    const uint16_t analog_values[]={0,1,100,999,5000,65535};
    const AnalogRead funcs[]={SensorPort_ReadSoil,SensorPort_ReadCO2,SensorPort_ReadLight};
    const unsigned kinds[]={1,2,4};
    CHECK(SENSOR_PORT_OK==1); CHECK(SENSOR_PORT_ERROR==0);
    /* Test every uint8 status, including DHT no-response/checksum failures.
       Broken drivers may write before failure; caller outputs must survive. */
    partial_write=1;
    for(i=0;i<256;++i) {
        dht_status=(uint8_t)i; analog_status=(uint8_t)i;
        input_t=(uint8_t)i; input_h=(uint8_t)(255-i); input_value=(uint16_t)(i*257U);
        t=17;h=41;reset_calls();
        result=SensorPort_ReadTempHumi(&t,&h);
        CHECK(result==(i==0?SENSOR_PORT_OK:SENSOR_PORT_ERROR));
        CHECK(calls==1);CHECK(order==3);
        CHECK(t==(i==0?input_t:17));CHECK(h==(i==0?input_h:41));
        for(j=0;j<3;++j) {
            v=12345;reset_calls();result=funcs[j](&v);
            CHECK(result==(i!=0?SENSOR_PORT_OK:SENSOR_PORT_ERROR));
            CHECK(calls==1);CHECK(order==kinds[j]);CHECK(v==(i!=0?input_value:12345));
        }
    }
    /* Unit/precision forwarding: no new clamping or range interpretation. */
    dht_status=0;analog_status=1;
    for(i=0;i<6;++i) for(j=0;j<6;++j) {
        input_t=dht_values[i];input_h=dht_values[j];t=17;h=41;reset_calls();
        CHECK(SensorPort_ReadTempHumi(&t,&h)==SENSOR_PORT_OK);
        CHECK(calls==1);CHECK(order==3);CHECK(t==input_t);CHECK(h==input_h);
    }
    for(i=0;i<6;++i) for(j=0;j<3;++j) {
        input_value=analog_values[i];v=12345;reset_calls();
        CHECK(funcs[j](&v)==SENSOR_PORT_OK);CHECK(calls==1);CHECK(order==kinds[j]);CHECK(v==input_value);
    }
    for(k=0;k<3;++k) {
        t=17;h=41;reset_calls();
        result=SensorPort_ReadTempHumi(k==0?&t:0,k==1?&h:0);
        CHECK(result==SENSOR_PORT_ERROR);CHECK(calls==0);CHECK(t==17 && h==41);
    }
    for(j=0;j<3;++j) {
        reset_calls();CHECK(funcs[j](0)==SENSOR_PORT_ERROR);CHECK(calls==0);
    }
    /* Preserve the existing driver's write order even for aliased outputs. */
    input_t=33;input_h=70;reset_calls();t=17;
    CHECK(SensorPort_ReadTempHumi(&t,&t)==SENSOR_PORT_OK);CHECK(calls==1);CHECK(order==3);CHECK(t==33);
    return checks==4625U?0:32000;
}
'''
    test = out / 'test.c'; test.write_text(harness, encoding='utf-8')
    exe = out / 'test.exe'
    run(common + ['-fuse-ld=lld', '-nostdlib', '-O0', '-Wl,/entry:main,/subsystem:console', '-I' + str(inc), str(test), str(adapter), '-o', str(exe)])
    r = subprocess.run([str(exe)])
    if r.returncode:
        print('FAIL: sensor port assertion at test.c line', r.returncode)
        if 0 < r.returncode <= len(harness.splitlines()): print(harness.splitlines()[r.returncode-1])
        sys.exit(1)
print('PASS: 4625 adapter assertions; all 256 driver statuses, failure-output preservation, null arguments, integer boundaries and call counts.')
print('PASS: actual service/public headers compile without HAL or driver headers; C++ public declarations accepted. No hardware/network.')
