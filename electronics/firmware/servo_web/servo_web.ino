// ============================================================
// PDI-1109MG × ESP32-WROOM 舵机网页控制台
// 接线（38脚DevKit左下角）：棕→GND(第14脚)  橙(信号)→GPIO13(第15脚)
//       红→VIN 5V(第19脚，外接电源时电源正→红、负→GND并共地)
//       第16~18脚是GPIO9/10/11(Flash)，不可接
//
// 使用：上电后 ESP32 开出 WiFi 热点 AeroShield-Servo（密码 aeroshield），
//       手机/电脑连上后浏览器打开 http://192.168.4.1
//       串口(115200)仍保留 0~180/s 命令
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char *AP_SSID = "AeroShield-Servo";
const char *AP_PASS = "aeroshield";  // 想改热点名/密码改这里（密码至少8位）

const int SERVO_PIN = 13;   // 信号脚；34~39 是只读脚不能用
const int PULSE_MIN = 500;  // 0° 对应脉宽 µs
const int PULSE_MAX = 2500; // 180° 对应脉宽 µs
// 若舵机转不到头或到头后发抖，收窄此范围，如 600~2400

Servo myServo;
WebServer server(80);

// 运动状态（统一由 loop 里 10ms 一拍的平滑器驱动）
int targetAngle = 90;   // 目标角度（网页/串口写入）
int currentAngle = 90;  // 当前实际角度
bool sweepMode = true;  // 自动扫摆
int sweepSpeed = 30;    // 扫摆速度 °/s
const int MOVE_SPEED = 300; // 手动目标追踪速度上限 °/s（平滑、护齿轮）
int sweepDir = 1;
unsigned long lastTick = 0;

// ---------------- 网页（存 Flash 里，不占内存） ----------------
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>AeroShield 舵机台</title>
<style>
:root{--bg:#0b1220;--card:#151e31;--ink:#e7edf7;--dim:#8fa0bd;--ac:#f5b942;--ac2:#53c8ff}
*{box-sizing:border-box;margin:0;-webkit-tap-highlight-color:transparent}
body{background:var(--bg);color:var(--ink);font-family:system-ui,-apple-system,"PingFang SC",sans-serif;
     display:flex;justify-content:center;min-height:100vh}
.wrap{width:100%;max-width:430px;padding:18px 16px 40px}
h1{font-size:17px;font-weight:600;letter-spacing:.5px;display:flex;align-items:center;gap:8px}
h1 .dot{width:9px;height:9px;border-radius:50%;background:#3ad07a;box-shadow:0 0 8px #3ad07a}
.card{background:var(--card);border:1px solid #26334d;border-radius:18px;padding:20px 18px;margin-top:14px}
.dial{position:relative;width:190px;height:190px;margin:6px auto 4px}
.dial .ring{position:absolute;inset:0;border-radius:50%;
  background:conic-gradient(from 270deg,var(--ac) 0 180deg,#223052 180deg 360deg);opacity:.28}
.dial .face{position:absolute;inset:14px;border-radius:50%;background:#101a2e;border:1px solid #2a3a5e;
  display:flex;align-items:center;justify-content:center;flex-direction:column}
.needle{position:absolute;left:50%;top:50%;width:4px;height:74px;background:linear-gradient(180deg,var(--ac),#fff3);
  border-radius:3px;transform-origin:50% 100%;transform:translate(-50%,-100%) rotate(0deg);
  transition:transform .12s linear}
.needle::after{content:"";position:absolute;left:50%;bottom:-6px;width:14px;height:14px;border-radius:50%;
  background:var(--ac);transform:translateX(-50%);box-shadow:0 0 12px var(--ac)}
.num{font-size:44px;font-weight:700;line-height:1}
.num small{font-size:16px;color:var(--dim);font-weight:500}
.deg{color:var(--dim);font-size:12px;margin-top:2px;letter-spacing:2px}
input[type=range]{width:100%;-webkit-appearance:none;height:30px;background:transparent;margin:2px 0 0}
input[type=range]::-webkit-slider-runnable-track{height:8px;border-radius:5px;
  background:linear-gradient(90deg,var(--ac) var(--fill,50%),#26334d var(--fill,50%))}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:26px;height:26px;margin-top:-9px;
  border-radius:50%;background:#fff;border:5px solid var(--ac);box-shadow:0 2px 8px #0008}
.rowlab{display:flex;justify-content:space-between;color:var(--dim);font-size:13px;margin:14px 2px 2px}
.chips{display:grid;grid-template-columns:repeat(5,1fr);gap:8px;margin-top:14px}
button{font:inherit;color:var(--ink);background:#1f2b45;border:1px solid #31436b;border-radius:12px;
  padding:9px 0;font-size:15px;cursor:pointer}
button:active{transform:scale(.96)}
.chips button[data-a]{font-variant-numeric:tabular-nums}
.chips button.act{border-color:var(--ac);color:var(--ac)}
.sweep{display:flex;align-items:center;gap:10px;margin-top:14px}
#sweepBtn{flex:0 0 96px;border-radius:14px;padding:10px 0;font-weight:600}
#sweepBtn.on{background:var(--ac);border-color:var(--ac);color:#241a05}
.foot{color:var(--dim);font-size:12px;text-align:center;margin-top:18px;line-height:1.7}
</style></head><body><div class="wrap">
<h1><span class="dot"></span>AeroShield 舵机控制台</h1>
<div class="card">
  <div class="dial">
    <div class="ring"></div>
    <div class="face">
      <div class="needle" id="needle"></div>
      <div class="num"><span id="angle">90</span><small>°</small></div>
      <div class="deg">PDI-1109MG</div>
    </div>
  </div>
  <input type="range" id="slider" min="0" max="180" value="90">
  <div class="chips">
    <button data-a="0">0°</button><button data-a="45">45°</button>
    <button data-a="90" class="act">90°</button><button data-a="135">135°</button>
    <button data-a="180">180°</button>
  </div>
  <div class="rowlab"><span>扫摆速度</span><span id="spdv">30°/s</span></div>
  <input type="range" id="spd" min="5" max="150" value="30">
  <div class="sweep"><button id="sweepBtn" class="on">扫摆 开</button></div>
</div>
<div class="foot">连接热点 AeroShield-Servo 后访问本页<br>发烫/异响请立刻断电 · 共地铁律</div>
<script>
const $=i=>document.getElementById(i);
const slider=$('slider'),spd=$('spd');let drag=false,tm=null,stm=null,sweep=true;
function paint(s){const p=(s/180*100).toFixed(1)+'%';slider.style.setProperty('--fill',p)}
function paintSpd(){const p=((spd.value-5)/145*100).toFixed(1)+'%';spd.style.setProperty('--fill',p)}
function ui(a){$('angle').textContent=Math.round(a);
  $('needle').style.transform='translate(-50%,-100%) rotate('+(a-90)+'deg)';
  if(!drag)slider.value=Math.round(a);paint(slider.value)}
async function api(p){try{const r=await fetch(p);return await r.json()}catch(e){return null}}
slider.addEventListener('input',e=>{drag=true;ui(+e.target.value);
  clearTimeout(tm);tm=setTimeout(()=>api('/set?angle='+e.target.value),60)});
slider.addEventListener('change',()=>{drag=false});
document.querySelectorAll('.chips button').forEach(b=>b.onclick=()=>{
  document.querySelector('.chips .act')?.classList.remove('act');b.classList.add('act');
  ui(+b.dataset.a);api('/set?angle='+b.dataset.a)});
$('sweepBtn').onclick=async()=>{const s=await api('/sweep?on='+(sweep?0:1));if(s)sweep=s.sweep};
spd.addEventListener('input',()=>{$('spdv').textContent=spd.value+'°/s';paintSpd();
  clearTimeout(stm);stm=setTimeout(()=>api('/speed?v='+spd.value),150)});
setInterval(async()=>{const s=await api('/status');if(!s)return;
  ui(s.angle);if(s.sweep!==sweep){sweep=s.sweep;
    $('sweepBtn').classList.toggle('on',sweep);$('sweepBtn').textContent=sweep?'扫摆 开':'扫摆 关'}
  if(document.activeElement!==spd){spd.value=s.speed;$('spdv').textContent=s.speed+'°/s';paintSpd()}},500);
paint(90);paintSpd();
</script></div></body></html>)rawliteral";

// ---------------- HTTP 接口 ----------------
void sendJson(const String &s) {
  server.send(200, "application/json", s);
}

void setupRoutes() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/set", HTTP_GET, []() {
    if (server.hasArg("angle")) {
      int a = server.arg("angle").toInt();
      if (a >= 0 && a <= 180) {
        targetAngle = a;
        sweepMode = false;
        Serial.printf("[web] 目标角度 %d°\n", a);
      }
    }
    sendJson("{\"angle\":" + String(currentAngle) + ",\"sweep\":false}");
  });
  server.on("/sweep", HTTP_GET, []() {
    sweepMode = server.arg("on") == "1";
    Serial.println(sweepMode ? "[web] 扫摆开" : "[web] 扫摆关");
    sendJson("{\"sweep\":" + String(sweepMode ? "true" : "false") + "}");
  });
  server.on("/speed", HTTP_GET, []() {
    int v = server.arg("v").toInt();
    if (v >= 5 && v <= 150) sweepSpeed = v;
    sendJson("{\"speed\":" + String(sweepSpeed) + "}");
  });
  server.on("/status", HTTP_GET, []() {
    sendJson("{\"angle\":" + String(currentAngle) +
             ",\"sweep\":" + String(sweepMode ? "true" : "false") +
             ",\"speed\":" + String(sweepSpeed) + "}");
  });
  server.onNotFound([]() { server.send(404, "text/plain", "404"); });
}

// ---------------- 串口命令（保留） ----------------
void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.equalsIgnoreCase("s")) {
    sweepMode = true;
    Serial.println("自动扫摆：开");
  } else {
    int angle = cmd.toInt();
    if (angle >= 0 && angle <= 180) {
      targetAngle = angle;
      sweepMode = false;
      Serial.printf("转到 %d°\n", angle);
    }
  }
}

void setup() {
  Serial.begin(115200);
  myServo.attach(SERVO_PIN, PULSE_MIN, PULSE_MAX);
  myServo.write(90);

  WiFi.softAP(AP_SSID, AP_PASS);
  setupRoutes();
  server.begin();
  Serial.printf("\nAP 已启动  SSID:%s  密码:%s\n", AP_SSID, AP_PASS);
  Serial.println("浏览器打开 http://192.168.4.1 控制舵机");
}

void loop() {
  server.handleClient();
  handleSerial();

  unsigned long now = millis();
  if (now - lastTick < 10) return;
  float dt = (now - lastTick) / 1000.0;
  lastTick = now;

  if (sweepMode) {
    targetAngle += sweepDir * sweepSpeed * dt;
    if (targetAngle >= 180) { targetAngle = 180; sweepDir = -1; }
    if (targetAngle <= 0)   { targetAngle = 0;   sweepDir = 1;  }
  }

  // 当前角向目标角平滑追踪（最多 MOVE_SPEED °/s），保护齿轮组
  float maxStep = MOVE_SPEED * dt;
  float diff = targetAngle - currentAngle;
  if (diff > maxStep)  diff = maxStep;
  if (diff < -maxStep) diff = -maxStep;
  currentAngle = (int)roundf(currentAngle + diff);
  myServo.write(currentAngle);
}
