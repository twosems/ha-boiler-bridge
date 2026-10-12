import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const path='firmware/candidates/2026.10.12.6/www/boiler.js';
let source=fs.readFileSync(path,'utf8');
source=source.replace(/\}\)\(\);\s*$/, 'globalThis.testApi={render,saveMqtt,page,setFeedback,setSettings:x=>showSettings=x};})();');
const fields=Object.fromEntries(['mqtt-host','mqtt-port','mqtt-user','mqtt-pass','wifi-ssid','wifi-password'].map(id=>[id,{value:''}]));
let renders=0;const root={querySelectorAll:()=>[],set innerHTML(v){this.html=v;renders++;for(const field of Object.values(fields))field.value='';}};
const requests=[];
const context={console,setTimeout:()=>1,clearTimeout:()=>{},encodeURIComponent,confirm:()=>true,
 document:{readyState:'loading',addEventListener:()=>{},activeElement:{tagName:'INPUT'},getElementById:id=>id==='boiler-root'?root:fields[id]},
 fetch:async(url,opts)=>{requests.push({url,opts});return {ok:true,status:200}}};
vm.createContext(context);vm.runInContext(source,context);
const api=context.testApi;assert.ok(api);api.setSettings(true);
fields['mqtt-host'].value='192.168.1.203';fields['mqtt-port'].value='1883';fields['mqtt-user'].value='test';fields['mqtt-pass'].value='example-only';
api.render();assert.equal(renders,0,'SSE must not replace a focused form');
api.setFeedback('test error');assert.equal(renders,1);assert.equal(fields['mqtt-host'].value,'192.168.1.203');assert.ok(root.html.includes('test error'));
await api.saveMqtt();assert.equal(requests.length,5);assert.ok(requests[0].url.endsWith('?value=192.168.1.203'));assert.ok(requests[1].url.endsWith('?value=1883'));assert.ok(requests[4].url.startsWith('/button/'));
requests.length=0;fields['mqtt-port'].value='1883bad';await api.saveMqtt();assert.equal(requests.length,0);assert.equal(fields['mqtt-port'].value,'1883bad');
assert.equal(api.page(),255,'Missing PAGE must never be fabricated as the main screen');
console.log('PASS: focused SSE form; draft preservation on feedback; exact MQTT write/apply order; invalid port rejection; unknown PAGE');
