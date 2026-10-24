// ============================================================
// PDI-1109MG × ESP32-WROOM 舵机测试
// 接线（38脚DevKit左下角）：棕→GND(第14脚)  橙(信号)→GPIO13(第15脚)
//       红→VIN 5V(第19脚，外接电源时电源正→红、负→GND并共地)
//       第16~18脚是GPIO9/10/11(Flash)，不可接
// 依赖：ESP32Servo 库（库管理器搜索 "ESP32Servo" 安装）
// 用法：上电自动扫摆；串口(115200)发 0~180 数字转到对应角度，
//       发 s 恢复自动扫摆
// ============================================================

#include <ESP32Servo.h>

Servo myServo;

const int SERVO_PIN = 13;   // 信号脚；34~39 是只读脚不能用
const int PULSE_MIN = 500;  // 0° 对应脉宽 µs
const int PULSE_MAX = 2500; // 180° 对应脉宽 µs
// 若舵机转不到头或到头后发抖，收窄此范围，如 600~2400

bool sweepMode = true;

void setup() {
  Serial.begin(115200);
  myServo.attach(SERVO_PIN, PULSE_MIN, PULSE_MAX);
  myServo.write(90);

  // 开机自检：0° → 180° → 回中
  delay(800);
  myServo.write(0);
  delay(1200);
  myServo.write(180);
  delay(1200);
  myServo.write(90);

  Serial.println("\n舵机就绪：发 0~180 的数字转到对应角度，发 s 恢复自动扫摆");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("s")) {
      sweepMode = true;
      Serial.println("自动扫摆：开");
    } else {
      int angle = cmd.toInt();
      if (angle >= 0 && angle <= 180) {
        sweepMode = false;
        myServo.write(angle);
        Serial.printf("转到 %d°\n", angle);
      }
    }
  }

  if (sweepMode) {
    static int angle = 90;
    static int dir = 1;
    myServo.write(angle);
    angle += 2 * dir;
    if (angle >= 180) { angle = 180; dir = -1; }
    if (angle <= 0)   { angle = 0;   dir = 1;  }
    delay(20);
  }
}
