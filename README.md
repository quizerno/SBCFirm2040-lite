# Steel Battalion Controller Firmware for the RP2040 with Input Options

SBCFirm2040-lite is a RP2040 firmware I cobbled together from [SteelBattalionControllerFirmware_RP2040](https://github.com/faha223/SteelBattalionControllerFirmware_RP2040) and 
the stock [PICO-PIO-USB Host example](https://github.com/raspberrypi/pico-examples/tree/master/usb/host/host_cdc_msc_hid). It is effectively a port of [ogx360_t4](https://github.com/Ryzee119/ogx360_t4) for the RP2040 boards, which are much cheaper than Teensy boards. It is very crudely built with lots of bug testing via AI assistance.

An added bonus is that the computer will actually read this as a Steel Battalion Controller.


### Set-up
For set-up clone [pico-sdk](https://github.com/raspberrypi/pico-sdk) to your computer and export path. You may also need to download python3

```
git clone --recurse-submodules https://github.com/quizerno/SBCFirm2040-lite.git
export PICO_SDK_PATH=$HOME/pico-sdk/
```

### Build Instructions

```bash
git clone --recurse-submodules https://github.com/quizerno/SBCFirm2040-lite.git
cd SBCFirm2040-lite
mkdir build
cd build
cmake ..
make
```
Once that's done, connect your RP2040 to your PC in Download mode (hold button while connecting), and copy SteelBattalionController.uf2 to the drive that shows up



# Other Notes
If it does not connect in Xemu immediately, open the monitor with the tilde key (```~```) and enter the commands
```
info usbhost #this will show you which port it is connected to

device_add usb-host,vendorid=0x0a7b,productid=0xd000,port=1.3 #this will add it to the port, the port might be different (13 = 1.3, 14 =1.4, etc)

```
