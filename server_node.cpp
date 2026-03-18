// TrainerLights Server
// Author: khader afeez
// khaderafeez16@gmail.com

#include <ESP8266mDNS.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <TaskScheduler.h>
#include <ArduinoJson.h>
#include <LinkedList.h>
#include <pgmspace.h>
#include <EEPROM.h>  // Added for config storage

extern "C" {
#include "user_interface.h"
}

// ========== EEPROM Configuration ==========
#define EEPROM_SIZE 128
#define CONFIG_ADDR 0
#define MAGIC_NUMBER 0xABCD  // To verify valid saved config

// ========== Pin Definitions ==========
#define STATUS_LED LED_BUILTIN  // Built-in LED for server status

// ========== HTML Interface (Stored in Flash) ==========
const char htmlHeader[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang=en><head><title>TrainerLights</title><meta name=viewport content="width=device-width,initial-scale=1,minimum-scale=1"><meta charset=UTF-8></head><body><div class=maincontainer><div class=topmenubar><div class=logo><a href=/ ><h1>TrainerLights</h1></a></div></div><div class=topmargin></div><div class=success-message></div><div class=main-wrapper>
)rawliteral";

const char htmlFooter[] PROGMEM = R"rawliteral(
</div><div class=bottommargin></div><div class=footerbar><a href=/ >© TrainerLights</a> | <a href=/ >TrainerLights.cc</a></div><script>function listSensors(){connection.send('{"type":"list_sensors"}')}function restartESP(){var e='{"type":"restart"}';document.getElementById("restartESP").checked&&(document.getElementById("restartESP").checked=!1,console.log(e),connection.send(e))}function sendConfig(){var e='{"type":"config"';e+=',"mode":"'+document.getElementById("mode").value+'"',e+=',"min_delay":"'+document.getElementById("min_delay").value+'"',e+=',"max_delay":"'+document.getElementById("max_delay").value+'"',e+=',"min_timeout":"'+document.getElementById("min_timeout").value+'"',e+=',"max_timeout":"'+document.getElementById("max_timeout").value+'"',e+=',"accelerate_delay_percent":"'+document.getElementById("accelerate_delay_percent").value+'"',e+=',"accelerate_delay_per_seconds":"'+document.getElementById("accelerate_delay_per_seconds").value+'"',e+=',"accelerate_timeout_percent":"'+document.getElementById("accelerate_timeout_percent").value+'"',e+=',"accelerate_timeout_per_seconds":"'+document.getElementById("accelerate_timeout_per_seconds").value+'"',e+=',"min_detection_range":"'+document.getElementById("min_detection_range").value+'"',e+=',"max_detection_range":"'+document.getElementById("max_detection_range").value+'"',e+="}",console.log(e),connection.send(e)}function startTest(){connection.send('{"type":"start_test"}')}function stopTest(){connection.send('{"type":"stop_test"}')}function updateTimer(){d=new Date,n=d.getTime(),tm=n-startTime+timeOffset;var e,t=Math.floor(tm/1e3/60/60),m=Math.floor(tm/6e4)%60,o=(o=tm/1e3%60).toString().match(/^-?\d+(?:\.\d{0,-1})?/)[0];e=(e=("00"+tm).slice(-3)/10).toString().match(/^-?\d+(?:\.\d{0,-1})?/)[0],m=(m<10?"0":"")+m,o=(o<10?"0":"")+o,e=(e<10?"0":"")+e,0==(t+=t>0?":":"")&&(t=""),document.getElementById("timer").innerHTML=t+m+":"+o+"<small>."+e+"</small>"}function pauseTimer(){timeOffset=tm,clearInterval(timerInterval),document.getElementById("startTimer").href="javascript:startTimer();",document.getElementById("startTimer").innerHTML="Start",stopTest()}function startTimer(){d=new Date,n=d.getTime(),startTime=n,clearInterval(timerInterval),timerInterval=setInterval(updateTimer,10),document.getElementById("startTimer").href="javascript:pauseTimer();",document.getElementById("startTimer").innerHTML="Stop",startTest()}function resetTimer(){timeOffset=0,tm=0,clearInterval(timerInterval),document.getElementById("timer").innerHTML="00:00<small>.00</small>",document.getElementById("startTimer").href="javascript:startTimer();",document.getElementById("startTimer").innerHTML="Start",stopTest()}var loc;loc=location.hostname,"localhost"==location.hostname&&(loc="192.168.4.1");var connection=new WebSocket("ws://"+loc+":81/",["arduino"]);connection.onopen=function(){var e='{"type":"app_connected"';e+=',"current_time":"'+(new Date).getTime()+'"',e+="}",connection.send(e)},connection.onerror=function(e){console.log("WebSocket Error ",e)},connection.onmessage=function(e){console.log("Server: ",e.data);var t,n="";if("sensor_list"==(t=JSON.parse(e.data)).type){for(var m=0,o=t.sensors.length;m<o;++m){t.sensors[m];console.log("Sensor IP: ",t.sensors[m].ip+" | num: "+t.sensors[m].num),n+='<div class="sensor"><h1>'+(m+1)+"</h1></div>"}document.getElementById("sensors").innerHTML=n}"stats"==t.type&&(document.getElementById("test_score").innerHTML=t.test_score,document.getElementById("test_errors").innerHTML=t.test_errors,document.getElementById("max_distance").innerHTML=t.max_distance,document.getElementById("min_distance").innerHTML=t.min_distance,document.getElementById("avg_distance").innerHTML=t.avg_distance,document.getElementById("max_response_time").innerHTML=t.max_response_time,document.getElementById("min_response_time").innerHTML=t.min_response_time,document.getElementById("avg_response_time").innerHTML=t.avg_response_time)};var timerInterval,d=new Date,n=d.getTime(),startTime=n,timeOffset=0,tm=0</script></body></html>
)rawliteral";

const char htmlCss[] PROGMEM = R"rawliteral(
<style>/* CSS Bootstrap Customizations Ricardo Lerch @RickLerch */body{width:100%;padding:0;margin:0;font-size:14px;font-family:"Helvetica Neue",Helvetica,Arial,sans-serif;color:#333;background-color:#fff;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}table{width:100%;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}hr{margin-top:2px;margin-bottom:2px;border:0;border-top:1px solid #CCC}h1,h2,h3{margin-top:5px;margin-bottom:5px;font-weight:700}h1{font-size:20px}h2{font-size:18px}h3{font-size:16px}a,a:visited{color:#428bca;text-decoration:none}a:active,a:hover{outline:0;text-decoration:none;color:#516496}a.btn,a.btn:visited,a.btn:hover,a.btn:active{color:#FFF;text-decoration:none}ul{margin:0}.big{font-size:60px;margin:auto}.mid{font-size:40px;margin:auto}.red{color:#982713}.green{color:#408114}.blue{color:#144881}input{width:100%;padding:12px 4px;font-size:18px;border:2px solid #ccc;-webkit-border-radius:4px;-moz-border-radius:4px;border-radius:4px;outline:none;margin:0;margin-bottom:10px;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}textarea{resize:none;width:100%;padding:12px 4px;font-size:18px;line-height:30px;border:2px solid #ccc;-webkit-border-radius:4px;-moz-border-radius:4px;border-radius:4px;outline:none;margin:0;margin-bottom:10px;font-family:"Helvetica Neue",Helvetica,Arial,sans-serif;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}input[type=range]{-webkit-appearance:none;margin:18px 0;width:100%;border:none}input[type=range]::-webkit-slider-runnable-track{height:30%;cursor:pointer;animate:.2s;background:-webkit-linear-gradient(top,#555,#444,#222,#444,#555);border-radius:3px;border:2px solid #010101}input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;border:1px solid #000;height:40px;width:60px;border-radius:5px;background:-webkit-linear-gradient(left,#AAA,#BBB,#BBB,#BBB,#CCC,#AAA,#CCC,#AAA,#CCC,#AAA,#BBB,#BBB,#BBB,#AAA);cursor:pointer;border:2px solid #010101;margin-top:-20px}input[type=radio],input[type=checkbox]{display:none}label:before{content:"";display:inline-block;width:50px;height:50px;margin-right:10px;position:relative;left:0;box-sizing:border-box}.slabelo:before{border-radius:15px}.slabel:before{border-radius:4px;background:#FFF;border:2px solid #ccc}.checkbox label{margin-bottom:10px}.checkbox label:before{border-radius:3px}input[type=radio] + label:before{border-radius:25px}input[type=radio]:checked + label:before{content:"\2022";color:green;font-size:100px;text-align:center;font-weight:700;line-height:48px}input[type=checkbox]:checked + label:before{content:"\2713";text-shadow:1px 1px 1px rgba(0,0,0,.2);font-size:40px;font-weight:700;color:green;text-align:center;line-height:50px}.sensor{width:30%;float:left;text-align:center;margin:1%;border:#000 1px solid;background:#eaffe4}.sensors{width:100%;display:inline-block;text-align:center;border:#000 1px solid}.entire{width:100%;padding:1%;box-sizing:border-box}.half{width:47%;float:left;padding:1%}.third{width:31%;float:left;padding:1%}.twothird{width:64%;float:left;padding:1%}.cont{width:100%;display:inline-block;text-align:center}.contone{width:98%;display:inline-block;text-align:center;padding:1%}.main-wrapper{display:block;padding:10px}.left-wrapper{background-color:#FAFAFA;display:none}.edit-controls{padding:15px}.form-container{width:90%;margin:auto}.detail-title{float:left}.topmenubar,.footerbar{color:#FFF;width:100%;background:#16191B;color:#1f3853;font-size:12px;font-weight:400;z-index:2000;position:relative;display:inline-block;box-sizing:border-box}.topmargin{display:inline-block;height:45px;width:100%}.bottommargin{display:inline-block;height:100px;width:100%}.topmenubar{top:0;left:0;height:45px;position:fixed}.footerbar{bottom:0;vertical-align:middle;text-align:center;z-index:auto}.topmenubar a,.footerbar a{color:#FFF}.topmenubar a:hover,.footerbar a:hover{color:#CCC;text-decoration:none}.logo{display:inline-block;float:left;margin-right:15px;color:#fff;font-size:13px;text-align:center;text-shadow:0 1px 2px rgba(0,0,0,0.5);margin-left:15px;margin-top:10px}.searchbar{display:inline-block;width:100%;float:left;margin-top:15px}.feed{position:relative;display:block;width:100%;background:#fff;border-bottom:1px solid #CCC;min-height:136px;overflow:hidden}.view-detail{position:absolute;right:5px;bottom:5px}.maincontainer{width:100%;box-sizing:border-box;display:inline-block}.option small{font-weight:400}.option{position:relative;font-size:18px;font-weight:700;padding:15px 5px;margin:0;background-color:#FFF;border-style:solid;border-width:1px;border-color:#BBB transparent transparent;cursor:pointer;vertical-align:middle}.opt,.opt:hover,.opt:visited{color:#000}.option:hover,.feed:hover{background-color:#F7F8FF}.option:hover .circ{background:#3a4951}.options-top-bar{width:1102px;padding-left:5px;background:#FFF;box-sizing:border-box;display:inline-block;transition:.5s}.options-top-bar-title{float:left;padding-top:5px}.option-price-title{float:left;text-align:right;margin-left:10px;width:40%}.circ{float:right;background-color:#FFF;border-style:solid;border-width:2px;border-color:#BBB;border-radius:12px;width:20px;height:20px;position:absolute;right:5px;top:14px}.optdesc{float:left}.optsubtitle{margin-top:5px}h1 small,h2 small,h3 small{color:#929292}.selector{width:100%;font-size:18px;padding:10px 0;background:#FFF;border:2px solid #ccc;-webkit-border-radius:4px;-moz-border-radius:4px;border-radius:4px;outline:none;margin:0;margin-bottom:10px;height:50px}.btn{outline:0;display:inline-block;margin-bottom:0;font-weight:700;text-align:center;vertical-align:middle;cursor:pointer;background-image:none;border:1px solid transparent;white-space:nowrap;padding:12px;font-size:14px;line-height:1.428571429;border-radius:4px;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}.btn-block{display:block;width:100%;padding-left:0;padding-right:0}.btn-success{background:green;color:#FFF}.btn-danger{background:#970101;color:#FFF}.btn-danger:hover,.btn-danger:active,.btn-danger.active{background:#B32626;color:#FFF}.btn-success:hover,.btn-success:active,.btn-success.active{background:#409E40;color:#FFF}.btn-blue{background:#0007A7;color:#FFF}.btn-blue:hover,.btn-blue:active,.btn-blue.active{background:#516496;color:#FFF}.btn-lblue{background:#2FC2EF;color:#FFF}.btn-lblue:hover,.btn-lblue:active,.btn-lblue.active{background:#68D5F7;color:#FFF}.btn-red{background:#970101;color:#FFF}.btn-red:hover,.btn-red:active,.btn-red.active{background:#B32626;color:#FFF}</style>
)rawliteral";

const char htmlContent[] PROGMEM = R"rawliteral(
<div class=entire><h1>Test Timer</h1><div class=cont><h1 class=big id=timer style=font-family:monospace>00:00.<small>00</small></h1></div><br><br><div class=cont><div class=twothird><a class="btn btn-block btn-lg btn-success"href=javascript:startTimer(); id=startTimer>Start</a></div><div class=third><a class="btn btn-block btn-lg btn-danger"href=javascript:resetTimer();>Reset</a></div></div><hr><div class=cont><div class=half><h1>Score</h1><h1 class="big green"id=test_score>0</h1></div><div class=half><h1>Errors</h1><h1 class="big red"id=test_errors>0</h1></div></div><hr><h1>Reaction Times (ms)</h1><div class=cont><div class=third><h1>Average</h1><h1 class="mid blue"id=avg_response_time>0</h1></div><div class=third><h1>Minimum</h1><h1 class="mid green"id=min_response_time>0</h1></div><div class=third><h1>Maximum</h1><h1 class="mid red"id=max_response_time>0</h1></div></div><hr><h1>Reaction Distances (cm)</h1><div class=cont><div class=third><h1>Average</h1><h1 class="mid blue"id=avg_distance>0</h1></div><div class=third><h1>Minimum</h1><h1 class="mid green"id=min_distance>0</h1></div><div class=third><h1>Maximum</h1><h1 class="mid red"id=max_distance>0</h1></div></div></div><hr><br><br><div class=entire><h2>Training Modes</h2><select class=selector id=mode><option value=random>Random<option value=sequence>Sequential</select></div><div class=entire style=display:none><h2>Stimulus Mode</h2><select class=selector id=stimulus_mode><option value=on>Light On<option value=blink>Blink<option value=blink_once>Blink Once</select></div><div class=entire style=display:none><h2>Test Time</h2>Test time in seconds <input id=time_test type=number value=0></div><div class=entire><h2>Delay</h2>Choose random delay (ms) between:</div><div class=cont><div class=half><input id=min_delay type=number value=0></div><div class=half><input id=max_delay type=number value=0></div></div><div class=entire><h2>Timeout</h2>Choose random timeout (ms) between:</div><div class=cont><div class=half>Minimum <input id=min_timeout type=number value=1000></div><div class=half>Maximum <input id=max_timeout type=number value=1000></div></div><div class=entire style=display:none><h2>Accelerate Delay</h2>Accelerate delay x% every x sec</div><div class=cont style=display:none><div class=half>% <input id=accelerate_delay_percent type=number value=0></div><div class=half>Seconds <input id=accelerate_delay_per_seconds type=number value=0></div></div><div class=entire style=display:none><h2>Accelerate Timeout</h2>Accelerate timeout x% every x sec</div><div class=cont style=display:none><div class=half>% <input id=accelerate_timeout_percent type=number value=0></div><div class=half>Seconds <input id=accelerate_timeout_per_seconds type=number value=0></div></div><div class=entire><h2>Detection Range</h2>Detect object between min and max (cm)</div><div class=cont><div class=half>Minimum <input id=min_detection_range type=number value=0></div><div class=half>Maximum <input id=max_detection_range type=number value=50></div></div><div class=entire><a class="btn btn-block btn-lg btn-success"href=javascript:sendConfig();>Configure</a></div><br><br><hr><br><br><div class=entire><a class="btn btn-block btn-lg btn-success"href=javascript:listSensors();>List Sensors</a><h1>Connected Sensors:</h1><div class=sensors id=sensors></div></div><br><br><hr><br><br><div class=cont style=display:none><div class=third><center><input id=restartESP type=checkbox value=0><label class=slabel for=restartESP></label></center></div><div class=twothird><a class="btn btn-block btn-lg btn-danger"href=javascript:restartESP();>Restart System</a></div></div>
)rawliteral";

// ========== Configuration Structure for EEPROM ==========
struct SystemConfig {
    uint16_t magic;          // MAGIC_NUMBER to verify valid data
    char mode[10];           // "random" or "sequence"
    int min_delay;
    int max_delay;
    int min_timeout;
    int max_timeout;
    int min_detection_range;
    int max_detection_range;
    int accelerate_delay_percent;
    int accelerate_delay_per_seconds;
    int accelerate_timeout_percent;
    int accelerate_timeout_per_seconds;
    uint16_t checksum;       // Simple validation
};

// ========== Configuration Variables ==========
String mode = "random";              // random or sequence
int min_delay = 0;                    // minimum delay before stimulus (ms)
int max_delay = 0;                    // maximum delay before stimulus (ms)
int min_timeout = 1000;                // minimum timeout to respond (ms)
int max_timeout = 1000;                // maximum timeout to respond (ms)
int min_detection_range = 0;           // minimum detection distance (cm)
int max_detection_range = 50;          // maximum detection distance (cm)

// Unused acceleration features (kept for HTML compatibility)
int accelerate_delay_percent = 0;
int accelerate_delay_per_seconds = 0;
int accelerate_timeout_percent = 0;
int accelerate_timeout_per_seconds = 0;

// Current stimulus values
int timeout = 1000;                    // actual timeout for current stimulus
int tdelay = 0;                        // actual delay for current stimulus

// ========== System State ==========
bool isTesting = false;                // test in progress?
int currentSensor = 0;                  // index of currently selected sensor
bool stimulating = false;               // is a stimulus active?
int lastSensor = 1000;                  // last sensor that received stimulus
uint8_t appConnected = 0;               // WebSocket number of connected phone (0 = none)
time_t app_time = 0;                    // phone's timestamp

// ========== Statistics ==========
int test_score = 0;                     // successful hits
int test_errors = 0;                     // errors/timeouts
int max_distance = 0;                    // max detection distance
int min_distance = 9999;                  // min detection distance
int avg_distance = 0;                     // average distance
int max_response_time = 0;                 // max reaction time
int min_response_time = 9999;              // min reaction time
int avg_response_time = 0;                 // average reaction time
int test_count = 0;                        // number of stimuli sent

// ========== Sensor Data Structure (FIXED) ==========
class Sensor {
  public:
    IPAddress ip;           // IP address of the sensor
    bool isEnabled;         // is sensor active?
    uint8_t num;            // WebSocket client number
    int timeoutCount;       // NEW: track consecutive timeouts
    unsigned long lastSeen; // NEW: last communication timestamp
    bool markedForRemoval;  // NEW: marked for cleanup
    
    Sensor() {
        timeoutCount = 0;
        lastSeen = millis();
        markedForRemoval = false;
        isEnabled = true;
    }
};

LinkedList<Sensor*> sensorList = LinkedList<Sensor*>();

// ========== Task Scheduler ==========
Scheduler ts;
void StimulusTimeout();
void CleanupSensors();  // NEW: periodic cleanup task

// Tasks
Task tStimulusTimeout(2000, TASK_ONCE, &StimulusTimeout, &ts, false);
Task tCleanupSensors(30000, TASK_FOREVER, &CleanupSensors, &ts, true);  // NEW: run every 30s

// ========== Web Server ==========
ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ========== WiFi Configuration ==========
const char* apName = "TrainerLights";
const char* apPassword = "1234567890";

// WiFi event handler
WiFiEventHandler stationDisconnectedHandler;
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) {
    Serial.println("** Station disconnected **");
    digitalWrite(STATUS_LED, HIGH);
    delay(50);
    digitalWrite(STATUS_LED, LOW);
}

// ========== EEPROM Functions ==========
uint16_t calculateChecksum(SystemConfig &cfg) {
    uint16_t sum = 0;
    uint8_t* ptr = (uint8_t*)&cfg;
    // Skip magic and checksum fields
    for(int i = sizeof(cfg.magic); i < sizeof(SystemConfig) - sizeof(cfg.checksum); i++) {
        sum += ptr[i];
    }
    return sum;
}

void loadConfig() {
    EEPROM.begin(EEPROM_SIZE);
    SystemConfig cfg;
    EEPROM.get(CONFIG_ADDR, cfg);
    
    // Verify magic number and checksum
    if (cfg.magic == MAGIC_NUMBER && cfg.checksum == calculateChecksum(cfg)) {
        mode = String(cfg.mode);
        min_delay = cfg.min_delay;
        max_delay = cfg.max_delay;
        min_timeout = cfg.min_timeout;
        max_timeout = cfg.max_timeout;
        min_detection_range = cfg.min_detection_range;
        max_detection_range = cfg.max_detection_range;
        accelerate_delay_percent = cfg.accelerate_delay_percent;
        accelerate_delay_per_seconds = cfg.accelerate_delay_per_seconds;
        accelerate_timeout_percent = cfg.accelerate_timeout_percent;
        accelerate_timeout_per_seconds = cfg.accelerate_timeout_per_seconds;
        
        Serial.println("✅ Configuration loaded from EEPROM");
        Serial.printf("Mode: %s, Delay: %d-%d, Timeout: %d-%d, Range: %d-%d\n",
                     mode.c_str(), min_delay, max_delay, 
                     min_timeout, max_timeout,
                     min_detection_range, max_detection_range);
    } else {
        Serial.println("⚠️ No valid config found, using defaults");
    }
    EEPROM.end();
}

void saveConfig() {
    SystemConfig cfg;
    cfg.magic = MAGIC_NUMBER;
    
    // Copy string with bounds checking
    strncpy(cfg.mode, mode.c_str(), sizeof(cfg.mode) - 1);
    cfg.mode[sizeof(cfg.mode) - 1] = '\0';
    
    cfg.min_delay = min_delay;
    cfg.max_delay = max_delay;
    cfg.min_timeout = min_timeout;
    cfg.max_timeout = max_timeout;
    cfg.min_detection_range = min_detection_range;
    cfg.max_detection_range = max_detection_range;
    cfg.accelerate_delay_percent = accelerate_delay_percent;
    cfg.accelerate_delay_per_seconds = accelerate_delay_per_seconds;
    cfg.accelerate_timeout_percent = accelerate_timeout_percent;
    cfg.accelerate_timeout_per_seconds = accelerate_timeout_per_seconds;
    
    cfg.checksum = calculateChecksum(cfg);
    
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(CONFIG_ADDR, cfg);
    EEPROM.commit();
    EEPROM.end();
    
    Serial.println("💾 Configuration saved to EEPROM");
    
    // Blink LED to confirm save
    for(int i=0; i<3; i++) {
        digitalWrite(STATUS_LED, HIGH);
        delay(100);
        digitalWrite(STATUS_LED, LOW);
        delay(100);
    }
}

// ========== Sensor Cleanup Function (FIXES the bug) ==========
void CleanupSensors() {
    unsigned long now = millis();
    Serial.printf("🧹 Running sensor cleanup. Current sensors: %d\n", sensorList.size());
    
    for(int i = 0; i < sensorList.size(); i++) {
        Sensor *s = sensorList.get(i);
        
        // Check if sensor hasn't been seen for 60 seconds
        if (now - s->lastSeen > 60000) {
            Serial.printf("❌ Removing dead sensor %d.%d.%d.%d (no signal for 60s)\n",
                         s->ip[0], s->ip[1], s->ip[2], s->ip[3]);
            delete s;
            sensorList.remove(i);
            i--;  // Adjust index after removal
        }
        // Check if sensor has too many timeouts
        else if (s->timeoutCount >= 5) {
            Serial.printf("⚠️ Removing problematic sensor %d.%d.%d.%d (%d timeouts)\n",
                         s->ip[0], s->ip[1], s->ip[2], s->ip[3], s->timeoutCount);
            delete s;
            sensorList.remove(i);
            i--;
        }
    }
    
    Serial.printf("🧹 Cleanup complete. Sensors: %d\n", sensorList.size());
}

// ========== WebSocket Event Handler ==========
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    IPAddress ip = webSocket.remoteIP(num);
    
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            // Mark sensor as disconnected (will be cleaned up by timer)
            for(int i = 0; i < sensorList.size(); i++) {
                Sensor *s = sensorList.get(i);
                if (s->num == num) {
                    s->lastSeen = 0;  // Mark as dead
                    break;
                }
            }
            break;
            
        case WStype_CONNECTED: 
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", 
                         num, ip[0], ip[1], ip[2], ip[3], payload);
            webSocket.sendTXT(num, "{\"status\":\"connected\"}");
            digitalWrite(STATUS_LED, HIGH);
            delay(100);
            digitalWrite(STATUS_LED, LOW);
            break;
        
        case WStype_TEXT:
            Serial.printf("[%u] Received: %s\n", num, payload);

            // Parse JSON
            StaticJsonDocument<512> root;
            auto error = deserializeJson(root, payload);
            
            if (error) {
                Serial.print("JSON parsing failed: ");
                Serial.println(error.c_str());
                break;
            }
            
            const char* jtype = root["type"];
            Serial.print("Message type: ");
            Serial.println(jtype);
            
            Sensor *s;
            int i;
            String a = "";
            
            // ===== List Sensors =====
            if (String(jtype) == String("list_sensors")) {
                Serial.println("Listing sensors");
                a = "{\"type\":\"sensor_list\",\"sensors\":[";
                for (i = 0; i < sensorList.size(); i++) {
                    s = sensorList.get(i);
                    if (i) a += ",";
                    a += "{\"id\":\"" + String(s->ip[0]) + String(s->ip[1]) + 
                         String(s->ip[2]) + String(s->ip[3]) + "\"";
                    a += ",\"ip\":\"" + String(s->ip[0]) + "." + String(s->ip[1]) + 
                         "." + String(s->ip[2]) + "." + String(s->ip[3]) + "\"";
                    a += ",\"num\":\"" + String(s->num) + "\"";
                    a += ",\"timeouts\":\"" + String(s->timeoutCount) + "\"";
                    a += ",\"state\":\"on\"}";
                }
                a += "]}";
                webSocket.sendTXT(num, a);
            }
            
            // ===== Sensor Registration =====
            if (String(jtype) == String("sensor")) {
                Serial.println("New sensor registered");
                
                // Check if sensor already exists
                bool sensorExists = false;
                for(i = 0; i < sensorList.size(); i++) {
                    s = sensorList.get(i);
                    if (s->ip == ip) {
                        // Update existing sensor
                        sensorExists = true;
                        s->num = num;  // Update WebSocket number
                        s->lastSeen = millis();
                        s->timeoutCount = 0;  // Reset timeout count
                        s->markedForRemoval = false;
                        Serial.printf("Sensor %d.%d.%d.%d reconnected\n",
                                     ip[0], ip[1], ip[2], ip[3]);
                        break;
                    }
                }
                
                if (!sensorExists) {
                    Sensor *newSensor = new Sensor();
                    newSensor->ip = ip;
                    newSensor->num = num;
                    newSensor->lastSeen = millis();
                    newSensor->timeoutCount = 0;
                    
                    sensorList.add(newSensor);
                    Serial.printf("✨ New sensor added. Total sensors: %d\n", sensorList.size());
                }
            }

            // ===== Sensor Response =====
            if (String(jtype) == String("response")) {
                Serial.println("Sensor response received");
                
                // Update sensor's last seen time
                for(i = 0; i < sensorList.size(); i++) {
                    s = sensorList.get(i);
                    if (s->num == num) {
                        s->lastSeen = millis();
                        s->timeoutCount = 0;  // Reset timeout count on successful response
                        break;
                    }
                }
                
                tStimulusTimeout.disable();
                stimulating = false;
                
                if (isTesting) {
                    int response_error = root["error"];
                    
                    if (response_error) {
                        test_errors++;
                    } else {
                        test_score++;
                        
                        int response_time = int(root["time"]);
                        int distance = int(root["distance"]);
                        
                        if (test_count == 0) {
                            // First response - initialize all values
                            max_distance = distance;
                            min_distance = distance;
                            avg_distance = distance;
                            max_response_time = response_time;
                            min_response_time = response_time;
                            avg_response_time = response_time;
                        } else {
                            // Update averages (proper cumulative average)
                            avg_distance = (avg_distance * test_count + distance) / (test_count + 1);
                            avg_response_time = (avg_response_time * test_count + response_time) / (test_count + 1);
                            
                            // Update min/max
                            if (response_time > max_response_time) max_response_time = response_time;
                            if (response_time < min_response_time) min_response_time = response_time;
                            if (distance > max_distance) max_distance = distance;
                            if (distance < min_distance) min_distance = distance;
                        }
                        
                        test_count++;
                    }

                    // Send stats to phone if connected
                    if (appConnected != 0) {
                        String stats;
                        stats = "{\"type\":\"stats\"";
                        stats += ",\"test_score\":\"" + String(test_score) + "\"";
                        stats += ",\"test_errors\":\"" + String(test_errors) + "\"";
                        stats += ",\"max_distance\":\"" + String(max_distance) + "\"";
                        stats += ",\"min_distance\":\"" + String(min_distance) + "\"";
                        stats += ",\"avg_distance\":\"" + String(avg_distance) + "\"";
                        stats += ",\"max_response_time\":\"" + String(max_response_time) + "\"";
                        stats += ",\"min_response_time\":\"" + String(min_response_time) + "\"";
                        stats += ",\"avg_response_time\":\"" + String(avg_response_time) + "\"";
                        stats += "}";
                        webSocket.sendTXT(appConnected, stats);
                    }
                }
            }

            // ===== Start Test =====
            if (String(jtype) == String("start_test")) {
                Serial.println("Starting test");
                isTesting = true;
                // Reset statistics
                test_score = 0;
                test_errors = 0;
                max_distance = 0;
                min_distance = 9999;
                avg_distance = 0;
                max_response_time = 0;
                min_response_time = 9999;
                avg_response_time = 0;
                test_count = 0;
            }
            
            // ===== Stop Test =====
            if (String(jtype) == String("stop_test")) {
                Serial.println("Stopping test");
                isTesting = false;
            }
                        
            // ===== App Connected =====
            if (String(jtype) == String("app_connected")) {
                appConnected = num;
                app_time = root["current_time"];
                Serial.printf("Phone connected on socket %u\n", appConnected);
            }

            // ===== Restart System =====
            if (String(jtype) == String("restart")) {
                Serial.println("Restart command received");
                // Tell all sensors to restart
                for (i = 0; i < sensorList.size(); i++) {
                    s = sensorList.get(i);
                    a = "{\"type\":\"restart\"}";
                    webSocket.sendTXT(s->num, a);
                }
                delay(100);
                ESP.restart();
            }

            // ===== Configuration Update =====
            if (String(jtype) == String("config")) {
                Serial.println("Updating configuration");
                
                const char* cmode = root["mode"];
                mode = String(cmode);
                min_delay = root["min_delay"];
                max_delay = root["max_delay"];
                min_timeout = root["min_timeout"];
                max_timeout = root["max_timeout"];
                
                // Store acceleration settings
                accelerate_delay_percent = root["accelerate_delay_percent"];
                accelerate_delay_per_seconds = root["accelerate_delay_per_seconds"];
                accelerate_timeout_percent = root["accelerate_timeout_percent"];
                accelerate_timeout_per_seconds = root["accelerate_timeout_per_seconds"];
                
                min_detection_range = root["min_detection_range"];
                max_detection_range = root["max_detection_range"];
                
                // Save to EEPROM
                saveConfig();
                
                Serial.println("Configuration updated and saved");
            }
            break;
    }
}

// ========== Stimulus Timeout Handler (FIXED) ==========
void StimulusTimeout() {
    Serial.println("⏰ Stimulus timeout occurred");
    
    if (lastSensor < sensorList.size()) {
        Sensor *s = sensorList.get(lastSensor);
        if (s) {
            s->timeoutCount++;
            s->lastSeen = millis();  // Update last seen (they're still connected, just didn't respond)
            
            Serial.printf("⚠️ Sensor %d.%d.%d.%d timeout #%d\n",
                         s->ip[0], s->ip[1], s->ip[2], s->ip[3], s->timeoutCount);
            
            // Only remove after 5 consecutive timeouts
            if (s->timeoutCount >= 5) {
                Serial.printf("❌ Removing sensor after %d timeouts\n", s->timeoutCount);
                delete s;
                sensorList.remove(lastSensor);
            } else {
                Serial.println("Sensor kept - will retry");
            }
        }
    }
    
    lastSensor = 1000;
    stimulating = false;
}

// ========== Web Server Root Handler ==========
void handleRoot() {
    webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    webServer.sendHeader("Pragma", "no-cache");
    webServer.sendHeader("Expires", "-1");
    
    webServer.setContentLength(strlen_P(htmlHeader) + 
                               strlen_P(htmlCss) + 
                               strlen_P(htmlContent) + 
                               strlen_P(htmlFooter));
    
    webServer.send(200, "text/html", "");
    webServer.sendContent_P(htmlHeader);
    webServer.sendContent_P(htmlCss);
    webServer.sendContent_P(htmlContent);
    webServer.sendContent_P(htmlFooter);
    webServer.client().stop();
}

// ========== Setup ==========
void setup() {
    delay(100);
    
    // Initialize status LED
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);
    
    Serial.begin(115200);
    Serial.setDebugOutput(false);
    
    Serial.println("\n\n\n*****************************");
    Serial.println("*                           *");
    Serial.println("*      TrainerLights        *");
    Serial.println("*         Server            *");
    Serial.println("*    By: khader afeez      *");
    Serial.println("*  khaderafeez16@gmail.com  *");
    Serial.println("*****************************\n");

    // Load configuration from EEPROM
    loadConfig();

    // Increase max WiFi connections
    struct softap_config config;
    wifi_softap_get_config(&config);
    config.max_connection = 32;
    wifi_softap_set_config(&config);

    // Disable WiFi sleep for faster response
    wifi_set_sleep_type(NONE_SLEEP_T);

    // Start Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName, apPassword);
    
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // Start WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    // Start MDNS
    if (MDNS.begin("trainerlights")) {
        Serial.println("MDNS responder started at trainerlights.local");
    }
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);

    // Start web server
    webServer.on("/", handleRoot);
    webServer.begin();
    Serial.println("HTTP server started on port 80");

    // Monitor disconnections
    stationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(
        &onStationDisconnected);
    
    Serial.println("Setup complete. Ready for connections.\n");
    
    // Blink to show ready
    for(int i=0; i<3; i++) {
        digitalWrite(STATUS_LED, HIGH);
        delay(100);
        digitalWrite(STATUS_LED, LOW);
        delay(100);
    }
}

// ========== Main Loop ==========
void loop() {
    webSocket.loop();
    webServer.handleClient();
    ts.execute();

    // Generate stimuli if not currently stimulating
    if (!stimulating && isTesting && sensorList.size() > 0) {
        
        // Select sensor based on mode
        if (mode == "random") {
            currentSensor = random(0, sensorList.size());
        } else {  // sequential
            currentSensor++;
            if (currentSensor >= sensorList.size()) {
                currentSensor = 0;
            }
        }

        // Generate random delay and timeout from configured ranges
        timeout = random(min_timeout, max_timeout + 1);
        tdelay = random(min_delay, max_delay + 1);

        // Safety limits
        if (timeout < 100) timeout = 100;
        if (tdelay < 0) tdelay = 0;
        
        // Send stimulus if it's a different sensor than last time
        if (currentSensor != lastSensor) {
            lastSensor = currentSensor;
            Sensor *s = sensorList.get(currentSensor);
            
            if (s && !s->markedForRemoval) {
                String cmd = "{\"type\":\"stimulus\"";
                cmd += ",\"timeout\":\"" + String(timeout) + "\"";
                cmd += ",\"delay\":\"" + String(tdelay) + "\"";
                cmd += ",\"min_detection_range\":\"" + String(min_detection_range) + "\"";
                cmd += ",\"max_detection_range\":\"" + String(max_detection_range) + "\"";
                cmd += ",\"light\":{\"mode\":\"on\",\"intensity\":\"100\","
                       "\"color\":{\"R\":\"255\",\"G\":\"255\",\"B\":\"255\"}}}";
                
                webSocket.sendTXT(s->num, cmd);
                
                // Set timeout for this stimulus
                tStimulusTimeout.setInterval(tdelay + timeout + 1000);
                tStimulusTimeout.restartDelayed();
                
                stimulating = true;
                
                Serial.printf("⚡ Stimulus sent to sensor %d\n", currentSensor);
            }
        }
    }
}