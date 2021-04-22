/***
 * TESTING U8G2Lib
 * @hpsaturn 
 ***/

#include <Arduino.h>
#include <GUILib.hpp>

bool toggle;

void testSensorLiveIcon() {
    gui.displaySensorLiveIcon();
}

void testSendDataIcon() {
    gui.displayDataOnIcon();
}

void testSavePrefIcon() {
    gui.displayPreferenceSaveIcon();
}

void (*functionPtr[])() = {
    testSensorLiveIcon,
    testSendDataIcon,
    testSavePrefIcon
};

void testExtraWelcomeLines() {
    gui.welcomeAddMessage("InfluxDB test1..");  // test for multipage
    gui.welcomeAddMessage("InfluxDB test2..");
    gui.welcomeAddMessage("InfluxDB test3..");
    gui.welcomeAddMessage("InfluxDB test4..");
    gui.welcomeAddMessage("InfluxDB test5..");
    gui.welcomeAddMessage("Line test welcome 1");
    gui.welcomeAddMessage("Line test welcome 2");
    gui.welcomeAddMessage("Line test welcome 3");
    gui.welcomeAddMessage("Line test welcome 4");
}

void setup(void) {
    Serial.begin(115200);
    Serial.println("\n== INIT SETUP ==\n");
    Serial.println("-->[SETUP] console ready");
    gui.displayInit();
    gui.showWelcome();
    gui.welcomeAddMessage("Sensor ready..");
    gui.welcomeAddMessage("GATT server..");
    gui.welcomeAddMessage("WiFi test..");
    // testExtraWelcomeLines();
    gui.welcomeAddMessage("==SETUP READY==");

    randomSeed(A0);

    delay(500);
    gui.showMain();
}

bool getBoolean() {
    return random(0, 2) == 1 ? true : false;
}

uint64_t count = 20;

void loop(void) {

    // gui.pageStart();

    if (count++ % 40 == 0 ) gui.displaySensorAverage(random(0, 250), 1);

    // gui.displaySensorAverage(1200, 4);

    if (count % 50 == 0) gui.displaySensorData(120, 230,random(0, 99), random(0, 800)/25.0, 65, 1);

    gui.checkButtons();

    // gui.displayStatus(getBoolean(), true, getBoolean());

    // functionPtr[rnd]();  // Call a test function in random sequence

    // gui.pageEnd();

    delay(80);
}