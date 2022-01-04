#define BLINKER_DUEROS_OUTLET//定义点灯提供的小度音响接口，需要OUTLET类型的控制
#define BLINKER_WIFI//定义点灯的WIFI  

#include <WiFiManager.h>//引用自动配网库，源代码:https://github.com/tzapu/WiFiManager，没有添加该库的需要自己下载添加，配网过程：手机连接该设备的WIFI，会自动弹出配网的网页，然后根据提示操作,配网完成后会自动保存WIFI信息，下次会自动连接
#include <ArduinoOTA.h>//引用OTA相关库，主要用于在WIFI更新程序
#include <LittleFS.h>//引用闪存相关库
#include <Blinker.h>//引用点灯相关库

char auth[] = "3d7c82ad46cc";//点灯APP上的设备ID
const char deviceName[] = "LYD-ESP-12E-3d7c82ad46cc";//自定义的设备名称

WiFiManager wifiManager;//实例化自动配网库

//实例化点灯APP上的按钮
BlinkerButton Btn_Open("btn-open");//打开窗帘按钮
BlinkerButton Btn_Close("btn-close");//关闭窗帘按钮
BlinkerButton Btn_Stop("btn-stop");//停止窗帘按钮
BlinkerSlider Slider("ran-mku");//持续时间的滑动条

//定义引脚
int OPEN_PIN = D5;//根据我买的电机控制板，需要使用PWM引脚,因为其他引脚通电瞬间会有电平输出，造成电机控制器在通电瞬间乱来
int CLOSE_PIN = D2;//根据我买的电机控制板，需要使用PWM引脚,因为其他引脚通电瞬间会有电平输出，造成电机控制器在通电瞬间乱来
int STOP_OPEN_PIN = D1;
int STOP_CLOSE_PIN = D0;

int durationValue = 5000;//延迟停止的时间(ms)
int curtainState = 0;//0-闲置,1-正在打开窗帘,2-正在关闭窗帘
String durationValue_fileStr = "/Config/durationValue.txt"; //开关窗帘持续时间保存到闪存中,此种方式控制窗帘并不是最佳，可以在窗帘两端安装限位开关

void setup() {
  Serial.begin(115200);
  BLINKER_DEBUG.stream(Serial);

  WifiConfig();//配置网络
  OtaConifg();//配置OTA

  initPin();//实例化需要用到的各个引脚，并设置为输出模式
  initBlinkerControl();//初始化点灯科技的控件
  initDurationValue();//初始化窗帘开关的持续时间

}
void WifiConfig()
{
  //设置连接WIFI超时时间20s
  wifiManager.setConfigPortalTimeout(20);
  wifiManager.autoConnect(deviceName);
  WiFi.hostname(deviceName);//设置主机名

  //blinker链接WIFI
  Blinker.begin(auth, WiFi.SSID().c_str(), WiFi.psk().c_str());
}
void OtaConifg()
{
  ArduinoOTA.setHostname(deviceName);
  ArduinoOTA.setPassword("12345678");
  ArduinoOTA.begin();
}
void initPin()
{
  //点亮LED灯，表示工作正常
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(OPEN_PIN, OUTPUT);
  pinMode(CLOSE_PIN, OUTPUT);
  pinMode(STOP_OPEN_PIN, OUTPUT);
  pinMode(STOP_CLOSE_PIN, OUTPUT);

  digitalWrite(OPEN_PIN, LOW);
  digitalWrite(CLOSE_PIN, LOW);
  digitalWrite(STOP_OPEN_PIN, LOW);
  digitalWrite(STOP_CLOSE_PIN, LOW);
}
void initBlinkerControl()
{
  //为点灯科技的控件绑定事件函数
  Btn_Open.attach(btnOpen_Click);
  Btn_Close.attach(btnClose_Click);
  Btn_Stop.attach(btnStop_Click);
  Slider.attach(slider_Click);
  //通过点灯科技,初始化小度,为小度绑定语音控制的事件函数(小度通过点灯科技之后把命令发到绑定的函数)
  BlinkerDuerOS.attachPowerState(duerPowerState);
}

void btnOpen_Click(const String& state)
{
  BLINKER_LOG("Open Curtain");
  curtainState = 1;
  digitalWrite(OPEN_PIN, HIGH);
  Blinker.delay(500);
  digitalWrite(OPEN_PIN, LOW);
  //延时一段时间后触发停止
  Blinker.delay(durationValue);
  btnStop_Click(state);
}

void btnClose_Click(const String& state)
{
  BLINKER_LOG("Close Curtain");
  curtainState = 2;
  digitalWrite(CLOSE_PIN, HIGH);
  Blinker.delay(1000);
  digitalWrite(CLOSE_PIN, LOW);
  //延时一段时间后触发停止
  Blinker.delay(durationValue);
  btnStop_Click(state);
}

void btnStop_Click(const String& state)
{
  BLINKER_LOG("Stop Curtain");
  switch (curtainState)
  {
    case 1:
      digitalWrite(STOP_OPEN_PIN, HIGH);
      digitalWrite(STOP_CLOSE_PIN, HIGH);
      Blinker.delay(500);
      digitalWrite(STOP_OPEN_PIN, LOW);
      digitalWrite(STOP_CLOSE_PIN, LOW);
      break;
    case 2:
      digitalWrite(STOP_CLOSE_PIN, HIGH);
      digitalWrite(STOP_CLOSE_PIN, HIGH);
      Blinker.delay(500);
      digitalWrite(STOP_CLOSE_PIN, LOW);
      digitalWrite(STOP_OPEN_PIN, LOW);
    default:
      break;
  }
  curtainState = 0;
}

void duerPowerState(const String& state)
{
  BlinkerDuerOS.powerState("on");
  BlinkerDuerOS.print();
  if (state == BLINKER_CMD_ON)
  {
    btnOpen_Click(state);
  }
  else if (state == BLINKER_CMD_OFF)
  {
    btnClose_Click(state);
  }
  BlinkerDuerOS.powerState("off");
  BlinkerDuerOS.print();
}

void slider_Click(int32_t value)
{
  durationValue = value * 1000;
  saveDurationValue(durationValue);
}

void saveDurationValue(int value)
{
  if (SPIFFS.begin())
  {
    File dataFile = SPIFFS.open(durationValue_fileStr, "w");//建立File的写入对象
    BLINKER_LOG("durationValue：", value);
    dataFile.println(value);//写入信息
    dataFile.close();
    BLINKER_LOG("WRITE:durationValue:", durationValue);
  }
}

void initDurationValue()
{
  //确认闪存中是否有durationValue_fileStr文件
  if (SPIFFS.begin() && SPIFFS.exists(durationValue_fileStr))
  {
    File dataFile = SPIFFS.open(durationValue_fileStr, "r");//建立File的读取对象
    durationValue = atoi(dataFile.readString().c_str());
    //完成文件读取后关闭文件
    dataFile.close();
    BLINKER_LOG(" FOUND:durationValue:", durationValue);
    Slider.print(durationValue / 1000);
  }
  else
  {
    BLINKER_LOG("NOT FOUND:durationValue");
  }
}

void loop()
{
  Blinker.run();//点灯科技随时检查是否有需要处理的东西
  ArduinoOTA.handle();//OTA随时坚持是否有需要处理的东西
}
