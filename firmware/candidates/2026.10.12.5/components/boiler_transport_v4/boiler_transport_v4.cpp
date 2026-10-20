#include "boiler_transport_v4.h"
#include "esphome/core/log.h"
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <cstdio>
#include <cmath>
#include <cinttypes>

extern "C" esp_err_t __real_esp_ota_mark_app_valid_cancel_rollback(void);
// Linker guard: time/shutdown/other framework callers cannot bypass transport health.
extern "C" esp_err_t __wrap_esp_ota_mark_app_valid_cancel_rollback(void){return ESP_ERR_INVALID_STATE;}
extern "C" esp_err_t __real_esp_task_wdt_reset(void);
static std::atomic<TaskHandle_t> detached_loop_task{nullptr};
// ESPHome's inline HAL feed is unconditional. Suppress only the deliberately
// detached loop's feed; preserve real watchdog feeding/errors for EVERY other task.
extern "C" esp_err_t __wrap_esp_task_wdt_reset(void){
  const TaskHandle_t detached=detached_loop_task.load();
  if(detached!=nullptr && detached==xTaskGetCurrentTaskHandle())return ESP_OK;
  return __real_esp_task_wdt_reset();
}

namespace esphome::boiler_transport_v4 {
static const char *const TAG="boiler_v4";
static uint32_t now_ms(){return static_cast<uint32_t>(esp_timer_get_time()/1000ULL);}
static TickType_t one_tick(){return std::max<TickType_t>(1,pdMS_TO_TICKS(1));}
void BoilerTransportV4::set_maintenance(bool x){
  if(x)maintenance_ms_.store(now_ms());
  maintenance_.store(x);
}

void BoilerTransportV4::setup(){
  start_ms_=now_ms();main_beat_.store(start_ms_);
  supervisor_task_=xTaskCreateStaticPinnedToCore(supervisor_entry,"boiler_guard",sizeof(supervisor_stack_),this,10,
                                                supervisor_stack_,&supervisor_tcb_,0);
  if(!supervisor_task_){fail(Fault::STARTUP);esp_restart();return;}
  bridge_task_=xTaskCreateStaticPinnedToCore(bridge_entry,"boiler_raw",sizeof(bridge_stack_),this,18,
                                            bridge_stack_,&bridge_tcb_,1);
  if(!bridge_task_){fail(Fault::STARTUP);mark_failed();return;}
  const uint32_t wait_start=now_ms();
  while(!ready_.load() && fault_.load()==0 && elapsed(now_ms(),wait_start)<1000)vTaskDelay(one_tick());
  if(!ready_.load()||!bridge_wdt_.load()){fail(Fault::STARTUP);mark_failed();return;}
  // Called from the ESPHome loop task, AFTER bridge + supervisor protection exists.
  const esp_err_t state=esp_task_wdt_status(nullptr);
  if(state==ESP_OK){
    if(esp_task_wdt_delete(nullptr)!=ESP_OK){fail(Fault::WATCHDOG);mark_failed();return;}
  }else if(state!=ESP_ERR_NOT_FOUND){fail(Fault::WATCHDOG);mark_failed();return;}
  detached_loop_task.store(xTaskGetCurrentTaskHandle());loop_detached_.store(true);
}
void BoilerTransportV4::bridge_entry(void *p){static_cast<BoilerTransportV4*>(p)->bridge_run();vTaskDelete(nullptr);}
void BoilerTransportV4::supervisor_entry(void *p){static_cast<BoilerTransportV4*>(p)->supervisor_run();vTaskDelete(nullptr);}

bool BoilerTransportV4::init_uart(Direction &d){
  // Never delete/reconfigure a UART owned by another component.
  if(uart_is_driver_installed(d.port))return false;
  if(uart_driver_install(d.port,2048,0,16,&d.events,0)!=ESP_OK)return false;
  uart_config_t cfg{};cfg.baud_rate=9600;cfg.data_bits=UART_DATA_8_BITS;
  cfg.parity=UART_PARITY_DISABLE;cfg.stop_bits=UART_STOP_BITS_1;
  cfg.flow_ctrl=UART_HW_FLOWCTRL_DISABLE;cfg.source_clk=UART_SCLK_DEFAULT;
  return uart_param_config(d.port,&cfg)==ESP_OK &&
         uart_set_pin(d.port,d.tx_pin,d.rx_pin,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE)==ESP_OK &&
         uart_set_rx_full_threshold(d.port,1)==ESP_OK && uart_set_rx_timeout(d.port,2)==ESP_OK;
}
void BoilerTransportV4::fail(Fault f){uint32_t empty=0;fault_.compare_exchange_strong(empty,static_cast<uint32_t>(f));}
void BoilerTransportV4::bridge_run(){
  if(!init_uart(dirs_[0])||!init_uart(dirs_[1])){fail(Fault::DRIVER);return;}
  if(esp_task_wdt_add(nullptr)!=ESP_OK){fail(Fault::WATCHDOG);return;}
  bridge_wdt_.store(true);
  for(auto &d:dirs_)d.last_progress.store(now_ms());
  bridge_beat_.store(now_ms());ready_.store(true);
  for(;;){
    const uint32_t now=now_ms();
    for(auto &d:dirs_)events(d);
    for(auto &d:dirs_)flush(d,now);
    // These UART pairs intentionally use RX and TX of the SAME hardware port.
    pump(dirs_[1],1,now);pump(dirs_[0],0,now);
    for(auto &d:dirs_)flush(d,now);
    bridge_beat_.store(now_ms());
    if(esp_task_wdt_reset()!=ESP_OK)fail(Fault::WATCHDOG);
    vTaskDelay(one_tick());
  }
}
void BoilerTransportV4::events(Direction &d){
  uart_event_t e{};size_t budget=16;
  while(budget-- && xQueueReceive(d.events,&e,0)==pdTRUE){
    switch(e.type){
      case UART_FIFO_OVF:losses_.fetch_add(1);epoch_.fetch_add(1);break;
      case UART_BUFFER_FULL:buffer_full_.fetch_add(1);epoch_.fetch_add(1);break;
      case UART_FRAME_ERR:frame_errors_.fetch_add(1);epoch_.fetch_add(1);break;
      case UART_PARITY_ERR:parity_errors_.fetch_add(1);epoch_.fetch_add(1);break;
      default:break;
    }
  }
  // Do NOT call uart_flush: it would delete additional stock bytes.
}
void BoilerTransportV4::flush(Direction &d,uint32_t now){
  const size_t n=d.tx.contiguous_size();if(n==0)return;
  const int accepted=uart_tx_chars(d.port,reinterpret_cast<const char*>(d.tx.data()),n);
  if(accepted<0){api_errors_.fetch_add(1);fail(Fault::DRIVER);return;}
  if(accepted>0){
    d.health_revision.fetch_add(1);
    d.tx.consume(static_cast<size_t>(accepted));d.accepted.fetch_add(accepted);d.last_progress.store(now);
    // oldest_ms is an upper bound for the age of a continuously nonempty backlog.
    if(d.tx.size()==0)d.oldest_ms.store(0);
    d.pending.store(d.tx.size());
    d.health_revision.fetch_add(1);
  }
}
void BoilerTransportV4::pump(Direction &d,uint8_t direction,uint32_t now){
  const size_t free=d.tx.free();if(free==0)return;
  MirrorBlock b{};
  const int n=uart_read_bytes(d.port,b.bytes.data(),std::min(free,b.bytes.size()),0);
  if(n<0){api_errors_.fetch_add(1);fail(Fault::DRIVER);return;}
  if(n==0)return;
  const bool empty=d.tx.size()==0;
  d.health_revision.fetch_add(1);
  if(d.tx.push(b.bytes.data(),n)!=static_cast<size_t>(n)){
    d.health_revision.fetch_add(1);fail(Fault::DRIVER);return;
  }
  if(empty){d.oldest_ms.store(now);d.last_progress.store(now);}
  d.rx.fetch_add(n);d.pending.store(d.tx.size());
  d.high_water.store(std::max(d.high_water.load(),static_cast<uint32_t>(d.tx.size())));
  d.health_revision.fetch_add(1);
  // Byte forwarding is already queued; the observer copy is best effort only.
  if(observer_enabled_){
    b.sequence=++sequence_;b.epoch=epoch_.load();b.captured_ms=now;b.direction=direction;b.length=n;
    if(!mirror_.push(b))mirror_drops_.fetch_add(1);
  }
}
uint32_t BoilerTransportV4::errors()const{
  return losses_.load()+buffer_full_.load()+frame_errors_.load()+parity_errors_.load()+api_errors_.load();
}
void BoilerTransportV4::supervisor_run(){
  const esp_partition_t *running=esp_ota_get_running_partition();esp_ota_img_states_t state=ESP_OTA_IMG_UNDEFINED;
  if(esp_ota_get_state_partition(running,&state)!=ESP_OK)state=ESP_OTA_IMG_UNDEFINED;
  ota_state_.store(static_cast<uint32_t>(state));
  bool confirmed=false;
  for(;;){
    const uint32_t now=now_ms();
    if(!ready_.load()){
      if(elapsed(now,start_ms_)>1000)fail(Fault::STARTUP);
    }else if(!maintenance_.load()){
      if(age_ms(now,bridge_beat_.load())>250)fail(Fault::TASK_STALL);
      for(auto &d:dirs_){
        const uint32_t revision=d.health_revision.load();
        const uint32_t pending=d.pending.load(),progress=d.last_progress.load(),oldest=d.oldest_ms.load();
        // Never combine old pending with a concurrently emptied/new backlog.
        if(!snapshot_consistent(revision,d.health_revision.load()))continue;
        if(tx_stalled(pending,now,progress,500))fail(Fault::TX_STALL);
        if(pending>256 && age_ms(now,oldest)>2000)fail(Fault::BACKLOG_AGE);
      }
    }
    if(maintenance_.load() && age_ms(now,maintenance_ms_.load())>120000)fail(Fault::MAINTENANCE_TIMEOUT);
    if(fault_.load()!=0){
      // No software-only bypass exists. Reset/rollback is explicitly a last resort.
      if(state==ESP_OTA_IMG_PENDING_VERIFY)esp_ota_mark_app_invalid_rollback_and_reboot();
      esp_restart();
    }
    if(allow_boot_confirm_ && !confirmed && state==ESP_OTA_IMG_PENDING_VERIFY && !maintenance_.load()){
      const uint32_t rx0=dirs_[0].rx.load(),rx1=dirs_[1].rx.load();
      const uint32_t tx0=dirs_[0].accepted.load(),tx1=dirs_[1].accepted.load();
      if(boot_can_confirm(ready_.load(),bridge_wdt_.load(),loop_detached_.load(),elapsed(now,start_ms_),
                          rx0,rx1,tx0,tx1,dirs_[0].pending.load()+dirs_[1].pending.load(),errors())){
        const esp_err_t result=__real_esp_ota_mark_app_valid_cancel_rollback();
        confirm_result_.store(static_cast<uint32_t>(result));
        if(result==ESP_OK){confirmed=true;ota_state_.store(ESP_OTA_IMG_VALID);}
      }
      // Silent/errored traffic leaves PENDING_VERIFY, not a reboot loop or false GOOD.
    }
    vTaskDelay(std::max<TickType_t>(1,pdMS_TO_TICKS(10)));
  }
}
void BoilerTransportV4::invalidate_observer(){
  for(auto &f:frames_)f.reset();last_observed_ms_=0;
  for(auto *s:{page_,boiler_sp_,dhw_sp_})if(s)s->publish_state(NAN);
  if(mode_)mode_->publish_state("UNKNOWN");
}
void BoilerTransportV4::observe(const MirrorBlock &b){
  if(b.length==0||b.length>b.bytes.size()||b.direction>1)return;
  if((last_sequence_!=0&&b.sequence!=last_sequence_+1)||b.epoch!=seen_epoch_)invalidate_observer();
  last_sequence_=b.sequence;seen_epoch_=b.epoch;
  if(elapsed(now_ms(),b.captured_ms)>30000){invalidate_observer();return;}
  const uint8_t dir=b.direction;
  if(elapsed(b.captured_ms,frame_byte_ms_[dir])>250)frames_[dir].reset();
  frame_byte_ms_[dir]=b.captured_ms;
  for(size_t i=0;i<b.length;i++){
    if(frames_[dir].append(b.bytes[i],dir==0)){frame(dir);frames_[dir].reset();}
  }
}
void BoilerTransportV4::frame(uint8_t dir){
  auto &f=frames_[dir];const auto *p=f.data();const size_t n=f.size();
  // No GET/reply attribution without a tested transaction arbiter.
  if(dir==1)return;
  if(n==5&&p[0]==0x66){if(page_)page_->publish_state(p[1]);last_observed_ms_=now_ms();return;}
  if(n==4&&p[0]==0x88){invalidate_observer();return;}
}
void BoilerTransportV4::loop(){
  const uint32_t now=now_ms();main_beat_.store(now);
  if(!external_mirror_consumer_){
    MirrorBlock b{};
    const uint32_t epoch=epoch_.load();
    if(epoch!=seen_epoch_){invalidate_observer();seen_epoch_=epoch;}
    const uint32_t drops=mirror_drops_.load();
    if(drops!=seen_drops_){
      invalidate_observer();seen_drops_=drops;last_sequence_=0;
      // Producer/consumer-safe discard, no concurrent stream reset.
      for(size_t i=0;i<128&&mirror_.pop(b);i++){}
    }else{for(size_t i=0;i<16&&mirror_.pop(b);i++)observe(b);}
    if(last_observed_ms_!=0&&elapsed(now,last_observed_ms_)>30000)invalidate_observer();
  }
  if(elapsed(now,published_ms_)>=1000){published_ms_=now;publish_diagnostics(now);}
}
void BoilerTransportV4::publish_diagnostics(uint32_t now){
  if(!diagnostics_)return;
  char out[768];
  std::snprintf(out,sizeof(out),
    "{\"fw\":\"2026.10.12.5\",\"mode\":\"PASSIVE\",\"write_capable\":false,"
    "\"ready\":%s,\"fault\":%" PRIu32 ",\"bridge_age_ms\":%" PRIu32 ",\"main_age_ms\":%" PRIu32 ","
    "\"bridge_wdt\":%s,\"loop_detached\":%s,\"rx\":[%" PRIu32 ",%" PRIu32 "],\"tx_fifo_accepted\":[%" PRIu32 ",%" PRIu32 "],"
    "\"pending\":[%" PRIu32 ",%" PRIu32 "],\"high_water\":[%" PRIu32 ",%" PRIu32 "],\"fifo_loss\":%" PRIu32 ",\"rx_full\":%" PRIu32 ","
    "\"frame_errors\":%" PRIu32 ",\"parity_errors\":%" PRIu32 ",\"api_errors\":%" PRIu32 ",\"mirror_drops\":%" PRIu32 ","
    "\"observer_age_ms\":%" PRIu32 ",\"ota_state\":%" PRIu32 ",\"ota_result\":%" PRIu32 ",\"reset_reason\":%" PRIu32 "}",
    ready_.load()?"true":"false",fault_.load(),age_ms(now,bridge_beat_.load()),age_ms(now,main_beat_.load()),
    bridge_wdt_.load()?"true":"false",loop_detached_.load()?"true":"false",
    dirs_[0].rx.load(),dirs_[1].rx.load(),dirs_[0].accepted.load(),dirs_[1].accepted.load(),
    dirs_[0].pending.load(),dirs_[1].pending.load(),dirs_[0].high_water.load(),dirs_[1].high_water.load(),
    losses_.load(),buffer_full_.load(),frame_errors_.load(),parity_errors_.load(),api_errors_.load(),mirror_drops_.load(),
    last_observed_ms_==0?UINT32_MAX:elapsed(now,last_observed_ms_),ota_state_.load(),confirm_result_.load(),
    static_cast<uint32_t>(esp_reset_reason()));
  diagnostics_->publish_state(out);
}
void BoilerTransportV4::dump_config(){
  ESP_LOGCONFIG(TAG,"FW .5 PASSIVE: dedicated IDF UART1/2; static tasks; NO actuator endpoints");
  ESP_LOGCONFIG(TAG,"Observer optional=%s; target requires bench and office provisioning",observer_enabled_?"enabled":"disabled");
}
}
