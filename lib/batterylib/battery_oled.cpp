#include <battery_oled.hpp>

#ifdef M5PICOD4
    #define ADC_PIN 36
    int channel_atten = ADC1_CHANNEL_6;
#elif TTGO_T7
    #define ADC_PIN 35
    int channel_atten = ADC1_CHANNEL_7;
#else
    #define ADC_PIN 34
    int channel_atten = 0;
#endif

float pavg = 0.0;

void Battery_OLED::setupBattADC() {
  // TODO: all here is deprecated we need review the documentation
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)channel_atten, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
  // Check type of calibration value used to characterize ADC
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("-->[BATT] ADC eFuse Vref:%u mV\r\n", adc_chars.vref);
    vref = adc_chars.vref;
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    Serial.printf("-->[BATT] ADC Two Point --> coeff_a:%umV coeff_b:%umV\r\n", adc_chars.coeff_a, adc_chars.coeff_b);
  } else {
    Serial.printf("-->[BATT] ADC Default Vref: %u mV\r\n", vref);
  }
}

void Battery_OLED::init(bool debug) {
  this->debug = debug;
  /*
    ADC_EN is the ADC detection enable port
    If the USB port is used for power supply, it is turned on by default.
    If it is powered by battery, it needs to be set to high level
    */
  pinMode(ADC_EN, OUTPUT);
  digitalWrite(ADC_EN, HIGH);
  delay(10);  // suggested by @ygator user in issue #2
  setupBattADC();
  delay(10);  // suggested by @ygator user in issue #2
}

float Battery_OLED::getVoltage() {
  return curv;
}

bool Battery_OLED::isCharging() {
  bool charging = false;
  if (isDischarging >= 0)
    charging = !isDischarging;
  else
    charging = curv > BATTERY_MAX_V + (BATTCHARG_MIN_V - BATTERY_MAX_V) / 2;
//   if (debug) Serial.printf("-->[BATT] Batt is charging\t: %s\r\n", charging ? "True" : "False");
  return charging;
}

void Battery_OLED::printValues() {
  if (!debug) return;
  Serial.printf("-->[BATT] Battery voltage  \t: %.3fv vref: %i Charge:%i\r\n", curv, vref, getCharge());  // Output voltage and current of Bat
}

void Battery_OLED::update() {
  digitalWrite(ADC_EN, HIGH);
  delay(10);  // suggested by @ygator user in issue #2
  uint16_t v = analogRead(ADC_PIN);
#ifdef TTGO_T7
  curv = ((float)v / 4095.0) * 7.58;
#else
  curv = ((float)v / 4095.0) * 15.83;
#endif
  if (!captureStage) digitalWrite(ADC_EN, LOW);  // for possible issue: https://github.com/Xinyuan-LilyGO/TTGO-T-Display/issues/6
}

int Battery_OLED::getCharge() {
  if (isCharging()) {
    return calcPercentage(curv, BATTCHARG_MAX_V, BATTCHARG_MIN_V);
  } else {
    return calcPercentage(curv, BATTERY_MAX_V, BATTERY_MIN_V);
  }
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_OLEDBATTERY)
    #ifndef M5STICKCPLUS
        #ifndef TTGO_TDISPLAY
        Battery_OLED battery;
        #endif
    #endif
#endif
