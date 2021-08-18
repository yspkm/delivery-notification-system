#include <Arduino.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>

#define DELIVERY          "delivery"  //e.g. "delivery, start, 1, 45" 
#define DELIVERY_START    "start"  
#define DELIVERY_END      "end"       //e.g. "delivery, end, 1"

#define MOTION            "motion" //e.g. "signal, motion, , "

#define BUZZER            "buzzer" //e.g. "buzzer, on/off, on, "
#define BUZZER_ONOFF      "on/off"
#define BUZZER_ONOFF_ON   "on"
#define BUZZER_ONOFF_OFF  "off"
#define BUZZER_LEVEL      "level" //1 ~ 3 , e.g. "buzzer, level, 1, " //default level is 1

typedef enum _EBuzzerLevel {
  LEVEL0, LEVEL1, LEVEL2, LEVEL3
} BuzzerLevel;

typedef enum _EBuzzerScale {
  C_4=262, CS_4=277, D_4=294, DS_4=311, E_4=330, F_4=349, FS_4=370, G_4=392, GS_4=415, A_4=440, AS_4=466, B_4=494
} BuzzerScale;

#define LCD               "lcd"
#define LCD_ARVTIME       "time"

const byte bt_rx_pin = 2;
const byte bt_tx_pin = 3;
const byte lcd_sda_pin = 4;
const byte lcd_scl_pin = 5;
const byte relay_pin[2] = {6, 7};
const byte buzzer_pin = 8;

const byte lcd_adrr = 0x27;
const byte lcd_col = 20;
const byte lcd_row = 4;

SoftwareSerial bt_serial(bt_tx_pin, bt_rx_pin);
LiquidCrystal_I2C lcd(lcd_adrr, lcd_col, lcd_row) ;

String received_str = "";
String module="", item="", value1="", value2="";


BuzzerLevel buzzer_level = LEVEL1;
BuzzerScale buzzer_scale = A_4;

void parseString(void) 
{
      byte ind1 = received_str.indexOf(',');            //finds location of ','
      byte ind2 = received_str.indexOf(',', ind1+1 );   
      byte ind3 = received_str.indexOf(',', ind2+1 );
      byte ind4 = received_str.indexOf(',', ind3+1 );

      module = received_str.substring(0, ind1);         //module name
      item = received_str.substring(ind1+1, ind2+1);   //item name
      value1 = received_str.substring(ind2+1, ind3+1); // value 1
      value2 = received_str.substring(ind3+1);         //value 2
}

void setBuzzerLevel(BuzzerLevel level)
{
  buzzer_level = level;
}

void setBuzzerScale(BuzzerScale scale)
{
  buzzer_scale = scale;
}

void buzzerOn(byte buzzer_pin, int freq, int dur_time, int iter_num, BuzzerScale scale)
{
  for (;iter_num > 0; iter_num--) {
    tone(buzzer_pin, scale, dur_time);
  }
}

void turnOnBuzzer(void) 
{
  //Buzzer noise occurs at different levels of volume
  //level0 is mute
  if(buzzer_level == LEVEL0) {
    digitalWrite(buzzer_pin, LOW);
  } else if (buzzer_level == LEVEL1) {
    digitalWrite(relay_pin[0], HIGH);
    digitalWrite(buzzer_pin, HIGH);
  } else if (buzzer_level == LEVEL2) {
    digitalWrite(relay_pin[0], LOW);
    digitalWrite(relay_pin[1], HIGH);
    digitalWrite(buzzer_pin, HIGH);
  } else if (buzzer_level == LEVEL3) {
    digitalWrite(relay_pin[0], LOW);
    digitalWrite(relay_pin[1], LOW);
    digitalWrite(buzzer_pin, HIGH);
  } else {
    Serial.println("buzzer level error");
  }
}

void turnOnBuzzerAtLevel(BuzzerLevel level)
{
  setBuzzerLevel(level);
  turnOnBuzzer();
}

void setup() {
  Serial.begin(9600); //진단용
  bt_serial.begin(9600); //블루투스통신을 직렬통신으로 바꿔렬
  lcd.init();
  lcd.init();
  lcd.backlight();
  pinMode(relay_pin[0], OUTPUT);
  pinMode(relay_pin[1], OUTPUT);
  pinMode(buzzer_pin, OUTPUT);
}

void loop() {
   if(bt_serial.available()) {
    //received_str = bt_serial.readStringUntil('\n');
    received_str = bt_serial.readString();
    //parseString();
    lcd.clear();
    lcd.clear();
    lcd.setCursor(0,0);
    if(received_str.equals("level0")) {
      turnOnBuzzerAtLevel(LEVEL0);
    } else if (received_str.equals("level1")) {
      turnOnBuzzerAtLevel(LEVEL1);
    } else if (received_str.equals("level2")) {
      turnOnBuzzerAtLevel(LEVEL2);
    } else if (received_str.equals("level3")) {
      turnOnBuzzerAtLevel(LEVEL3);
    }
    lcd.println(received_str);
    /*
    if (module.equals(DELIVERY)) {
      if (item.equals(DELIVERY_START)) {
       //배달 시작에 관한  
      } else if (item.equals(DELIVERY_END)) {
        //배달 종료에 관한
      } else {
        Serial.println("error occurs");
        return;
      }
    } else if (module.equals(BUZZER)) { //부저 관련 블럭
      if (item.equals(BUZZER_ONOFF)) { //부저 온오프 제어
        if (value1.equals(BUZZER_ONOFF_ON)) {
          turnOnBuzzer();
        } else if (value1.equals(BUZZER_ONOFF_OFF)) {
          digitalWrite(buzzer_pin, LOW);
        }
      } else {
        Serial.println("error: buzzer on/off");
      }
    } else if (module.equals(LCD)) {
    }
    if(received_str.equals(DELIVERY)) {

    }
    */
  } else {
    Serial.println("bluetooth error");
   }
}