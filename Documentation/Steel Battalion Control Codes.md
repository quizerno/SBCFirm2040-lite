# Steel Battalion Control Codes
Located [here](https://github.com/faha223/tusb_gamepad/blob/2fd4b6da389a8b1c3da55b9d0eaf2cd30979cd57/src/drivers/xboxog/xid/xid_steelbattalion.h).
Struct [here.](https://github.com/faha223/tusb_gamepad/blob/2fd4b6da389a8b1c3da55b9d0eaf2cd30979cd57/src/Gamepad.h#L95)

**Buttons and Toggles**

Can only be TRUE or FALSE, 1 or 0.

Referenced via: ```steel_battalion_out_report.dButtons```
```
	//Buttons
	uint16_t MainWeapon : 1;
	uint16_t Fire : 1;
	uint16_t LockOn : 1;
	uint16_t Eject : 1;
	uint16_t CockpitHatch : 1;
	uint16_t Ignition : 1;
	uint16_t Start : 1;
	uint16_t MultiMonitorOpenClose : 1;
	uint16_t MultiMonitorMapZoomInOut : 1;
	uint16_t MultiMonitorModeSelect : 1;
	uint16_t MultiMonitorSubMonitor : 1;
	uint16_t MainMonitorZoomIn : 1;
	uint16_t MainMonitorZoomOut : 1;
	uint16_t ForecastShootingSystem : 1;
	uint16_t Manipulator : 1;
	uint16_t LineColorChange : 1;
	uint16_t Washing : 1;
	uint16_t Extinguisher : 1;
	uint16_t Chaff : 1;
	uint16_t TankDetach : 1;
	uint16_t Override : 1;
	uint16_t NightScope : 1;
	uint16_t Function1 : 1;
	uint16_t Function2 : 1;
	uint16_t Function3 : 1;
	uint16_t WeaponConMain : 1;
	uint16_t WeaponConSub : 1;
	uint16_t WeaponConMagazine : 1;
	uint16_t Comm1 : 1;
	uint16_t Comm2 : 1;
	uint16_t Comm3 : 1;
	uint16_t Comm4 : 1;
	uint16_t Comm5 : 1;

	//Toggles
	uint16_t SightChange : 1;
	uint16_t ToggleFiltControl : 1;
	uint16_t ToggleOxygenSupply : 1;
	uint16_t ToggleFuelFlowRate : 1;
	uint16_t ToggleBufferMaterial : 1;
	uint16_t ToggleVTLocation : 1;
	uint16_t notUsed : 9;

	Example Usage
steel_battalion_out_report.dButtons.MainWeapon = 1
```

**Gear Lever**
Values are 7 to 13

Referenced via ```steel_battalion_out_report.gearLever```
```
int8_t gearLever;        //7-13 is gears R,1,2,3,4,5

	Example Usage
steel_battalion_out_report.dButtons.gearLever = (int) 4;
```

**Tuner Dial**

Referenced via ```steel_battalion_out_report.tunerDial =```
```	int8_t tunerDial;        //0-15 is from 9oclock, around clockwise```

**Weapon Aim (Right Joystick)**	

Referenced via ```steel_battalion_out_report.aimingX =``` and ```steel_battalion_out_report.aimingX =```
```
uint16_t aimingX;       //0 to 2^16 left to right, does not recenter
uint16_t aimingY;       //0 to 2^16 top to bottom, does not recenter
```

**Rotation (Left Joystick)**

Referenced via ```steel_battalion_out_report.rotationLever =```
```
int16_t rotationLever; // -32768 (Left) to 32767 (Right), re-centers to 0

```
**Camera (Left Hatstick)**

Referenced via ```steel_battalion_out_report.INPUT_HERE =```
```
int16_t sightChangeX;
int16_t sightChangeY;
```

**Analog Pedals**

Referenced via ```steel_battalion_out_report.INPUT_HERE =```
```
	uint16_t leftPedal;      //Sidestep, 0x0000 to 0xFF00
	uint16_t middlePedal;    //Brake, 0x0000 to 0xFF00
	uint16_t rightPedal;     //Acceleration, 0x0000 to oxFF00
```
