#include "pico/stdlib.h"
#include "stdio.h"
#include "pico/util/queue.h"
#include <config.h>
#include "hardware/i2c.h"
#include "MS5611.h"
#include "SPL06.h"
#include "BMP280.h"
#include <vario.h>
#include <voltage.h>
#include <gps.h>
#include <tusb.h>
#include <crsf_in.h>
#include <crsf_out.h>
#include <param.h>
#include "hardware/pio.h"
#include "sbus_out_pwm.h"
#include "sbus_in.h"
#include "hardware/watchdog.h"
#include "sport.h"
#include "jeti.h"
#include "param.h"
#include "tools.h"
#include "ws2812.h"
#include "rpm.h"

// to do : add current and rpm telemetry fields to jeti protocol
//         support ex bus jeti protocol on top of ex jeti protocol
//         support Frsky Fport on top of sbus+sport protocol  

// Look at file config.h for more details
//
// This project can be interfaced with 
//    - one or two receivers. 
//    - receivers can be ELRS, FRSKY or Jeti
// It can be used to:
//    - convert ELRS signal to SBUS
//    - generate up to 16 PWM signals (based on ELRS TX or on SBUS)
//    - provide telemetry data (Altitude, Vertical speed, RPM, up to 4 Voltages, GPS)
// When connected to 2 receivers:
//    - it provides receiver diversity
//    - PWM and SBUS are generated on the latest received frame (if one receiver fails to provide RC data)
//    - the 2 receivers must use the same protocol (ELRS/SPORT/JETI).
//    - only one (the primary) can transmit the telemetry data
// It is possible in some way to define which pins are used and for what function
// PWM signals are generated on:
//     - pins GPIO0 up to GPIO15 (without use of a pio)
//  When interfaced with 2 receivers, 


// So pio 0 sm0 is used for CRSF Tx  or for Sport TX or JETI TX
//        0   1                         for Sport Rx
//        0   2            sbus out             

//        1   0 is used for gps Tx  (was used for one pwm)        
//        1   1 is used for gps Rx  (was used for one pwm)
//        1   3 is used for RGB led
// So UART0 is used for Secondary crsf of Sbus in ( was GPS before)
//    UART1 is used for primary crsf in of SBUS IN (was only Sbus in before)
 
// Pin that can be used are:
// C1 = 0/15  ... C16 = 0/15
// GPS_TX = 0/29
// GPS_RX = 0/29
// PRI = 5 ,9, 21 ,25  (UART1)
// SEC = 1, 13 , 17 ,29 (UART0) 
// SBUS_OUT = 0/29
// TLM = 0/29
// VOLT1= 26/29 ... VOLT4 = 26/29
// SDA = 2, 6, 10, 14, 18, 22, 26  (I2C1)
// SCL = 3, 7, 11, 15, 19, 23, 27  (I2C1)
// RPM = 0/29 
// LED = 16


//#define DEBUG  // force the MCU to wait for some time for the USB connection; still continue if not connected

VOLTAGE voltage ;    // class to handle voltages

MS5611 baro1( (uint8_t) 0x77  );    // class to handle MS5611; adress = 0x77 or 0x76
SPL06 baro2( (uint8_t) 0x76  );    // class to handle SPL06; adress = 0x77 or 0x76
BMP280 baro3( (uint8_t) 0x76) ;    // class to handle BMP280; adress = 0x77 or 0x76

VARIO vario1;

//queue_t gpsQueue ; // queue is used to transfer the data from the uart0 used by GPS
GPS gps;

// CRSF is managed with 2 pio and not with the internal uart1 in order to freely select the pins

extern CONFIG config;
bool configIsValid = true;
bool configIsValidPrev = true;


//#define PICO_I2C1_SDA_PIN 14  
//#define PICO_I2C1_SCL_PIN 15  

uint32_t lastBlinkMillis;

void setupI2c(){
    if ( config.pinScl == 255 or config.pinSda == 255) return; // skip if pins are not defined
    i2c_init( i2c1, 400 * 1000);
    gpio_set_function(config.pinSda, GPIO_FUNC_I2C);
    gpio_set_function(config.pinScl, GPIO_FUNC_I2C);
    gpio_pull_up(config.pinSda);
    gpio_pull_up(config.pinScl); 
}

void getSensors(void){
  voltage.getVoltages();
  if ( baro1.baroInstalled){
    if ( baro1.getAltitude() == 0) { // if an altitude is calculated
      vario1.calculateAltVspeed(baro1.altitude , baro1.altIntervalMicros ); // Then calculate Vspeed ... 
    }
  } else if ( baro2.baroInstalled){
    if ( baro2.getAltitude() == 0) { // if an altitude is calculated
      vario1.calculateAltVspeed(baro2.altitude , baro2.altIntervalMicros); // Then calculate Vspeed ... 
    }
  } else if (baro3.baroInstalled) {
    if ( baro3.getAltitude() == 0) { // if an altitude is calculated
      vario1.calculateAltVspeed(baro3.altitude , baro3.altIntervalMicros); // Then calculate Vspeed ... 
    }
  } 
  gps.readGps();
  readRpm();
}

void mergeSeveralSensors(void){
}

void setup() {
  stdio_init_all();
  setupLed();
  setRgbColor(10,0,10); // start with 2 color
  #ifdef DEBUG
  uint16_t counter = 0;                      // after an upload, watchdog_cause_reboot is true.
  if ( watchdog_caused_reboot() ) counter = 0; // avoid the UDC wait time when reboot is caused by the watchdog   
  while ( (!tud_cdc_connected()) && (counter--)) { 
    sleep_ms(200); 
    toggleRgb();
    //watchdog_update();
    }
  #endif
  sleep_ms(1000);
  if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
    } else {
        printf("Clean boot\n");
    }
  watchdog_enable(1500, 0); // require an update once every 1500 msec
  setRgbColor(0,0,10);  // switch to blue if we are connected
  watchdog_update();
  setupConfig(); // retrieve the config parameters (crsf baudrate, voltage scale & offset, type of gps, failsafe settings)
  watchdog_update();
  if (configIsValid){ // continue with setup only if config is valid
      setupListOfFields(); // initialise the list of fields being used
      watchdog_update();
      voltage.begin();
      setupI2c();      // setup I2C
      baro1.begin();  // check MS5611; when ok, baro1.baroInstalled  = true
      watchdog_update();
      baro2.begin();  // check SPL06;  when ok, baro2.baroInstalled  = true
      watchdog_update();
      baro3.begin(); // check BMP280;  when ok, baro3.baroInstalled  = true
      watchdog_update();
      
      gps.setupGps();  //use a Pio
      watchdog_update();
      if ( config.protocol == 'C'){
        setupCrsfIn();  // setup one/two uart and the irq handler (for primary Rx) 
        setupCrsf2In();  // setup one/two uart and the irq handler (for secondary Rx) 
        setupCrsfOut(); //  setup 1 pio/sm (for TX ) and the DMA (for TX)   
      } else if (config.protocol == 'S') {
        setupSbusIn();
        setupSbus2In();
        setupSport();
      } else if (config.protocol == 'J') {
        setupSbusIn();
        setupSbus2In();
        setupJeti();
      }
      watchdog_update();
      if (config.pinSbusOut != 255) { // configure 1 pio/sm for SBUS out (only if Sbus out is activated in config).
          setupSbusOutPio();
        }
      setupPwm();
      watchdog_update();
      setupRpm(); // this function perform the setup of pio Rpm
      printConfig();
      watchdog_update();
      setRgbColor(0,10,0);
      watchdog_enable(500, 0); // require an update once every 500 msec
  } else {
    printConfig(); // config is not valid
  }
}


void loop() {
  watchdog_update();
  if (configIsValid){
      getSensors();
      mergeSeveralSensors();
      watchdog_update();
      if ( config.protocol == 'C'){
        fillCRSFFrame();
        handleCrsfIn();
        handleCrsf2In();
        fillSbusFrame();
      } else if (config.protocol == 'S') {
        handleSportRxTx();
        handleSbusIn();
        handleSbus2In();
        fillSbusFrame();
      } else if (config.protocol == 'J') {
        handleJetiTx();
        handleSbusIn();
        handleSbus2In();
        fillSbusFrame();
      } 
      watchdog_update();
      updatePWM();
            //updatePioPwm();
      watchdog_update();
  }
  //if (tud_cdc_connected()) {
  handleUSBCmd();  // process the commands received from the USB
  tud_task();      // I suppose that this function has to be called periodicaly
  //}  
  if ( configIsValidPrev != configIsValid) {
    configIsValidPrev = configIsValid;
    if (configIsValid) {
        setRgbColor(0,10,0);
    } else {
        setRgbColor(10,0,0);  
    }
  }
  if (( millis() - lastBlinkMillis) > 300 ){
    toggleRgb();
    lastBlinkMillis = millis();
  }
}

int main(){
  setup();
  while(1) loop();
}