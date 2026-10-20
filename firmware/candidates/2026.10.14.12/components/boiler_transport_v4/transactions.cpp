#include "boiler_transport_v4.h"
#include <esp_timer.h>
#include <cstdio>

namespace esphome::boiler_transport_v4 {
uint32_t BoilerTransportV4::submit(Request r){
  r.id=request_sequence_.fetch_add(1)+1;
  r.created_ms=static_cast<uint32_t>(esp_timer_get_time()/1000ULL);
  if(!requests_||!telemetry_enabled()||maintenance_.load()||!ready_.load()||fault_.load()!=0)return false;
  if(r.kind==RequestKind::CLICK&&(!active_enabled_.load()||!bkcmd_verified_.load()||!(bkcmd_zero_.load()||(panel_bkcmd_.load()==2&&marker_verified_.load()))))return false;
  if(xQueueSend(requests_,&r,0)!=pdTRUE){request_drops_.fetch_add(1);return false;}
  return r.id;
}
bool BoilerTransportV4::pop_result(Result &r){return results_&&xQueueReceive(results_,&r,0)==pdTRUE;}
void BoilerTransportV4::result(const Request &r,ResultCode c,const uint8_t *bytes,size_t n){
  Result out{};out.request=r;out.code=c;out.length=std::min(n,out.bytes.size());
  if(bytes&&out.length)std::memcpy(out.bytes.data(),bytes,out.length);
  if(xQueueSend(results_,&out,0)!=pdTRUE){result_drops_.fetch_add(1);epoch_.fetch_add(1);active_enabled_.store(false);arm_requested_=false;marker_verified_=false;}
}
void BoilerTransportV4::ownership_failure(){
  ownership_fault_=true;active_enabled_.store(false);arm_requested_.store(false);marker_verified_.store(false);epoch_.fetch_add(1);
  // Keep the ledger, including timed-out OWN tombstones. A late response must
  // never be re-labelled as the next stock response or the next own request.
}
void BoilerTransportV4::wire_command(uint8_t b,uint32_t now){
  command_byte_ms_=now;wire_generation_++;
  if(stock_command_.size()==512)ownership_failure();
  if(!stock_command_.append(b,false))return;
  const uint8_t *p=stock_command_.data();const size_t n=stock_command_.size();
  RequestKind kind;
  if(tracked_stock_command(p,n,kind)){
    Owner owner{};owner.reply=kind;owner.sent_ms=now;
    if(!owners_.push(owner))ownership_failure();
  }
  // bkcmd is global panel state; never change it ourselves. The validated
  // query/click contract supports bkcmd=0. Revalidate if the boiler changes it.
  if(n>=9&&std::memcmp(p,"bkcmd=",6)==0){bkcmd_zero_=false;bkcmd_verified_=false;panel_bkcmd_=255;active_enabled_.store(false);arm_requested_.store(false);marker_verified_.store(false);}
  stock_command_.reset();
}
void BoilerTransportV4::wire_response(Direction &d,uint8_t b,uint32_t now){
  response_byte_ms_=now;wire_generation_++;
  const Owner *owner=owners_.front();
  const bool withholding=held_length_!=0||(response_frame_.size()==0&&((owner&&owner->own&&!ownership_fault_)||fence_pending_));
  if(withholding){
    if(held_length_==held_.size()){
      emit_stock(d,held_.data(),held_length_,now);held_length_=0;ownership_failure();
    }
    if(!ownership_fault_)held_[held_length_++]=b;else emit_stock(d,&b,1,now);
  }else emit_stock(d,&b,1,now);
  if(response_frame_.append(b,true)){
    finish_response(d,now,held_length_!=0);response_frame_.reset();
  }else if(response_frame_.size()==0&&held_length_!=0){
    emit_stock(d,held_.data(),held_length_,now);held_length_=0;ownership_failure();
  }
}
void BoilerTransportV4::finish_response(Direction &d,uint32_t now,bool withheld){
  const uint8_t *p=response_frame_.data();const size_t n=response_frame_.size();
  Owner *owner=owners_.front();bool consume=false;
  if(handle_fence(p,n,now,consume)){
    // Markers delimit our serial bundle; asynchronous data remains stock.
  }else if(owner&&response_matches(owner->reply,p,n)){
    consume=owner->own&&!ownership_fault_;
    if(owner->own){
      if(!owner->expired&&!ownership_fault_){
        const Request r=owner->request;
        if(r.kind==RequestKind::BKCMD){
          const uint32_t mode=uint32_t(p[1])|(uint32_t(p[2])<<8)|(uint32_t(p[3])<<16)|(uint32_t(p[4])<<24);
          panel_bkcmd_=mode;bkcmd_verified_=mode<=3;bkcmd_zero_=mode==0;
          mode_checked_request_=mode<=3?r.id:0;
          // GET and sendme return data independently of bkcmd. Only CLICK
          // requires the separately validated zero-ACK actuation contract.
          if(mode!=0&&!(mode==2&&marker_verified_.load()))active_enabled_.store(false);
          if(mode>3)ownership_failure();
          result(r,mode<=3?ResultCode::OK:ResultCode::REJECTED,p,n);
        }else if(owner->reply==RequestKind::PAGE&&r.kind!=RequestKind::PAGE){
          if(p[1]!=r.page)result(r,ResultCode::WRONG_PAGE,p,n);
          else {deferred_click_=r;click_pending_=true;click_due_ms_=now+20;verified_generation_=wire_generation_;}
        }else result(r,ResultCode::OK,p,n);
      }
    }
    if(owner->own&&owner->expired)timeout_latched_.store(false);
    owners_.pop();
  }else if(instruction_error(p,n)||(n==4&&p[0]==0x01&&bkcmd_zero_)){
    // An error has no transaction ID. Do not guess whether a stock assignment
    // or query failed. Preserve the bytes and disable every active injection.
    ownership_failure();
  }
  if(withheld){
    if(consume)suppressed_.fetch_add(held_length_);
    else emit_stock(d,held_.data(),held_length_,now);
    held_length_=0;
  }
  if(n==4&&p[0]==0x88){
    // A real panel startup is a protocol reset boundary, not a parser guess.
    owners_.clear();bkcmd_zero_=false;bkcmd_verified_=false;panel_bkcmd_=255;active_enabled_.store(false);ownership_fault_=false;timeout_latched_=false;click_pending_=false;marker_verified_=false;arm_requested_=false;fence_pending_=false;
  }
}
bool BoilerTransportV4::begin_fence(const Request &r,uint32_t now,bool click){
  if(fence_pending_||owners_.size()!=0||marker_sequence_==UINT32_MAX)return false;
  ++marker_sequence_;
  char token[17];std::snprintf(token,sizeof(token),"%08x%08x",unsigned(marker_nonce_),unsigned(marker_sequence_));
  for(auto *m:{&fence_start_,&fence_end_}){
    (*m)[0]=0xfd;(*m)[1]='B';(*m)[2]='R';(*m)[3]='G';
    std::memcpy(m->data()+5,token,16);(*m)[21]=(*m)[22]=(*m)[23]=0xff;
  }
  fence_start_[4]=1;fence_end_[4]=2;
  char command[256]{};size_t at=0;
  auto marker=[&](const auto &bytes){
    at+=std::snprintf(command+at,sizeof(command)-at,"printh");
    for(uint8_t b:bytes)at+=std::snprintf(command+at,sizeof(command)-at," %02X",unsigned(b));
    std::memcpy(command+at,"\xff\xff\xff",3);at+=3;
  };
  marker(fence_start_);
  if(click)at+=std::snprintf(command+at,sizeof(command)-at,"click %u,1\xff\xff\xff" "click %u,0\xff\xff\xff",r.component,r.component);
  marker(fence_end_);at-=3;command[at]=0; // inject appends final instruction terminator
  if(at>=sizeof(command)||!inject(command,now))return false;
  fence_request_=r;fence_sent_ms_=now;fence_pending_=true;fence_started_=false;fence_failed_=false;fence_expired_=false;
  return true;
}
bool BoilerTransportV4::handle_fence(const uint8_t *p,size_t n,uint32_t now,bool &consume){
  if(!fence_pending_)return false;
  if(n==fence_start_.size()&&std::memcmp(p,fence_start_.data(),n)==0){
    consume=true;if(fence_started_){fence_failed_=true;ownership_failure();}fence_started_=true;return true;
  }
  if(n==fence_end_.size()&&std::memcmp(p,fence_end_.data(),n)==0){
    consume=true;
    const bool good=fence_started_&&!fence_failed_&&!fence_expired_&&!ownership_fault_;
    if(!fence_started_)ownership_failure();
    if(!fence_expired_)result(fence_request_,good?(fence_request_.kind==RequestKind::FENCE?ResultCode::OK:ResultCode::SENT_UNCONFIRMED):ResultCode::REJECTED);
    if(fence_request_.kind==RequestKind::FENCE){
      marker_verified_=good&&panel_bkcmd_.load()==2;
      if(marker_verified_&&arm_requested_.load()&&ready_&&fault_.load()==0&&!maintenance_&&age_ms(now,main_beat_.load())<=2000)active_enabled_=true;
    }
    if(!good){active_enabled_=false;arm_requested_=false;marker_verified_=false;}
    fence_pending_=false;
    // A timed-out fence remains a tombstone until its exact end marker arrives.
    if(fence_expired_)timeout_latched_=false;
    return true;
  }
  if(fence_started_){
    if(instruction_error(p,n)||(n==4&&p[0]==1)){
      consume=true;fence_failed_=true;active_enabled_=false;arm_requested_=false;marker_verified_=false;
    }
    // Preserve touch/page/numeric/print events and do not match them to stock
    // GETs queued behind this bundle. Their responses execute after END.
    return true;
  }
  return false;
}
bool BoilerTransportV4::injection_window(uint32_t now){
  if(fence_pending_||ownership_fault_||maintenance_.load()||owners_.size()!=0||held_length_!=0||
     stock_command_.size()!=0||response_frame_.size()!=0||age_ms(now,last_wire_ms_)<30)return false;
  if(dirs_[0].tx.size()!=0||dirs_[1].tx.size()!=0)return false;
  size_t n=0;
  for(auto &d:dirs_){
    if(uart_get_buffered_data_len(d.port,&n)!=ESP_OK||n!=0||uart_wait_tx_done(d.port,0)!=ESP_OK)return false;
  }
  return true;
}
bool BoilerTransportV4::inject(const char *command,uint32_t now){
  const size_t n=std::strlen(command);auto &d=dirs_[1];
  if(n+3>d.tx.free())return false;
  static const uint8_t tail[]{0xff,0xff,0xff};
  d.health_revision.fetch_add(1);
  d.tx.push(reinterpret_cast<const uint8_t*>(command),n);d.tx.push(tail,3);
  d.oldest_ms.store(now);d.last_progress.store(now);d.pending.store(d.tx.size());
  injected_.fetch_add(n+3);d.health_revision.fetch_add(1);last_injection_ms_=now;
  return true;
}
void BoilerTransportV4::service_requests(uint32_t now){
  if(stock_command_.size()!=0&&age_ms(now,command_byte_ms_)>250){stock_command_.reset();ownership_failure();}
  if(response_frame_.size()!=0&&age_ms(now,response_byte_ms_)>250){
    if(held_length_){emit_stock(dirs_[0],held_.data(),held_length_,now);held_length_=0;}
    response_frame_.reset();ownership_failure();
  }
  auto *owner=owners_.front();
  if(owner&&owner->own&&!owner->expired&&age_ms(now,owner->sent_ms)>1500){
    owner->expired=true;timeout_latched_.store(true);own_timeouts_.fetch_add(1);result(owner->request,ResultCode::TIMEOUT);
    active_enabled_.store(false);click_pending_=false;
    // Keep this tombstone to swallow a late exact reply. No automatic retry.
  }
  if(fence_pending_&&!fence_expired_&&age_ms(now,fence_sent_ms_)>1500){
    fence_expired_=true;fence_failed_=true;timeout_latched_=true;active_enabled_=false;arm_requested_=false;marker_verified_=false;
    result(fence_request_,ResultCode::TIMEOUT);
  }
  if(ownership_fault_||timeout_latched_){active_enabled_.store(false);arm_requested_=false;}
  if(!ready_.load()||fault_.load()!=0||maintenance_.load()||age_ms(now,main_beat_.load())>2000){active_enabled_=false;arm_requested_=false;}
  if(fence_pending_)return;
  if(arm_requested_.load()&&!active_enabled_.load()&&age_ms(now,arm_requested_ms_.load())>3000){
    arm_requested_=false;Request r{};r.kind=RequestKind::FENCE;result(r,ResultCode::EXPIRED);
  }
  if(arm_requested_.load()&&!active_enabled_.load()&&panel_bkcmd_.load()==2&&bkcmd_verified_.load()&&
     ready_.load()&&!maintenance_.load()&&telemetry_enabled()&&injection_window(now)){
    Request probe{};probe.kind=RequestKind::FENCE;probe.id=request_sequence_.fetch_add(1)+1;probe.created_ms=now;
    if(!begin_fence(probe,now,false)){arm_requested_=false;result(probe,ResultCode::REJECTED);}
    return;
  }
  const bool enabled=active_enabled_.load();
  // Existing verification remains valid until a stock bkcmd change/reset.
  was_enabled_=enabled;
  if(!telemetry_enabled()||maintenance_.load()||!ready_.load()||fault_.load()!=0){
    Request r;for(size_t i=0;i<8&&xQueueReceive(requests_,&r,0)==pdTRUE;i++)result(r,ResultCode::REJECTED);
    if(click_pending_){result(deferred_click_,ResultCode::REJECTED);click_pending_=false;}
    return;
  }
  if(click_pending_){
    if(deferred_click_.kind==RequestKind::CLICK&&(!enabled||!(bkcmd_zero_||(panel_bkcmd_.load()==2&&marker_verified_.load())))){result(deferred_click_,ResultCode::REJECTED);click_pending_=false;return;}
    if(request_expired(deferred_click_,now)){result(deferred_click_,ResultCode::EXPIRED);click_pending_=false;expired_.fetch_add(1);return;}
    if(static_cast<int32_t>(now-click_due_ms_)<0||!injection_window(now))return;
    // Verified current page immediately precedes the command. Any new stock
    // packet cancels this click (including a page transition). Never replay.
    if(wire_generation_!=verified_generation_){result(deferred_click_,ResultCode::DESYNC);click_pending_=false;return;}
    char command[64];
    if(deferred_click_.kind==RequestKind::CLICK){
      // A single contiguous pair cannot leave a virtual finger held when the
      // observer freezes; neither part is replayed after a timeout.
      std::snprintf(command,sizeof(command),"click %u,1\xff\xff\xff" "click %u,0",deferred_click_.component,deferred_click_.component);
      if(panel_bkcmd_.load()==2){
        if(!begin_fence(deferred_click_,now,true))result(deferred_click_,ResultCode::REJECTED);
      }else if(inject(command,now))result(deferred_click_,ResultCode::SENT_UNCONFIRMED);
      else result(deferred_click_,ResultCode::REJECTED);
    }else{
      if(deferred_click_.page==10&&deferred_click_.component==254)std::snprintf(command,sizeof(command),"get page10.TsR.val");
      else std::snprintf(command,sizeof(command),"get b[%u].%s",deferred_click_.component,deferred_click_.kind==RequestKind::TEXT?"txt":"val");
      Owner own{};own.own=true;own.request=deferred_click_;own.reply=deferred_click_.kind;own.sent_ms=now;
      if(inject(command,now))owners_.push(own);else result(deferred_click_,ResultCode::REJECTED);
    }
    click_pending_=false;return;
  }
  Request r;
  if(xQueuePeek(requests_,&r,0)!=pdTRUE)return;
  if(request_expired(r,now)){xQueueReceive(requests_,&r,0);expired_.fetch_add(1);result(r,ResultCode::EXPIRED);return;}
  if(!injection_window(now)||age_ms(now,last_injection_ms_)<150)return;
  if(r.kind==RequestKind::CLICK&&!enabled){xQueueReceive(requests_,&r,0);result(r,ResultCode::REJECTED);return;}
  if(!bkcmd_verified_||(r.kind==RequestKind::CLICK&&mode_checked_request_!=r.id)){
    Owner check{};check.own=true;check.reply=RequestKind::NUMBER;check.sent_ms=now;check.request.kind=RequestKind::BKCMD;check.request.id=r.id;
    if(inject("get bkcmd",now))owners_.push(check);
    return;
  }
  xQueueReceive(requests_,&r,0);
  Owner own{};own.own=true;own.request=r;own.sent_ms=now;
  char command[64];
  own.reply=RequestKind::PAGE;std::snprintf(command,sizeof(command),"sendme");
  if(inject(command,now))owners_.push(own);else result(r,ResultCode::REJECTED);
}
}
