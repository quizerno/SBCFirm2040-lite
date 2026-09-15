# SBCFirm2040-lite

Seel Battalion Controller Firmware for the RP2040 with Input Options. This enables you to emulate a Steel Battalion Controller on Xbox.
The options it includes are:
  * Using up to 4 HID devices to play Steel Battalion
  * Using direct GPIO inputs to play Steel Battalion
  * Connecting an existing (or emulated) Steel Battalion Controller and altering the settings
  * Any of the above together


# History
SBCFirm2040-lite is a RP2040 package I cobbled together from [SteelBattalionControllerFirmware_RP2040](https://github.com/faha223/SteelBattalionControllerFirmware_RP2040) and 
the stock [PICO-PIO-USB Host example](https://github.com/raspberrypi/pico-examples/tree/master/usb/host/host_cdc_msc_hid). It is effectively a port of [ogx360_t4](https://github.com/Ryzee119/ogx360_t4) for the RP2040 boards, which are much cheaper than Teensy boards. An added bonus is that the computer will actually read this as a Steel Battalion Controller since it has the correct VID (9A7B) and PID (D000).

For disclosure. It is very crudely built with lots of vibecoding and bug testing via AI assistance. I had attempted to use several barebones examples of the host driver and try to get this working myself. But kept running into problems with dependencies and understanding how the configuration settings worked.




### Hardware

* 1 x Raspberry Pi Pico/RP2040
* 1 x USB Host Board or Host Cable
* 2 x 4.7k Ohm resistors (if using a Pico 2, but also good for safety)
* 1 x Neopixel LED (WS2812) - Optional but very helpful for troubleshooting
* 1 x Powered USB HUB - Needed if connecting more than one Device


# Wiring 

**Notes:** I have only tested this with a regular PICO

**Host Cable** <br/>
Follow either of the examples here
* [GP2040 example](https://gp2040-ce.info/controller-build/usb-host/)
* [OGX Mini example](https://github.com/MegaCadeDev/OGX-Mini-2026/tree/master/hardware)

**Neopixel LED**<br/>
The Pico runs on 3.3v so you will need to power the LED via the 3.3 out on the Pico.

# Firmware

**Set-up**<br/>
For set-up clone [pico-sdk](https://github.com/raspberrypi/pico-sdk) to your computer and export path. You may also need to download python3

```
git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
export PICO_SDK_PATH=$HOME/pico-sdk/
```

**Build Instructions**<br/>
```bash
git clone --recurse-submodules https://github.com/quizerno/SBCFirm2040-lite.git
cd SBCFirm2040-lite
mkdir build
cd build
cmake ..
make
```
Once that's done, connect your RP2040 to your PC in Download mode (hold button while connecting), and copy SteelBattalionController.uf2 to the drive that shows up

# Configuration


## USB Host
### Keyboard
### Gamepads-Joystick-Flightstick
### Steel Battalion Passthrough
## GPIO

##

# Connecting in Xemu
If it does not connect in Xemu immediately, open the monitor with the tilde key (```~```) and enter the commands
```
#this will show you which port it is connected to
info usbhost 

#this will add it to the port, the port might be different (13 = 1.3, 14 =1.4, etc)
device_add usb-host,vendorid=0x0a7b,productid=0xd000,port=1.3
```

# Completed
  - Mouse and Keyboard Support
  - Emulated/Real Steel Battalion Controller Mounting
  - USB HUB Support
  - LED Color Verification
  - 
# To Do
  -  Gamepad Support ([Playstation 4 Controller Support added in branch](https://github.com/quizerno/SBCFirm2040-lite/tree/ps4-controller))
  -  Flightstick Support
  -  Control Configurator




# Repositories Used
* [PICO-PIO-USB Host example](https://github.com/raspberrypi/pico-examples/tree/master/usb/host/host_cdc_msc_hid).
* [TinyUSB HID Controller](https://github.com/hathach/tinyusb/tree/2d56dc533e45e4e91b15e93fdab5e22e964f328d/examples/host/hid_controller)
* [Pico Battalion](https://github.com/sonik-br/pico_battalion)
* [ogx360_t4](https://github.com/Ryzee119/ogx360_t4)
* [Neopixel](https://github.com/adafruit/adafruit_neopixel)
  
