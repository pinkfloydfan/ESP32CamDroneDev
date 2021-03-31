#include <iostream>
#include <string>
//#include <arduino.h>
#include <HardwareSerial.h>
#include <sstream>

namespace msplib {

    class MspInterface {
        protected: 
            HardwareSerial *mspPort;

        public: 

            void setSerialPort(HardwareSerial *serialPort) {
                this->mspPort = serialPort;
            }
    };
    //
    class MspSender : public MspInterface {
        private:

            const int MSP_CODE_RAWRC   = 200;
            const int MSP_CODE_REQUESTATTITUDE = 109;

            const int rcChannelCount   = 8;
            const int rawRCPacketWidth = 22;


        public:

            void writeRawRCPacket(volatile int *rcChannels) {

                unsigned char mspPacket[rawRCPacketWidth];

                //Elements 0-2 are the MSP header symbols
                mspPacket[0]       = '$';
                mspPacket[1]       = 'M';
                mspPacket[2]       = '<'; 

                //Length of payload = 16 bytes, 8 channels of 2 bytes each
                mspPacket[3]       = 16; 

                //code to tell the flight controller that we're sending a raw RC signal 
                mspPacket[4] = MSP_CODE_RAWRC;


                for (int i = 0; i < rcChannelCount; i++) {

                    int channelValue = *(rcChannels + i);


                    //converts each channel value into two little endian bytes  
                    int byte0 = channelValue % 256;
                    int byte1 = channelValue / 256;

                    mspPacket[2*i + 5] = byte0;
                    mspPacket[2*i + 6] = byte1;
                }

                int checksum = 0;
                //I SPENT HOURS DEBUGGING THIS ONLY TO REALISE I GOT THE CHECKSUM WRONG - mspPacketLength-1 is the correct iteration count. 
                for (int i = 3; i<rawRCPacketWidth-1; i++) {
                    checksum ^= mspPacket[i];
                }

                mspPacket[rawRCPacketWidth-1] = checksum;

                mspPort->write(mspPacket, 22);

            }

            void writeAttitudeRequest() {
                
                unsigned char mspPacket[] = {'$', 'M', '<', 0x0, 0x6C, 0x6C};

                mspPort->write(mspPacket, 6);

            }

            void writeIMURequest() {

                unsigned char mspPacket[] = {'$', 'M', '<', 0x0, 0x66, 0x66};

                mspPort->write(mspPacket, 6);

            }

    };

    class MspReceiver: public MspInterface {

        #define MSP_HEADER_BEGIN 36
        #define MSP_ATTITUDE 108
        #define MSP_IMU 102

        bool readLock = false;
        int16_t attitude[9] = {0};

        public:

        //Function to read roll, pitch, yaw messages from the serial port by detecting the right MSP code, returning a pointer to an int[3]
        //todo: this can probably take a message code as a function parameter and be extended to read more generic MSP messages

        /*
        void printMSP() {
            while (mspPort->available()) {
                unsigned char c = mspPort->read();
                Serial.print(c);
            }
        }
        */

        void getAttitude() {

            readLock = true;

            
            Serial.println("getting attitude...?");

            Serial.println(" roll: " + String(attitude[0]));
            Serial.println(" pitch: " + String(attitude[1]));
            Serial.println(" yaw: " + String(attitude[2])); 

            Serial.println(" accx: " + String(attitude[3]));
            Serial.println(" accy: " + String(attitude[4]));
            Serial.println(" accz: " + String(attitude[5]));     

            Serial.println(" gyrx: " + String(attitude[6]));
            Serial.println(" gyry: " + String(attitude[7]));
            Serial.println(" gyrz: " + String(attitude[8]));  

            /*

            std::ostringstream os; 

            os << "{";
            for (int i = 0; 8; i++) {
                
                    Serial.println(String(attitude[i]) + ",");


  
            }

            os << "}";
            
            
            std::string s = os.str();
            const char* cstr = s.c_str();

            Serial.println(cstr);

            */

            readLock = false;

            
            //return attitude;
        }

        void readMSPOutput() {

            int count = 0;
            int messageType = 0; 



            //Serial.println("trying to read msp...?");

            int16_t roll = 0;
            int16_t pitch = 0;
            int16_t yaw = 0;

            int16_t accx = 0;
            int16_t accy = 0;
            int16_t accz = 0;

            int16_t gyrx = 0;
            int16_t gyry = 0;
            int16_t gyrz = 0;


            while (mspPort->available()) {

                //count += 1;
                unsigned char c = mspPort->read();
                //Serial.println(c);

                //int messageLength; 

                if (c == MSP_HEADER_BEGIN) {

                    //resets the byte counter when the character $ (indicating MSP packet start) is detected
                    count = 0;
                    //Boolean that is TRUE when the 4th character of the MSP message corresponds to 108 = ATTITUDE
                    messageType = 0; 

                } else {
                    
                    //increments the byte counter
                    count += 1;

                }


                if (count == 4) {

                    //Serial.println(" Message type byte: " + String(c));

                    messageType = c;

                }

                if (messageType == MSP_ATTITUDE) {

                    switch (count) {
                        case 5:
                            roll = c;
                            break;
                        case 6:
                            roll <<= 8;
                            roll += c;
                            roll = (roll & 0xFF00) >> 8 | (roll & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 7:
                            pitch = c;
                            break;
                        case 8:
                            pitch <<= 8;
                            pitch += c;
                            pitch = (pitch & 0xFF00) >> 8 | (pitch & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 9:        
                            yaw = c;
                            break;
                        case 10: //final byte, hence print the roll/pitch/yaw value (and in the future trigger a callback to send them )
                            //Serial.println("bois we at 10");

                            yaw <<= 8;
                            yaw += c;
                            yaw = (yaw & 0xFF00) >> 8 | (yaw & 0x00FF) << 8; // Reverse the order of bytes  

                            //static int16_t orientation[3];   

                            if (readLock == false) {
                                attitude[0] = roll;
                                attitude[1] = pitch;
                                attitude[2] = yaw;
                            }
                        

                            //return orientation;

                            //break;
                    }

                
                //Serial.println(" byte count: " + String(count));

                } else if (messageType == MSP_IMU) {
                    //Serial.println("Reading imu packet");

                    switch (count) {
                        case 5:
                            accx = c;
                            break;
                        case 6:
                            accx <<= 8;
                            accx += c;
                            accx = (accx & 0xFF00) >> 8 | (accx & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 7:
                            accy = c;
                            break;
                        case 8:
                            accy <<= 8;
                            accy += c;
                            accy = (accy & 0xFF00) >> 8 | (accy & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 9:        
                            accz = c;
                            break;
                        case 10: 
                            accz <<= 8;
                            accz += c;
                            accz = (accz & 0xFF00) >> 8 | (accz & 0x00FF) << 8; // Reverse the order of bytes  
                        case 11:
                            gyrx = c;
                            break;
                        case 12:
                            gyrx <<= 8;
                            gyrx += c;
                            gyrx = (gyrx & 0xFF00) >> 8 | (gyrx & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 13:
                            gyry = c;
                            break;
                        case 14:
                            gyry <<= 8;
                            gyry += c;
                            gyry = (gyry & 0xFF00) >> 8 | (gyry & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 15:        
                            gyrz = c;
                            break;
                        case 16: 
                            gyrz <<= 8;
                            gyrz += c;
                            gyrz = (gyrz & 0xFF00) >> 8 | (gyrz & 0x00FF) << 8; // Reverse the order of bytes  

                            //static int16_t imuData[6];

                            if (readLock == false) {

                                attitude[3] = accx;
                                attitude[4] = accy;
                                attitude[5] = accz;
                                attitude[6] = gyrx;
                                attitude[7] = gyry;
                                attitude[8] = gyrz; 

                            }
                            


                            

                            

                            //return imuData;

                    }

                
                //Serial.println(" byte count: " + String(count));

                }

            }   


        }


        int16_t* readMSPOrientation() { 

            int16_t roll = 0;
            int16_t pitch = 0;
            int16_t yaw = 0;

            int count = 0;
            int messageType = 0; 

            while (mspPort->available()) {
                //count += 1;
                unsigned char c = mspPort->read();

                //int messageLength; 

                if (c == MSP_HEADER_BEGIN) {

                    //resets the byte counter when the character $ (indicating MSP packet start) is detected
                    count = 0;
                    //Boolean that is TRUE when the 4th character of the MSP message corresponds to 108 = ATTITUDE
                    messageType = 0; 

                } else {
                    
                    //increments the byte counter
                    count += 1;

                }


                if (count == 4) {

                    //Serial.println(" Message type byte: " + String(c));

                    messageType = c;

                }

                if (messageType == MSP_ATTITUDE) {

                    switch (count) {
                        case 5:
                            roll = c;
                            break;
                        case 6:
                            roll <<= 8;
                            roll += c;
                            roll = (roll & 0xFF00) >> 8 | (roll & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 7:
                            pitch = c;
                            break;
                        case 8:
                            pitch <<= 8;
                            pitch += c;
                            pitch = (pitch & 0xFF00) >> 8 | (pitch & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 9:        
                            yaw = c;
                            break;
                        case 10: //final byte, hence print the roll/pitch/yaw value (and in the future trigger a callback to send them )
                            //Serial.println("bois we at 10");

                            yaw <<= 8;
                            yaw += c;
                            yaw = (yaw & 0xFF00) >> 8 | (yaw & 0x00FF) << 8; // Reverse the order of bytes  

                            //static int16_t orientation[3];   
                            
                            attitude[0] = roll;
                            attitude[1] = pitch;
                            attitude[2] = yaw;

                            Serial.println(" roll: " + String(attitude[0]));
                            Serial.println(" pitch: " + String(attitude[1]));
                            Serial.println(" yaw: " + String(attitude[2])); 

                            //return orientation;

                            //break;
                    }

                
                //Serial.println(" byte count: " + String(count));

                }

            }   

            return(0); 
            
        }

        //reads gyroscope and accelerometer values, returning a pointer to an int[6].
        int16_t* readMSPIMU() { 
            //delay(20);

            int count = 0;
            int messageType = 0; 

            int16_t accx = 0;
            int16_t accy = 0;
            int16_t accz = 0;

            int16_t gyrx = 0;
            int16_t gyry = 0;
            int16_t gyrz = 0;

            while (mspPort->available()) {
                //count += 1;
                unsigned char c = mspPort->read();

                //int messageLength; 

                if (c == MSP_HEADER_BEGIN) {

                    //Serial.println("MSP begin character detected");
                    //resets the byte counter when the character $ (indicating MSP packet start) is detected
                    count = 0;
                    //Boolean that is TRUE when the 4th character of the MSP message corresponds to 108 = ATTITUDE
                    messageType = 0; 

                } else {
                    
                    //increments the byte counter
                    count += 1;

                }


                if (count == 4) {

                    //Serial.println(" Message type byte: " + String(c));

                    messageType = c;

                }

                if (messageType == MSP_IMU) {

 
                    //Serial.println("Reading imu packet");

                    switch (count) {
                        case 5:
                            accx = c;
                            break;
                        case 6:
                            accx <<= 8;
                            accx += c;
                            accx = (accx & 0xFF00) >> 8 | (accx & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 7:
                            accy = c;
                            break;
                        case 8:
                            accy <<= 8;
                            accy += c;
                            accy = (accy & 0xFF00) >> 8 | (accy & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 9:        
                            accz = c;
                            break;
                        case 10: 
                            accz <<= 8;
                            accz += c;
                            accz = (accz & 0xFF00) >> 8 | (accz & 0x00FF) << 8; // Reverse the order of bytes  
                        case 11:
                            gyrx = c;
                            break;
                        case 12:
                            gyrx <<= 8;
                            gyrx += c;
                            gyrx = (gyrx & 0xFF00) >> 8 | (gyrx & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 13:
                            gyry = c;
                            break;
                        case 14:
                            gyry <<= 8;
                            gyry += c;
                            gyry = (gyry & 0xFF00) >> 8 | (gyry & 0x00FF) << 8; // Reverse the order of bytes
                            break;
                        case 15:        
                            gyrz = c;
                            break;
                        case 16: 
                            gyrz <<= 8;
                            gyrz += c;
                            gyrz = (gyrz & 0xFF00) >> 8 | (gyrz & 0x00FF) << 8; // Reverse the order of bytes  

                            //static int16_t imuData[6];
                            
                            attitude[3] = accx;
                            attitude[4] = accy;
                            attitude[5] = accz;
                            attitude[6] = gyrx;
                            attitude[7] = gyry;
                            attitude[8] = gyrz; 

                            
                            Serial.println(" accx: " + String(attitude[3]));
                            Serial.println(" accy: " + String(attitude[4]));
                            Serial.println(" accz: " + String(attitude[5]));     

                            Serial.println(" gyrx: " + String(attitude[6]));
                            Serial.println(" gyry: " + String(attitude[7]));
                            Serial.println(" gyrz: " + String(attitude[8]));  
                            

                            //return imuData;

                    }

                
                //Serial.println(" byte count: " + String(count));

                }

            }   

            return(0); 
            
        }

    };

}
    






    //Function to return a prepared RC packet from an int array of rc channels
 
