#include <SSD1306Ascii.h>
#include <SSD1306AsciiAvrI2c.h>
#include <SSD1306AsciiSoftSpi.h>
#include <SSD1306AsciiSpi.h>
#include <SSD1306AsciiWire.h>
#include <SSD1306init.h>

/*
  
  Copyright (C) 2022 Sony Computer Science Laboratories
  
  Author(s) Peter Hanappe, Douglas Boari, Aliénor Lahlou
  
  free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see
  <http://www.gnu.org/licenses/>.
  
 */
#include <ArduinoSerial.h>
#include <RomiSerial.h>
#include <RomiSerialErrors.h>
#include <stdint.h>
#include "DigitalPulse.h"
#include "ActivityManager.h"
#include "BoardFactory.h"
#include "ITimer.h"
#include "IBoard.h"
#include "EndSwitch.h"
#include "Relay.h"


#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

// 0X3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS 0x3C

SSD1306AsciiWire oled;

using namespace romiserial;



void send_info(IRomiSerial *romiSerial, int16_t *args, const char *string_arg);
void handle_add_digital_pulse(IRomiSerial *romiSerial, int16_t *args,
                              const char *string_arg);
void handle_set_secondary(IRomiSerial *romiSerial, int16_t *args, const char *string_arg);
void handle_stop_measurements(IRomiSerial *romiSerial, int16_t *args,
                              const char *string_arg);
void handle_start_mesurements(IRomiSerial *romiSerial, int16_t *args,
                              const char *string_arg);
void handle_add_relay(IRomiSerial *romiSerial, int16_t *args, const char *string_arg);
void handle_is_active(IRomiSerial *romiSerial, int16_t *args, const char *string_arg);
void handle_reset(IRomiSerial *romiSerial, int16_t *args, const char *string_arg);

const static MessageHandler handlers[] = {
        { 'd', 8, false, handle_add_digital_pulse },
        { 'r', 1, false, handle_add_relay },
        { 's', 2, false, handle_set_secondary },
        { 'b', 2, false, handle_start_mesurements },
        { 'e', 0, false, handle_stop_measurements },
        { 'R', 0, false, handle_reset },
        { 'A', 0, false, handle_is_active },
        { '?', 0, false, send_info },
};

static int kMaxActivitiesCode = 1;
static int kBadStartCode = 2;
static int kBadPeriodCode = 3;
static int kBadDurationCode = 4;
static int kBadPinCode = 5;

static char *kMaxActivitiesMessage = "Too many activities";
static char *kBadStartMessage = "Bad start";
static char *kBadPeriodMessage = "Bad period";
static char *kBadDurationMessage = "Bad duration";
static char *kBadPinMessage = "Bad pin";

const byte interruptPin = 2; // switching between manual and serial mode, possible interrupt pin on Uno board: 2 and 3. 
const byte MANUAL = LOW;
const byte USB = HIGH;
byte currentMode = USB; // default mode
int analogPin = A0;  // Potentiometer connected to analog branch A0
int analogPin2 = A1;
int potentio = 255;        // Variable storing the potentiometer value
int potentio2 = 255;
int potentio_prev = 255;        // Variable storing the potentiometer value
int potentio_prev2 = 255;        // Variable storing the potentiometer value


float calc = 0;
int ledPin = 6; // LED pin. PWM pins not interfering with the timer: select among 3, 6, 10 and 11. NOT 5 or 9 !
int ledPin2 = 3; // LED pin. PWM pins not interfering with the timer: select among 3, 6, 10 and 11. NOT 5 or 9 !
volatile byte flag=0; // interrupt switch. Only put volatile variable in the interrupt func, short content, no read/write, no serial. 

#if defined(ARDUINO_SAM_DUE)
ArduinoSerial serial(SerialUSB);
#else
ArduinoSerial serial(Serial);
#endif

RomiSerial romiSerial(serial, serial, handlers, sizeof(handlers) / sizeof(MessageHandler));
ActivityManager activityManager;

IBoard& board = BoardFactory::get();
ITimer& timer = board.get_timer(); // get the board timer

extern volatile int32_t timer_interrupts_;

void set_mode() {
  flag=1;
}



void initial_display_mode(byte currentMode) {
        oled.clear(); //efface affichage écran

        oled.set1X(); //taile normal
        oled.setCursor(0, 0); //positionne curseur en haut à gauche
        oled.println("Mode");
        oled.set2X(); //double la taille
        
        if (currentMode==USB){
          oled.println("SERIAL");
        }
        else if (currentMode==MANUAL) {
          oled.println("MANUAL");
        }
        oled.println(""); //ligne vide
        oled.set1X();
        oled.println("Setting");
        oled.set1X();
        oled.print("1: ");
        oled.print(potentio);
        oled.setCursor(0, 7);
        oled.print("2: ");
        oled.print(potentio2);

        
}

void refresh_mode(byte currentMode) {
        oled.clear(0,128,1,2);
        
        if (currentMode==USB){
          oled.println("SERIAL");
        }
        else if (currentMode==MANUAL) {
          oled.println("MANUAL");
        }

}

int refresh_setting(int potentio, int potentio_prev, int potentio2, int potentio_prev2){
  if (potentio != potentio_prev){
    oled.clear(0,128,6,7);
    oled.print("1: ");
    oled.print(potentio);
    oled.setCursor(0, 7);
    oled.print("2: ");
    oled.print(potentio2);

    return potentio;
  }
  if (potentio2 != potentio_prev2){
    oled.clear(0,128,6,7);
    oled.print("1: ");
    oled.print(potentio);
    oled.setCursor(0, 7);
    oled.print("2: ");
    oled.print(potentio2);

    return potentio2;
  }
  
}


void setup()
{   
    Wire.begin();
    Wire.setClock(400000L);
    oled.begin(&Adafruit128x64, I2C_ADDRESS);
    delay(2000);
    oled.clear();
    oled.setFont(Adafruit5x7);
 
  
#if defined(ARDUINO_SAM_DUE)
        SerialUSB.begin(0);
        while(!SerialUSB);
#else
        Serial.begin(115200);
        while (!Serial)
                ;
#endif
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), set_mode, CHANGE);
  currentMode = digitalRead(interruptPin); //default mode when no switch is used is Serial (currentMode = USB)
  initial_display_mode(currentMode);
  setupSerial(); // when the code is used separately from the spacific board, it is in serial mode by default (interruptPin HIGH (pullup))
 
}


int read_analog(int pin) //ajout de cette fonction pour éviter les variations
{
    long sum = 0;
    const int numLectures = 10; // Nombre de lectures à moyenner
    for (int i = 0; i < numLectures; i++) 
    {
        sum += analogRead(pin);
        delay(5);  // Petits délais pour éviter de trop ralentir
    }
    return sum / numLectures;
}

void loop()
{
      if (flag == 1) {
        currentMode = digitalRead(interruptPin); // when the switch is switched, a flag is raised to change mode. Mode is indicated by the switch position
      }
      
      if ((flag == 1) && (currentMode==USB)){ // switching from manual to USB
        cleanManual();
        setupSerial();
        refresh_mode(currentMode);
        flag=0;
        }
      else if ((flag == 1) && (currentMode==MANUAL)){ // switching from USB to manual
        cleanSerial();
        setupManual();
        refresh_mode(currentMode);
        flag=0;
        }

      if (currentMode==USB){
        romiSerial.handle_input();
        delay(10);
      }
      else {
      //potentio = analogRead(analogPin);  //read analog value corresponding to potentiometer 
      potentio = read_analog(analogPin); 
      potentio = potentio/4; // adjust the analog range (0-1023) to PWM range (0-255)
      analogWrite(ledPin, potentio);  
      potentio_prev = refresh_setting(potentio, potentio_prev, potentio2, potentio_prev2);

      //potentio2 = analogRead(analogPin2);  //read analog value corresponding to potentiometer
      potentio2 = read_analog(analogPin2);
      potentio2 = potentio2/4; // adjust the analog range (0-1023) to PWM range (0-255)
      analogWrite(ledPin2, potentio2);  
      potentio_prev2 = refresh_setting(potentio, potentio_prev, potentio2, potentio_prev2);

      delay(10);
      }

}

void setupManual(){
    pinMode(ledPin,OUTPUT);
    pinMode(ledPin2,OUTPUT);

}
void cleanManual(){
     analogWrite(ledPin, 0);//j'ai changé ici à 1
     analogWrite(ledPin2, 0);//j'ai changé ici à 1

}

void setupSerial(){
  timer.init();
  timer.start(activityManager.getActivities(),
                    activityManager.countActivities());  
  }


void cleanSerial(){
        analogWrite(ledPin, 0);  //j'ai changé ici à 1
        analogWrite(ledPin2, 0);  //j'ai changé ici à 1

        //IBoard& board = BoardFactory::get();
        //ITimer& timer = board.get_timer();
        timer.stop();
        activityManager.clear();
        delay(10);
  
}


void send_info(IRomiSerial *romiSerial, int16_t *args, const char *string_arg)
{
        romiSerial->send("[\"LightControl\",\"0.1\"]"); 
}

void handle_add_digital_pulse(IRomiSerial *romiSerial, int16_t *args,
                              const char *string_arg)
{
        int8_t pin = (uint8_t) args[0];
        int32_t start_delay = (int32_t) args[1] * 1000 + args[2];
        int32_t duration = (int32_t) args[3] * 1000 + args[4];
        int32_t period = (int32_t) args[5] * 1000 + args[6];
        int value = args[7];
    
        if (start_delay < 0)
                romiSerial->send_error(kBadStartCode, kBadStartMessage);
        else if (period <= 0)
                romiSerial->send_error(kBadPeriodCode, kBadPeriodMessage);
        else if (duration < 0 || duration > period)
                romiSerial->send_error(kBadDurationCode, kBadDurationMessage);
        else if (activityManager.availableSpace() == 0) 
                romiSerial->send_error(kMaxActivitiesCode, kMaxActivitiesMessage);
        else {
                auto activity = new DigitalPulse(pin, start_delay, period,
                                                 duration, value);
                activityManager.addActivity(activity);
                romiSerial->send_ok();
        }
}

void handle_set_secondary(IRomiSerial *romiSerial, int16_t *args, const char *string_arg)
{
        int8_t primary_pin = (uint8_t) args[0];
        int8_t secondary_pin = (uint8_t) args[1];

        IActivity *primary_activity = activityManager.getActivityOnPin(primary_pin);
        IActivity *secondary_activity = activityManager.getActivityOnPin(secondary_pin);

        if (primary_activity == nullptr
            || secondary_activity == nullptr) {
                romiSerial->send_error(kBadPinCode, kBadPinMessage);                
        } else {
                secondary_activity->setPrimary(primary_activity);
                romiSerial->send_ok();
        }
}

void handle_start_mesurements(IRomiSerial *romiSerial, int16_t *args,
                              const char *string_arg)
{
        int32_t duration = (int32_t) args[0] * 1000 + args[1];
        if (duration > 0) {
                auto endSwitch = new EndSwitch(duration);
                activityManager.addActivity(endSwitch);
        }
        
        IBoard& board = BoardFactory::get();
        ITimer& timer = board.get_timer();
        timer.start(activityManager.getActivities(),
                    activityManager.countActivities());
        
        romiSerial->send_ok();
}

void handle_stop_measurements(IRomiSerial *romiSerial, int16_t *args,
                              const char *string_arg)
{
        IBoard& board = BoardFactory::get();
        ITimer& timer = board.get_timer();
        timer.stop();
        
        romiSerial->send_ok();
}

void handle_add_relay(IRomiSerial *romiSerial, int16_t *args, const char *string_arg)
{
        uint8_t pin = args[0];
        auto relay = new Relay(pin);
        activityManager.addActivity(relay);
        romiSerial->send_ok();
}

void handle_is_active(IRomiSerial *romiSerial, int16_t *args, const char *string_arg)
{
        IBoard& board = BoardFactory::get();
        ITimer& timer = board.get_timer();
        if (timer.isActive()) {
                romiSerial->send("[0,1]"); 
        } else {
                romiSerial->send("[0,0]"); 
        }
}

void handle_reset(IRomiSerial *romiSerial, int16_t *args, const char *string_arg)
{
        IBoard& board = BoardFactory::get();
        ITimer& timer = board.get_timer();
        timer.stop();
        activityManager.clear();
        romiSerial->send_ok();
}
