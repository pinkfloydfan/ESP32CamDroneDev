/*
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

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include "esp_camera.h"

// Constants

//Allocate GPIO12 to sbus tx
//#define E_UART_TXD (GPIO_NUM_12)

HardwareSerial Serial_1(1);
//#define SERIAL1_TX 12

const int ibusPacketLength = 32;
const int rcChannelNumber = 8;
//WiFi AP
const char* ssid = "COVID-19 5G Tower";
const char* password = "password";

int uart_num; 

IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);

int rcChannels[14] = {1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};


uint8_t ibusPacket [ibusPacketLength];
//time counter for updating ibus signals
uint32_t loopTime = 0;
//time interval between camera and rc signal updates (ms)
const uint8_t loopDelay = 50;

//TODO: improve this implementation
bool wsClientConnected = false; 
//webSocket client ID defaults to 0 
uint8_t wsClientID = 0;

// Globals 
// also we've disabled CORS access control with the 2nd argument...
WebSocketsServer webSocket = WebSocketsServer(80, "*");

void prepareibusPacket() {
  for (int i = 0; i < rcChannelNumber; i++) {
    uint16_t channelValue = rcChannels[i];

    //little endian

    uint8_t byte0 = channelValue % 256;
    uint8_t byte1 = channelValue / 256;

    ibusPacket[2*i + 2] = byte0;
    ibusPacket[2*i + 3] = byte1;
  }
  uint16_t total = 0xFFFF; 
  //for calculating checksum - use 0xFFFF and subtract each byte from i = 0 -> i = packetlength-2 from it 
  for (int i = 0; i<ibusPacketLength-2; i++) {
    total -= ibusPacket[i];
  }
  uint8_t checksumByte0 = total % 256;
  uint8_t checksumByte1 = total / 256;

  ibusPacket[ibusPacketLength-2] = checksumByte0;
  ibusPacket[ibusPacketLength-1] = checksumByte1;

}

//initialises OV2640, relies upon the pin definitions in camera_pins.h
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
  it appears the ESP32CAM may have PSRAM but we're using it to stream so potato quality is enforced. 
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
    //tbf consider improving the comms so this can be adjusted from the client?
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 32;
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

  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  // create access point 
  Serial.println("Creating AP");
  WiFi.softAP(ssid, password, 1, false, 4);
  // Print our IP address
  Serial.println("Connected!");
  Serial.print("My IP address: ");
  Serial.println(WiFi.softAPIP());

}

//sends an image buffer blob over the websocket. N.B. maybe binary will work better?
void sendImageBuffer(const uint8_t* imageBuffer, const size_t len) {

  //check if there's a websocket client connected - will need more rigour when scaling this for multiple aircraft
  if (wsClientConnected == true) {

      //send the binary image buffer + length of the buffer
      webSocket.sendBIN(wsClientID, imageBuffer, len);
  }


}


//attempts to access ESP camera and get its file buffer
void takeImage() {
  //*fb is pointer to image data
  camera_fb_t *fb = esp_camera_fb_get(); 

  if (!fb) {
    Serial.println("Could not capture image");
  } else {
    
      //-> = access members buf (image buffer) and len (length of image buffer array) from pointer *fb
    const uint8_t *data = (const uint8_t *)fb->buf;
    const size_t len = fb -> len;
    sendImageBuffer(data, len); 
  }

}


// gives iBus packet the correct header bytes
void setupIbus() {
  ibusPacket[0] = 0x20;
  ibusPacket[1] = 0x40;
   Serial1.begin(115200, SERIAL_8N1, 13, 12);

}

//TODO: perhaps flesh this function out to parse some 'headers' for websocket messages
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
        Serial.printf("[%u] Connection from ", num);
        Serial.println(ip.toString());
      }
      break;

    case WStype_TEXT:
      wsClientID = num;
      Serial.print(wsClientID);
      Serial.printf("[%u] Text: %s\n", num, payload);
      processCommandArray((const char*)payload);
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


void setup() {

  // Start Serial monitor port
  Serial.begin(115200);

  setupCamera();

  setupAP();

  setupIbus();

  // Start WebSocket server and assign callback
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  //Setup IBUS output


}

void loop() {
  
    uint32_t currentMillis = millis();

    /*
     * Here you can modify values of rcChannels while keeping it in 1000:2000 range
     */

    
      if (currentMillis > loopTime) {
        takeImage() ;
        prepareibusPacket() ;

        Serial1.write(ibusPacket, ibusPacketLength);

        loopTime = currentMillis + loopDelay;
    }

  webSocket.loop();
}