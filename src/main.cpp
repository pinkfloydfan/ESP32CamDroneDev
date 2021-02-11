#include <WiFi.h>
#include <WebSocketsServer.h>
#include <HardwareSerial.h>

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

int rcChannels[14] = {1500, 1400, 1300, 1200, 1100, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500, 1500};
uint32_t sbusTime = 0;


uint8_t ibusPacket [ibusPacketLength];
uint32_t ibusTime = 0;


//SBUS end

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


// Called when receiving any WebSocket message
void onWebSocketEvent(uint8_t num,
                      WStype_t type,
                      uint8_t * payload,
                      size_t length) {

  // Figure out the type of WebSocket event
  switch(type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;

    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connection from ", num);
        Serial.println(ip.toString());
      }
      break;

    // Echo text message back to client
    case WStype_TEXT:
      Serial.printf("[%u] Text: %s\n", num, payload);
      webSocket.sendTXT(num, payload);
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
  //Serial.begin(115200);
  Serial.begin(115200); //test 

  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  // create access point 
  Serial.println("Creating AP");
  WiFi.softAP(ssid, password, 1, false, 4);
  // Print our IP address
  Serial.println("Connected!");
  Serial.print("My IP address: ");
  Serial.println(WiFi.softAPIP());

  // Start WebSocket server and assign callback
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  //Setup IBUS output
  ibusPacket[0] = 0x20;
  ibusPacket[1] = 0x40;

  Serial1.begin(115200, SERIAL_8N1, 13, 12);

}

void loop() {
  
    uint32_t currentMillis = millis();

    /*
     * Here you can modify values of rcChannels while keeping it in 1000:2000 range
     */
      if (currentMillis > ibusTime) {
        prepareibusPacket() ;
        //uart_write_bytes(uart_num, rcChannels, SBUS_PACKET_LENGTH)
        //Serial.print("Writing SBUS... ");
        Serial1.write(ibusPacket, ibusPacketLength);
        //Serial.write(sbusPacket, SBUS_PACKET_LENGTH);


        ibusTime = currentMillis + 7;
    }






  // Look for and handle WebSocket data - disabled just in case this was interfering with sbus
  //webSocket.loop();
}