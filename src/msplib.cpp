#include <iostream>
#include <string>
//#include <arduino.h>
#include <HardwareSerial.h>

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

    };

    class MspReceiver: public MspInterface {

        #define MSP_HEADER_BEGIN 36;

        public:
        //todo: this can probably take a message code as a function parameter and be extended to read more generic MSP messages
        void readData() { 
            delay(100);

            int16_t roll = 0;
            int16_t pitch = 0;
            int16_t yaw = 0;

            int count = 0;
            bool attitudeMessageDetected = false; 

            while (mspPort->available()) {
                //count += 1;
                unsigned char c = mspPort->read();

                //int messageLength; 

                if (c == 36) {

                    //Serial.println("MSP begin character detected");
                    //resets the byte counter when the character $ (indicating MSP packet start) is detected
                    count = 0;
                    //Boolean that is TRUE when the 4th character of the MSP message corresponds to 108 = ATTITUDE
                    attitudeMessageDetected = false; 

                } else {
                    
                    //increments the byte counter
                    count += 1;

                }


                if (count == 4) {

                    //Serial.println(" Message type byte: " + String(c));

                    if (c == 108) {
                
                        attitudeMessageDetected = true; 
                        //Serial.println("Code 108 detected");

                    }


                }

                if (attitudeMessageDetected == true) {

                    //Serial.println("Reading attitude packet");

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

                            
                            Serial.println("Roll: " + String(roll/10.0));
                            Serial.println(" Pitch: " + String(pitch/10.0));
                            Serial.println(" Yaw: " + String(yaw));                          
                

                            break;
                    }


                }

                //Serial.println(" byte count: " + String(count));

            }
            
        }

    };

}
    






    //Function to return a prepared RC packet from an int array of rc channels
 
