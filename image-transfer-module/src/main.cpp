#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

#include "addons/TokenHelper.h" //Provide the token generation process info.
#include "addons/RTDBHelper.h"  //Provide the RTDB payload printing info and other helper functions

#include "esp_camera.h"
#include "Base64.h"

#include "private_info.h" //Wifi ssid, pwd, api-key, project-url
#include "device_info.h"  //camera fin, camera model, etc
/**************************global variables***********************************/
String device_location = ""; // Device Location config
String database_path = "";   // Firebase database path
String photo_path = "";
String signal_path = "";
String photo_data = "";
String sensor_control_path = "";

/*Data Objects*/
FirebaseData firebase_data;     // Firebase Realtime Database Object
FirebaseAuth firebase_auth;     // Firebase Authentication Object
FirebaseConfig firebase_config; // Firebase configuration Object
camera_config_t cam_config;

String fuid = ""; // Firebase Unique Identifier

boolean is_authenticated = false; // Store device authentication status
boolean is_motion_detected = false;
boolean is_light_detected = false;
boolean sensor_control = false;
int idx = 0;
/*****************************************************************************/

/**************************pin number ****************************************/
const int motion_pin = GPIO_NUM_14;
const int builtin_led = BUILTIN_LED;
/*****************************************************************************/

/***************user-defined function*****************************************/
//camera instruction
void photo2Base64(camera_fb_t *cam_fb)
{
  photo_data.clear();
  //photo_data.concat("data:image/jpeg;base64,"); // image file - for JSON tree
  char *input = (char *)cam_fb->buf;
  char output[base64_enc_len(3)];

  for (int i = 0; i < cam_fb->len; i++)
  {
    base64_encode(output, (input++), 3);
    if (i % 3 == 0)
      photo_data.concat(String(output));
  }
  photo_data.concat(String(output));
  //photo_data += "\"";
}

bool cameraInit(void)
{

  cam_config.ledc_channel = LEDC_CHANNEL_0;
  cam_config.ledc_timer = LEDC_TIMER_0;
  cam_config.pin_d0 = Y2_GPIO_NUM;
  cam_config.pin_d1 = Y3_GPIO_NUM;
  cam_config.pin_d2 = Y4_GPIO_NUM;
  cam_config.pin_d3 = Y5_GPIO_NUM;
  cam_config.pin_d4 = Y6_GPIO_NUM;
  cam_config.pin_d5 = Y7_GPIO_NUM;
  cam_config.pin_d6 = Y8_GPIO_NUM;
  cam_config.pin_d7 = Y9_GPIO_NUM;
  cam_config.pin_xclk = XCLK_GPIO_NUM;
  cam_config.pin_pclk = PCLK_GPIO_NUM;
  cam_config.pin_vsync = VSYNC_GPIO_NUM;
  cam_config.pin_href = HREF_GPIO_NUM;
  cam_config.pin_sscb_sda = SIOD_GPIO_NUM;
  cam_config.pin_sscb_scl = SIOC_GPIO_NUM;
  cam_config.pin_pwdn = PWDN_GPIO_NUM;
  cam_config.pin_reset = RESET_GPIO_NUM;
  cam_config.xclk_freq_hz = 20000000;
  cam_config.pixel_format = PIXFORMAT_JPEG;
  // ESP32-CAM AI-Thinker has PSRAM
  cam_config.frame_size = FRAMESIZE_UXGA;
  cam_config.jpeg_quality = 10;
  cam_config.fb_count = 2;
  esp_err_t err = esp_camera_init(&cam_config);
  if (err != ESP_OK)
  {
    Serial.printf("cameraInit() failed with error 0x%x", err);
    return false;
  }
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);
  return true;
}

void wifiInit(void)
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

// for Firebase initialization, use Firebase API
void firebaseInit()
{
  firebase_config.api_key = API_KEY;           // configure firebase API Key
  firebase_config.database_url = DATABASE_URL; // configure firebase realtime database url
  Firebase.reconnectWiFi(true);                // Enable WiFi reconnection

  Serial.println("------------------------------------");
  Serial.println("Connecting to Firebase...");

  if (Firebase.signUp(&firebase_config, &firebase_auth, "", ""))
  { // Sign in to firebase
    Serial.println("Success");
    is_authenticated = true;
    fuid = firebase_auth.token.uid.c_str();
  }
  else
  {
    Serial.printf("Failed, %s\n", firebase_config.signer.signupError.message.c_str());
    is_authenticated = false;
  }
  firebase_config.token_status_callback = tokenStatusCallback; // Assign the callback function for the long running token generation task, see addons/TokenHelper.h
  Firebase.begin(&firebase_config, &firebase_auth);            // Initialise the firebase library
}

void sendMotionSignalToFirebase(boolean signal)
{
  if (is_authenticated && Firebase.ready())
  {
    if (Firebase.set(firebase_data, signal_path.c_str(), (int)signal))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + firebase_data.dataPath());
      Serial.println("TYPE: " + firebase_data.dataType());
      Serial.println("ETag: " + firebase_data.ETag());
      Serial.print("VALUE: ");
      printResult(firebase_data); //see addons/RTDBHelper.h
      Serial.println("------------------------------------");
      Serial.println();
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebase_data.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
  }
}

void getPhotoThenSendToFirebase(void)
{
  camera_fb_t *cam_fb = NULL;
  cam_fb = esp_camera_fb_get();

  photo2Base64(cam_fb);

  if (is_authenticated && Firebase.ready())
  {
    if (Firebase.setString(firebase_data, photo_path.c_str(), photo_data))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + firebase_data.dataPath());
      Serial.println("TYPE: " + firebase_data.dataType());
      Serial.println("ETag: " + firebase_data.ETag());
      Serial.print("VALUE: ");
      Serial.println("complete");
      //printResult(firebase_data); //see addons/RTDBHelper.h
      Serial.println("------------------------------------");
      Serial.println();
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebase_data.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
  }

  photo_data.clear();
  esp_camera_fb_return(cam_fb); //free memory
}

void getPhotoThenSendToFirebaseWithIndex(int idx)
{
  camera_fb_t *cam_fb = NULL;
  cam_fb = esp_camera_fb_get();

  photo2Base64(cam_fb);

  if (is_authenticated && Firebase.ready())
  {
    if (Firebase.setString(firebase_data, (photo_path + "_" + String(idx)).c_str(), photo_data))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + firebase_data.dataPath());
      Serial.println("TYPE: " + firebase_data.dataType());
      Serial.println("ETag: " + firebase_data.ETag());
      Serial.print("VALUE: ");
      Serial.println("complete");
      //printResult(firebase_data); //see addons/RTDBHelper.h
      Serial.println("------------------------------------");
      Serial.println();
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + firebase_data.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
  }

  photo_data.clear();
  esp_camera_fb_return(cam_fb); //free memory
}

/*****************************************************************************/
void setup()
{
  Serial.begin(115200);                  // Initialize serial port for diagnosis
  database_path = "/" + device_location; // Set the database path where updates will be loaded for this device
  photo_path = database_path + "/imgdata";
  signal_path = database_path + "/sgndata";
  sensor_control_path = database_path + "/sensor_control";

  // Set pinmode
  pinMode(motion_pin, INPUT);
  pinMode(builtin_led, OUTPUT);

  wifiInit();     // Initialize Connection with location WiFi
  firebaseInit(); // Initialise firebase configuration and signup anonymously
  cameraInit();   // Initialise OV2640 camera module
}

void loop()
{
  sensor_control = Firebase.getBool(firebase_data, sensor_control_path);
  if (sensor_control)
  {
    //is_motion_detected = false;
    is_motion_detected = digitalRead(motion_pin);
    sendMotionSignalToFirebase(is_motion_detected);
    if (is_motion_detected)
    {
      Serial.println("motion detected");
      digitalWrite(builtin_led, HIGH);
      getPhotoThenSendToFirebase();
      digitalWrite(builtin_led, LOW);
      delay(10000);
    }
    delay(100);
  }
}
/*}***************************************************************************/