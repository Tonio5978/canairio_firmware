#include <wifi.hpp>

int rssi = 0;
String hostId = "";
float humi, temp;


void selectTempAndHumidity() {
    humi = sensors.getHumidity();
    if(humi == 0.0) humi = sensors.getCO2humi();
    temp = sensors.getTemperature();
    if(temp == 0.0) temp = sensors.getCO2temp();
}

/******************************************************************************
*   H A S S   M E T H O D S
******************************************************************************/

HAMqttDevice hassSensor("CanAirIO Device", HAMqttDevice::SENSOR);
HAMqttDevice anaireSensor("CanAirIO Device", HAMqttDevice::SENSOR);

EspMQTTClient hassMQTT;
EspMQTTClient anaireMQTT(
  "mqtt.anaire.org",
  80,
  "", 
  "",
  "AnaireMQTTClient"
);

void onConnectionEstablished() {
    hassMQTT.subscribe(hassSensor.getCommandTopic(), [](const String& payload) {
        if (payload.equals("ON"))
            Serial.printf("-->[MQTT] Hass command: %d\n",true);
        else if (payload.equals("OFF"))
            Serial.printf("-->[MQTT] Hass command %d\n",false);

        hassMQTT.publish(hassSensor.getStateTopic(), payload);
        // valueChangedMillis = millis();
    });

    hassMQTT.subscribe("#/status", [](const String& payload) {
        if (payload.equals("online")) {
            Serial.println("-->[MQTT] Hass is online");
            hassMQTT.publish(hassSensor.getConfigTopic(), hassSensor.getConfigPayload());

            // hassMQTT.executeDelayed(send_states_delay, []() {
            //     client.publish(dimmableLight.getStateTopic(), lightValues.lightOn ? "ON" : "OFF");
            //     client.publish(dimmableLight.getTopic() + "/br/state", String(lightValues.brightness));

            //     dimmableLight
            //         .clearAttributes()
            //         .addAttribute("IP", WiFi.localIP().toString());
            //     client.publish(dimmableLight.getAttributesTopic(), dimmableLight.getAttributesPayload());
            // });
        }
    });
}

void onAnaireConnectionEstablished() {
    if (cfg.devmode) {
        Serial.printf("-->[MQTT] Anaire connected to %s\n", anaireMQTT.getMqttServerIp());
        Serial.printf("-->[MQTT] Anaire deviceId: %s\n", cfg.getStationName().c_str());
    }
    // subscription for see all Anaire devices:
    // anaireMQTT.subscribe("measurement", [](const String& payload) {
    //     Serial.printf("-->[MQTT] Anaire measurement: %s\n", payload.c_str());
    // });
}

void anaireMqttPublish() {
    char MQTT_message[256];

    int deviceType = sensors.getPmDeviceTypeSelected();
    selectTempAndHumidity();

    if (deviceType <= 3) {
        sprintf(MQTT_message, "{id: %s,pm1: %d,pm25: %d, pm10: %d, tmp: %f, hum: %f, geo: %s}",
                cfg.getStationName().c_str(),
                sensors.getPM1(),
                sensors.getPM25(),
                sensors.getPM10(),
                temp,
                humi,
                cfg.geo.c_str());
    } else {
        sprintf(MQTT_message, "{id: %s,CO2: %d,humidity: %f,temperature: %f,VBat: %f}",
                cfg.getStationName().c_str(),
                sensors.getCO2(),
                humi,
                temp,
                0.0);
    }

    anaireMQTT.publish("measurement", MQTT_message);
}

void mqttPublish() {
    anaireMqttPublish();
}

bool isHassEnabled() {
    return !cfg.hassip.isEmpty();
}

void hassInit() {
    if(!isHassEnabled()) return;
    hassSensor
        .enableAttributesTopic()
        .addConfigVar("bri_stat_t", "~/br/state")
        .addConfigVar("bri_cmd_t", "~/br/cmd")
        .addConfigVar("bri_scl", "100");

    if(cfg.devmode) hassMQTT.enableDebuggingMessages();
    hassMQTT.setMqttClientName(cfg.getDeviceId().c_str());
    hassMQTT.setMqttServer(cfg.hassip.c_str(), cfg.hassusr.c_str(), cfg.hasspsw.c_str(), cfg.hasspt);
    hassMQTT.setOnConnectionEstablishedCallback(onConnectionEstablished);
    hassMQTT.setKeepAlive(60);
    if(cfg.devmode) Serial.printf("-->[MQTT] connecting to %s\n", hassMQTT.getMqttServerIp());
}

void anaireInit() { 
    if(cfg.devmode) anaireMQTT.enableDebuggingMessages();
    anaireMQTT.setOnConnectionEstablishedCallback(onAnaireConnectionEstablished);
    anaireMQTT.setKeepAlive(120);
    anaireMQTT.setMaxPacketSize(512);
    if(cfg.devmode) Serial.printf("-->[MQTT] connecting to %s\n", anaireMQTT.getMqttServerIp());
}

void mqttInit() {
    Serial.println("-->[MQTT] mqttInit");
    hassInit();
    anaireInit();
}

void mqttLoop () {
    if(!WiFi.isConnected()) return; 
    static uint_fast64_t mqttTimeStamp = 0;
    if (millis() - mqttTimeStamp > cfg.stime * 1000) {
        mqttTimeStamp = millis();
        mqttPublish();
    }
    hassMQTT.loop();
    anaireMQTT.loop();
}

/******************************************************************************
*   I N F L U X D B   M E T H O D S
******************************************************************************/

InfluxDBClient influx;
Point sensor ("fixed_stations_01");
bool ifx_ready;

bool influxDbIsConfigured() {
    if(cfg.ifx.db.length() > 0 && cfg.ifx.ip.length() > 0 && cfg.geo.length()==0) {
        Serial.println("-->[W][IFDB] ifxdb is configured but Location (GeoHash) is missing!");
    }
    return cfg.ifx.db.length() > 0 && cfg.ifx.ip.length() > 0 && cfg.geo.length() > 0;
}

void influxDbAddTags() {
    sensor.addTag("mac",cfg.deviceId.c_str());
    sensor.addTag("geo3",cfg.geo.substring(0,3).c_str());
    sensor.addTag("name",cfg.getStationName().c_str());
}

void influxDbInit() {
    if (!ifx_ready && WiFi.isConnected() && cfg.isIfxEnable() && influxDbIsConfigured()) {
        String url = "http://"+cfg.ifx.ip+":"+String(cfg.ifx.pt);
        influx.setInsecure();
        // influx = InfluxDBClient(url.c_str(),cfg.ifx.db.c_str());
        influx.setConnectionParamsV1(url.c_str(),cfg.ifx.db.c_str());
        if(cfg.devmode) Serial.printf("-->[IFDB] config: %s@%s:%i\n",cfg.ifx.db.c_str(),cfg.ifx.ip.c_str(),cfg.ifx.pt);
        influxDbAddTags();
        if(influx.validateConnection()) {
            Serial.printf("-->[IFDB] connected to %s\n",influx.getServerUrl().c_str());
            ifx_ready = true;
        }
        else Serial.println("-->[E][IFDB] connection error!");
        delay(100);
    }
}

/**
 * @influxDbParseFields:
 *
 */
void influxDbParseFields() {
    sensor.clearFields();
    selectTempAndHumidity();
    sensor.addField("pm1",sensors.getPM1());
    sensor.addField("pm25",sensors.getPM25());
    sensor.addField("pm10",sensors.getPM10());
    sensor.addField("co2",sensors.getCO2());
    sensor.addField("co2hum",sensors.getCO2humi());
    sensor.addField("co2tmp",sensors.getCO2temp());
    sensor.addField("tmp",temp);
    sensor.addField("hum",humi);
    sensor.addField("geo",cfg.geo.c_str());
    sensor.addField("prs",sensors.getPressure());
    sensor.addField("gas",sensors.getGas());
    sensor.addField("alt",sensors.getAltitude());
    sensor.addField("name",cfg.getStationName().c_str());
}

bool influxDbWrite() {
    influxDbParseFields();
    log_d("[IFDB] %s",influx.pointToLineProtocol(sensor).c_str());
    if (!influx.writePoint(sensor)) {
        Serial.print("-->[E][IFDB] Write Point failed: ");
        Serial.println(influx.getLastErrorMessage());
        return false;
    }
    return true;
}

void influxDbLoop() {
    static uint_fast64_t timeStamp = 0;
    if (millis() - timeStamp > cfg.stime * 2 * 1000) {
        timeStamp = millis();
        if (ifx_ready && sensors.isDataReady() && WiFi.isConnected() && cfg.isIfxEnable()) {
            if (influxDbWrite()){
                if(cfg.devmode) Serial.println("-->[IFDB] write done.");
                gui.displayDataOnIcon();
            }
            else
                Serial.printf("-->[E][IFDB] write error to %s@%s:%i \n",cfg.ifx.db.c_str(),cfg.ifx.ip.c_str(),cfg.ifx.pt);
        }
    }  
}

/******************************************************************************
*   W I F I   M E T H O D S
******************************************************************************/


class MyOTAHandlerCallbacks : public OTAHandlerCallbacks {
    void onStart() {
        gui.showWelcome();
    };
    void onProgress(unsigned int progress, unsigned int total) {
        gui.showProgress(progress, total);
    };
    void onEnd() {
        gui.showWelcome();
        gui.welcomeAddMessage("");
        gui.welcomeAddMessage("success!");
        delay(2000);
        gui.welcomeAddMessage("rebooting..");
        delay(3000);
    }
    void onError() {
        gui.showWelcome();
        gui.welcomeAddMessage("");
        gui.welcomeAddMessage("!OTA Error!");
        gui.welcomeAddMessage("!Please try again!");
        delay(5000);
        gui.showWelcome();
        gui.showMain();
    }
};

void otaLoop() {
    if (WiFi.isConnected()) {
        wd.pause();
        ota.loop();
        wd.resume();
    }
}

void onUpdateMessage(const char *msg){
    gui.suspendTaskGUI();
    gui.showWelcome();
    gui.welcomeAddMessage("");
    gui.welcomeAddMessage("Updating to:");
    gui.welcomeAddMessage(msg);
    gui.welcomeAddMessage("please wait..");
}

void otaInit() {
    hostId = "CanAirIO"+cfg.getDeviceIdShort();
    ota.setup(hostId.c_str(), "CanAirIO");
    gui.displayBottomLine(hostId);
    ota.setCallbacks(new MyOTAHandlerCallbacks());
    ota.setOnUpdateMessageCb(&onUpdateMessage);
}

void wifiConnect(const char* ssid, const char* pass) {
    Serial.print("-->[WIFI] connecting to ");
    Serial.print(ssid);
    int wifi_retry = 0;
    WiFi.begin(ssid, pass);
    while (!WiFi.isConnected() && wifi_retry++ <= WIFI_RETRY_CONNECTION) {
        Serial.print(".");
        delay(200);  // increment this delay on possible reconnect issues
    }
    delay(500);
    if (WiFi.isConnected()) {
        cfg.isNewWifi = false;  // flag for config via BLE
        Serial.println(" done.");
        Serial.print("-->[WIFI] IP: ");
        Serial.println(WiFi.localIP());
        Serial.println("-->[WIFI] publish interval: "+String(cfg.stime * 2)+" sec.");
        wd.pause();
        otaInit();
        ota.checkRemoteOTA();
        mqttInit();
        wd.resume();
    } else {
        Serial.println("fail!\n-->[E][WIFI] disconnected!");
    }
}

void wifiInit() {
    if (cfg.isWifiEnable() && cfg.ssid.length() > 0 ){
        wifiConnect(cfg.ssid.c_str(), cfg.pass.c_str());
    }
}

void wifiStop() {
    if (WiFi.isConnected()) {
        Serial.println("-->[WIFI] Disconnecting..");
        WiFi.disconnect(true);
        delay(100);
    }
}

void wifiRestart() {
    wifiStop();
    wifiInit();
}

void wifiLoop() {
    static uint_least64_t wifiTimeStamp = 0;
    if (millis() - wifiTimeStamp > 5000) {
        wifiTimeStamp = millis();
        if (cfg.isWifiEnable() && cfg.ssid.length() > 0 && !WiFi.isConnected()) {
            wifiInit();
        }
        influxDbInit();
        cfg.setWifiConnected(WiFi.isConnected());
    }
    mqttLoop();
}

int getWifiRSSI() {
    if (WiFi.isConnected()) return WiFi.RSSI();
    else return 0;
}

String getDeviceInfo () {
    String info = String(FLAVOR) + "\n";
    info = info + "Rev" + String(REVISION) +" v" + String(VERSION) + "\n";
    info = info + sensors.getPmDeviceSelected() + "\n\n";

    info = info + "Host: " + hostId + "\n";
    info = info + "(" + WiFi.localIP().toString() + ")\n";
    info = info + "OTA: " + String(TARGET) + " channel\n\n";
    info = info + "Fixed station:\n";
    info = info + cfg.getStationName();
    return info;
}
