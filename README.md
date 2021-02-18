# ESP32CamDroneDev

Attempting to leverage the ESP32Cam's abilities to add some intelligence to drones.  The main idea is using websockets for video streaming/control/telemetry, with the ESP translating websocket commands into a mock RC signal that can then be fed into a drone flight controller. The video stream can then be ingested by an offboard computer running a more powerful CV package. 

The main routine of this program is able to listen for a formatted Websocket string containing roll/pitch/throttle/yaw commands and turn them into a serial Flysky iBus signal, which can be fed into an off-the-shelf hobby grade flight controller running Betaflight or similar. iBus was picked because it's a simple, relatively decently documented protocol. Some sample code for encoding iBus can be found here: https://github.com/pinkfloydfan/iBUSEncoder 

Additional functions capture a potato-quality image from the ESP32Cam's OV2640 sensor. In fact, a low res video stream may work even better with optical flow algorithms like Lucas-Kanade. The standard ESP32Cam library contains the supremely useful `camera_fb_t` structure which allows direct manipulation of the pixels of a captured image. In my implementation, I capture JPEG data, and then send the raw binary as blobs over Websockets. It seems that QVGA is the best compromise between picture quality and speed. This is a crude method, but it's lightweight and low-latency. A bare-bones companion desktop app for interacting with the drone over websockets can be found here: https://github.com/pinkfloydfan/ESPDroneClient

# To compile

- You need a FTDI tool 
- Get yourself PlatformIO from https://platformio.org/
- Some comments in the code may (or may not) explain how to wire everything up. 

# TODOs

- attach a proper pinout of the ESP32Cam as used in this experiment somewhere and make it easier to compile 
- improve the 2-way comms between board and camera, perhaps even allowing tweaking the camera resolution on the fly

