# -*- coding: utf-8 -*-
"""Boiler ESP32 OTA Manager. All OTA operations require explicit local GUI action."""
from __future__ import annotations
import argparse
import datetime as dt
import hashlib
import json
import logging
import os
from pathlib import Path
import queue
import socket
import struct
import sys
import threading
import time
import tkinter as tk
from tkinter import messagebox, scrolledtext, ttk

ROOT = Path(__file__).resolve().parent
PRIVATE = ROOT / "operation-private.json"
MANIFEST = ROOT / "installer-manifest.json"
LOGFILE = ROOT / "installer.log"
TARGET_IP = "192.168.1.172"
OTA_PORT = 3232
CHECK_TTL = 75
SAMPLE_SECONDS = 85

def timestamp():
    return dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while data := f.read(131072):
            h.update(data)
    return h.hexdigest()

def load_manifest():
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert data["target_ip"] == TARGET_IP
    assert data["schema_version"] == 1
    return data

def load_secrets():
    if not PRIVATE.is_file():
        raise RuntimeError("Нет локального файла авторизации operation-private.json")
    cfg = json.loads(PRIVATE.read_text(encoding="utf-8"))
    for k in ("ota_password", "mqtt_default_host", "mqtt_default_user", "mqtt_default_password"):
        if not cfg.get(k):
            raise RuntimeError("Не указан обязательный параметр подключения: " + k)
    return cfg

def fw_profile(which):
    data=load_manifest()["images"][which]
    path=(ROOT / data["relative_path"]).resolve()
    # No arbitrary images or directories outside pinned boiler17/boiler16.
    allowed=(ROOT / "firmware.ota.bin").resolve() if which=="v17" else (ROOT.parent/"boiler16"/"firmware.ota.bin").resolve()
    if path != allowed:
        raise RuntimeError("Путь образа не совпадает с зафиксированным.")
    if not path.is_file() or path.stat().st_size != data["bytes"]:
        raise RuntimeError(f"Файл {which} отсутствует или имеет неправильный размер")
    digest=sha(path)
    if digest != data["sha256"]:
        raise RuntimeError(f"Контрольная сумма {which} НЕ совпадает")
    return path, data

def tcp_probe(host,port):
    with socket.create_connection((host,port),timeout=3):
        pass

def _mqtt_str(s):
    b=s.encode("utf-8")
    return len(b).to_bytes(2,"big")+b

def _mqtt_packet(kind,body):
    rest=len(body);head=bytearray([kind])
    while True:
        byte=rest & 127; rest >>= 7
        head.append(byte | (128 if rest else 0))
        if not rest:
            return bytes(head)+body

def _mqtt_exact(sock,n):
    data=b""
    while len(data)<n:
        part=sock.recv(n-len(data))
        if not part:
            raise ConnectionError("MQTT connection closed")
        data+=part
    return data

def _mqtt_recv(sock):
    kind=_mqtt_exact(sock,1)[0]
    length=0;mul=1
    for _ in range(4):
        b=_mqtt_exact(sock,1)[0]
        length+=(b&127)*mul
        if not b&128:
            if length>65536:
                raise RuntimeError("Слишком большой MQTT пакет")
            return kind,_mqtt_exact(sock,length)
        mul*=128
    raise RuntimeError("Некорректная длина MQTT")

def collect_readonly(cfg,seconds=SAMPLE_SECONDS):
    """Subscribe only. Never send a message of MQTT type PUBLISH."""
    host=cfg["mqtt_default_host"]
    mqtt_port=1883
    diag_frames=[]
    states={}
    retained_diag=None
    with socket.create_connection((host,mqtt_port),timeout=5) as sock:
        sock.settimeout(1.6)
        client_id="boiler-ota-inspector-"+str(int(time.time()))
        body=(_mqtt_str("MQTT")+bytes([4,0xC2])+struct.pack("!H",40)
              +_mqtt_str(client_id)+_mqtt_str(cfg["mqtt_default_user"])
              +_mqtt_str(cfg["mqtt_default_password"]))
        sock.sendall(_mqtt_packet(0x10,body))
        kind,payload=_mqtt_recv(sock)
        if kind!=0x20 or payload!=b"\x00\x00":
            raise RuntimeError("MQTT авторизация не прошла (код %s)" % (payload.hex(),))
        sock.sendall(_mqtt_packet(0x82,b"\x00\x01"+_mqtt_str("office/boiler/#")+b"\x00"))
        kind,ack=_mqtt_recv(sock)
        if kind!=0x90 or not ack.endswith(b"\x00"):
            raise RuntimeError("MQTT подписка не принята")
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            try:
                kind,buf=_mqtt_recv(sock)
            except socket.timeout:
                continue
            if kind>>4!=3 or len(buf)<2:
                continue
            n=int.from_bytes(buf[:2],"big")
            if len(buf)<2+n:
                continue
            topic=buf[2:2+n].decode("utf-8","replace")
            pos=2+n+(2 if kind&6 else 0)
            raw=buf[pos:].decode("utf-8","replace")
            retained=bool(kind&1)
            if topic=="office/boiler/diagnostics/bridge_diag":
                try:
                    d=json.loads(raw)
                    if isinstance(d,dict):
                        if retained:
                            retained_diag=(time.monotonic(),d,True)
                        else:
                            diag_frames.append((time.monotonic(),d,False))
                except (ValueError,TypeError):
                    pass
            elif topic.endswith("/state") and any(s in topic for s in (
                "active_requests","mode_label","btemp","dtemp","page"
            )):
                if len(raw)<=128:
                    states[topic]=(raw,retained)
            # A retained baseline plus a fresh live diagnostic is sufficient
            # for preflight. The caller separately verifies the firmware version.
            if diag_frames and retained_diag and _state(states,"active_requests")[0] is not None:
                break
            if len(diag_frames)>=2 and _state(states,"active_requests")[0] is not None:
                break
        sock.sendall(b"\xe0\x00")
    if retained_diag and len(diag_frames)<2:
        return [retained_diag]+diag_frames,states
    return diag_frames,states

def _state(states,fragment):
    for k,(val,retained) in states.items():
        if fragment in k and k.endswith("/state"):
            return val,retained
    return None,None

def evaluate(diags,states,expected_fw=None,postflight=False):
    problems=[]
    if not diags:
        problems.append("Нет ни одного СВЕЖЕГО сообщения диагностики UART.")
        return problems,{}
    first=diags[0][1]
    last=diags[-1][1]
    live=[x for x in diags if len(x)>=3 and not x[2]]
    if not live:
        problems.append("Нет СВЕЖЕЙ (не retained) диагностики UART.")
    elif not postflight and len(diags)<2:
        problems.append("Нужно два контрольных замера: retained+live или два live.")
    if postflight:
        # A reboot legitimately resets all counters and retained .16 is stale.
        last=live[-1][1] if live else last
        first=last
    version=last.get("fw")
    if expected_fw and version!=expected_fw:
        problems.append(f"Ожидалась версия {expected_fw}, получена {version}.")
    for key in ("ready","telemetry_enabled"):
        if last.get(key) is not True:
            problems.append(f"{key} должен быть true")
    if last.get("fault")!=0 or last.get("ownership_fault") is not False:
        problems.append("Ошибка UART-моста или рассинхронизация!")
    if last.get("mode")!="READ_ONLY_TELEMETRY" or last.get("write_capable") is not False:
        problems.append("Управление котлом должно быть заблокировано.")
    for key in ("fifo_loss","rx_full","frame_errors","parity_errors","api_errors","own_timeouts"):
        if int(last.get(key,-1))!=0:
            problems.append(f"{key} не равен нулю")
    if list(last.get("pending",[]))!=[0,0]:
        problems.append("В UART имеются ожидающие данные")
    if not postflight:
        if first.get("fw")!=last.get("fw"):
            problems.append("Версия между контрольными снимками изменилась.")
        for key in ("mirror_drops","query_errors","own_timeouts"):
            if int(last.get(key,0)) > int(first.get(key,0)):
                problems.append(f"{key} увеличивается во время проверки")
        rx_before=first.get("rx")
        rx_after=last.get("rx")
        if isinstance(rx_before,list) and isinstance(rx_after,list) and len(rx_before)==len(rx_after)==2:
            if any(a<b for a,b in zip(rx_after,rx_before)):
                problems.append("ESP перезагрузился между контрольными снимками.")
    active,_=_state(states,"active_requests")
    if active!="OFF":
        problems.append("ACTIVE_REQUESTS не подтверждён как OFF.")
    btemp,_=_state(states,"btemp/state")
    dtemp,_=_state(states,"dtemp/state")
    page,_=_state(states,"page/state")
    summary={
        "fw":version,"ready":last.get("ready"),"fault":last.get("fault"),
        "mode":last.get("mode"),"uart_pending":last.get("pending"),
        "fifo_loss":last.get("fifo_loss"),"mirror_drops":last.get("mirror_drops"),
        "active_requests":active,"temperature":btemp,"dhw_temperature":dtemp,
         "page":page,"live_diagnostics":len(live),
        "baseline_retained":any(len(x)>=3 and x[2] for x in diags)}
    return problems,summary

def preflight(target,emit):
    path,profile=fw_profile(target)
    emit(f"Проверен образ {target} · {path.stat().st_size} байт · SHA256 {profile['sha256']}")
    cfg=load_secrets()
    for port in (80,OTA_PORT,6053):
        tcp_probe(TARGET_IP,port)
    emit("ESP32 доступен по портам 80 / 3232 / 6053.")
    emit(f"Собираю свежую диагностику MQTT ({SAMPLE_SECONDS} секунд)...")
    frames,states=collect_readonly(cfg,SAMPLE_SECONDS)
    expected=profile["expected_before"]
    problems,status=evaluate(frames,states,expected_fw=expected)
    emit("Показания: "+json.dumps(status,ensure_ascii=False))
    if problems:
        raise RuntimeError("Предпроверка НЕ пройдена: "+"; ".join(problems))
    emit("Предпроверка пройдена. Автоматического запуска OTA не будет.")
    return status

class LogHandler(logging.Handler):
    def __init__(self,post):
        super().__init__(logging.INFO)
        self.post=post
    def emit(self,record):
        msg=record.getMessage()
        # Log progress/errors from ESPHome without credentials.
        self.post(f"ESPHome: {msg[:450]}")

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Котёл ESP32 — локальный установщик")
        self.geometry("860x735")
        self.minsize(760,620)
        self.events=queue.Queue()
        self.busy=False
        self.prechecked=None
        self.current_target=tk.StringVar(value="v17")
        self.local_person=tk.BooleanVar(value=False)
        self.local_shutdown=tk.BooleanVar(value=False)
        self.phrase=tk.StringVar()
        self.status=tk.StringVar(value="Установка не запускалась")
        self._layout()
        self.after(150,self._drain)
        self.log("Установщик открыт. Изменений в котле нет.")
        self.log("Доступные образы фиксированы SHA256. Секреты не печатаются.")
        self.log("Для прошивки обязательны предпроверка и физический наблюдатель.")

    def _layout(self):
        root=ttk.Frame(self,padding=17)
        root.pack(fill="both",expand=True)
        ttk.Label(root,text="Котёл • Установщик ESP32",font=("Segoe UI",17,"bold")).pack(anchor="w")
        ttk.Label(root,text="IP 192.168.1.172   |   Только подтверждённые .17 и .16   |   Без автоматической OTA",foreground="#555555").pack(anchor="w",pady=(4,12))
        box=ttk.LabelFrame(root,text="1. Выбор прошивки",padding=10)
        box.pack(fill="x")
        ttk.Radiobutton(box,text="Установить диагностическую .17 (только чтение)",variable=self.current_target,value="v17",command=self._invalidate).pack(anchor="w",pady=2)
        ttk.Radiobutton(box,text="Вернуть резервную .16 (откат)",variable=self.current_target,value="v16",command=self._invalidate).pack(anchor="w",pady=2)
        ttk.Label(box,text="Ни одна версия не запускается автоматически. Повторных попыток после сбоя нет.",foreground="#555555").pack(anchor="w",pady=(4,0))
        box2=ttk.LabelFrame(root,text="2. Проверка перед OTA",padding=10)
        box2.pack(fill="x",pady=(10,0))
        buttons=ttk.Frame(box2)
        buttons.pack(fill="x")
        self.btn_check=ttk.Button(buttons,text="Проверить файлы, сеть и UART",command=self._start_check)
        self.btn_check.pack(side="left")
        self.btn_status=ttk.Button(buttons,text="Обновить диагностику (без записи)",command=self._start_check)
        self.btn_status.pack(side="left",padx=8)
        ttk.Label(box2,textvariable=self.status).pack(anchor="w",pady=(8,0))
        box3=ttk.LabelFrame(root,text="3. Подтверждение на месте",padding=10)
        box3.pack(fill="x",pady=(10,0))
        ttk.Checkbutton(box3,text="Человек находится непосредственно у котла и наблюдает за физическим экраном",variable=self.local_person).pack(anchor="w")
        ttk.Checkbutton(box3,text="Он может безопасно остановить котёл; режим котла допускает разрыв UART при OTA",variable=self.local_shutdown).pack(anchor="w")
        row=ttk.Frame(box3);row.pack(fill="x",pady=(7,0))
        ttk.Label(row,text="Для запуска ввести ПОДТВЕРЖДАЮ:").pack(side="left")
        ttk.Entry(row,textvariable=self.phrase,width=22).pack(side="left",padx=8)
        self.btn_flash=ttk.Button(box3,text="УСТАНОВИТЬ ВЫБРАННУЮ ПРОШИВКУ (OTA)",command=self._start_flash)
        self.btn_flash.pack(anchor="w",pady=(10,0))
        ttk.Label(box3,text="Во время OTA связь контроллера с экраном прервётся. При ошибке не повторять OTA автоматически.",foreground="#9A3A00").pack(anchor="w")
        box4=ttk.LabelFrame(root,text="Журнал и проверка после OTA",padding=8)
        box4.pack(fill="both",expand=True,pady=(10,0))
        self.output=scrolledtext.ScrolledText(box4,height=10,wrap="word",font=("Consolas",9),state="disabled")
        self.output.pack(fill="both",expand=True)
        row2=ttk.Frame(root);row2.pack(fill="x",pady=(8,0))
        ttk.Button(row2,text="Открыть папку с журналом",command=lambda: os.startfile(ROOT)).pack(side="left")
        ttk.Label(row2,text="Журнал: boiler17\\installer.log",foreground="#555555").pack(side="left",padx=14)

    def _invalidate(self):
        self.prechecked=None
        self.status.set("Выбрана другая версия. Нужна новая предпроверка.")

    def log(self,message):
        entry=timestamp()+" | "+str(message)
        try:
            with LOGFILE.open("a",encoding="utf-8") as f:
                f.write(entry+"\n")
        except OSError:
            pass
        self.output.configure(state="normal")
        self.output.insert("end",entry+"\n")
        self.output.see("end")
        self.output.configure(state="disabled")

    def emit(self,text):
        self.events.put(("log",text))

    def _drain(self):
        while True:
            try:typ,value=self.events.get_nowait()
            except queue.Empty:break
            if typ=="log":
                self.log(value)
            elif typ=="check_ok":
                target,checked_status=value
                self.prechecked=(target,time.monotonic())
                self.status.set("Предпроверка пройдена. Можно запускать OTA после подтверждения на месте.")
                self.log("PASS: проверка завершена. Установка ещё НЕ начиналась.")
                self._busy(False)
            elif typ=="flash_end":
                ok,explanation=value
                self.prechecked=None
                self.status.set(explanation)
                self.log(("PASS: " if ok else "ПРОВЕРКА НЕ ПРОЙДЕНА: ")+explanation)
                self._busy(False)
            elif typ=="error":
                self.prechecked=None
                self.status.set("Остановлено: "+str(value)[:200])
                self.log("STOP: "+str(value)[:1500])
                self._busy(False)
        self.after(150,self._drain)

    def _busy(self,val):
        self.busy=val
        state="disabled" if val else "normal"
        for b in (self.btn_check,self.btn_status,self.btn_flash):
            b.configure(state=state)

    def _work(self,fn):
        self._busy(True)
        def thread_fn():
            try:fn()
            except Exception as exc:self.events.put(("error",str(exc)))
        threading.Thread(target=thread_fn,daemon=True).start()

    def _start_check(self):
        if self.busy:return
        target=self.current_target.get()
        self.prechecked=None
        self.status.set("Выполняется предпроверка…")
        self._work(lambda:self.events.put(("check_ok",(target,preflight(target,self.emit)))))

    def _start_flash(self):
        if self.busy:return
        target=self.current_target.get()
        if not self.prechecked or self.prechecked[0]!=target or time.monotonic()-self.prechecked[1]>CHECK_TTL:
            messagebox.showwarning("OTA запрещена","Сначала пройдите свежую предпроверку. Она действительна 75 секунд.")
            return
        if not self.local_person.get() or not self.local_shutdown.get() or self.phrase.get().strip()!="ПОДТВЕРЖДАЮ":
            messagebox.showwarning("OTA запрещена","Нужно подтвердить наблюдение у котла, возможность отключения и ввести ПОДТВЕРЖДАЮ.")
            return
        version=load_manifest()["images"][target]["version"]
        if not messagebox.askyesno("Подтвердите единственную OTA",f"Отправить {version} на {TARGET_IP}?\n\nОператор должен видеть физический экран.\nРазрыв UART во время OTA неизбежен.\nАвтоматический откат НЕ выполняется.",icon="warning"):
            return
        self.prechecked=None
        self.phrase.set("")
        self._work(lambda:self._flash_worker(target))

    def _flash_worker(self,target):
        import msvcrt
        import esphome.espota2 as espota2
        path,profile=fw_profile(target)  # Again, immediately before write.
        cfg=load_secrets()
        # Single firmware update across all installer windows.
        lock=ROOT/"ota.lock"
        with lock.open("a+b") as fh:
            if fh.tell()==0:
                fh.write(b" ")
                fh.flush()
            fh.seek(0)
            try:
                msvcrt.locking(fh.fileno(),msvcrt.LK_NBLCK,1)
            except OSError as e:
                raise RuntimeError("Другая OTA уже запущена. Прекращаю.") from e
            try:
                tcp_probe(TARGET_IP,OTA_PORT)
                self.emit("ПРЕДУПРЕЖДЕНИЕ: старт OTA "+profile["version"]+". UART на время перезагрузки прервётся.")
                handler=LogHandler(self.emit)
                logger=logging.getLogger("esphome")
                logger.addHandler(handler)
                oldlevel=logger.level
                logger.setLevel(logging.INFO)
                # No hidden automatic retries after interrupted upload.
                espota2.EXTRA_UPLOAD_ATTEMPTS=0
                started=time.monotonic()
                try:
                    code,reason=espota2.run_ota(TARGET_IP,OTA_PORT,cfg["ota_password"],path)
                finally:
                    logger.removeHandler(handler)
                    logger.setLevel(oldlevel)
                self.emit(f"OTA загрузчик: result_code={code}, duration={time.monotonic()-started:.1f} сек.")
                if code!=0:
                    raise RuntimeError("Загрузчик сообщил об ошибке. Не повторять автоматически. Проверьте физический экран.")
                self.emit("Загрузчик сообщил об успешной OTA. Это ещё НЕ подтверждает безопасную работу котла.")
                self.emit("Сверяю НОВУЮ версию и обмен UART по свежим данным MQTT…")
                time.sleep(9)
                frames,states=collect_readonly(cfg,85)
                problems,status=evaluate(frames,states,expected_fw=profile["version"],postflight=True)
                self.emit("Postflight: "+json.dumps(status,ensure_ascii=False))
                if problems:
                    raise RuntimeError("OTA отправлена, но postflight НЕ пройден: "+"; ".join(problems)+". НУЖНА ПРОВЕРКА НА МЕСТЕ.")
                self.events.put(("flash_end",(True,"Версия "+profile["version"]+" подтверждена по живой диагностике; экран проверить физически.")))
            finally:
                try:msvcrt.locking(fh.fileno(),msvcrt.LK_UNLCK,1)
                except OSError:pass

def selftest():
    assert load_manifest()["images"]["v16"]["expected_before"]=="2026.10.23.17"
    for which in ("v16","v17"):
        file,profile=fw_profile(which)
        print("SELFTEST",which,"PASS","BYTES",file.stat().st_size,"SHA256",profile["sha256"])
    sample={"fw":"2026.10.20.16","ready":True,"fault":0,"telemetry_enabled":True,
            "ownership_fault":False,"mode":"READ_ONLY_TELEMETRY","write_capable":False,
            "fifo_loss":0,"rx_full":0,"frame_errors":0,"parity_errors":0,
            "api_errors":0,"own_timeouts":0,"pending":[0,0],
            "query_errors":1,"mirror_drops":2018}
    now=time.monotonic()
    rows=[(now,sample,True),(now+3,sample,False)]
    state={"office/boiler/switch/test_active_requests/state":("OFF",True),
           "office/boiler/sensor/x_btemp/state":("39.6",False)}
    problems,info=evaluate(rows,state,expected_fw="2026.10.20.16")
    assert not problems,(problems,info)
    bad=dict(sample,fault=1)
    problems,_=evaluate([(now,sample,True),(now+3,bad,False)],state)
    assert problems
    print("SELFTEST_EVALUATION PASS")
    print("SELFTEST_NO_OTA_TRIGGERED PASS")

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--self-test",action="store_true")
    parser.add_argument("--verify-only",action="store_true")
    args=parser.parse_args()
    if args.self_test:
        selftest()
    elif args.verify_only:
        for target in ("v17","v16"):
            _,record=fw_profile(target)
            print(target,record["version"],"PASS")
    else:
        App().mainloop()
