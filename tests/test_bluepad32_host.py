"""Host regression tests for adapter slots and bonded display-name metadata.
Run: python3 tests/test_bluepad32_host.py
Compiles actual C bodies with radio/SDK types and services stubbed; firmware builds
validate the real headers and ABI. Simulated TLV retention is not physical flash QA.
"""
from pathlib import Path
import re, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
shim = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define ENABLE_BLUEPAD32 1
#define CONFIG_BLUEPAD32_PLATFORM_CUSTOM 1
#define BLUEPAD32_MAX_BT_KEYBOARDS 2
#define BLUEPAD32_MAX_BT_MICE 2
#define BT_POST_READY_ADB_SETTLE_MS 1
#define BT_GAMEPAD_DISCOVERY_SETTLE_MS 1
#define BT_GAMEPAD_CORE1_RESUME_DELAY_MS 1
#define ARG_UNUSED(x) (void)(x)
#define logi(...) ((void)0)
#define loge(...) ((void)0)
#define UNI_ERROR_SUCCESS 0
#define UNI_BT_CONN_PROTOCOL_BLE 1
#define BD_ADDR_TYPE_LE_PUBLIC 0
#define BD_ADDR_TYPE_LE_RANDOM 1
#define UNI_CONTROLLER_CLASS_KEYBOARD 1
#define UNI_CONTROLLER_CLASS_MOUSE 2
#define UNI_CONTROLLER_CLASS_GAMEPAD 3
#define UNI_PLATFORM_OOB_BLUETOOTH_ENABLED 1
typedef uint8_t bd_addr_t[6];
typedef int uni_error_t;
typedef int uni_property_t;
typedef int uni_property_idx_t;
typedef int uni_platform_oob_event_t;
typedef struct { int value; } uni_keyboard_t;
typedef struct { int value; } uni_mouse_t;
typedef struct { uint8_t dpad; uint16_t buttons; uint8_t misc_buttons; } uni_gamepad_t;
typedef struct { int protocol, handle; bd_addr_t btaddr; } connection_t;
typedef struct { connection_t conn; char name[64]; int klass; } uni_hid_device_t;
typedef struct { int klass; uni_keyboard_t keyboard; uni_mouse_t mouse; uni_gamepad_t gamepad; } uni_controller_t;
struct uni_platform {
 const char *name;
 void (*init)(int,const char**); void (*on_init_complete)(void);
 uni_error_t (*on_device_discovered)(bd_addr_t,const char*,uint16_t,uint8_t);
 void (*on_device_connected)(uni_hid_device_t*); void (*on_device_disconnected)(uni_hid_device_t*);
 uni_error_t (*on_device_ready)(uni_hid_device_t*);
 void (*on_oob_event)(uni_platform_oob_event_t,void*);
 void (*on_controller_data)(uni_hid_device_t*,uni_controller_t*);
 const uni_property_t* (*get_property)(uni_property_idx_t);
};
static void uni_hid_device_set_name(uni_hid_device_t*d,const char*n){snprintf(d->name,sizeof(d->name),"%s",n);}
static bool uni_hid_device_has_name(uni_hid_device_t*d){return d->name[0]!=0;}
static bool uni_hid_device_is_keyboard(uni_hid_device_t*d){return d->klass==1;}
static bool uni_hid_device_is_mouse(uni_hid_device_t*d){return d->klass==2;}
static bool uni_hid_device_is_gamepad(uni_hid_device_t*d){return d->klass==3;}
static void uni_bt_conn_get_address(connection_t*c,bd_addr_t a){memcpy(a,c->btaddr,6);}
static uint32_t time_us_32(void){return 1;}
static void busy_wait_us(uint32_t n){(void)n;}
static int core1_get_bt_pause_depth(void){return 0;}
static void core1_pause_for_bt_enumeration(void){}
static void core1_wait_for_pause_active(int n){(void)n;}
static void core1_force_release_bt_pause(void){}
static void core1_resume_after_bt_enumeration(void){}
static void uni_bt_start_scanning_and_autoconnect_unsafe(void){}
static int clears;
static void uni_bt_del_keys_unsafe(void){clears++;}
static int identity_type[4];
static bd_addr_t identity_addr[4];
static int sm_le_device_index(int h){return h;}
static int le_device_db_max_count(void){return 4;}
static void le_device_db_info(int i,int*t,bd_addr_t a,void*k){(void)k;*t=identity_type[i];memcpy(a,identity_addr[i],6);}
typedef struct { int (*get_tag)(void*,uint32_t,uint8_t*,uint32_t); int (*store_tag)(void*,uint32_t,const uint8_t*,uint32_t); void (*delete_tag)(void*,uint32_t); } btstack_tlv_t;
static uint8_t records[4][128];
static unsigned sizes[4];
static int writes;
static int get_tag(void*c,uint32_t t,uint8_t*d,uint32_t n){(void)c;unsigned i=t&255;assert(i<4);if(sizes[i])memcpy(d,records[i],n<sizes[i]?n:sizes[i]);return sizes[i];}
static int store_tag(void*c,uint32_t t,const uint8_t*d,uint32_t n){(void)c;unsigned i=t&255;assert(i<4&&n<=128);memcpy(records[i],d,n);sizes[i]=n;writes++;return 0;}
static void delete_tag(void*c,uint32_t t){(void)c;sizes[t&255]=0;}
static const btstack_tlv_t tlv={get_tag,store_tag,delete_tag};
static void btstack_tlv_get_instance(const btstack_tlv_t**t,void**c){*t=&tlv;*c=NULL;}
'''
tests = r'''
static uni_hid_device_t device(int klass,int slot,const char*name){
 uni_hid_device_t d={.conn={.protocol=1,.handle=slot},.klass=klass};
 d.conn.btaddr[0]=(uint8_t)(slot+1);identity_addr[slot][0]=(uint8_t)(slot+1);
 uni_hid_device_set_name(&d,name);return d;
}
int main(void){
 uni_hid_device_t k=device(1,0,"MX Keys"),m=device(2,1,"MX Mouse"),g=device(3,2,"Pad"),k2=device(1,3,"Second keys");
 uni_controller_t report={.klass=3,.gamepad={.buttons=42}};
 // Unknown/pre-ready reports and disconnects must not consume any class slot.
 my_platform_on_controller_data(&g,&report);
 assert(gamepad_device_map[0]==NULL);
 my_platform_on_device_disconnected(&k);
 assert(gamepad_device_map[0]==NULL && mouse_device_map[0]==NULL);
 my_platform_on_device_ready(&k);my_platform_on_device_ready(&m);my_platform_on_device_ready(&g);my_platform_on_device_ready(&k2);
 assert(gamepad_device_map[0]==&g && writes==4);
 my_platform_on_controller_data(&g,&report);
 uni_gamepad_t outg;assert(bluepad32_get_gamepad(0,&outg)&&outg.buttons==42);
 // Gap: public UI and input index zero both resolve the remaining keyboard.
 report.klass=1;report.keyboard.value=73;my_platform_on_controller_data(&k2,&report);
 my_platform_on_device_disconnected(&k);
 uni_keyboard_t outk;assert(bluepad32_get_keyboard_count()==1);
 assert(strcmp(bluepad32_get_device_name('K',0),"Second keys")==0);
 assert(bluepad32_get_keyboard(0,&outk)&&outk.value==73);
 assert(!bluepad32_peek_keyboard(1,&outk));
 assert(gamepad_device_map[0]==&g);
 // Directed reconnect restores a genuine name and avoids duplicate flash writes.
 uni_hid_device_set_name(&k,"BLE keyboard");my_platform_on_device_ready(&k);
 assert(strcmp(k.name,"MX Keys")==0 && writes==4);
 my_platform_on_device_disconnected(&m);uni_hid_device_set_name(&m,"");my_platform_on_device_ready(&m);
 assert(strcmp(m.name,"MX Mouse")==0 && writes==4);
 // Mouse gaps use the same public ordering as names.
 uni_hid_device_t m2=device(2,3,"Second mouse");m2.conn.protocol=0;
 my_platform_on_device_ready(&m2);report.klass=2;report.mouse.value=91;
 my_platform_on_controller_data(&m2,&report);my_platform_on_device_disconnected(&m);
 uni_mouse_t outm;assert(strcmp(bluepad32_get_device_name('M',0),"Second mouse")==0);
 assert(bluepad32_get_mouse(0,&outm)&&outm.value==91);
 my_platform_on_device_disconnected(&m2);
 // Recycled slot with changed address/type must never inherit metadata.
 identity_addr[0][1]=9;uni_hid_device_set_name(&k,"");ultrausbt_restore_ble_name(&k);assert(!k.name[0]);identity_addr[0][1]=0;
 identity_type[0]=1;ultrausbt_restore_ble_name(&k);assert(!k.name[0]);identity_type[0]=0;
 // Corrupt records and invalid/no bond must be ignored.
 sizes[0]--;ultrausbt_restore_ble_name(&k);assert(!k.name[0]);sizes[0]++;
 ((struct ble_name_record*)records[0])->version=99;ultrausbt_restore_ble_name(&k);assert(!k.name[0]);((struct ble_name_record*)records[0])->version=1;
 memset(((struct ble_name_record*)records[0])->name,'x',32);ultrausbt_restore_ble_name(&k);assert(!k.name[0]);
 uni_hid_device_set_name(&k,"Renamed keys");ultrausbt_save_ble_name(&k);assert(writes==5);
 k.conn.handle=-1;uni_hid_device_set_name(&k,"");ultrausbt_restore_ble_name(&k);assert(!k.name[0]);k.conn.handle=0;
 k.conn.protocol=0;ultrausbt_restore_ble_name(&k);assert(!k.name[0]);k.conn.protocol=1;
 // Clearing volatile adapter state models a reboot; TLV survives.
 memset(bt_keyboards,0,sizeof(bt_keyboards));memset(keyboard_device_map,0,sizeof(keyboard_device_map));memset(pending_names_by_addr,0,sizeof(pending_names_by_addr));
 my_platform_on_device_ready(&k);assert(strcmp(bluepad32_get_device_name('K',0),"Renamed keys")==0&&writes==5);
 // All mappings released, including reserved-but-not-ready mappings.
 my_platform_on_device_disconnected(&g);assert(gamepad_device_map[0]==NULL);
 get_gamepad_storage(&g,true);my_platform_on_device_disconnected(&g);assert(gamepad_device_map[0]==NULL);
 // A genuine discovery name beats fallback and is persisted at ready.
 my_platform_on_device_disconnected(&g);uni_hid_device_set_name(&g,"BLE gamepad");store_pending_name_by_addr(g.conn.btaddr,"Named pad");my_platform_on_device_ready(&g);
 assert(strcmp(bluepad32_get_device_name('G',0),"Named pad")==0&&writes==6);
 bluepad32_delete_pairing_keys();assert(clears==1);
 for(int i=0;i<4;i++)assert(sizes[i]==0);
 uni_hid_device_set_name(&g,"BLE gamepad");ultrausbt_save_ble_name(&g);assert(writes==6);
 puts("PASS: slot lifecycle, connected enumeration, name restore/dedup/identity/corruption/reboot/clear");
}
'''
sources = '\n'.join(re.sub(r'^#include .*\n', '', (root/'src/firmware/src'/f).read_text(), flags=re.M) for f in ['bluepad32_names.c','bluepad32_platform.c'])
with tempfile.TemporaryDirectory(prefix='adb-host-test-') as d:
 p=Path(d);(p/'test.c').write_text(shim+sources+tests)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
