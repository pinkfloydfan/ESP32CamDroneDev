#include <iostream>
#include <string>
#include <arduino.h>
#include <HardwareSerial.h>


namespace msplib {

    const int MSP_CODE_RAWRC   = 200;
    const int rcChannelCount   = 8;
    const int rawRCPacketWidth = 22;


    void writeRawRCPacket(int *rcChannels, HardwareSerial *serialPort) {

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

        serialPort->write(mspPacket, 22);

    }

        



    

}





    //Function to return a prepared RC packet from an int array of rc channels
 
