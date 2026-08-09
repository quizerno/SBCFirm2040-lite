# Steel Battalion Controller Firmware for the RP2040

This project is an example of how to produce firmware that makes an RP2040 report to the host machine that it's a Steel Battalion controller using a modified fork of tusb_gamepad

device_add usb-host,vendorid=0x0a7b,productid=0xd000,port=1.3

### Build Instructions
To build the firmware, do this:
```bash
mkdir build
cd build
cmake ..
make
```

Once that's done, connect your RP2040 to your PC in Download mode, and copy SteelBattalionController.uf2 to the drive that shows up
