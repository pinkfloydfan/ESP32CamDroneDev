/*
Just a helpful note to self regarding the camera resolutions you can set
typedef enum {
    FRAMESIZE_96X96,    // 96x96
    FRAMESIZE_QQVGA,    // 160x120
    FRAMESIZE_QCIF,     // 176x144
    FRAMESIZE_HQVGA,    // 240x176
    FRAMESIZE_240X240,  // 240x240
    FRAMESIZE_QVGA,     // 320x240
    FRAMESIZE_CIF,      // 400x296
    FRAMESIZE_HVGA,     // 480x320
    FRAMESIZE_VGA,      // 640x480
    FRAMESIZE_SVGA,     // 800x600
    FRAMESIZE_XGA,      // 1024x768
    FRAMESIZE_HD,       // 1280x720
    FRAMESIZE_SXGA,     // 1280x1024
    FRAMESIZE_UXGA,     // 1600x1200
    // 3MP Sensors
    FRAMESIZE_FHD,      // 1920x1080
    FRAMESIZE_P_HD,     //  720x1280
    FRAMESIZE_P_3MP,    //  864x1536
    FRAMESIZE_QXGA,     // 2048x1536
    // 5MP Sensors
    FRAMESIZE_QHD,      // 2560x1440
    FRAMESIZE_WQXGA,    // 2560x1600
    FRAMESIZE_P_FHD,    // 1080x1920
    FRAMESIZE_QSXGA,    // 2560x1920
    FRAMESIZE_INVALID
} framesize_t;
*/


#include <iostream>

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <HardwareSerial.h>

#include <string>
#include <sstream>
#include <vector>

//Tells the ESP initialization code what camera we#'re using 
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include "esp_camera.h"

#include "StreamUtils.h"
#include "ArduinoJson.h"
#include "base64.h"

//Variables for the MSP library
#include "msplib.cpp"
msplib::MspSender mspSender;
msplib::MspReceiver mspReceiver;


//DUAL CORE BABY

TaskHandle_t Core0TaskHandle;

int currentTime;

int loopCtr = 0;

//How many RC channels we need to write to the flight controller. Needed for the MSP protocol checksum
const int rcChannelNumber = 8;

//WiFi AP constants. 

const bool APMode = true; //choose between access point or connect to wifi network

const char* ap_ssid = "drone";
const char* ap_password = "password";

const char* ssid = "VM1013606";
const char* password = "qxmm5ssGpfgw";

//networking gibberish
IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);

//The RC signal for each channel: Aileron/Elevator/Throttle/Rudder and 4 aux channels
volatile int rcChannels[8] = {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};

//time counter for serial signal send loop
uint8_t loopTime = 10;

// how many bytes each MSP_SET_RAW_RC packet is. It really shouldn't be here.
const int mspPacketLength = 22;

//uint8_t mspPacket [mspPacketLength]; 

//TODO: improve this implementation
bool wsClientConnected = false; 
//webSocket client ID defaults to 0 
uint8_t wsClientID = 0;

//Initialises websocket server
WebSocketsServer webSocket = WebSocketsServer(80, "*");


//initialises OV2640 camera, relies upon the pin definitions in camera_pins.h
void setupCamera() { 
  Serial.printf("attempting to setup camera...");


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
  //Have tried raw, didn't work! 
  config.pixel_format = PIXFORMAT_JPEG; 
  //init with high specs to pre-allocate larger buffers

  /*
  it appears the ESP32CAM may have PSRAM but we're using it to stream so potato quality is enforced and we do not use PSRAM. 
  if(psramFound()){
    Serial.printf("psram");
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    Serial.printf("psramn't");

    config.frame_size = FRAMESIZE_SVGA;
    //potato quality
    config.jpeg_quality = 32;
    config.fb_count = 1;

    }

  */

    //early 2000s nokia phone camera quality
    //tbf consider improving the websocket exchange so this can be adjusted from the client?
    config.frame_size = FRAMESIZE_HVGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;



#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }


  // code not needed but these s->*do something* functions are interesting 
  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }


#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  Serial.printf("Camera ready!");

}

//set up wifi access point
void setupAP() {

  if (APMode == true) {

    Serial.print("Setting soft-AP configuration ... ");
    Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

    // create access point 
    Serial.println("Creating AP");
    WiFi.softAP(ap_ssid, ap_password, 1, false, 4);
    // Print our IP address
    Serial.println("Connected!");
    Serial.print("My IP address: ");
    Serial.println(WiFi.softAPIP());

  } else {

    WiFi.begin(ssid, password);

  }



}


//sends an image buffer blob over the websocket. This is currently a compressed JPEG binary.
void sendImageBuffer(const uint8_t* imageBuffer, const size_t len) {

  //check if there's a websocket client connected - will need more rigour when scaling this for multiple aircraft
  if (wsClientConnected == true) {

      //send the binary image buffer + length of the buffer
      webSocket.sendBIN(wsClientID, imageBuffer, len);
  }


}

void sendImageString(String image) {
  //Serial.println(image);

  String stampedImage = (image + ", " + millis());

  webSocket.sendTXT(wsClientID, stampedImage);
  
}


//attempts to access ESP camera and get its file buffer
void takeAndSendImage() {
  //*fb is pointer to image data
  camera_fb_t *fb = esp_camera_fb_get(); 

  if (!fb) {
    Serial.println("Could not capture image");
  } else {
    
      //-> = access members buf (image buffer) and len (length of image buffer array) from pointer *fb
      // Initializes array from the image buffer and gets its length
    const uint8_t *data = (const uint8_t *)fb->buf;
    const size_t len = fb -> len;
    
    mspReceiver.setCameraStamp();

    const String base64str = base64::encode(data, len);
    //Serial.println(base64str);
    sendImageBuffer(data, len);

    //sendImageString(base64str); 
  }

}

// polls msplib for the attitude information and then sends it as a string over websockets
void getAndSendAttitude(bool CamSync) {

  String attitudeString;

  if (CamSync == false) {
    attitudeString = mspReceiver.getAttitude();
  } else {
    attitudeString = mspReceiver.getCamSyncAttitude();
  }
  //Serial.println(attitudeString);
    
    //check if there's a websocket client connected - will need more rigour when scaling this for multiple aircraft
  if (wsClientConnected == true) {

      //  Serial.println(attitudeString);

      //send the binary image buffer + length of the buffer
      webSocket.sendTXT(wsClientID, attitudeString);
  } 


}


// opens the read & write serial ports on the ESP and passes their address to the MSP communication classes
void setupMSP() {


  //3rd parameter = rx GPIO, 4th = tx GPIO. 
   Serial1.begin(115200, SERIAL_8N1, 13, 12);
   Serial2.begin(115200, SERIAL_8N1, 15, 14);

   mspSender.setSerialPort(&Serial1);
   mspReceiver.setSerialPort(&Serial2);


}

// turns a raw RC command string seperated by commas into a vector
void processCommandArray(std::string input) {

    std::vector<int> vect;

    std::stringstream ss(input);

    for (int i = 0; ss >> i;) {
        vect.push_back(i);    
        if (ss.peek() == ',') {
                    ss.ignore();

        }
    }

    for (int j = 0; j < vect.size(); j++) {
      rcChannels[j] = vect[j];
    }
}


// Called when receiving any WebSocket message
void onWebSocketEvent(uint8_t num,
                      WStype_t type,
                      uint8_t * payload,
                      size_t length) {

  // Figure out the type of WebSocket event
  switch(type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      wsClientConnected = false;
      Serial.printf("[%u] Disconnected!\n", num);
      break;

    // New client has connected
    case WStype_CONNECTED:
      {
        //ghetto solution for now: as soon as a client connects, store its ID as a global. 
        wsClientConnected = true;
        wsClientID = num;
        IPAddress ip = webSocket.remoteIP(num);
        /*
        Serial.printf("[%u] Connection from ", num);
        Serial.println(ip.toString());
        */
      }
      break;

    case WStype_TEXT:
      wsClientID = num;

      //Serial.println((const char*)payload);

      
      // assumes any incoming text connection is a command array
      processCommandArray((const char*)payload);
      
      mspSender.writeRawRCPacket(rcChannels);
      
      //webSocket.sendTXT(num, payload);
      
      break;

    // For everything else: do nothing
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }
}

//core 0 process -> handles networking and serial I/O
void Core0Task(void *pvParameters) {






    for(;;) {

      mspSender.writeAttitudeRequest();
      mspSender.writeIMURequest();

      mspReceiver.readMSPOutput();

      delay(20);



    }

}

void setup() {

  currentTime = millis();


    // Start WebSocket server and assign callback

  
  // Start Serial monitor port
  Serial.begin(115200);

  setupCamera();

  setupAP();

  setupMSP();

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  xTaskCreatePinnedToCore(Core0Task,
                          "Core0Task",
                          10000,
                          NULL,
                          5,
                          &Core0TaskHandle,
                          0);

}

// core 1 process -> sends image & aircraft attitude
void loop() {

    if ((micros() - currentTime) > loopTime) {

      loopCtr++;

      if (loopCtr > 3) {
          takeAndSendImage(); 
          getAndSendAttitude(true);
          loopCtr = 0;

      } else {
        getAndSendAttitude(false);
      }

      currentTime = micros();
      webSocket.loop();


    } 




}