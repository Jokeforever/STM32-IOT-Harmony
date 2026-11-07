from pathlib import Path
import argparse,shutil,subprocess,sys,tempfile

parser=argparse.ArgumentParser(description='Pure PID differential regression against frozen pre-extraction code.')
parser.add_argument('--firmware-root',type=Path,default=Path(__file__).resolve().parents[1])
parser.add_argument('--overlay',type=Path)
parser.add_argument('--clang',default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
args=parser.parse_args()
def read(rel):
    p=args.overlay/rel if args.overlay else args.firmware_root/rel
    return (p if p.exists() else args.firmware_root/rel).read_text(encoding='utf-8')

vectors=r"""
static int vectors(void)
{
    PID_Config c={200,0,1500,0,0,1000,1000,5000};
    PID_State s={0}, saved;
    uint16_t out=54321;
    PID_Reset(&s,0);
    CHECK(PID_Update(&s,&c,100,0,&out)==PID_HOLD);CHECK(out==54321);
    CHECK(PID_Update(&s,&c,100,1000,&out)==PID_OUTPUT_READY);CHECK(out==150);
    CHECK(PID_Update(&s,&c,101,2000,&out)==PID_OUTPUT_READY);CHECK(out==148);
    CHECK(PID_Update(&s,&c,400,3000,&out)==PID_OUTPUT_READY);CHECK(out==0);
    CHECK(PID_Update(&s,&c,-1000,4000,&out)==PID_OUTPUT_READY);CHECK(out==1000);
    /* Deadzone is inclusive; preserve warm-up status and hardware output. */
    c.dead_zone=20;PID_Reset(&s,0);out=321;
    CHECK(PID_Update(&s,&c,180,1000,&out)==PID_HOLD);CHECK(s.initialized==0);CHECK(out==321);
    CHECK(PID_Update(&s,&c,179,2000,&out)==PID_HOLD);CHECK(s.initialized==1);
    CHECK(PID_Update(&s,&c,100,3000,&out)==PID_OUTPUT_READY);
    CHECK(PID_Update(&s,&c,220,4000,&out)==PID_HOLD);CHECK(s.initialized==1);CHECK(s.last_tick==4000);
    CHECK(PID_Update(&s,&c,100,5000,&out)==PID_OUTPUT_READY);CHECK(out==150);
    /* Exact5s uses real dt; strictly over5s rebuilds history with nominal1s. */
    c.dead_zone=0;c.kp_x1000=0;c.ki_x1000=1000;
    PID_Reset(&s,0);(void)PID_Update(&s,&c,100,0,&out);
    CHECK(PID_Update(&s,&c,100,5000,&out)==PID_OUTPUT_READY);CHECK(s.integral==500);CHECK(out==500);
    PID_Reset(&s,0);(void)PID_Update(&s,&c,100,0,&out);
    CHECK(PID_Update(&s,&c,100,5001,&out)==PID_OUTPUT_READY);CHECK(s.integral==100);CHECK(out==100);
    /* Positive/negative integral limits and Ki0 clear history as before. */
    c.ki_x1000=10000;PID_Reset(&s,0);(void)PID_Update(&s,&c,100,0,&out);
    CHECK(PID_Update(&s,&c,100,5000,&out)==PID_OUTPUT_READY);CHECK(s.integral==100);CHECK(out==1000);
    CHECK(PID_Update(&s,&c,400,10000,&out)==PID_OUTPUT_READY);CHECK(s.integral==-100);CHECK(out==0);
    c.ki_x1000=0;CHECK(PID_Update(&s,&c,100,11000,&out)==PID_OUTPUT_READY);CHECK(s.integral==0);
    c.kd_x1000=1000;PID_Reset(&s,0);(void)PID_Update(&s,&c,190,0,&out);
    CHECK(PID_Update(&s,&c,180,1000,&out)==PID_OUTPUT_READY);CHECK(out==10);
    CHECK(PID_Update(&s,&c,100,7000,&out)==PID_OUTPUT_READY);CHECK(out==0);
    /* Zero dt holds; subtraction spans a uint32 wrap. */
    c.kd_x1000=0;c.ki_x1000=1000;PID_Reset(&s,0xFFFFFF00U);
    (void)PID_Update(&s,&c,100,0xFFFFFF00U,&out);saved=s;out=321;
    CHECK(PID_Update(&s,&c,100,0xFFFFFF00U,&out)==PID_HOLD);CHECK(equal_state(&s,&saved));CHECK(out==321);
    CHECK(PID_Update(&s,&c,100,744,&out)==PID_OUTPUT_READY);CHECK(out==100);
    /* Invalid requests never mutate state or the caller's output. */
    saved=s;out=321;
    CHECK(PID_Update(0,&c,100,2000,&out)==PID_INVALID);
    CHECK(PID_Update(&s,0,100,2000,&out)==PID_INVALID);
    CHECK(PID_Update(&s,&c,100,2000,0)==PID_INVALID);
    c.nominal_period_ms=0;CHECK(PID_Update(&s,&c,100,2000,&out)==PID_INVALID);
    c.nominal_period_ms=1000;c.max_gap_ms=999;CHECK(PID_Update(&s,&c,100,2000,&out)==PID_INVALID);
    c.max_gap_ms=5000;c.output_max=0;CHECK(PID_Update(&s,&c,100,2000,&out)==PID_INVALID);
    c.output_max=1000;c.kp_x1000=-1;CHECK(PID_Update(&s,&c,100,2000,&out)==PID_INVALID);
    c.kp_x1000=10001;CHECK(PID_Update(&s,&c,100,2000,&out)==PID_INVALID);
    c.kp_x1000=0;c.ki_x1000=-1;CHECK(PID_Update(&s,&c,100,2000,&out)==PID_INVALID);
    c.ki_x1000=10001;CHECK(PID_Update(&s,&c,100,2000,&out)==PID_INVALID);
    c.ki_x1000=1000;c.kd_x1000=-1;CHECK(PID_Update(&s,&c,100,2000,&out)==PID_INVALID);
    c.kd_x1000=10001;CHECK(PID_Update(&s,&c,100,2000,&out)==PID_INVALID);
    CHECK(equal_state(&s,&saved));CHECK(out==321);PID_Reset(0,0);
    /* Independent states and a non-1000 output limit exercise portable use. */
    {PID_State other={0};PID_Config p={200,0,1500,0,0,255,1000,5000};
     saved=s;PID_Reset(&other,0);(void)PID_Update(&other,&p,0,0,&out);
     CHECK(PID_Update(&other,&p,0,1000,&out)==PID_OUTPUT_READY);CHECK(out==255);CHECK(equal_state(&s,&saved));}
    return 0;
}
"""

prefix=r"""
#include "mw_pid.h"
#include "app_config.h"
#include <stddef.h>
typedef struct {uint8_t enabled;int16_t current_lux;uint16_t pwm_value;float integral,last_error;uint32_t last_tick;uint8_t pid_initialized;} LegacyLED;
static LegacyLED led_ctrl;
static RuntimeConfig runtime;
static uint32_t tick;
static unsigned writes;
static uint16_t hardware_pwm;
static unsigned assertions;
int _fltused=0;
void *memset(void *p,int c,size_t n) {uint8_t *b=p;while(n--)*b++=(uint8_t)c;return p;}
void *memcpy(void *p,const void *q,size_t n) {uint8_t *b=p;const uint8_t *a=q;while(n--)*b++=*a++;return p;}
const RuntimeConfig *AppConfig_Get(void) {return &runtime;}
uint32_t HAL_GetTick(void) {return tick;}
static float fabsf(float v) {return v<0?-v:v;}
#define PWM_MAX 1000
#define LED_CONTROL_INTERVAL_MS 1000U
#define LED_PID_MAX_GAP_MS 5000U
#define __HAL_TIM_SET_COMPARE(timer,channel,value) do{hardware_pwm=(uint16_t)(value);writes++;}while(0)
#define CHECK(x) do{assertions++;if(!(x))return __LINE__;}while(0)
static int equal_state(const PID_State *a,const PID_State *b)
{return a->integral==b->integral&&a->last_error==b->last_error&&a->last_tick==b->last_tick&&a->initialized==b->initialized;}
"""
main=r"""
int main(void)
{
    PID_State state={0};
    PID_Config c;
    static const uint32_t intervals[]={0,1,500,1000,1500,4999,5000,5001,10000};
    static const int32_t gains[]={0,1,123,1500,10000};
    unsigned i,rng=0x12345678U;
    tick=0xFFFFFF00U;led_ctrl.enabled=1;led_ctrl.pwm_value=hardware_pwm=300;
    LED_PID_Reset();PID_Reset(&state,tick);
    for(i=0;i<4096;i++)
    {
        uint16_t output=54321;
        int16_t measured;
        PID_Result result;
        rng=rng*1664525U+1013904223U;
        runtime.pid_target_lux=(int16_t)(rng%1000);
        runtime.pid_dead_zone=(uint16_t)((rng>>8)%201);
        runtime.pid_kp_x1000=gains[i%5];runtime.pid_ki_x1000=gains[(i/5)%5];runtime.pid_kd_x1000=gains[(i/25)%5];
        c.target=runtime.pid_target_lux;c.dead_zone=runtime.pid_dead_zone;
        c.kp_x1000=runtime.pid_kp_x1000;c.ki_x1000=runtime.pid_ki_x1000;c.kd_x1000=runtime.pid_kd_x1000;
        c.output_max=1000;c.nominal_period_ms=1000;c.max_gap_ms=5000;
        if(i%31==0) {tick=0xFFFFFF00U;LED_PID_Reset();PID_Reset(&state,tick);}
        if(i%17==0) {LED_PID_Reset();PID_Reset(&state,tick);}
        if(i%29==0) {led_ctrl.integral=state.integral=(i%2?4000.0f:-4000.0f);}
        tick+=intervals[i%9];
        switch(i%6)
        {
            case 0:measured=c.target;break;
            case 1:measured=(int16_t)(c.target+c.dead_zone);break;
            case 2:measured=(int16_t)(c.target-c.dead_zone-1);break;
            case 3:measured=(int16_t)(rng%1000);break;
            case 4:measured=(int16_t)(rng>>16);break;
            default:measured=(int16_t)(c.target+c.dead_zone+1);break;
        }
        writes=0;LED_Update(measured);
        result=PID_Update(&state,&c,measured,tick,&output);
        CHECK(result!=PID_INVALID);
        CHECK((result==PID_OUTPUT_READY)==(writes==1));CHECK(writes<=1);
        CHECK(result==PID_OUTPUT_READY?output==led_ctrl.pwm_value:output==54321);
        CHECK(state.integral==led_ctrl.integral);CHECK(state.last_error==led_ctrl.last_error);
        CHECK(state.last_tick==led_ctrl.last_tick);CHECK(state.initialized==led_ctrl.pid_initialized);
        CHECK(hardware_pwm==led_ctrl.pwm_value);
    }
    {int line=vectors();if(line)return line;}
    return assertions==EXPECTED_ASSERTIONS?0:32000;
}
"""
fixture=read('tests/fixtures/pid_before_extract.c')
expected=4096*9+vectors.count('CHECK(')
harness=(prefix+'\n'+fixture+'\n'+vectors+'\n'+main).replace('EXPECTED_ASSERTIONS',str(expected))
# The algorithm is built in an environment with no HAL, GPIO, timers, or configuration implementation.
with tempfile.TemporaryDirectory(prefix='stm32-pid-') as td:
    out=Path(td)
    for name,rel in [('mw_pid.h','modules/middleware/inc/mw_pid.h'),('app_config.h','modules/app/inc/app_config.h')]:
        (out/name).write_text(read(rel),encoding='utf-8')
    (out/'test.c').write_text(harness,encoding='utf-8')
    (out/'mw_pid.c').write_text(read('modules/middleware/src/mw_pid.c'),encoding='utf-8')
    cmd=[args.clang,'--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-fuse-ld=lld','-nostdlib','-O0',
         '-Wall','-Wextra','-Werror','-Wl,/entry:main,/subsystem:console','-I'+str(out),str(out/'test.c'),str(out/'mw_pid.c'),'-o',str(out/'test.exe')]
    r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if r.returncode:print(r.stdout+r.stderr);sys.exit(r.returncode)
    r=subprocess.run([str(out/'test.exe')])
    if r.returncode:
        print('FAIL at test.c line',r.returncode)
        if 0<r.returncode<=len(harness.splitlines()):print(harness.splitlines()[r.returncode-1])
        sys.exit(1)
print('PASS: 4096 legacy/new PID steps plus independent boundary vectors; assertions:',expected)
