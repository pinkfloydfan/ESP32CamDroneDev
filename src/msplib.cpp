#include <iostream>
#include <string>
#include <arduino.h>
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

            void writeRawRCPacket(int *rcChannels) {

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
        void readData() {
            delay(100);

            unsigned char count = 0;

            int16_t roll;
            int16_t pitch;
            int16_t yaw;

            while (mspPort->available()) {
                //count += 1;
                unsigned char c = mspPort->read();
                Serial.println(c);
                int count = 0;


                if (c == 36) {

                    count = 0;

                } else {

                    count += 1;

                }

                if (count == 4 && c == 108) {

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
                        pitch += c;
                        break;
                    case 8:
                        pitch <<= 8;
                        pitch += c;
                        pitch = (pitch & 0xFF00) >> 8 | (pitch & 0x00FF) << 8; // Reverse the order of bytes
                        break;
                    case 9:
                        yaw += c;
                        break;
                    case 10:
                        yaw <<= 8;
                        yaw += c;
                        yaw = (yaw & 0xFF00) >> 8 | (yaw & 0x00FF) << 8; // Reverse the order of bytes
                        break;
                }


                }


            }
            
            Serial.print("Roll: " + String(roll/10.0));
            Serial.print(" Pitch: " + String(pitch/10.0));
            Serial.println(" Yaw: " + String(yaw));
            
        }

    };

}
    






    //Function to return a prepared RC packet from an int array of rc channels
 
