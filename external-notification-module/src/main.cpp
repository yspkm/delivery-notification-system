#include <Arduino.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <ThreeWire.h>
#include <RtcDateTime.h>
#include <RtcDS1302.h>

typedef enum _ELcdControl {
  TIME_NOW_LINE0,
  TIME_ARRV_LINE1,
  MOTION_LINE2,
  DELIVERY_END_LINE3,
  LCDINIT
} LcdControl;

typedef enum _EBuzzerLevel {
  LEVEL0,
  LEVEL1,
  LEVEL2,
  LEVEL3
} BuzzerLevel;

const byte bt_rx_pin = 2;
const byte bt_tx_pin = 3;
const byte lcd_sda_pin = 4;
const byte lcd_scl_pin = 5;
const byte relay_pin[2] = {6, 7};
const byte buzzer_pin = 8;

const byte lcd_adrr = 0x27;
const byte lcd_col = 20;
const byte lcd_row = 4;

const byte rtc_clk_pin = 9;
const byte rtc_dat_pin = 10;
const byte rtc_rst_pin = 11;

SoftwareSerial bt_serial(bt_rx_pin, bt_tx_pin);
LiquidCrystal_I2C lcd(lcd_adrr, lcd_col, lcd_row);
//ThreeWire(uint8_t ioPin /*dat_pin*/, uint8_t clkPin/*clk_pin*/, uint8_t cePin/*rst_pin*/) : 
ThreeWire rtc_wire(rtc_dat_pin, rtc_clk_pin, rtc_rst_pin);
RtcDS1302<ThreeWire> rtc(rtc_wire);

String received_str = "";
String header_module = "", header_item = "", header_value = "";
String time_form_str = ""; 
uint16_t arrv_time = 0;
uint8_t last_minute = 0;
BuzzerLevel buzzer_level = LEVEL1;

void parseString(void)
{
  byte idx_module = received_str.indexOf(','); //finds location of ','
  byte idx_item = received_str.indexOf(',', idx_module + 1);

  header_module = received_str.substring(0, idx_module);           //module name
  header_item = received_str.substring(idx_module + 1, idx_item);  //item name
  header_value = received_str.substring(idx_item + 1, received_str.length()); // value

  header_module.replace(',', ' ');
  header_item.replace(',', ' ');
  header_value.replace(',', ' ');

  header_module.trim();
  header_item.trim();
  header_value.trim();
}

void setBuzzerLevel(BuzzerLevel level)
{
  buzzer_level = level;
}

void turnOnBuzzer(void)
{
  //Buzzer noise occurs at different levels of volume
  //level0 is mute
  if (buzzer_level == LEVEL0)
  {
    digitalWrite(buzzer_pin, LOW);
  }
  else if (buzzer_level == LEVEL1)
  {
    digitalWrite(relay_pin[0], HIGH);
    digitalWrite(buzzer_pin, HIGH);
  }
  else if (buzzer_level == LEVEL2)
  {
    digitalWrite(relay_pin[0], LOW);
    digitalWrite(relay_pin[1], HIGH);
    digitalWrite(buzzer_pin, HIGH);
  }
  else if (buzzer_level == LEVEL3)
  {
    digitalWrite(relay_pin[0], LOW);
    digitalWrite(relay_pin[1], LOW);
    digitalWrite(buzzer_pin, HIGH);
  }
  else
  {
    Serial.println("buzzer level error");
  }
}

void getTimeNow(uint16_t *hour, uint16_t *min, uint16_t *day, uint16_t *month, uint16_t *year)
{
  RtcDateTime date_time = rtc.GetDateTime();
  *hour = date_time.Hour();
  *min = date_time.Minute();
  *day = date_time.Day();
  *month = date_time.Month();
  *year = date_time.Year();
}

void getTimeFormString(uint16_t hour, uint16_t min, uint16_t day, uint16_t month, uint16_t year)
{
  char* time_fstr = NULL;
  time_fstr = (char*) malloc(18 * sizeof(char));
  sprintf(time_fstr, "%02u:%02u|%4u-%02u-%02u", hour, min, year, month, day);
  time_form_str = String(time_fstr);
  Serial.println("time is: " + time_form_str);
  free(time_fstr);
}

void getTimeFormStringNow(void) 
{
  uint16_t hour,  min,  day,  month, year;
  getTimeNow(&hour, &min, &day, &month, &year);
  getTimeFormString(hour,  min,  day,  month, year);
}

void getTimeFormStringArrv(uint16_t arrv_minute)
{
  uint16_t min_new, hour_new;
  uint16_t hour,  min,  day,  month, year;
  getTimeNow(&hour, &min, &day, &month, &year);
  min += arrv_minute;
  min_new = min % 60;
  hour_new = hour + (min / 60);
  getTimeFormString(hour_new,  min_new, day, month, year);
}

void lcdPrintStatus(LcdControl lcd_control)
{
  /* LCD Display
  LCD Display Example
  NOW:08:15|2021-08-21
  EXP:08:45|2021-08-21
  MOTION DETECTED!
  DELIVERY COMPLETE!!!
   */
  switch(lcd_control) {
    case LCDINIT:
      lcd.setCursor(0, 0); // setCursor(col, row) 
      getTimeFormStringNow();
      lcd.print("NOW:" + time_form_str); // Display Current Time
      lcd.setCursor(0, 1);   
      getTimeFormStringArrv(arrv_time);
      lcd.print("EXP:" + time_form_str); // Display estimated arrival time
      lcd.setCursor(0, 2);   
      lcd.print("MOTION CHECKING..."); // Display if motion is detected
      break;
    case TIME_NOW_LINE0:
      lcd.setCursor(0, 0);  
      getTimeFormStringNow();
      last_minute = rtc.GetDateTime().Minute(); 
      lcd.print("NOW:" + time_form_str); // Display Current Time
      break;
    case MOTION_LINE2:
      lcd.setCursor(0, 2);
      lcd.print("MOTION DETECTED!!"); // Motion Detection Notification
      break;
    case DELIVERY_END_LINE3:
      lcd.setCursor(0, 3);      
      lcd.print("DELIVERY COMPLETE!!"); // Delivery End Notification
      break;
    default:
      Serial.println("error: lcd control");
  }
}

void turnOnBuzzerAtLevel(BuzzerLevel level)
{
  setBuzzerLevel(level);
  turnOnBuzzer();
}

void initRtc(void) 
{
    Serial.print("compiled: ");
    Serial.print(__DATE__);
    Serial.println(__TIME__);
    rtc.Begin();
    RtcDateTime compiled_date_time = RtcDateTime(__DATE__, __TIME__);
    
    if (rtc.GetIsWriteProtected()) {
        Serial.println("RTC was write protected, enabling writing now");
        rtc.SetIsWriteProtected(false);
    } 
    
    if (!rtc.GetIsRunning()) {
        Serial.println("RTC was not actively running, starting now");
        rtc.SetIsRunning(true);
    }

    RtcDateTime now = rtc.GetDateTime();
    if (now < compiled_date_time) {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        rtc.SetDateTime(compiled_date_time);
    } else if (now > compiled_date_time) {
        Serial.println("RTC is newer than compile time. (this is expected)");
    }
    else if (now == compiled_date_time) {
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }
}

void updateTime() 
{
  if (rtc.GetDateTime().Minute() != last_minute) {
    lcdPrintStatus(TIME_NOW_LINE0);
  }
} 

void setup()
{
  Serial.begin(9600);    // For local diagnostics
  bt_serial.begin(9600); // Convert Bluetooth to Serial Communication
  initRtc();
  lcd.init();
  lcd.backlight();
  pinMode(relay_pin[0], OUTPUT);
  pinMode(relay_pin[1], OUTPUT);
  pinMode(buzzer_pin, OUTPUT);
  lcdPrintStatus(TIME_NOW_LINE0);
}

void loop()
{
  if (bt_serial.available()) {
    received_str = bt_serial.readStringUntil('\n');
    Serial.println(received_str);
    parseString();  // header_module, header_item, header_value 
                    // e.g. "buzzer, level, 1" 
    Serial.println("header_module: " + header_module);
    Serial.println("header_item: " + header_item);
    Serial.println("header_value: " + header_value);
    //1 Blocks on Delivery
    if (header_module.equals("delivery")) {
      //1.1 Delivery Start
      if (header_item.equals("start")) { 
        pinMode(buzzer_pin, LOW);
        arrv_time = header_value.toInt();
        lcdPrintStatus(LCDINIT);
      }
      //1.2 Delivery complete
      else if (header_item.equals("end")) {
        lcdPrintStatus(DELIVERY_END_LINE3);
        turnOnBuzzer();
      } else {
        Serial.println("error: delivery");
        return;
      }
    //2 Blocks on Buzzer
    } else if (header_module.equals("buzzer")) { 
      //2.1 buzzer on/off control
      if (header_item.equals("on/off")) { 
        if (header_value.equals("on")) {
          turnOnBuzzer();
        } else if (header_value.equals("off")) {
          pinMode(buzzer_pin, LOW);
        }
      //2.2 Buzer volume control
      } else if (header_item.equals("level")) {
        setBuzzerLevel((BuzzerLevel) header_value.toInt());
      // parsing error, "buzzer"
      } else {
        Serial.println("error: buzzer on/off");
      }
    //3. Blocks on Buzzer
    } else if (header_module.equals("motion")) {
      lcdPrintStatus(MOTION_LINE2);
    }
  // Bluetooth Connection Error
  } else {
    Serial.println("bluetooth error");
  }
  updateTime();
}