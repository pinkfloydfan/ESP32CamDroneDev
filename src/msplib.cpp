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
        
        // Returns the attitude & IMU information in the form { roll, pitch, yaw, accx, accy, accz, gyrx, gyry, gyrz }
        String getAttitude() {
            
            //prevent race condition
            readLock = true;

            String attitudeString = (String(attitude[0]) + ", " + String(attitude[1]) + ", " + String(attitude[2]) + ", " +  String(attitude[3]) + ", " +  String(attitude[4]) + ", " + String(attitude[5]) + ", " + String(attitude[6]) + ", " + String(attitude[7]) + ", " + String(attitude[8])) ;

            readLock = false;

            return(String(attitudeString));

        }

        // Function to listen to the serial port for MSP messages, currently only IMU information and attitude information is supported. Information is then written to the attitude array. 
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


    };

}
    






    //Function to return a prepared RC packet from an int array of rc channels
 
