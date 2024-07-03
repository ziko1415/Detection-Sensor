

// Dont forget to download the needed Libraries
#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"

//Write your Wifi credentials
const char *service_name = "Home";
const char *pop = "123456789";
static uint8_t WIFI_LED   = 2;
static uint8_t RESET_PIN  = 0;
static uint8_t PIR_PIN    = 4;
static uint8_t BUZZER_PIN = 21;

boolean PIR_STATE_NEW     = LOW;  // current state
boolean PIR_STATE_OLD     = LOW;  // previous state

boolean BUZZER_STATE        = false;
unsigned long buzzer_timer  = 0;

char device1[] = "SecuritySwitch";

static Switch SecuritySwitch(device1, NULL);

bool SECURITY_STATE = false;

uint32_t chipId = 0;

void sysProvEvent(arduino_event_t *sys_event)
{
    switch (sys_event->event_id) {      
        case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32
        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
        printQR(service_name, pop, "ble");
#else
        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
        printQR(service_name, pop, "softap");
#endif        
        break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.printf("\nConnected to Wi-Fi!\n");
        digitalWrite(WIFI_LED, HIGH);
        break;
    }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
  const char *device_name = device->getDeviceName();
  const char *param_name = param->getParamName();
  
  if(strcmp(device_name, device1) == 0) {
    
    Serial.printf("SecuritySwitch = %s\n", val.val.b? "true" : "false");
    
    if(strcmp(param_name, "Power") == 0) {
        Serial.printf("Received value = %s for %s - %s\n", 
                      val.val.b? "true" : "false", device_name, param_name);
        SECURITY_STATE = val.val.b;
        param->updateAndReport(val);
        //________________________________________________
        if(SECURITY_STATE == true){
         esp_rmaker_raise_alert("Security is ON");
        }
        else{
          esp_rmaker_raise_alert("Security is OFF");
        }
        
        
    }
  }
  
}


void setup(){

  Serial.begin(115200);
 
  pinMode(RESET_PIN, INPUT);
  pinMode(WIFI_LED, OUTPUT);
  digitalWrite(WIFI_LED, LOW);
 
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  //------------------------------------------------------------------------------
  Node my_node;    
  my_node = RMaker.initNode("Ahmad_Logs");
  //------------------------------------------------------------------------------
  //Standard switch device
  SecuritySwitch.addCb(write_callback);
  //------------------------------------------------------------------------------
  //Add switch device to the node   
  my_node.addDevice(SecuritySwitch);
  //------------------------------------------------------------------------------
  //This is optional 
  RMaker.enableOTA(OTA_USING_PARAMS);

  RMaker.enableTZService();
  RMaker.enableSchedule();
  //------------------------------------------------------------------------------
  //Service Name
  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  Serial.printf("\nChip ID:  %d Service Name: %s\n", chipId, service_name);
  //------------------------------------------------------------------------------
  Serial.printf("\nStarting ESP-RainMaker\n");
  RMaker.start();
  //------------------------------------------------------------------------------
  WiFi.onEvent(sysProvEvent);
  #if CONFIG_IDF_TARGET_ESP32
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name);
  #else
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
  #endif
  //------------------------------------------------------------------------------  
  SecuritySwitch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, false);
  //my_switch1.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, STATE_RELAY_1);
  
  //Serial.printf("Relay2 is %s \n", STATE_RELAY_2? "ON" : "OFF");
  //------------------------------------------------------------------------------
}


void loop()
{
  //------------------------------------------------------------------------------
  // Read GPIO0 (external button to reset device
  if(digitalRead(RESET_PIN) == LOW) { //Push button pressed
    Serial.printf("Reset Button Pressed!\n");
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while(digitalRead(RESET_PIN) == LOW) delay(50);
    int endTime = millis();
    //_______________________________________________________________________
    if ((endTime - startTime) > 10000) {
      // If key pressed for more than 10secs, reset all
      Serial.printf("Reset to factory.\n");
      RMakerFactoryReset(2);
    } 
    //_______________________________________________________________________
    else if ((endTime - startTime) > 3000) {
      Serial.printf("Reset Wi-Fi.\n");
      // If key pressed for more than 3secs, but less than 10, reset Wi-Fi
      RMakerWiFiReset(2);
    }
    //_______________________________________________________________________
  }
  //------------------------------------------------------------------------------
  delay(100);
  
  if (WiFi.status() != WL_CONNECTED){
    //Serial.println("WiFi Not Connected");
    digitalWrite(WIFI_LED, LOW);
  }
  else{
    //Serial.println("WiFi Connected");
    digitalWrite(WIFI_LED, HIGH);
  }
  //------------------------------------------------------------------------------
  detectMotion();
  controlBuzzer();
  //------------------------------------------------------------------------------
}


void detectMotion() {
  if(SECURITY_STATE == true) {
    PIR_STATE_OLD = PIR_STATE_NEW; // store old state
    PIR_STATE_NEW = digitalRead(PIR_PIN); //read new state
    //------------------------------------------------------------------------
    if(PIR_STATE_OLD == LOW && PIR_STATE_NEW == HIGH) {
      Serial.println("Motion detected!");
      esp_rmaker_raise_alert("Security Alert!\nMotion is detected.");
      digitalWrite(BUZZER_PIN, HIGH);
      BUZZER_STATE = true;
      buzzer_timer = millis();
    }
    //------------------------------------------------------------------------
  }
}

void controlBuzzer(){
  if (BUZZER_STATE == true) {
    if (millis() - buzzer_timer > 5000) {
      digitalWrite(BUZZER_PIN, LOW);
      BUZZER_STATE = false;
      buzzer_timer = 0;
    }
  }
}
