#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>  // 改用ESP32的WiFi库
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <map>
#include <SPIFFS.h>  // ESP32使用SPIFFS而不是LittleFS
#include <ESP32Servo.h>  // 使用ESP32专用的舵机库
#include <Adafruit_NeoPixel.h>
#include <driver/gpio.h>  // ESP32 GPIO配置

#define MSG_BUFFER_SIZE	(4096)//mqtt服务器的配置
#define AGAIN 100//定义步骤特殊变量

//-=-=-=-=-=-=-=-=↓用户配置↓-=-=-=-=-=-=-=-=-=-=
String wifiName;//
String wifiKey;//
String bambu_mqtt_broker;//
String bambu_mqtt_password;//
String bambu_device_serial;//
int filamentID;//
String ha_mqtt_broker;
String ha_mqtt_user;
String ha_mqtt_password;
//-=-=-=-=-=-=-=-=↑用户配置↑-=-=-=-=-=-=-=-=-=-=

//-=-=-=-=-=-↓系统配置↓-=-=-=-=-=-=-=-=-=
bool debug = false;
String sw_version = "v1.6.1";
String bambu_mqtt_user = "bblp";
String bambu_mqtt_id = "ams";
String bambu_topic_subscribe;// = "device/" + String(bambu_device_serial) + "/report";
String bambu_topic_publish;// = "device/" + String(bambu_device_serial) + "/request";
String bambu_resume = "{\"print\":{\"command\":\"resume\",\"sequence_id\":\"1\"},\"user_id\":\"1\"}"; // 重试|继续打印
String bambu_unload = "{\"print\":{\"command\":\"ams_change_filament\",\"curr_temp\":220,\"sequence_id\":\"1\",\"tar_temp\":220,\"target\":255},\"user_id\":\"1\"}";
String bambu_load = "{\"print\":{\"command\":\"ams_change_filament\",\"curr_temp\":220,\"sequence_id\":\"1\",\"tar_temp\":220,\"target\":254},\"user_id\":\"1\"}";
String bambu_done = "{\"print\":{\"command\":\"ams_control\",\"param\":\"done\",\"sequence_id\":\"1\"},\"user_id\":\"1\"}"; // 完成
String bambu_clear = "{\"print\":{\"command\": \"clean_print_error\",\"sequence_id\":\"1\"},\"user_id\":\"1\"}";
String bambu_status = "{\"pushing\": {\"sequence_id\": \"0\", \"command\": \"pushall\"}}";

// ESP32的GPIO引脚定义
int servoPin = 13;  // 可以根据需要修改
int motorPin1 = 4;  // 可以根据需要修改
int motorPin2 = 5;  // 可以根据需要修改
int bufferPin1 = 14;  // 可以根据需要修改
int bufferPin2 = 16;  // 可以根据需要修改
unsigned int bambuRenewTime = 1250;//拓竹更新时间
int backTime = 1000;
unsigned int ledBrightness;//led默认亮度
String filamentType;
int filamentTemp;
int ledR;
int ledG;
int ledB;
#define ledPin 12//led引脚
#define ledPixels 3//led数量
//-=-=-=-=-=-↑系统配置↑-=-=-=-=-=-=-=-=-=

//-=-=-=-=-=-mqtt回调逻辑需要的变量-=-=-=-=-=-=
long sendOutTimeOut;
bool isSendOut;
int sendOutTimes;
long pullBackTimeOut;
bool isPullBack;
int pullBackTimes;
int lastFilament;
int nextFilament;
int subTaskID = 0;
bool sameTask;
bool NeedLoad;
bool CanPush = false;//用于等待另一通道回抽完毕
long NeedStopTime = 0;//用于判断是否要发送停止信息
//-=-=-=-=-=-=end

unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];
WiFiClientSecure bambuWifiClient;
PubSubClient bambuClient(bambuWifiClient);

Adafruit_NeoPixel leds(ledPixels, ledPin, NEO_GRB + NEO_KHZ800);

unsigned long bambuLastTime = millis();
unsigned long bambuCheckTime = millis();//mqtt定时任务
int inLed = 2;//跑马灯led变量
int waitLed = 2;
int completeLed = 2;

// 定义舵机对象
Servo servo;

void ledAll(unsigned int r, unsigned int g, unsigned int b) {//led填充
    leds.fill(leds.Color(r,g,b));
    leds.show();
}

//连接wifi
void connectWF(String wn,String wk) {
    ledAll(0,0,0);
    int led = 2;
    int count = 1;
    WiFi.begin(wn, wk);
    Serial.print("连接到wifi [" + String(wifiName) + "]");
    while (WiFi.status() != WL_CONNECTED) {
        count ++;
        if (led == -1){
            led = 2;
            ledAll(0,0,0);
        }else{
            leds.setPixelColor(led,leds.Color(0,255,0));
            leds.show();
            led--;
        }
        Serial.print(".");
        delay(250);
        if (count > 100){
            ledAll(255,0,0);
            Serial.println("WIFI连接超时!请检查你的wifi配置");
            Serial.println("WIFI名["+String(wifiName)+"]");
            Serial.println("WIFI密码["+String(wifiKey)+"]");
            Serial.println("本次输出[]内没有内置空格!");
            Serial.println("你将有两种选择:");
            Serial.println("1:已经确认我的wifi配置没有问题!继续重试!");
            Serial.println("2:我的配置有误,删除配置重新书写");
            Serial.println("请输入你的选择:");
            while (Serial.available() == 0){
                Serial.print(".");
                delay(100);
            }
            String content = Serial.readString();
            if (content == "2"){if(SPIFFS.remove("/config.json")){Serial.println("SUCCESS!");ESP.restart();}}
            ESP.restart();
        }
    }
    Serial.println("");
    Serial.println("WIFI已连接");
    Serial.println("IP: ");
    Serial.println(WiFi.localIP());
    ledAll(50,255,50);
}

//获取配置数据
JsonDocument getCData(){
    File file = SPIFFS.open("/config.json", "r");
    JsonDocument Cdata;
    deserializeJson(Cdata, file);
    file.close();
    return Cdata;
}
//写入配置数据
void writeCData(JsonDocument Cdata){
    // 添加位置配置
    JsonArray trayPositions = Cdata.createNestedArray("trayPositions");
    for(int i = 1; i <= 4; i++) {
        JsonObject tray = trayPositions.createNestedObject();
        tray["id"] = i;
        tray["position"] = (i-1) * 100;  // 默认位置间隔100单位
    }
    
    // 原有的配置写入
    File file = SPIFFS.open("/config.json", "w");
    serializeJson(Cdata, file);
    file.close();
}

//定义电机驱动类和舵机控制类
class Machinery {
  private:
    int pin1;
    int pin2;
    bool isStop = true;
    String state = "停止";
  public:
    Machinery(int pin1, int pin2) {
      this->pin1 = pin1;
      this->pin2 = pin2;
      pinMode(pin1, OUTPUT);
      pinMode(pin2, OUTPUT);
    }

    void forward() {
      digitalWrite(pin1, HIGH);
      digitalWrite(pin2, LOW);
      isStop = false;
      state = "前进";
    }

    void backforward() {
      digitalWrite(pin1, LOW);
      digitalWrite(pin2, HIGH);
      isStop = false;
      state = "后退";
    }

    void stop() {
      digitalWrite(pin1, HIGH);
      digitalWrite(pin2, HIGH);
      isStop = true;
      state = "停止";
    }

    bool getStopState() {
        return isStop;
    }
    String getState(){
        return state;
    }
};
class ServoControl {
    private:
        int angle = -1;
        String state = "自定义角度";
    public:
        ServoControl(){
        }
        void push() {
            servo.write(0);
            angle = 0;
            state = "推";
        }
        void pull() {
            servo.write(180);
            angle = 180;
            state = "拉";
        }
        void writeAngle(int angle) {
            servo.write(angle);
            angle = angle;
            state = "自定义角度";
        }
        int getAngle(){
            return angle;
        }
        String getState(){
            return state;
        }
    };
//定义电机舵机变量
ServoControl sv;
Machinery mc(motorPin1, motorPin2);

void statePublish(String content){
    Serial.println(content);
    // 保存GPIO2的当前模式
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << 2);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
    
    Serial1.println("[通道"+String(filamentID)+"]"+content);
    Serial1.flush();
    
    // 恢复GPIO2为UART功能
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);
}

//连接拓竹mqtt
void connectBambuMQTT() {
    int count = 1;
    while (!bambuClient.connected()) {
        count ++;
        Serial.println("尝试连接拓竹mqtt|"+bambu_mqtt_id+"|"+bambu_mqtt_user+"|"+bambu_mqtt_password);
        if (bambuClient.connect(bambu_mqtt_id.c_str(), bambu_mqtt_user.c_str(), bambu_mqtt_password.c_str())) {
            Serial.println("连接成功!");
            //Serial.println(bambu_topic_subscribe);
            bambuClient.subscribe(bambu_topic_subscribe.c_str());
            ledAll(ledR,ledG,ledB);
        } else {
            Serial.print("连接失败，失败原因:");
            Serial.print(bambuClient.state());
            Serial.println("在一秒后重新连接");
            mc.stop();
            sv.pull();
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("WiFi连接已断开,正在尝试重连...");
                connectWF(wifiName, wifiKey);
            }
            delay(1000);
            ledAll(255,0,0);
        }

        if (count > 10){
            ledAll(255,0,0);
            Serial.println("拓竹连接超时!请检查你的配置");
            Serial.println("拓竹ip地址["+String(bambu_mqtt_broker)+"]");
            Serial.println("拓竹序列号["+String(bambu_device_serial)+"]");
            Serial.println("拓竹访问码["+String(bambu_mqtt_password)+"]");
            Serial.println("本次输出[]内没有内置空格!");
            Serial.println("你将有两种选择:");
            Serial.println("1:已经确认我的配置没有问题!继续重试!");
            Serial.println("2:我的配置有误,删除配置重新书写");
            Serial.println("请输入你的选择:");
            int getCount = 1;
            while (Serial.available() == 0){
                if (getCount > 50){ESP.restart();}
                Serial.print(".");
                delay(100);
                getCount += 1;
            }
            String content = Serial.readString();
            if (content == "2"){if(SPIFFS.remove("/config.json")){Serial.println("SUCCESS!");ESP.restart();}}
            ESP.restart();
        }
    }
}

//拓竹mqtt回调
void bambuCallback(char* topic, byte* payload, unsigned int length) {
    JsonDocument* data = new JsonDocument();
    deserializeJson(*data, payload, length);
    String sequenceId = (*data)["print"]["sequence_id"].as<String>();
    String amsStatus = (*data)["print"]["ams_status"].as<String>();
    String printError = (*data)["print"]["print_error"].as<String>();
    String hwSwitchState = (*data)["print"]["hw_switch_state"].as<String>();
    String gcodeState = (*data)["print"]["gcode_state"].as<String>();
    String mcPercent = (*data)["print"]["mc_percent"].as<String>();
    String mcRemainingTime = (*data)["print"]["mc_remaining_time"].as<String>();
    String layerNum = (*data)["print"]["layer_num"].as<String>();
    String command = (*data)["print"]["command"].as<String>();
    if ((*data)["print"]["subtask_id"].as<String>() != "null"){
        if (subTaskID != (*data)["print"]["subtask_id"].as<int>()){
            sameTask = false;
            subTaskID = (*data)["print"]["subtask_id"].as<int>();
        }
    }
    // 手动释放内存
    delete data;

    if (!(amsStatus == printError && printError == hwSwitchState && hwSwitchState == gcodeState && gcodeState == mcPercent && mcPercent == mcRemainingTime && mcRemainingTime == "null")) {
        if (debug){
            statePublish(sequenceId+"|ams["+amsStatus+"]"+"|err["+printError+"]"+"|hw["+hwSwitchState+"]"+"|gcode["+gcodeState+"]"+"|mcper["+mcPercent+"]"+"|mcrtime["+mcRemainingTime+"]");
            //Serial.print("Free memory: ");
            //Serial.print(ESP.getFreeHeap());
            //Serial.println(" bytes");
            statePublish("Free memory: "+String(ESP.getFreeHeap())+" bytes");
            statePublish("-=-=-=-=-");}
        bambuLastTime = millis();
    }

    if (command.indexOf("APAMS|") != -1){
        if (command.indexOf("|CANPUSH") != -1){
            command.replace("APAMS|","");
            command.replace("|CANPUSH","");
            if (command.toInt() == filamentID){
                CanPush = true;
                statePublish("可以开始进料!");
            }
        }
    }

    if (gcodeState == "PAUSE" and mcPercent.toInt() > 100){
        nextFilament = mcPercent.toInt() - 110 + 1;
        lastFilament = layerNum.toInt() -10 + 1; 
        
        // 检查是否需要换料
        if (nextFilament == lastFilament) {
            // 颜色相同，不需要换料
            if (amsStatus == "261"){
                // 原有的送料逻辑
                if (filamentID == lastFilament){
                    // 本机通道在上料
                    if (nextFilament == filamentID){
                        // 本机通道,上料通道,下一耗材通道全部相同!无需换色!
                    }else{
                        // 需要退料
                        sv.pull();
                        mc.backforward();
                    }
                }else{
                    // 本机通道不在上料
                    if (nextFilament == filamentID){
                        // 需要进料
                        sv.push();
                        mc.forward();
                    }
                }
            }
        } else {
            // 需要换料，检查是否需要移动
            if (gantryController.needToMove(nextFilament)) {
                // 获取目标位置
                int targetPosition = gantryController.getPositionFromTrayId(nextFilament);
                
                // 调用龙门架移动函数
                gantryController.moveToPosition(targetPosition);
                
                // 等待移动完成
                while(!gantryController.isAtPosition()) {
                    delay(100);
                }
            }
            
            // 执行送料动作
            if (amsStatus == "261"){
                // 原有的送料逻辑
                if (filamentID == lastFilament){
                    // 本机通道在上料
                    if (nextFilament == filamentID){
                        // 本机通道,上料通道,下一耗材通道全部相同!无需换色!
                    }else{
                        // 需要退料
                        sv.pull();
                        mc.backforward();
                    }
                }else{
                    // 本机通道不在上料
                    if (nextFilament == filamentID){
                        // 需要进料
                        sv.push();
                        mc.forward();
                    }
                }
            }
        }
    }

    if (command.indexOf("APAMS|") != -1){
        if (command.indexOf("|CANPUSH") != -1){
            command.replace("APAMS|","");
            command.replace("|CANPUSH","");
            if (command.toInt() == filamentID){
                CanPush = true;
                statePublish("可以开始进料!");
            }
        }
    }

    if (gcodeState == "PAUSE" and mcPercent.toInt() > 100){
        nextFilament = mcPercent.toInt() - 110 + 1;
        lastFilament = layerNum.toInt() -10 + 1; 
        
        // 获取目标位置
        int targetPosition = positionManager.getPositionFromTrayId(nextFilament);
        
        // 如果不在目标位置，先移动
        if (gantryController.getCurrentPosition() != targetPosition) {
            gantryController.moveToPosition(targetPosition);
            // 等待移动完成
            while(!gantryController.isAtPosition()) {
                delay(100);
            }
        }
        
        leds.setPixelColor(2,leds.Color(255,0,0));
        leds.setPixelColor(1,leds.Color(0,255,0));
        leds.setPixelColor(0,leds.Color(0,0,255));
        leds.show();
        
        if (filamentID == lastFilament){
            if (nextFilament == filamentID){
                bambuClient.publish(bambu_topic_publish.c_str(),bambu_resume.c_str());
            }else{
                if (amsStatus == "1280"){
                    statePublish("发送退料指令");
                    bambuClient.publish(bambu_topic_publish.c_str(),bambu_unload.c_str());
                    leds.setPixelColor(2,leds.Color(0,0,255));
                    leds.setPixelColor(1,leds.Color(0,0,0));
                    leds.setPixelColor(0,leds.Color(0,0,0));
                    leds.show();
                    isPullBack = true;
                }else if (amsStatus == "260"){
                    statePublish("拔出耗材");
                    leds.setPixelColor(2,leds.Color(0,0,255));
                    leds.setPixelColor(1,leds.Color(0,0,255));
                    leds.setPixelColor(0,leds.Color(0,0,0));
                    leds.show();
                    mc.backforward();
                    sv.push();
                    if (isPullBack){
                        pullBackTimeOut = millis();
                        isPullBack = false;
                    }else{
                        if ((millis() - pullBackTimeOut) > 30*1000){
                            mc.stop();
                            sv.pull();
                            statePublish("退料超时,请手动操作并检查挤出机状态");
                        }
                    }
                }else if (amsStatus == "0"){
                    statePublish("应该拔出耗材,但需要等待拔出结束");
                    leds.setPixelColor(2,leds.Color(0,0,0));
                    leds.setPixelColor(1,leds.Color(0,0,0));
                    leds.setPixelColor(0,leds.Color(0,0,255));
                    leds.show();
                    if (NeedStopTime == 0){
                        NeedStopTime = millis();
                    }
                }
            }
        }else{
            if (nextFilament == filamentID){
                if (lastFilament == -4 and !sameTask){
                    statePublish("首次换色！先退料再送料！");
                    bambuClient.publish(bambu_topic_publish.c_str(),bambu_unload.c_str());
                    sameTask = true;
                    NeedLoad = true;
                    delay(5000);
                }
                if (amsStatus == "0"){
                    if (lastFilament == -4){
                        if (NeedLoad){
                            bambuClient.publish(bambu_topic_publish.c_str(), bambu_load.c_str());
                            NeedLoad = false;
                            statePublish("发送进料指令");}
                    }else{
                        if (CanPush){
                            bambuClient.publish(bambu_topic_publish.c_str(), bambu_load.c_str());
                            statePublish("发送进料指令");
                        }
                    }
                    leds.setPixelColor(2,leds.Color(255,0,255));
                    leds.setPixelColor(1,leds.Color(0,0,0));
                    leds.setPixelColor(0,leds.Color(0,0,0));
                    leds.show();
                    isSendOut = true;
                }else if (amsStatus == "261"){
                    CanPush = false;
                    statePublish("插入耗材");
                    mc.forward();
                    sv.push();
                    leds.setPixelColor(2,leds.Color(255,0,255));
                    leds.setPixelColor(1,leds.Color(255,0,255));
                    leds.setPixelColor(0,leds.Color(0,0,0));
                    leds.show();
                    if (isSendOut){
                        sendOutTimeOut = millis();
                        sendOutTimes = 0;
                        isSendOut = false;
                    }else{
                        if (sendOutTimes <= 3){
                            if ((millis() - sendOutTimeOut) > static_cast<unsigned long>(30*1000 + backTime)){
                                statePublish("进料超时,正在尝试重试中");
                                sendOutTimes += 1;
                                sendOutTimeOut = millis();
                                mc.backforward();
                                sv.push();
                                delay(5000);
                                mc.forward();
                                sv.push();
                            }
                        }else{
                            statePublish("进料超时,请手动操作并检查挤出机状态");
                            mc.stop();
                            sv.pull();                 
                            leds.setPixelColor(2,leds.Color(255,0,0));
                            leds.show();
                            delay(10);
                            leds.setPixelColor(2,leds.Color(0,0,0));
                            leds.show();
                            delay(10);                 
                            leds.setPixelColor(2,leds.Color(255,0,0));
                            leds.show();
                        }
                    }
                }else if (amsStatus == "262" and hwSwitchState == "1"){
                    statePublish("送入成功,停止插入");
                    sv.pull();
                    mc.stop();
                    bambuClient.publish(bambu_topic_publish.c_str(),bambu_done.c_str());
                    leds.setPixelColor(2,leds.Color(255,0,255));
                    leds.setPixelColor(1,leds.Color(255,0,255));
                    leds.setPixelColor(0,leds.Color(255,0,255));
                    leds.show();
                }else if (amsStatus == "768"){
                    statePublish("换色完成,继续打印");
                    bambuClient.publish(bambu_topic_publish.c_str(), bambu_resume.c_str());
                    leds.setPixelColor(2,leds.Color(0,0,0));
                    leds.setPixelColor(1,leds.Color(0,0,0));
                    leds.setPixelColor(0,leds.Color(255,0,255));
                    leds.show();
                }
            }else{
                statePublish("本机通道与本次换色无关，无需换色");
                ledAll(255,255,255);
            }
        }
    }else{
        ledAll(ledR,ledG,ledB);
    }
}

//定时任务
void bambuTimerCallback() {
    if (debug){Serial.println("bambu定时任务执行！");}
    bambuClient.publish(bambu_topic_publish.c_str(), bambu_status.c_str());
}

void setup() {
    leds.begin();
    Serial.begin(115200);
    Serial1.begin(115200, SERIAL_8N1, 16, 17);  // ESP32的Serial1需要指定RX和TX引脚
    
    // 配置GPIO2为UART功能
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << 2);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
    
    if(!SPIFFS.begin(true)){  // 使用SPIFFS替代LittleFS
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    delay(1);
    leds.clear();
    leds.show();

    if (!SPIFFS.exists("/config.json")) {
        ledAll(255,0,0);
        Serial.println("");
        Serial.println("不存在配置文件!创建配置文件!");
        Serial.println("1.请输入wifi名:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        wifiName = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+wifiName);

        delay(500);
        ledAll(255,0,0);
        
        Serial.println("2.请输入wifi密码:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        wifiKey = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+wifiKey);
        
        delay(500);
        ledAll(255,0,0);

        Serial.println("3.请输入拓竹打印机的ip地址:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        bambu_mqtt_broker = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+bambu_mqtt_broker);
        
        delay(500);
        ledAll(255,0,0);

        Serial.println("4.请输入拓竹打印机的访问码:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        bambu_mqtt_password = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+bambu_mqtt_password);
        
        delay(500);
        ledAll(255,0,0);
        
        Serial.println("5.请输入拓竹打印机的序列号:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        bambu_device_serial = Serial.readString();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+bambu_device_serial);
        
        delay(500);
        ledAll(255,0,0);
    
        Serial.println("6.请输入本机通道编号:");
        while (!(Serial.available() > 0)){
            delay(100);
        }
        filamentID = Serial.readString().toInt();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+String(filamentID));
        
        delay(500);
        ledAll(255,0,0);

        Serial.println("7.请输入回抽延时[单位毫秒]:");
        while (!(Serial.available() > 0)){
            delay(100);
        }

        backTime = Serial.readString().toInt();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+String(backTime));
        
        Serial.println("8.请输入本通道耗材温度(后续可更改):");
        while (!(Serial.available() > 0)){
            delay(100);
        }

        filamentTemp = Serial.readString().toInt();
        ledAll(0,255,0);
        Serial.println("获取到的数据-> "+String(filamentTemp));
        
        JsonDocument Cdata;
        Cdata["wifiName"] = wifiName;
        Cdata["wifiKey"] = wifiKey;
        Cdata["bambu_mqtt_broker"] = bambu_mqtt_broker;
        Cdata["bambu_mqtt_password"] = bambu_mqtt_password;
        Cdata["bambu_device_serial"] = bambu_device_serial;
        Cdata["filamentID"] = filamentID;
        Cdata["ledBrightness"] = 100;
        Cdata["backTime"] = backTime;
        Cdata["filamentTemp"]  = filamentTemp;
        Cdata["filamentType"] = filamentType;
        ledR = 0;
        ledG = 0;
        ledB = 255;
        Cdata["ledR"] = ledR;
        Cdata["ledG"] = ledG;
        Cdata["ledB"] = ledB;
        ledBrightness = 100;
        writeCData(Cdata);
    }else{
        JsonDocument Cdata = getCData();
        
        // 读取位置配置
        if(Cdata.containsKey("trayPositions")) {
            JsonArray positions = Cdata["trayPositions"];
            for(JsonObject tray : positions) {
                positionManager.addTray(
                    tray["id"].as<int>(),
                    tray["position"].as<int>()
                );
            }
        }
        
        serializeJsonPretty(Cdata,Serial);
        wifiName = Cdata["wifiName"].as<String>();
        wifiKey = Cdata["wifiKey"].as<String>();
        bambu_mqtt_broker = Cdata["bambu_mqtt_broker"].as<String>();
        bambu_mqtt_password = Cdata["bambu_mqtt_password"].as<String>();
        bambu_device_serial = Cdata["bambu_device_serial"].as<String>();
        filamentID = Cdata["filamentID"].as<int>();
        ledBrightness = Cdata["ledBrightness"].as<unsigned int>();
        backTime = Cdata["backTime"].as<int>();
        filamentTemp = Cdata["filamentTemp"].as<int>();
        filamentType = Cdata["filamentType"].as<String>();
        ledR = Cdata["ledR"];
        ledG = Cdata["ledG"];
        ledB = Cdata["ledB"];
        ledAll(0,255,0);
    }
    bambu_topic_subscribe = "device/" + String(bambu_device_serial) + "/report";
    bambu_topic_publish = "device/" + String(bambu_device_serial) + "/request";
    leds.setBrightness(ledBrightness);

    mc.stop();
    sv.pull();
    connectWF(wifiName,wifiKey);

    // 初始化ESP32的PWM通道
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    
    servo.setPeriodHertz(50);  // 标准50Hz PWM
    servo.attach(servoPin, 500, 2500);  // 设置舵机引脚和PWM范围

    pinMode(bufferPin1, INPUT_PULLDOWN);
    pinMode(bufferPin2, INPUT_PULLDOWN);

    bambuWifiClient.setInsecure();
    bambuClient.setServer(bambu_mqtt_broker.c_str(), 8883);
    bambuClient.setCallback(bambuCallback);
    bambuClient.setBufferSize(4096);

    connectBambuMQTT();
}

void loop() {
    if (!bambuClient.connected()) {
        connectBambuMQTT();
    }
    bambuClient.loop();

    unsigned long nowTime =  millis();
    if (nowTime-bambuLastTime > bambuRenewTime and nowTime-bambuCheckTime > bambuRenewTime*0.8){
        bambuTimerCallback();
        bambuCheckTime = millis();
        leds.setPixelColor(0,leds.Color(10,255,10));
        leds.show();
        delay(10);
        leds.setPixelColor(0,leds.Color(0,0,0));
        leds.show();
    }

    if (not mc.getStopState()){
        if (digitalRead(bufferPin1) == 1 or digitalRead(bufferPin2) == 1){
        mc.stop();}
        delay(100);
    }

    if (NeedStopTime != 0){
        if ((millis()-NeedStopTime) > backTime){
            statePublish("停止拔出耗材");
            mc.stop();
            sv.pull();
            bambuClient.publish(bambu_topic_publish.c_str(),("{\"print\":{\"command\": \"APAMS|"+String(nextFilament)+"|CANPUSH\"}}").c_str());
            NeedStopTime = 0;
        }else if(mc.getStopState() or sv.getState() == "拉"){
            mc.backforward();
            sv.push();
        }
    }

    if (Serial.available()){
        String GETcontent = Serial.readString(); 
        GETcontent.replace("\r", ""); // 移除所有的 \r 字符
        GETcontent.replace("\n", "");
        int msgID;
        String content;
        if (GETcontent.indexOf("|") != -1){
            msgID = GETcontent.substring(0,GETcontent.indexOf("|")).toInt();
            content = GETcontent.substring(GETcontent.indexOf("|") + 1);
        }else{
            msgID = filamentID;
            content = GETcontent;
        }
        if (msgID != filamentID){
            Serial.println(String(msgID)+"|"+content);
        }else{
            if (content=="delet config"){
                if(SPIFFS.remove("/config.json")){Serial.println("SUCCESS!");ESP.restart();}
            }else if (content == "delet data")
            {
                if(SPIFFS.remove("/data.json")){Serial.println("SUCCESS!");ESP.restart();}
            }else if (content == "delet ha")
            {
                if(SPIFFS.remove("/ha.json")){Serial.println("SUCCESS!");ESP.restart();}
            }else if (content == "confirm")
            {
                bambuClient.publish(bambu_topic_publish.c_str(),bambu_done.c_str());
                Serial.println("confirm SEND!");
            }else if (content == "resume")
            {
                bambuClient.publish(bambu_topic_publish.c_str(),bambu_resume.c_str());
                Serial.println("resume SEND!");
            }else if (content == "debug")
            {
                debug = not debug;
                Serial.println("debug change");
            }else if (content == "push")
            {
                sv.push();
                Serial.println("push COMPLETE");
            }else if (content == "pull")
            {
                sv.pull();
                Serial.println("pull COMPLETE");
            }else if (content.indexOf("sv") != -1)
            {   
                String numberString = "";
                for (unsigned int i = 0; i < content.length(); i++) {
                if (isdigit(content[i])) {
                    numberString += content[i];
                }
                }
                int number = numberString.toInt(); 
                sv.writeAngle(number);
                Serial.println("["+numberString+"]COMPLETE");
            }else if (content == "forward" or content == "fw")
            {
                mc.forward();
                Serial.println("forwarding!");
            }else if (content == "backforward" or content == "bfw")
            {
                mc.backforward();
                Serial.println("backforwarding!");
            }else if (content == "stop"){
                mc.stop();
                Serial.println("Stop!");
            }else if (content.indexOf("renewTime") != -1 or content.indexOf("rt") != -1)        {
                String numberString = "";
                for (unsigned int i = 0; i < content.length(); i++) {
                if (isdigit(content[i])) {
                    numberString += content[i];
                }}
                unsigned int number = numberString.toInt();
                bambuRenewTime = number;
                Serial.println("["+numberString+"]COMPLETE");
            }else if (content.indexOf("ledbright") != -1 or content.indexOf("lb") != -1)        {
                String numberString = "";
                for (unsigned int i = 0; i < content.length(); i++) {
                if (isdigit(content[i])) {
                    numberString += content[i];
                }}
                unsigned int number = numberString.toInt();
                ledBrightness = number;
                JsonDocument Cdata = getCData();
                Cdata["ledBrightness"] = ledBrightness;
                writeCData(Cdata);
                Serial.println("["+numberString+"]修改成功！亮度重启后生效");
            }else if (content == "rgb"){
                Serial.println("RGB Testing......");
                ledAll(255,0,0);
                delay(1000);
                ledAll(0,255,0);
                delay(1000);
                ledAll(0,0,255);
                delay(1000);
            }else if (content.indexOf("backTime") != -1 or content.indexOf("bt") != -1){
                String numberString = "";
                for (unsigned int i = 0; i < content.length(); i++) {
                if (isdigit(content[i])) {
                    numberString += content[i];
                }}
                unsigned int number = numberString.toInt();
                backTime = number;
                JsonDocument Cdata = getCData();
                Cdata["backTime"] = backTime;
                writeCData(Cdata);
                Serial.println("["+numberString+"]COMPLETE");
            }else if(content == "CanPushTest"){
                bambuClient.publish(bambu_topic_publish.c_str(),("{\"print\":{\"command\": \"APAMS|"+String(nextFilament)+"|CANPUSH\"}}").c_str());
            }
        }
    }    

    // 更新龙门架状态
    gantryController.update();
}
