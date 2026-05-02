#undef DEFAULT
#include <FluxGarage_RoboEyes.h>
#define DEFAULT 0

// ================= 抖音【极客狐尼克】=================//
#include <WiFiManager.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <DNSServer.h>
#define MODE_CLOCK 0                       // 时钟模式（开机默认）
#define MODE_CAR 1                         // 小车模式
volatile uint8_t deviceMode = MODE_CLOCK;  // 当前设备模式
bool roboEyesInited = false;               // RoboEyes初始化标志位
bool roboEyesEnabled = false;              // RoboEyes使能标志：时钟禁用/小车使能
bool motorPinLocked = true;                // 电机引脚锁死标志：时钟锁死/小车解锁

// ===== 天问Block语音控制 全局变量 =====
String asrRecvBuffer = "";     // ASR语音指令接收缓冲区
HardwareSerial ASR_Serial(1);  // UART1 (RX=20, TX=21)

// ===== 5个指令计数+隐藏信息全局变量 =====
uint8_t clockCount = 0, offCount = 0, nikeCount = 0, aimokCount = 0, fengmaoCount = 0;
bool showSecretInfo = false;
String secretShowText = "";
unsigned long lastCmdTime = 0;
const unsigned long CMD_TIMEOUT = 5000;

// ================= 硬件引脚定义 =================
#define LF 0        // 左轮正转
#define LB 1        // 左轮反转
#define RF 2        // 右轮正转
#define RB 3        // 右轮反转
#define I2C_SDA 9   // I2C SDA（OLED）
#define I2C_SCL 8   // I2C SCL（OLED）
#define LED_RED 5   // 红色LED（警灯/氛围灯）
#define LED_BLUE 7  // 蓝色LED

// OLED配置（128*64）
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET 0
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);

// ================= WebServer/DNSServer 全局定义 =================
WebServer server(80);
DNSServer dnsServer;
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);
const char* apName = "尼克狐尼克";

// ================= 小车枚举/全局变量/配置参数 =================
enum RoboEmotion : uint8_t {
  EMOTION_DEFAULT = 0,
  EMOTION_HAPPY = 1,
  EMOTION_ANGRY = 2,
  EMOTION_TIRED = 3,
  EMOTION_LAUGH = 4,
  EMOTION_CONFUSED = 5,
  EMOTION_SWEAT = 6,
  EMOTION_SURPRISED = 7,
  EMOTION_SLEEPY = 8,
  EMOTION_SPEECHLESS = 9
};
enum CustomAction : uint8_t {
  ACTION_NONE,
  ACTION_DANCE,
  ACTION_SPIN_CIRCLE,
  ACTION_RANDOM_SMALL,
  ACTION_SWING_ALWAYS,
  ACTION_SWING_DANCE,
  ACTION_POLICE_LIGHT,  // 警灯模式（新增）
  ACTION_AMBIENT_LIGHT  // 氛围灯（呼吸）（新增）
};
enum RandomMode : uint8_t {
  RANDOM_OFF,
  RANDOM_SOFT,
  RANDOM_NORMAL
};
volatile uint8_t lastMotorCmdBeforeTurn = 0;  // 保存转向前的电机指令
volatile RoboEmotion currentEmotion = EMOTION_DEFAULT;
volatile unsigned long lastEmotionChangeTime = 0;
volatile CustomAction currentAction = ACTION_NONE;
volatile uint8_t dancePhase = 0;
volatile unsigned long danceTimer = 0;
volatile uint8_t danceStepCount = 0;
uint8_t spinPhase = 0;
unsigned long spinTimer = 0;
volatile uint8_t randomSmallActionType = 0;
volatile unsigned long randomSmallActionTimer = 0;
volatile unsigned long lastRandomAction = 0;
volatile bool manualActive = false;
volatile unsigned long manualControlEndTime = 0;
volatile uint8_t currentMotorCmd = 0;
volatile unsigned long lastDanceSpinAction = 0;
volatile unsigned long lastRandomEmotionCheck = 0;
volatile RandomMode randomMode = RANDOM_OFF;
unsigned long motorTimer = 0;
uint8_t motorStep = 0;
bool motorOn = false;

volatile uint8_t swingAlwaysPhase = 0;
volatile unsigned long swingAlwaysTimer = 0;
volatile uint8_t swingDancePhase = 0;
volatile unsigned long swingDanceTimer = 0;
volatile unsigned long swingDanceStart = 0;
volatile uint8_t swingDanceGroupCount = 0;
volatile uint8_t swingDanceLeftRightCount = 0;

// ===== 警灯状态变量 =====
volatile uint8_t policeLightStep = 0;
volatile unsigned long policeLightTimer = 0;

// ===== 氛围灯(呼吸)状态变量 =====
volatile uint8_t ambientBrightness = 0;
volatile int8_t ambientDir = 1;
volatile unsigned long ambientTimer = 0;
const unsigned long AMBIENT_UPDATE_INTERVAL = 20;

// ================= 动作配置参数 =================
const unsigned long DANCE_SPIN_COOLDOWN = 150000;
const uint16_t DANCE_SHAKE_INTERVAL = 500;
const uint16_t SPIN_TOTAL_DURATION = 10000;
const uint8_t DANCE_PHASE2_SPIN_STEPS = 50;
const uint8_t DANCE_PHASE4_SPIN_STEPS = 50;
const uint16_t DANCE_SPIN_STEP_DELAY = 200;
const uint8_t DANCE_PHASE1_SHAKE_STEPS = 20;
const uint8_t DANCE_PHASE3_SHAKE_STEPS = 27;
const uint16_t SWING_ALWAYS_INTERVAL = 1000;
const uint16_t SWING_DANCE_SINGLE_DUR = 500;
const uint8_t SWING_DANCE_GROUP_TIMES = 2;
const unsigned long SWING_DANCE_TOTAL_DUR = 28000;

// ================= 表情配置参数 =================
const uint8_t WEIGHT_DEFAULT = 50;
const uint8_t WEIGHT_HAPPY = 5;
const uint8_t WEIGHT_ANGRY = 2;
const uint8_t WEIGHT_TIRED = 3;
const uint8_t WEIGHT_LAUGH = 2;
const uint8_t WEIGHT_CONFUSED = 2;
const uint8_t WEIGHT_SWEAT = 1;
const uint8_t WEIGHT_SURPRISED = 1;
const uint8_t WEIGHT_SLEEPY = 1;
const uint8_t WEIGHT_SPEECHLESS = 1;
const unsigned long DURATION_DEFAULT = 30000;
const unsigned long DURATION_HAPPY = 5000;
const unsigned long DURATION_ANGRY = 8000;
const unsigned long DURATION_TIRED = 10000;
const unsigned long DURATION_LAUGH = 6000;
const unsigned long DURATION_CONFUSED = 7000;
const unsigned long DURATION_SWEAT = 9000;
const unsigned long DURATION_SURPRISED = 4000;
const unsigned long DURATION_SLEEPY = 12000;
const unsigned long DURATION_SPEECHLESS = 15000;
const unsigned long EMOTION_SWITCH_INTERVAL_MIN = 10000;
const unsigned long EMOTION_SWITCH_INTERVAL_MAX = 20000;
const unsigned long RANDOM_SMALL_ACTION_INTERVAL = 20000;
const uint8_t RANDOM_SMALL_ACTION_PROB = 10;
const uint16_t TOTAL_WEIGHT = WEIGHT_DEFAULT + WEIGHT_HAPPY + WEIGHT_ANGRY + WEIGHT_TIRED + WEIGHT_LAUGH + WEIGHT_CONFUSED + WEIGHT_SWEAT + WEIGHT_SURPRISED + WEIGHT_SLEEPY + WEIGHT_SPEECHLESS;

// ================= 函数原型 =================
inline void setRoboEmotion(RoboEmotion emotion);
inline unsigned long getCurrentEmotionDuration();
void reconfigWiFi();
void parseASRCommand(String cmd);
void doSwingAlwaysAction();
void doSwingDanceAction();
void stopAllActions();
void motorDirectControl(byte c);
void showSecretOLEDInfo();
void resetAllCmdCount();
void doPoliceLightAction();
void doAmbientLightAction();

// ================= 隐藏信息OLED显示函数 =================
void showSecretOLEDInfo() {
  if (!showSecretInfo) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setRotation(0);
  display.setTextSize(1);
  display.setCursor(45, 8);
  display.print("Secret Info");
  display.drawLine(5, 20, 123, 20, SSD1306_WHITE);
  display.setTextSize(1);
  int x = (128 - secretShowText.length() * 12) / 2;
  display.setCursor(x + 8, 40);
  display.print(secretShowText);
  display.display();
}

// ================= 重置所有指令计数 =================
void resetAllCmdCount() {
  clockCount = 0;
  offCount = 0;
  nikeCount = 0;
  aimokCount = 0;
  fengmaoCount = 0;
  showSecretInfo = false;
  secretShowText = "";
  lastCmdTime = 0;
}

// ================= 时钟模块 =================
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.aliyun.com", 8 * 3600, 3600000);
unsigned long lastreconnect_Clock = 0;
unsigned long lastNTPUpdate_Clock = 0;

void updateDisplay_Clock() {
  timeClient.update();
  time_t epochTime = timeClient.getEpochTime();
  tm* localTime = localtime(&epochTime);
  int year = localTime->tm_year + 1900;
  int month = localTime->tm_mon + 1;
  int day = localTime->tm_mday;
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int seconds = timeClient.getSeconds();
  int wday = timeClient.getDay();
  const char* days[] = { "Sun.", "Mon.", "Tue.", "Wed.", "Thu.", "Fri.", "Sat." };

  char hStr[3], mStr[3], sStr[3];
  sprintf(hStr, "%02d", hours);
  sprintf(mStr, "%02d", minutes);
  sprintf(sStr, "%02d", seconds);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setRotation(0);

  display.setTextSize(1);
  display.setCursor(5, 2);
  display.print(days[wday]);
  display.print(" ");
  display.print(month);
  display.print("/");
  display.print(day);
  display.print("  ");
  display.print(WiFi.status() == WL_CONNECTED ? "W+" : "W-");
  display.print(" C");

  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setTextSize(4);
  display.setCursor(0, 20);
  display.print(hStr);
  display.setCursor(42, 20);
  display.print(": ");
  display.setCursor(59, 20);
  display.print(mStr);
  display.setTextSize(2);
  display.setCursor(104, 35);
  display.print(sStr);

  if (WiFi.status() != WL_CONNECTED && millis() - lastreconnect_Clock > 10000) {
    WiFi.reconnect();
    lastreconnect_Clock = millis();
  }

  display.display();
}

// ================= 智能小车核心函数 =================
void motorWifi(byte c) {
  if (deviceMode == MODE_CLOCK || motorPinLocked) return;
  if (currentMotorCmd == c) return;

  motorDirectControl(c);
  currentMotorCmd = c;

  if (c == 0) ASR_Serial.println("电机状态：停止");
  else if (c == 1) ASR_Serial.println("电机状态：前进（持续，可打断）");
  else if (c == 2) ASR_Serial.println("电机状态：后退（持续，可打断）");
  else if (c == 3) ASR_Serial.println("电机状态：左转（持续，可打断）");
  else if (c == 4) ASR_Serial.println("电机状态：右转（持续，可打断）");

  if (c != 0) {
    manualActive = true;
    manualControlEndTime = millis() + 10000;
  } else {
    manualActive = false;
    roboEyes.setHFlicker(OFF, 0);
    roboEyes.setVFlicker(OFF, 0);
    setRoboEmotion(EMOTION_DEFAULT);
    roboEyes.blink();
  }
}

void motorDirectControl(byte c) {
  switch (c) {
    case 0:
      digitalWrite(LF, LOW);
      digitalWrite(LB, LOW);
      digitalWrite(RF, LOW);
      digitalWrite(RB, LOW);
      break;
    case 1:
      digitalWrite(LF, HIGH);
      digitalWrite(LB, LOW);
      digitalWrite(RF, LOW);
      digitalWrite(RB, HIGH);
      setRoboEmotion(EMOTION_HAPPY);
      roboEyes.setPosition(DEFAULT);
      roboEyes.setIdleMode(OFF);
      break;
    case 2:
      digitalWrite(LF, LOW);
      digitalWrite(LB, HIGH);
      digitalWrite(RF, HIGH);
      digitalWrite(RB, LOW);
      roboEyes.anim_confused();
      roboEyes.setPosition(S);
      break;
    case 3:
      digitalWrite(LF, LOW);
      digitalWrite(LB, HIGH);
      digitalWrite(RF, LOW);
      digitalWrite(RB, HIGH);
      setRoboEmotion(EMOTION_DEFAULT);
      roboEyes.setPosition(E);
      roboEyes.setHFlicker(ON, 2);
      break;
    case 4:
      digitalWrite(LF, HIGH);
      digitalWrite(LB, LOW);
      digitalWrite(RF, HIGH);
      digitalWrite(RB, LOW);
      setRoboEmotion(EMOTION_DEFAULT);
      roboEyes.setPosition(W);
      roboEyes.setHFlicker(ON, 2);
      break;
  }
}

void stopAllActions() {
  if (deviceMode != MODE_CAR) return;
  motorDirectControl(0);
  currentMotorCmd = 0;
  currentAction = ACTION_NONE;
  swingAlwaysPhase = 0;
  swingDancePhase = 0;
  dancePhase = 0;
  spinPhase = 0;
  randomSmallActionType = 0;
  // 重置警灯/呼吸灯状态
  policeLightStep = 0;
  ambientBrightness = 0;
  ambientDir = 1;
  ledcWrite(0, 0);
  ledcWrite(1, 0);
  manualActive = false;
  manualControlEndTime = 0;
  setRoboEmotion(EMOTION_DEFAULT);
  roboEyes.setHFlicker(OFF, 0);
  roboEyes.setVFlicker(OFF, 0);
  roboEyes.blink();
  ASR_Serial.println("【全局停止】所有动作已终止");
}

inline void setRoboEmotion(RoboEmotion emotion) {
  if (!roboEyesEnabled) return;
  if (currentAction != ACTION_NONE && !(emotion == EMOTION_HAPPY || emotion == EMOTION_SURPRISED || emotion == EMOTION_CONFUSED)) {
    return;
  }

  currentEmotion = emotion;
  roboEyes.setSweat(OFF);
  roboEyes.setHFlicker(OFF, 0);
  roboEyes.setVFlicker(OFF, 0);

  if (emotion == EMOTION_DEFAULT || emotion == EMOTION_HAPPY || emotion == EMOTION_ANGRY) {
    roboEyes.setCuriosity(ON);
  } else {
    roboEyes.setCuriosity(OFF);
  }

  roboEyes.setWidth(35, 35);
  roboEyes.setHeight(30, 30);

  switch (emotion) {
    case EMOTION_DEFAULT:
      roboEyes.setMood(DEFAULT);
      roboEyes.setAutoblinker(ON, 3, 2);
      roboEyes.setIdleMode(ON, 5, 1);
      ASR_Serial.println("表情：默认（好奇观察）");
      break;
    case EMOTION_HAPPY:
      roboEyes.setMood(HAPPY);
      roboEyes.setAutoblinker(ON, 1, 1);
      roboEyes.setIdleMode(ON, 4, 1);
      ASR_Serial.println("表情：开心（高频眨眼）");
      break;
    case EMOTION_ANGRY:
      roboEyes.setMood(ANGRY);
      roboEyes.setAutoblinker(OFF);
      roboEyes.setIdleMode(OFF);
      roboEyes.setHFlicker(ON, 3);
      ASR_Serial.println("表情：生气（眼球抖动）");
      break;
    case EMOTION_TIRED:
      roboEyes.setMood(TIRED);
      roboEyes.setAutoblinker(ON, 5, 2);
      roboEyes.setIdleMode(ON, 15, 2);
      ASR_Serial.println("表情：困倦（慢眨眼+打盹）");
      break;
    case EMOTION_LAUGH:
      roboEyes.setMood(HAPPY);
      roboEyes.setAutoblinker(ON, 1, 1);
      roboEyes.setIdleMode(ON, 1, 1);
      roboEyes.anim_laugh();
      ASR_Serial.println("表情：大笑（上下抖眼）");
      break;
    case EMOTION_CONFUSED:
      roboEyes.setMood(DEFAULT);
      roboEyes.setPosition(DEFAULT);
      roboEyes.setAutoblinker(ON, 10, 2);
      roboEyes.setIdleMode(OFF);
      roboEyes.anim_confused();
      ASR_Serial.println("表情：困惑（左右抖眼）");
      break;
    case EMOTION_SWEAT:
      roboEyes.setMood(TIRED);
      roboEyes.setAutoblinker(OFF);
      roboEyes.setIdleMode(OFF);
      roboEyes.setSweat(ON);
      ASR_Serial.println("表情：流汗（困倦+流汗）");
      break;
    case EMOTION_SURPRISED:
      roboEyes.setMood(DEFAULT);
      roboEyes.setAutoblinker(ON, 8, 0);
      roboEyes.setIdleMode(OFF);
      roboEyes.setPosition(N);
      roboEyes.setWidth(25, 25);
      roboEyes.setHeight(52, 52);
      roboEyes.blink();
      roboEyes.blink();
      ASR_Serial.println("表情：惊讶（眼球变大+快速眨眼）");
      break;
    case EMOTION_SLEEPY:
      roboEyes.setMood(TIRED);
      roboEyes.setAutoblinker(ON, 8, 2);
      roboEyes.setIdleMode(ON, 6, 3);
      roboEyes.setPosition(DEFAULT);
      roboEyes.setWidth(35, 35);
      roboEyes.setHeight(15, 15);
      roboEyes.blink();
      ASR_Serial.println("表情：困倦（眼球缩小）");
      break;
    case EMOTION_SPEECHLESS:
      roboEyes.setMood(DEFAULT);
      roboEyes.setAutoblinker(ON, 4, 2);
      roboEyes.setIdleMode(ON, 6, 3);
      roboEyes.setPosition(DEFAULT);
      roboEyes.setWidth(35, 35);
      roboEyes.setHeight(10, 10);
      roboEyes.blink();
      ASR_Serial.println("表情：无语（眼球极小）");
      break;
  }
}

inline unsigned long getCurrentEmotionDuration() {
  switch (currentEmotion) {
    case EMOTION_DEFAULT: return DURATION_DEFAULT;
    case EMOTION_HAPPY: return DURATION_HAPPY;
    case EMOTION_ANGRY: return DURATION_ANGRY;
    case EMOTION_TIRED: return DURATION_TIRED;
    case EMOTION_LAUGH: return DURATION_LAUGH;
    case EMOTION_CONFUSED: return DURATION_CONFUSED;
    case EMOTION_SWEAT: return DURATION_SWEAT;
    case EMOTION_SURPRISED: return DURATION_SURPRISED;
    case EMOTION_SLEEPY: return DURATION_SLEEPY;
    case EMOTION_SPEECHLESS: return DURATION_SPEECHLESS;
    default: return DURATION_DEFAULT;
  }
}

void randomSwitchEmotion() {
  if (!roboEyesEnabled) return;
  if (manualActive || currentAction != ACTION_NONE) return;

  int randWeight = random(1, TOTAL_WEIGHT + 1);
  RoboEmotion randomEmo = EMOTION_DEFAULT;
  int weightSum = 0;

  weightSum += WEIGHT_DEFAULT;
  if (randWeight <= weightSum) randomEmo = EMOTION_DEFAULT;
  else if (randWeight <= (weightSum += WEIGHT_HAPPY)) randomEmo = EMOTION_HAPPY;
  else if (randWeight <= (weightSum += WEIGHT_ANGRY)) randomEmo = EMOTION_ANGRY;
  else if (randWeight <= (weightSum += WEIGHT_TIRED)) randomEmo = EMOTION_TIRED;
  else if (randWeight <= (weightSum += WEIGHT_LAUGH)) randomEmo = EMOTION_LAUGH;
  else if (randWeight <= (weightSum += WEIGHT_CONFUSED)) randomEmo = EMOTION_CONFUSED;
  else if (randWeight <= (weightSum += WEIGHT_SWEAT)) randomEmo = EMOTION_SWEAT;
  else if (randWeight <= (weightSum += WEIGHT_SURPRISED)) randomEmo = EMOTION_SURPRISED;
  else if (randWeight <= (weightSum += WEIGHT_SLEEPY)) randomEmo = EMOTION_SLEEPY;
  else if (randWeight <= (weightSum += WEIGHT_SPEECHLESS)) randomEmo = EMOTION_SPEECHLESS;

  setRoboEmotion(randomEmo);
  lastEmotionChangeTime = millis();
}

void doDanceAction() {
  if (!roboEyesEnabled || deviceMode != MODE_CAR) return;
  uint8_t phase1_step_total = DANCE_PHASE1_SHAKE_STEPS;
  uint8_t phase3_step_total = DANCE_PHASE3_SHAKE_STEPS;

  if (dancePhase == 0) {
    setRoboEmotion(EMOTION_HAPPY);
    manualActive = true;
    currentEmotion = EMOTION_HAPPY;
    danceTimer = millis();
    danceStepCount = 0;
    dancePhase = 1;
    ASR_Serial.println("开始跳舞：阶段1-前后摇晃");
    lastDanceSpinAction = millis();
  }

  unsigned long now = millis();
  switch (dancePhase) {
    case 1:
      if (now - danceTimer >= DANCE_SHAKE_INTERVAL) {
        danceTimer = now;
        if (danceStepCount % 2 == 0) motorDirectControl(1);
        else motorDirectControl(2);
        danceStepCount++;
        if (danceStepCount >= phase1_step_total) {
          motorDirectControl(0);
          dancePhase = 2;
          danceTimer = now;
          danceStepCount = 0;
          ASR_Serial.println("跳舞：阶段2-转圈");
        }
      }
      break;
    case 2:
      if (now - danceTimer >= DANCE_SPIN_STEP_DELAY) {
        danceTimer = now;
        motorDirectControl(3);
        danceStepCount++;
        if (danceStepCount >= DANCE_PHASE2_SPIN_STEPS) {
          motorDirectControl(0);
          dancePhase = 3;
          danceTimer = now;
          danceStepCount = 0;
          ASR_Serial.println("跳舞：阶段3-再次摇晃");
        }
      }
      break;
    case 3:
      if (now - danceTimer >= DANCE_SHAKE_INTERVAL) {
        danceTimer = now;
        if (danceStepCount % 2 == 0) motorDirectControl(1);
        else motorDirectControl(2);
        danceStepCount++;
        if (danceStepCount >= phase3_step_total) {
          motorDirectControl(0);
          dancePhase = 4;
          danceTimer = now;
          danceStepCount = 0;
          ASR_Serial.println("跳舞：阶段4-快速转圈");
        }
      }
      break;
    case 4:
      if (now - danceTimer >= DANCE_SPIN_STEP_DELAY) {
        danceTimer = now;
        motorDirectControl(3);
        danceStepCount++;
        if (danceStepCount >= DANCE_PHASE4_SPIN_STEPS) {
          motorDirectControl(0);
          dancePhase = 5;
          ASR_Serial.println("跳舞：结束");
        }
      }
      break;
    case 5:
      manualActive = false;
      currentAction = ACTION_NONE;
      dancePhase = 0;
      setRoboEmotion(EMOTION_DEFAULT);
      break;
  }
  roboEyes.update();
}

void doSpinCircleAction() {
  if (!roboEyesEnabled || deviceMode != MODE_CAR) return;
  if (spinPhase == 0) {
    setRoboEmotion(EMOTION_HAPPY);
    currentEmotion = EMOTION_HAPPY;
    spinTimer = millis();
    manualActive = true;
    spinPhase = 1;
    ASR_Serial.println("开始转圈：开心表情");
    lastDanceSpinAction = millis();
    return;
  }

  unsigned long elapsedTime = millis() - spinTimer;

  switch (spinPhase) {
    case 1:
      motorDirectControl(3);
      if (elapsedTime >= SPIN_TOTAL_DURATION / 2) {
        setRoboEmotion(EMOTION_SURPRISED);
        currentEmotion = EMOTION_SURPRISED;
        spinPhase = 2;
        ASR_Serial.println("转圈：切换惊讶表情");
      }
      break;
    case 2:
      motorDirectControl(3);
      if (elapsedTime >= SPIN_TOTAL_DURATION) {
        motorDirectControl(0);
        setRoboEmotion(EMOTION_CONFUSED);
        currentEmotion = EMOTION_CONFUSED;
        manualActive = false;
        currentAction = ACTION_NONE;
        spinPhase = 3;
        ASR_Serial.println("转圈：结束，困惑表情");
      }
      break;
    case 3:
      spinPhase = 0;
      setRoboEmotion(EMOTION_DEFAULT);
      break;
  }
  roboEyes.update();
}

void doRandomSmallAction() {
  if (!roboEyesEnabled || deviceMode != MODE_CAR) return;
  if (randomSmallActionType == 0) {
    randomSmallActionType = random(1, 9);
    randomSmallActionTimer = millis();
    manualActive = true;
    ASR_Serial.print("随机小动作：类型");
    ASR_Serial.println(randomSmallActionType);
  }

  unsigned long elapsedTime = millis() - randomSmallActionTimer;
  static bool turnLeft = true;
  if (randomSmallActionType == 7 && randomSmallActionTimer == millis()) {
    turnLeft = random(0, 2);
  }

  switch (randomSmallActionType) {
    case 1:
      if (elapsedTime < 5000) {
        motorDirectControl(1);
        setRoboEmotion(EMOTION_HAPPY);
      } else {
        motorDirectControl(0);
        randomSmallActionType = 0;
        currentAction = ACTION_NONE;
        manualActive = false;
        ASR_Serial.println("小动作：前进结束");
      }
      break;
    case 2:
      if (elapsedTime < 5000) {
        motorDirectControl(2);
        setRoboEmotion(EMOTION_CONFUSED);
      } else {
        motorDirectControl(0);
        randomSmallActionType = 0;
        currentAction = ACTION_NONE;
        manualActive = false;
        ASR_Serial.println("小动作：后退结束");
      }
      break;
    case 3:
      if (elapsedTime < 1000) motorDirectControl(3);
      else if (elapsedTime < 2000) motorDirectControl(4);
      else {
        motorDirectControl(0);
        randomSmallActionType = 0;
        currentAction = ACTION_NONE;
        manualActive = false;
        ASR_Serial.println("小动作：转头结束");
      }
      break;
    case 4:
      if (elapsedTime < 800 || (elapsedTime >= 1600 && elapsedTime < 2400)) motorDirectControl(1);
      else if (elapsedTime < 1600 || (elapsedTime >= 2400 && elapsedTime < 3200)) motorDirectControl(2);
      else {
        motorDirectControl(0);
        randomSmallActionType = 0;
        currentAction = ACTION_NONE;
        manualActive = false;
        ASR_Serial.println("小动作：摇晃结束");
      }
      break;
    case 5:
      if (elapsedTime < 10000) {
        motorDirectControl(0);
        setRoboEmotion(EMOTION_ANGRY);
      } else {
        randomSmallActionType = 0;
        currentAction = ACTION_NONE;
        manualActive = false;
        ASR_Serial.println("小动作：生气结束");
      }
      break;
    case 6:
      if ((elapsedTime < 200) || (elapsedTime >= 300 && elapsedTime < 500) || (elapsedTime >= 600 && elapsedTime < 800)) motorDirectControl(1);
      else motorDirectControl(0);
      if (elapsedTime >= 900) {
        motorDirectControl(0);
        randomSmallActionType = 0;
        currentAction = ACTION_NONE;
        manualActive = false;
        ASR_Serial.println("小动作：点头结束");
      }
      break;
    case 7:
      if (elapsedTime < 1500) motorDirectControl(turnLeft ? 3 : 4);
      else {
        motorDirectControl(0);
        randomSmallActionType = 0;
        currentAction = ACTION_NONE;
        manualActive = false;
        ASR_Serial.println("小动作：单侧转头结束");
      }
      break;
    case 8:
      if (elapsedTime < 2000) {
        motorDirectControl(0);
        setRoboEmotion(EMOTION_SWEAT);
      } else {
        randomSmallActionType = 0;
        currentAction = ACTION_NONE;
        manualActive = false;
        ASR_Serial.println("小动作：流汗结束");
      }
      break;
  }
  roboEyes.update();
}

void doSwingAlwaysAction() {
  if (!roboEyesEnabled || deviceMode != MODE_CAR) return;
  if (swingAlwaysPhase == 0) {
    setRoboEmotion(EMOTION_HAPPY);
    manualActive = true;
    swingAlwaysTimer = millis();
    swingAlwaysPhase = 1;
    ASR_Serial.println("开始持续摇摆：2s来回1次，直到被打断");
    return;
  }

  unsigned long now = millis();
  switch (swingAlwaysPhase) {
    case 1:
      motorDirectControl(1);
      if (now - swingAlwaysTimer >= SWING_ALWAYS_INTERVAL) {
        swingAlwaysTimer = now;
        swingAlwaysPhase = 2;
      }
      break;
    case 2:
      motorDirectControl(2);
      if (now - swingAlwaysTimer >= SWING_ALWAYS_INTERVAL) {
        swingAlwaysTimer = now;
        swingAlwaysPhase = 1;
      }
      break;
  }
  roboEyes.update();
}

void doSwingDanceAction() {
  if (!roboEyesEnabled || deviceMode != MODE_CAR) return;
  if (swingDancePhase == 0) {
    setRoboEmotion(EMOTION_HAPPY);
    manualActive = true;
    swingDanceTimer = millis();
    swingDanceStart = millis();
    swingDanceGroupCount = 0;
    swingDanceLeftRightCount = 0;
    swingDancePhase = 1;
    ASR_Serial.println("开始摇摆舞：前后两次+左右两次为一组，循环28秒");
    return;
  }

  if (millis() - swingDanceStart >= SWING_DANCE_TOTAL_DUR) {
    motorDirectControl(0);
    manualActive = false;
    currentAction = ACTION_NONE;
    swingDancePhase = 0;
    ASR_Serial.println("摇摆舞：结束，恢复默认表情");
    setRoboEmotion(EMOTION_DEFAULT);
    return;
  }

  unsigned long now = millis();
  switch (swingDancePhase) {
    case 1:
      motorDirectControl(1);
      if (now - swingDanceTimer >= SWING_DANCE_SINGLE_DUR) {
        swingDanceTimer = now;
        swingDancePhase = 2;
      }
      break;
    case 2:
      motorDirectControl(2);
      if (now - swingDanceTimer >= SWING_DANCE_SINGLE_DUR) {
        swingDanceTimer = now;
        swingDanceGroupCount++;
        if (swingDanceGroupCount >= SWING_DANCE_GROUP_TIMES) {
          swingDanceGroupCount = 0;
          swingDancePhase = 3;
        } else {
          swingDancePhase = 1;
        }
      }
      break;
    case 3:
      motorDirectControl(3);
      if (now - swingDanceTimer >= SWING_DANCE_SINGLE_DUR) {
        swingDanceTimer = now;
        swingDancePhase = 4;
      }
      break;
    case 4:
      motorDirectControl(4);
      if (now - swingDanceTimer >= SWING_DANCE_SINGLE_DUR) {
        swingDanceTimer = now;
        swingDanceLeftRightCount++;
        if (swingDanceLeftRightCount >= SWING_DANCE_GROUP_TIMES) {
          swingDanceLeftRightCount = 0;
          swingDancePhase = 1;
        } else {
          swingDancePhase = 3;
        }
      }
      break;
  }
  roboEyes.update();
}

// ===== 警灯模式 =====
void doPoliceLightAction() {
  if (!roboEyesEnabled || deviceMode != MODE_CAR) return;
  if (policeLightStep == 0) {
    policeLightTimer = millis();
    policeLightStep = 1;
    ASR_Serial.println("开始警灯模式：红蓝交替闪烁");
    return;
  }

  unsigned long now = millis();
  const uint16_t STEP_DUR = 100;

  if (now - policeLightTimer >= STEP_DUR) {
    policeLightTimer = now;
    policeLightStep++;
    if (policeLightStep > 16) policeLightStep = 1;

    switch (policeLightStep) {
      case 1:
        ledcWrite(0, 255);
        ledcWrite(1, 0);
        break;
      case 2:
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        break;
      case 3:
        ledcWrite(0, 255);
        ledcWrite(1, 0);
        break;
      case 4:
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        break;
      case 5:
        ledcWrite(0, 0);
        ledcWrite(1, 255);
        break;
      case 6:
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        break;
      case 7:
        ledcWrite(0, 0);
        ledcWrite(1, 255);
        break;
      case 8:
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        break;
      case 9:
        ledcWrite(0, 255);
        ledcWrite(1, 0);
        break;
      case 10:
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        break;
      case 11:
        ledcWrite(0, 0);
        ledcWrite(1, 255);
        break;
      case 12:
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        break;
      case 13:
        ledcWrite(0, 255);
        ledcWrite(1, 0);
        break;
      case 14:
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        break;
      case 15:
        ledcWrite(0, 0);
        ledcWrite(1, 255);
        break;
      case 16:
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        break;
    }
  }
}

// ===== 氛围灯（呼吸） =====
void doAmbientLightAction() {
  if (!roboEyesEnabled || deviceMode != MODE_CAR) return;
  if (ambientBrightness == 0 && ambientDir == 1) {
    ambientTimer = millis();
    ambientBrightness = 0;
    ambientDir = 1;
    ASR_Serial.println("开始氛围灯：红蓝同步呼吸");
    return;
  }

  unsigned long now = millis();
  if (now - ambientTimer >= AMBIENT_UPDATE_INTERVAL) {
    ambientTimer = now;
    ambientBrightness += ambientDir;
    if (ambientBrightness >= 255) {
      ambientBrightness = 255;
      ambientDir = -1;
    } else if (ambientBrightness <= 0) {
      ambientBrightness = 0;
      ambientDir = 1;
    }
    ledcWrite(0, ambientBrightness);
    ledcWrite(1, ambientBrightness);
  }
}

// ================= 主动重新配网函数 =================
void reconfigWiFi() {
  motorDirectControl(0);
  deviceMode = MODE_CLOCK;
  roboEyesEnabled = false;
  motorPinLocked = true;
  digitalWrite(LF, LOW);
  digitalWrite(LB, LOW);
  digitalWrite(RF, LOW);
  digitalWrite(RB, LOW);
  ASR_Serial.println("\n===== 收到重新配网指令，进入配网流程 =====");

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 20);
  display.setTextSize(1);
  display.println("Config WiFi...");
  display.display();

  // 停止现有的server和dnsServer，避免端口冲突
  server.stop();
  dnsServer.stop();

  WiFiManager wm;
  wm.setDebugOutput(true);
  wm.setConfigPortalTimeout(300);

  if (!wm.startConfigPortal("Nick_net")) {
    ASR_Serial.println("❌ 重新配网超时/失败，即将重启");
    display.clearDisplay();
    display.setCursor(20, 20);
    display.println("Config Fail!");
    display.println("  Restart 3s...");
    display.display();
    delay(3000);
    ESP.restart();
  }

  ASR_Serial.println("✅ 重新配网成功！");
  ASR_Serial.print("📶 新WiFi IP：");
  ASR_Serial.println(WiFi.localIP());
  display.clearDisplay();
  display.setCursor(20, 20);
  display.println("Config OK!");
  display.print("  IP:");
  display.println(WiFi.localIP());
  display.display();
  delay(2000);

  // 重新开启AP模式（以便手机直连控制），同时保持STA连接
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(apName);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);

  // 重启WebServer
  server.begin();
  ASR_Serial.println("✅ Web服务器和AP已重新启动");

  timeClient.update();
}

// ================= 网页界面（保留原有完整页面，但不添加警灯/氛围灯按钮）=================
void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>尼克狐尼克控制</title>
<style>
/* ==================== 全局样式 ==================== */
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
  user-select: none;                      /* 禁止选中文字，提升触摸体验 */
  -webkit-tap-highlight-color: transparent; /* 移除移动端点击灰色背景 */
}

/* 页面主体 - 卡其色渐变背景 */
body {
  min-height: 100vh;
  background: linear-gradient(135deg, #f5e6d3 0%, #e8d4b8 100%);
  padding: 12px;
  font-family: 'Segoe UI', 'PingFang SC', Roboto, 'Helvetica Neue', sans-serif;
}

/* 主容器 - 全屏高度布局 */
.container {
  max-width: 500px;
  margin: 0 auto;
  height: calc(100vh - 24px);
  display: flex;
  flex-direction: column;
  gap: 12px;
}

/* ==================== 头部区域 ==================== */
.header {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  padding: 0 5px;
}
h1 {
  font-size: 20px;
  color: #8b5a2b;
  font-weight: 600;
  letter-spacing: 1px;
}
/* 重新配网按钮 */
.reconfig-btn {
  background: #d4b68a;
  color: #fff;
  text-decoration: none;
  padding: 5px 12px;
  border-radius: 20px;
  font-size: 11px;
  font-weight: 500;
  transition: all 0.2s;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}
.reconfig-btn:active {
  transform: scale(0.95);
  background: #b88d5e;
}

/* ==================== 模式切换区域 ==================== */
.mode-switch {
  display: flex;
  gap: 12px;
  background: #faf0e1;
  padding: 6px;
  border-radius: 40px;
  box-shadow: inset 0 1px 3px rgba(0,0,0,0.1), 0 2px 4px rgba(0,0,0,0.05);
}
.mode-btn {
  flex: 1;
  padding: 10px;
  border: none;
  border-radius: 30px;
  font-size: 15px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s;
  background: transparent;
  color: #9b7a4a;
}
/* 激活状态 */
.mode-btn.active {
  background: #c4a27a;
  color: white;
  box-shadow: 0 2px 6px rgba(0,0,0,0.15);
}
.mode-btn:active {
  transform: scale(0.96);
}

/* ==================== 通用卡片样式 ==================== */
.card {
  background: #fef7e8;
  border-radius: 20px;
  padding: 12px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.08);
}
.card-title {
  font-size: 12px;
  color: #b88d5e;
  font-weight: 600;
  margin-bottom: 8px;
  letter-spacing: 0.5px;
  display: flex;
  align-items: center;
  gap: 6px;
}
.card-title span {
  font-size: 16px;
}

/* ==================== 方向控制区 ==================== */
.direction-panel {
  background: #e8d9c4;
  border-radius: 28px;
  padding: 16px;
}
.direction-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 12px;
  max-width: 260px;
  margin: 0 auto;
}
/* 方向按钮 - 圆形大按钮 */
.dir-btn {
  aspect-ratio: 1;
  border: none;
  border-radius: 20px;
  font-size: 24px;
  font-weight: bold;
  cursor: pointer;
  transition: all 0.05s linear;
  background: #f5e9d8;
  color: #8b5a2b;
  box-shadow: 0 4px 8px rgba(0,0,0,0.1);
  display: flex;
  align-items: center;
  justify-content: center;
}
.dir-btn:active {
  transform: scale(0.92);
  background: #d4b68a;
  color: white;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}
/* 停止按钮 - 全局停止专用样式（红色调） */
.stop-big {
  background: #e0b8a0;
  color: #c94f2c;
  font-size: 20px;
  font-weight: bold;
}
.stop-big:active {
  background: #c94f2c;
  color: white;
}

/* ==================== 按钮网格布局 ==================== */
.btn-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(80px, 1fr));
  gap: 8px;
}
/* 通用小按钮样式 */
.btn-sm {
  padding: 8px 4px;
  border: none;
  border-radius: 12px;
  font-size: 12px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.1s;
  background: #f3e9db;
  color: #7c5a3a;
  box-shadow: 0 2px 4px rgba(0,0,0,0.05);
  white-space: nowrap;
}
.btn-sm:active {
  transform: scale(0.96);
  background: #d4b68a;
  color: white;
}
/* 激活状态样式 */
.btn-active {
  background: #c4a27a;
  color: white;
  box-shadow: 0 2px 6px rgba(0,0,0,0.1);
}
/* 警告按钮 */
.btn-warning {
  background: #e8cfb0;
  color: #b45a2e;
}

/* ==================== 表情控制区 - 网格布局 ==================== */
.emotion-grid {
  display: grid;
  grid-template-columns: repeat(5, 1fr);  /* 5列布局，一行显示5个 */
  gap: 6px;
}
.emotion-btn {
  padding: 8px 4px;
  border: none;
  border-radius: 25px;
  font-size: 11px;
  font-weight: 500;
  background: #f3e9db;
  color: #7c5a3a;
  cursor: pointer;
  transition: all 0.1s;
  text-align: center;
  white-space: nowrap;
}
.emotion-btn:active {
  transform: scale(0.94);
  background: #d4b68a;
  color: white;
}

/* ==================== 禁用状态 ==================== */
.disabled {
  opacity: 0.5;
  pointer-events: none;  /* 禁止点击 */
}

/* ==================== 滚动区域 ==================== */
.scroll-area {
  flex: 1;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
  gap: 12px;
}
/* 自定义滚动条样式 */
.scroll-area::-webkit-scrollbar {
  width: 3px;
}
.scroll-area::-webkit-scrollbar-track {
  background: #f0e5d6;
  border-radius: 3px;
}
.scroll-area::-webkit-scrollbar-thumb {
  background: #c4a27a;
  border-radius: 3px;
}
</style>
</head>
<body>
<div class="container">
  <!-- 头部区域：标题 + 重新配网按钮 -->
  <div class="header">
    <h1>🦊 尼克狐尼克</h1>
    <a href="/reconfig" class="reconfig-btn" onclick="return confirm('确定重新配网吗？')">⚙️ 配网</a>
  </div>

  <!-- 模式切换：时钟模式 / 小车模式 -->
  <div class="mode-switch">
    <button class="mode-btn clock active" id="btnClock">⏰ 时钟模式</button>
    <button class="mode-btn car" id="btnCar">🚗 小车模式</button>
  </div>

  <!-- 小车控制区域（时钟模式下禁用） -->
  <div class="scroll-area" id="carArea">
    <!-- 1. 方向控制区 -->
    <div class="card" id="controlCard">
      <div class="card-title"><span>🎮</span> 方向控制</div>
      <div class="direction-panel">
        <div class="direction-grid">
          <div></div>                           <!-- 左上角占位 -->
          <button class="dir-btn" id="btnUp">▲</button>    <!-- 前进 -->
          <div></div>                           <!-- 右上角占位 -->
          <button class="dir-btn" id="btnLeft">◀</button>  <!-- 左转 -->
          <button class="dir-btn stop-big" id="btnGlobalStop">⏹️</button> <!-- 全局停止按钮 -->
          <button class="dir-btn" id="btnRight">▶</button> <!-- 右转 -->
          <div></div>                           <!-- 左下角占位 -->
          <button class="dir-btn" id="btnDown">▼</button>  <!-- 后退 -->
          <div></div>                           <!-- 右下角占位 -->
        </div>
      </div>
    </div>

    <!-- 2. 随机模式区：睡眠/摆动/好奇 -->
    <div class="card">
      <div class="card-title"><span>🎲</span> 随机模式</div>
      <div class="btn-grid">
        <button class="btn-sm mode-random active" id="modeOff">💤 睡眠</button>
        <button class="btn-sm mode-random" id="modeSoft">🔄 摆动</button>
        <button class="btn-sm mode-random" id="modeNormal">👀 好奇</button>
      </div>
    </div>

    <!-- 3. 动作指令区（已删除全局停止按钮） -->
    <div class="card">
      <div class="card-title"><span>🎬</span> 动作指令</div>
      <div class="btn-grid">
        <button class="btn-sm" id="btnDance">💃 跳舞</button>
        <button class="btn-sm" id="btnSpin">🔄 转圈</button>
        <button class="btn-sm" id="btnSwingAlways">🎵 持续摇摆</button>
        <button class="btn-sm" id="btnSwingDance">💫 摇摆舞</button>
      </div>
    </div>

    <!-- 4. 表情控制区 - 网格布局，直接铺开（5x2网格） -->
    <div class="card">
      <div class="card-title"><span>😊</span> 表情控制</div>
      <div class="emotion-grid">
        <button class="emotion-btn" id="emoDefault">😐 默认</button>
        <button class="emotion-btn" id="emoHappy">😊 开心</button>
        <button class="emotion-btn" id="emoAngry">😠 生气</button>
        <button class="emotion-btn" id="emoTired">😴 疲惫</button>
        <button class="emotion-btn" id="emoLaugh">😆 大笑</button>
        <button class="emotion-btn" id="emoConfused">😕 困惑</button>
        <button class="emotion-btn" id="emoSweat">😅 流汗</button>
        <button class="emotion-btn" id="emoSurprised">😲 惊讶</button>
        <button class="emotion-btn" id="emoSleepy">🥱 困倦</button>
        <button class="emotion-btn" id="emoSpeechless">😶 无语</button>
      </div>
    </div>
  </div>
</div>

<script>
// ==================== DOM 元素获取 ====================
const btnClock = document.getElementById('btnClock');              // 时钟模式按钮
const btnCar = document.getElementById('btnCar');                  // 小车模式按钮
const carArea = document.getElementById('carArea');                // 小车控制区域容器
const btnUp = document.getElementById('btnUp');                    // 前进按钮
const btnDown = document.getElementById('btnDown');                // 后退按钮
const btnLeft = document.getElementById('btnLeft');                // 左转按钮
const btnRight = document.getElementById('btnRight');              // 右转按钮
const btnGlobalStop = document.getElementById('btnGlobalStop');    // 全局停止按钮

// ==================== 网络请求函数 ====================
function sendCmd(url) {
  fetch(url, {method: 'GET', cache: 'no-cache'}).catch(() => {});
}

// ==================== 方向控制逻辑 ====================
let moveInterval = null;  // 定时器句柄

/**
 * 开始移动 - 按下按钮时调用
 * @param {number} cmd 指令码: 1=前进, 2=后退, 3=左转, 4=右转
 */
function startMove(cmd) {
  if (moveInterval) clearInterval(moveInterval);  // 清除之前的定时器
  sendCmd('/joystick?cmd=' + cmd);                 // 立即发送一次指令
  // 每隔150ms持续发送，实现按住连续移动
  moveInterval = setInterval(() => sendCmd('/joystick?cmd=' + cmd), 150);
}

/**
 * 停止移动 - 松开按钮时调用
 * 注意：这里只停止方向移动，不调用全局停止
 */
function stopMove() {
  if (moveInterval) {
    clearInterval(moveInterval);
    moveInterval = null;
  }
  sendCmd('/joystick?cmd=0');  // 发送停止指令
}

/**
 * 全局停止 - 停止所有动作和移动
 */
function globalStop() {
  // 先停止方向移动的定时器
  if (moveInterval) {
    clearInterval(moveInterval);
    moveInterval = null;
  }
  // 发送全局停止指令
  sendCmd('/action?cmd=stop_all');
}

// 为四个方向按钮绑定鼠标/触摸事件
[btnUp, btnDown, btnLeft, btnRight].forEach(btn => {
  // 鼠标事件
  btn.addEventListener('mousedown', () => startMove(parseInt(btn.dataset.cmd)));
  btn.addEventListener('mouseup', stopMove);
  btn.addEventListener('mouseleave', stopMove);
  // 触摸事件（移动端）
  btn.addEventListener('touchstart', (e) => { 
    e.preventDefault(); 
    startMove(parseInt(btn.dataset.cmd)); 
  });
  btn.addEventListener('touchend', stopMove);
});

// 设置方向按钮对应的指令码
btnUp.dataset.cmd = 1;      // 前进
btnDown.dataset.cmd = 2;    // 后退
btnLeft.dataset.cmd = 3;    // 左转
btnRight.dataset.cmd = 4;   // 右转

// 全局停止按钮点击事件 - 执行全局停止
btnGlobalStop.addEventListener('click', globalStop);

// ==================== 模式切换函数 ====================
/**
 * 设置小车模式是否启用
 * @param {boolean} enable true=启用小车模式, false=启用时钟模式
 */
function setCarMode(enable) {
  if (enable) {
    carArea.classList.remove('disabled');  // 移除禁用样式，启用所有小车控件
  } else {
    carArea.classList.add('disabled');     // 添加禁用样式，禁用所有小车控件
  }
}

// 时钟模式按钮点击事件
btnClock.onclick = () => {
  sendCmd('/mode_clock');                  // 通知设备切换到时钟模式
  btnClock.classList.add('active');        // 激活时钟按钮样式
  btnCar.classList.remove('active');       // 取消小车按钮激活样式
  setCarMode(false);                       // 禁用小车控件
};

// 小车模式按钮点击事件
btnCar.onclick = () => {
  sendCmd('/mode_car');                    // 通知设备切换到小车模式
  btnCar.classList.add('active');          // 激活小车按钮样式
  btnClock.classList.remove('active');     // 取消时钟按钮激活样式
  setCarMode(true);                        // 启用小车控件
};

// ==================== 随机模式控制 ====================
// 睡眠模式
document.getElementById('modeOff').onclick = () => {
  sendCmd('/mode_off');
  document.querySelectorAll('.mode-random').forEach(b => b.classList.remove('btn-active'));
  document.getElementById('modeOff').classList.add('btn-active');
};
// 摆动模式
document.getElementById('modeSoft').onclick = () => {
  sendCmd('/mode_soft');
  document.querySelectorAll('.mode-random').forEach(b => b.classList.remove('btn-active'));
  document.getElementById('modeSoft').classList.add('btn-active');
};
// 好奇模式
document.getElementById('modeNormal').onclick = () => {
  sendCmd('/mode_normal');
  document.querySelectorAll('.mode-random').forEach(b => b.classList.remove('btn-active'));
  document.getElementById('modeNormal').classList.add('btn-active');
};

// ==================== 动作指令控制 ====================
document.getElementById('btnDance').onclick = () => sendCmd('/action?cmd=dance');          // 跳舞
document.getElementById('btnSpin').onclick = () => sendCmd('/action?cmd=spin');            // 转圈
document.getElementById('btnSwingAlways').onclick = () => sendCmd('/action?cmd=swing_always'); // 持续摇摆
document.getElementById('btnSwingDance').onclick = () => sendCmd('/action?cmd=swing_dance');   // 摇摆舞
// 注意：全局停止按钮已移到方向控制区，这里不再需要

// ==================== 表情控制 ====================
document.getElementById('emoDefault').onclick = () => sendCmd('/emotion?cmd=default');      // 默认表情
document.getElementById('emoHappy').onclick = () => sendCmd('/emotion?cmd=happy');          // 开心
document.getElementById('emoAngry').onclick = () => sendCmd('/emotion?cmd=angry');          // 生气
document.getElementById('emoTired').onclick = () => sendCmd('/emotion?cmd=tired');          // 疲惫
document.getElementById('emoLaugh').onclick = () => sendCmd('/emotion?cmd=laugh');          // 大笑
document.getElementById('emoConfused').onclick = () => sendCmd('/emotion?cmd=confused');    // 困惑
document.getElementById('emoSweat').onclick = () => sendCmd('/emotion?cmd=sweat');          // 流汗
document.getElementById('emoSurprised').onclick = () => sendCmd('/emotion?cmd=surprised');  // 惊讶
document.getElementById('emoSleepy').onclick = () => sendCmd('/emotion?cmd=sleepy');        // 困倦
document.getElementById('emoSpeechless').onclick = () => sendCmd('/emotion?cmd=speechless'); // 无语

// ==================== 初始化状态 ====================
setCarMode(false);  // 初始为时钟模式，小车控件禁用
</script>
</body>
</html>
)rawliteral";

  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send(200, "text/html", page);
}
// ================= 服务器路由 =================
void setupServer() {
  server.client().setTimeout(3000);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);
  server.on("/", handleRoot);
  server.on("/joystick", []() {
    if (server.hasArg("cmd")) {
      byte cmd = server.arg("cmd").toInt();
      if (deviceMode == MODE_CAR) motorWifi(cmd);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "ERR");
    }
  });
  server.on("/action", []() {
    if (!server.hasArg("cmd") || deviceMode != MODE_CAR) {
      server.send(400, "text/plain", "ERR");
      return;
    }
    String cmd = server.arg("cmd");
    if (cmd == "dance" && currentAction == ACTION_NONE) {
      currentAction = ACTION_DANCE;
      dancePhase = 0;
    } else if (cmd == "spin" && currentAction == ACTION_NONE) {
      currentAction = ACTION_SPIN_CIRCLE;
      spinPhase = 0;
    } else if (cmd == "swing_always" && currentAction == ACTION_NONE) {
      currentAction = ACTION_SWING_ALWAYS;
      swingAlwaysPhase = 0;
    } else if (cmd == "swing_dance" && currentAction == ACTION_NONE) {
      currentAction = ACTION_SWING_DANCE;
      swingDancePhase = 0;
    } else if (cmd == "stop_all") {
      stopAllActions();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/emotion", []() {
    if (!server.hasArg("cmd") || deviceMode != MODE_CAR || !roboEyesEnabled) {
      server.send(400, "text/plain", "ERR");
      return;
    }
    String cmd = server.arg("cmd");
    if (cmd == "default") setRoboEmotion(EMOTION_DEFAULT);
    else if (cmd == "happy") setRoboEmotion(EMOTION_HAPPY);
    else if (cmd == "angry") setRoboEmotion(EMOTION_ANGRY);
    else if (cmd == "tired") setRoboEmotion(EMOTION_TIRED);
    else if (cmd == "laugh") setRoboEmotion(EMOTION_LAUGH);
    else if (cmd == "confused") setRoboEmotion(EMOTION_CONFUSED);
    else if (cmd == "sweat") setRoboEmotion(EMOTION_SWEAT);
    else if (cmd == "surprised") setRoboEmotion(EMOTION_SURPRISED);
    else if (cmd == "sleepy") setRoboEmotion(EMOTION_SLEEPY);
    else if (cmd == "speechless") setRoboEmotion(EMOTION_SPEECHLESS);
    server.send(200, "text/plain", "OK");
  });
  server.on("/reconfig", []() {
    server.send(200, "text/plain", "Reconfig Start");
    server.client().stop();
    delay(500);
    reconfigWiFi();
  });
  server.on("/mode_clock", []() {
    deviceMode = MODE_CLOCK;
    motorPinLocked = true;
    roboEyesEnabled = false;
    stopAllActions();
    server.send(200, "text/plain", "CLOCK");
  });
  server.on("/mode_car", []() {
    deviceMode = MODE_CAR;
    roboEyesEnabled = true;
    motorPinLocked = false;
    if (!roboEyesInited) {
      roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 60);
      roboEyesInited = true;
      ASR_Serial.println("✅ RoboEyes首次初始化完成");
    }
    server.send(200, "text/plain", "CAR");
  });
  server.on("/mode_off", []() {
    if (deviceMode == MODE_CAR) randomMode = RANDOM_OFF;
    server.send(200, "text/plain", "OK");
  });
  server.on("/mode_soft", []() {
    if (deviceMode == MODE_CAR) randomMode = RANDOM_SOFT;
    server.send(200, "text/plain", "OK");
  });
  server.on("/mode_normal", []() {
    if (deviceMode == MODE_CAR) randomMode = RANDOM_NORMAL;
    server.send(200, "text/plain", "OK");
  });
  server.onNotFound([]() {
    handleRoot();
  });
  server.begin();
  ASR_Serial.println("✅ Web服务器启动成功");
}

// ================= 天问Block语音指令解析 =================
// ================= 天问Block语音指令解析 =================
void parseASRCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();  // 统一转小写，便于比较
  if (cmd.length() == 0) return;

  ASR_Serial.print("[天问ASR] 解析指令：");
  ASR_Serial.println(cmd);

  // 指令间隔超时判断
  if (millis() - lastCmdTime > CMD_TIMEOUT && lastCmdTime != 0) {
    resetAllCmdCount();
    ASR_Serial.println("【计数重置】指令间隔超时，重置所有计数");
  }
  lastCmdTime = millis();

  // 5个目标指令计数逻辑
  bool isTargetCmd = false;
  if (cmd == "clock") {
    isTargetCmd = true;
    clockCount++;
    ASR_Serial.print("clock计数：");
    ASR_Serial.println(clockCount);
    if (clockCount >= 3) {
      showSecretInfo = true;
      secretShowText = "dy Cap_0109";
    }
  } else if (cmd == "off" && deviceMode == MODE_CAR) {
    isTargetCmd = true;
    offCount++;
    ASR_Serial.print("off计数：");
    ASR_Serial.println(offCount);
    if (offCount >= 7) {
      showSecretInfo = true;
      secretShowText = "cao xi si ma";
    }
  } else if (cmd == "nike" && deviceMode == MODE_CAR) {
    isTargetCmd = true;
    nikeCount++;
    ASR_Serial.print("nike计数：");
    ASR_Serial.println(nikeCount);
    if (nikeCount >= 3) {
      showSecretInfo = true;
      secretShowText = "NIKE is the creator of this code .";
    }
  } else if (cmd == "ak" && deviceMode == MODE_CAR) {
    isTargetCmd = true;
    aimokCount++;
    ASR_Serial.print("aimok计数：");
    ASR_Serial.println(aimokCount);
    if (aimokCount >= 3) {
      showSecretInfo = true;
      secretShowText = "A IM'OK";
    }
  } else if (cmd == "fm" && deviceMode == MODE_CAR) {
    isTargetCmd = true;
    fengmaoCount++;
    ASR_Serial.print("fengmao计数：");
    ASR_Serial.println(fengmaoCount);
    if (fengmaoCount >= 9) {
      showSecretInfo = true;
      secretShowText = "FENGMAO";
    }
  }

  if (!isTargetCmd) {
    resetAllCmdCount();
    ASR_Serial.println("【计数重置】非目标指令，重置所有计数");
  }
  // 基础移动指令
  if (cmd == "go") motorWifi(1);
  else if (cmd == "back") motorWifi(2);
  else if (cmd == "left") {
    // 保存当前电机状态
    lastMotorCmdBeforeTurn = currentMotorCmd;
    // 执行左转
    motorWifi(3);
    delay(300);  // 转向持续时间（毫秒），可根据需要调整
    // 恢复之前的电机状态
    motorWifi(lastMotorCmdBeforeTurn);
    ASR_Serial.print("左转完成，已恢复");
    if (lastMotorCmdBeforeTurn == 1) ASR_Serial.println("前进");
    else if (lastMotorCmdBeforeTurn == 2) ASR_Serial.println("后退");
    else if (lastMotorCmdBeforeTurn == 0) ASR_Serial.println("停止");
    else ASR_Serial.println("原状态");
  } else if (cmd == "right") {
    // 保存当前电机状态
    lastMotorCmdBeforeTurn = currentMotorCmd;
    // 执行右转
    motorWifi(4);
    delay(250);  // 转向持续时间（毫秒），可根据需要调整
    // 恢复之前的电机状态
    motorWifi(lastMotorCmdBeforeTurn);
    ASR_Serial.print("右转完成，已恢复");
    if (lastMotorCmdBeforeTurn == 1) ASR_Serial.println("前进");
    else if (lastMotorCmdBeforeTurn == 2) ASR_Serial.println("后退");
    else if (lastMotorCmdBeforeTurn == 0) ASR_Serial.println("停止");
    else ASR_Serial.println("原状态");
  } else if (cmd == "stop") {
    stopAllActions();
    resetAllCmdCount();
  }
  // 模式切换
  else if (cmd == "car") {
    deviceMode = MODE_CAR;
    roboEyesEnabled = true;
    motorPinLocked = false;
    if (!roboEyesInited) {
      roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 60);
      roboEyesInited = true;
    }
    ASR_Serial.println("已切换为小车模式，等待指令");
    resetAllCmdCount();
  } else if (cmd == "clock") {
    deviceMode = MODE_CLOCK;
    motorPinLocked = true;
    roboEyesEnabled = false;
    stopAllActions();
    ASR_Serial.println("已切换为时钟模式，电机已锁死");
  }
  // 高级动作指令
  else if (cmd == "dance" && deviceMode == MODE_CAR && currentAction == ACTION_NONE) {
    currentAction = ACTION_DANCE;
    dancePhase = 0;
  } else if (cmd == "spin" && deviceMode == MODE_CAR && currentAction == ACTION_NONE) {
    currentAction = ACTION_SPIN_CIRCLE;
    spinPhase = 0;
  } else if (cmd == "swing" && deviceMode == MODE_CAR && currentAction == ACTION_NONE) {
    currentAction = ACTION_SWING_ALWAYS;
    swingAlwaysPhase = 0;
  } else if ((cmd == "swing28" || cmd == "swingdance") && deviceMode == MODE_CAR && currentAction == ACTION_NONE) {
    currentAction = ACTION_SWING_DANCE;
    swingDancePhase = 0;
  }
  // 随机模式指令
  else if (cmd == "off" && deviceMode == MODE_CAR) {
    randomMode = RANDOM_OFF;
  } else if (cmd == "soft" && deviceMode == MODE_CAR) {
    randomMode = RANDOM_SOFT;
  } else if (cmd == "normal" && deviceMode == MODE_CAR) {
    randomMode = RANDOM_NORMAL;
  }
  // 警灯/氛围灯指令
  else if (cmd == "police" && deviceMode == MODE_CAR) {
    stopAllActions();
    currentAction = ACTION_POLICE_LIGHT;
    policeLightStep = 0;
    ASR_Serial.println("启动警灯模式");
  } else if (cmd == "ambient" && deviceMode == MODE_CAR) {
    stopAllActions();
    currentAction = ACTION_AMBIENT_LIGHT;
    ambientBrightness = 0;
    ambientDir = 1;
    ASR_Serial.println("启动氛围灯呼吸模式");
  }
  // ===== 新增：关灯指令 "gd" =====
  else if (cmd == "gd" && deviceMode == MODE_CAR) {
    // 如果当前是警灯或氛围灯动作，则停止该动作
    if (currentAction == ACTION_POLICE_LIGHT || currentAction == ACTION_AMBIENT_LIGHT) {
      currentAction = ACTION_NONE;
      policeLightStep = 0;
      ambientBrightness = 0;
      ambientDir = 1;
    }
    // 强制关闭红蓝LED
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    ASR_Serial.println("执行关灯指令 (gd)");
  }
  // 配网指令
  else if (cmd == "reconfig") {
    reconfigWiFi();
    resetAllCmdCount();
  }
}
// ================= 初始化 =================
void setup() {
  ASR_Serial.begin(9600, SERIAL_8N1, 20, 21);
  while (!ASR_Serial)
    ;

  pinMode(LF, OUTPUT);
  digitalWrite(LF, LOW);
  pinMode(LB, OUTPUT);
  digitalWrite(LB, LOW);
  pinMode(RF, OUTPUT);
  digitalWrite(RF, LOW);
  pinMode(RB, OUTPUT);
  digitalWrite(RB, LOW);

  ASR_Serial.println("===== 时钟+智能小车融合系统启动 =====");
  ASR_Serial.println("✅ 天问Block语音控制模块已加载（UART1: RX20/TX21）");
  ASR_Serial.println("✅ 全动作非阻塞模式已启用，支持实时指令打断");
  ASR_Serial.println("✅ 5指令计数隐藏信息功能已加载");
  ASR_Serial.println("✅ 新增警灯(POLICE)和氛围灯(AMBIENT)功能（仅语音控制）");

  Wire.begin(I2C_SDA, I2C_SCL);
  ASR_Serial.println("✅ I2C初始化完成");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    ASR_Serial.println("❌ OLED初始化失败！");
    for (;;)
      ;
  }
  display.clearDisplay();
  display.display();
  ASR_Serial.println("✅ OLED初始化完成");

  // PWM初始化
  ledcSetup(0, 1000, 8);
  ledcAttachPin(LED_RED, 0);
  ledcSetup(1, 1000, 8);
  ledcAttachPin(LED_BLUE, 1);
  ledcWrite(0, 0);
  ledcWrite(1, 0);
  ASR_Serial.println("✅ LED PWM已初始化 (红5,蓝7)");

  // 开启AP热点，让用户可以连接并访问控制页面
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(apName);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);
  ASR_Serial.print("✅ AP已开启，SSID: ");
  ASR_Serial.println(apName);
  ASR_Serial.print("AP IP地址: ");
  ASR_Serial.println(apIP);

  // 尝试连接上一次的WiFi
  // ========== 添加配网超时机制 ==========
  // 尝试连接上一次的WiFi，设置超时时间（10秒）
  ASR_Serial.println("⏳ 正在尝试连接WiFi...");
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.setTextSize(1);
  display.println("Connecting WiFi...");
  display.setCursor(10, 35);
  display.display();

  WiFi.begin();

  // 等待连接，最多等待10秒（10000毫秒）
  unsigned long wifiConnectStart = millis();
  const unsigned long WIFI_TIMEOUT = 000;  // 秒超时
  bool wifiConnected = false;

  while (millis() - wifiConnectStart < WIFI_TIMEOUT) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      ASR_Serial.println("✅ WiFi连接成功！");
      ASR_Serial.print("📶 IP地址：");
      ASR_Serial.println(WiFi.localIP());

      // 显示连接成功信息
      display.clearDisplay();
      display.setCursor(10, 20);
      display.println("WiFi Connected!");
      display.setCursor(10, 35);
      display.print("IP:");
      display.println(WiFi.localIP());
      display.display();
      delay(2000);
      break;
    }
    delay(100);
  }

  if (!wifiConnected) {
    // 超时未连接，跳过配网
    ASR_Serial.println("⚠️ WiFi连接超时（1秒），跳过配网，进入时钟模式");
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 20);
    display.setTextSize(1);
    display.println("WiFi Timeout!");
    display.setCursor(10, 35);
    display.println("Skip Config");
    display.setCursor(10, 50);
    display.println("Clock Mode");
    display.display();
    delay(2000);

    // 保持AP模式开启，但不再进行进一步的配网
    // 用户仍然可以连接AP进行后续配网（通过重新配网功能）
  }
  // ========== 修改结束 ==========
  timeClient.begin();
  ASR_Serial.println("⏰ 时钟模式已启动");

  setupServer();
  roboEyesInited = false;
  roboEyesEnabled = false;
  motorPinLocked = true;
  lastEmotionChangeTime = millis();
  lastRandomEmotionCheck = millis();
  lastRandomAction = millis();
  motorDirectControl(0);
  currentMotorCmd = 0;
  resetAllCmdCount();
}

// ================= 主循环 =================
void loop() {
  // 处理网络和网页请求
  dnsServer.processNextRequest();
  server.handleClient();

  // 处理语音指令
  while (ASR_Serial.available() > 0) {
    char ch = ASR_Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (asrRecvBuffer.length() > 0) {
        parseASRCommand(asrRecvBuffer);
        asrRecvBuffer = "";
      }
    } else {
      asrRecvBuffer += ch;
    }
  }

  // 时钟模式
  if (deviceMode == MODE_CLOCK) {
    if (millis() - lastNTPUpdate_Clock >= 500) {
      lastNTPUpdate_Clock = millis();
      if (showSecretInfo) {
        showSecretOLEDInfo();
      } else {
        updateDisplay_Clock();
      }
    }
  }
  // 小车模式
  else if (deviceMode == MODE_CAR) {
    if (roboEyesEnabled) roboEyes.update();
    if (showSecretInfo) {
      showSecretOLEDInfo();
    }

    if (manualActive && millis() > manualControlEndTime) {
      manualActive = false;
    }

    // 执行当前动作
    if (currentAction == ACTION_DANCE) {
      doDanceAction();
    } else if (currentAction == ACTION_SPIN_CIRCLE) {
      doSpinCircleAction();
    } else if (currentAction == ACTION_RANDOM_SMALL) {
      doRandomSmallAction();
    } else if (currentAction == ACTION_SWING_ALWAYS) {
      doSwingAlwaysAction();
    } else if (currentAction == ACTION_SWING_DANCE) {
      doSwingDanceAction();
    } else if (currentAction == ACTION_POLICE_LIGHT) {
      doPoliceLightAction();
    } else if (currentAction == ACTION_AMBIENT_LIGHT) {
      doAmbientLightAction();
    }

    // 随机表情切换
    if (millis() - lastRandomEmotionCheck >= random(EMOTION_SWITCH_INTERVAL_MIN, EMOTION_SWITCH_INTERVAL_MAX) && !manualActive && currentAction == ACTION_NONE && !showSecretInfo) {
      lastRandomEmotionCheck = millis();
      randomSwitchEmotion();
    }

    // 随机小动作触发
    if (millis() - lastRandomAction >= random(RANDOM_SMALL_ACTION_INTERVAL / 2, RANDOM_SMALL_ACTION_INTERVAL) && !manualActive && currentAction == ACTION_NONE && randomMode != RANDOM_OFF && !showSecretInfo) {
      lastRandomAction = millis();
      currentAction = ACTION_RANDOM_SMALL;
      randomSmallActionType = 0;
    }
  }
}
