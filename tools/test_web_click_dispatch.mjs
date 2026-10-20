import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
function fixture(path){
 let src=fs.readFileSync(path,'utf8').replace(/\}\)\(\);\s*$/, 'globalThis.api={S,N,B,idOf,render,connectEvents,commandsEnabled};})();');
 const events={},docEvents={},timers=[],requests=[];
 let button=null,domVersion=0;
 const root={
  boilerCommandsBound:false,
  addEventListener(n,f){(events[n]??=[]).push(f)},
  contains(el){return el===button},
  querySelector(){return null},
  querySelectorAll(s){return s==='[data-btn]'?[button]:[]},
  set innerHTML(v){domVersion++;button={dataset:{btn:'Главная режим MODEBTN'},disabled:false,
   classList:{add(){},remove(){}},setAttribute(){},
   closest(s){return s==='[data-btn]'?this:null}}}
 };
 const ctx={console,Date,encodeURIComponent,confirm:()=>true,
  setTimeout(fn){timers.push(fn);return timers.length},clearTimeout(){},
  document:{readyState:'loading',activeElement:null,getElementById:id=>id==='boiler-root'?root:null,
   addEventListener(n,f){(docEvents[n]??=[]).push(f)}},
  EventSource:class{addEventListener(n,f){events['es:'+n]=[f]}},
  fetch:async(url,opts)=>{requests.push({url,opts});return {ok:true,status:200}}};
 vm.createContext(ctx);vm.runInContext(src,ctx);
 const a=ctx.api;function state(key,value){for(const f of events['es:state']||[])f({data:JSON.stringify({id:a.idOf(a.N[key]),value})})}
 a.connectEvents();for(const f of events['es:open'])f();
 state('active','ON');state('page',1);
 state('diag',JSON.stringify({ready:true,fault:0,ownership_fault:false,bkcmd_verified:true,control_supported:true,
 panel_bkcmd:2,telemetry_enabled:true,write_capable:true,marker_verified:true}));
 function emit(name,el=button,key){for(const f of events[name]||[])f({target:el,key})}
 function documentEmit(name){for(const f of docEvents[name]||[])f({})}
 return {a,state,emit,documentEmit,requests,timers,ctx,get button(){return button},get domVersion(){return domVersion}};
}
const old=fixture('firmware/candidates/2026.10.14.13/www/boiler.js');
assert(old.a.commandsEnabled());old.emit('click');
assert.equal(old.requests.length,0,'Reproduced .13: enabled web button sends no request');
const f=fixture('firmware/candidates/2026.10.19.14/www/boiler.js');
assert(f.a.commandsEnabled());f.emit('click');assert.equal(f.requests.length,1);
assert.equal(f.requests[0].url,'/button/'+encodeURIComponent(f.a.B.mode)+'/press');assert.equal(f.requests[0].opts.method,'POST');
// Re-rendering must never attach a second delegated click handler.
f.a.render();f.a.render();f.emit('click');assert.equal(f.requests.length,2);
// During a real press, SSE updates must not replace the event target.
const target=f.button,version=f.domVersion;
f.emit('pointerdown',target);f.state('page',1);assert.equal(f.domVersion,version);assert.equal(f.button,target);
f.documentEmit('pointerup');f.state('page',1);assert.equal(f.button,target);
f.emit('click',target);assert.equal(f.requests.length,3);
// Button disabled after any gate failure. No stale UI request escapes.
f.state('active','OFF');f.emit('click',target);assert.equal(f.requests.length,3);
f.state('active','ON');f.state('diag',JSON.stringify({ready:true,fault:0,ownership_fault:true}));
f.emit('click',target);assert.equal(f.requests.length,3);
assert(f.requests.every(x=>x.url.includes('MODEBTN')),'Tests must contain no actuator request');
// Keyboard gestures also preserve the target through the native click.
const k=fixture('firmware/candidates/2026.10.19.14/www/boiler.js'),kb=k.button,kversion=k.domVersion;
k.emit('keydown',kb,'Enter');k.state('page',1);assert.equal(k.domVersion,kversion);
k.documentEmit('keyup');k.emit('click',kb);assert.equal(k.requests.length,1);

// A newly wired actuator button must still require the pre-existing confirmation.
const cancelled=fixture('firmware/candidates/2026.10.19.14/www/boiler.js');
cancelled.ctx.confirm=()=>false;cancelled.button.dataset.btn=cancelled.a.B.run;
cancelled.emit('click');assert.equal(cancelled.requests.length,0);
// Stale effective state must block even if a displayed button still looks enabled.
const stale=fixture('firmware/candidates/2026.10.19.14/www/boiler.js');
stale.ctx.Date={now:()=>Date.now()+6001};stale.emit('click');
assert.equal(stale.requests.length,0);
console.log('PASS actual .13 no-dispatch reproduced; .14 single POST, no duplicate handler, SSE gesture preservation, keyboard and OFF/fault gates');
