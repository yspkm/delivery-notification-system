#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

#include "addons/TokenHelper.h" //Provide the token generation process info.
#include "addons/RTDBHelper.h" //Provide the RTDB payload printing info and other helper functions

#include "esp_camera.h"
#include "Base64.h"
#include "private_info.h" //Wifi ssid, pwd, api-key, project-url

/**************************Device Information*********************************/
// Device ID
#define DEVICE_UID "1X"
// camera module
#define CAMERA_MODEL_AI_THINKER
// camera fin
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
/*****************************************************************************/

/**************************global variables***********************************/
String device_location = "Entrance"; // Device Location config

/*Data Objects*/
FirebaseData fbdo; // Firebase Realtime Database Object
FirebaseAuth auth; // Firebase Authentication Object
FirebaseConfig config; // Firebase configuration Object

String databasePath = ""; // Firebase database path
String fuid = ""; // Firebase Unique Identifier

unsigned long elapsedMillis = 0; // Stores the elapsed time from device start up
unsigned long update_interval = 10000; // The frequency of sensor updates to firebase, set to 10seconds
int count = 0; // Dummy counter to test initial firebase updates
bool isAuthenticated = false; // Store device authentication status
/*****************************************************************************/

/***************user-defined function*****************************************/
//camera instruction

String photo2Base64(void) {
    camera_fb_t* fb = esp_camera_fb_get(); 
    String ret = "data:image/jpeg;base64,"; // image file

    char *input = (char *)fb->buf;
    char output[base64_enc_len(3)];

    for (int i=0;i<fb->len;i++) {
      base64_encode(output, (input++), 3);
      if (i%3==0) ret += String(output);
    }
    ret += String(output);
    
    esp_camera_fb_return(fb);
    
    return ret;
}

void setCameraConfig(void) 
{
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);
}

void wifiInit() 
{ 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
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
  config.api_key = API_KEY; // configure firebase API Key
  config.database_url = DATABASE_URL;// configure firebase realtime database url
  Firebase.reconnectWiFi(true);// Enable WiFi reconnection 

  Serial.println("------------------------------------");
  Serial.println("Sign up new user...");

  if (Firebase.signUp(&config, &auth, "", "")) { // Sign in to firebase Anonymously
    Serial.println("Success");
    isAuthenticated = true;
    databasePath = "/" + device_location; // Set the database path where updates will be loaded for this device
    fuid = auth.token.uid.c_str();
  } else {
    Serial.printf("Failed, %s\n", config.signer.signupError.message.c_str());
    isAuthenticated = false;
  }
  config.token_status_callback = tokenStatusCallback; // Assign the callback function for the long running token generation task, see addons/TokenHelper.h
  Firebase.begin(&config, &auth); // Initialise the firebase library
}


// test function
void databaseTest(void) 
{ 

  // Check that 10 seconds has elapsed before, device is authenticated and the firebase service is ready.
  if (millis() - elapsedMillis > update_interval && isAuthenticated && Firebase.ready()) { 
    elapsedMillis = millis();
    Serial.println("------------------------------------");
    Serial.println("Set int test...");
    // Specify the key value for our data and append it to our path
    String node = databasePath + "/captured"; 
    // Send the value our count to the firebase realtime database 
    if (Firebase.setString(fbdo, node.c_str(), photo2Base64())) {
      // Print firebase server response
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
      Serial.println("ETag: " + fbdo.ETag());
      Serial.print("VALUE: ");
      printResult(fbdo); //see addons/RTDBHelper.h
      Serial.println("------------------------------------");
      Serial.println();
    } else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
      Serial.println("------------------------------------");
      Serial.println();
    }
  }
}
/*****************************************************************************/

void setup() 
{
  Serial.begin(115200); // Initialise serial communication for local diagnostics

  wifiInit(); // Initialise Connection with location WiFi
  firebaseInit();// Initialise firebase configuration and signup anonymously
  setCameraConfig(); // Initialise OV2640 camera module
}

void loop() 
{
  databaseTest();
  delay(10000);
}