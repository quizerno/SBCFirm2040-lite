#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "tusb.h"
#include "bsp/board_api.h"
#include "tusb_gamepad.h"

#include <math.h>

#define DEG_2_RAD (3.14159 / 180.0)

void update_gamepad(Gamepad *gp);
int t = 0;

int main(void)
{
    board_init();

    init_tusb_gamepad(INPUT_MODE_XBOXORIGINAL);

    stdio_init_all();

	// TODO: Initialize any communication channels, as necessary

    Gamepad *gp = gamepad(0);

    while (1)
    {
        update_gamepad(gp);

        tusb_gamepad_task();

        sleep_ms(1);
        tud_task();
    }

    return 0;
}

void update_gamepad(Gamepad *gp)
{
	// TODO: Update the state of the controller:
    gp->steel_battalion_in_report.rotationLever = 32767 * sin(++t * DEG_2_RAD);
    gp->steel_battalion_in_report.aimingX = 32767 + 32767 * cos(t * DEG_2_RAD);
    gp->steel_battalion_in_report.aimingY = 32767 + 32767 * sin(t * DEG_2_RAD);

    gp->steel_battalion_in_report.leftPedal   = 0xFC00 * sin(t * DEG_2_RAD);
    gp->steel_battalion_in_report.middlePedal = 0xFC00 * sin((t + 60) * DEG_2_RAD);
    gp->steel_battalion_in_report.rightPedal  = 0xFC00 * sin((t + 120) * DEG_2_RAD);
    gp->steel_battalion_in_report.tunerDial = (int)floor(t / 22.5) % 16;

	gp->steel_battalion_in_report.dButtons.Eject		= (t / 256) % 3 == 0;
	gp->steel_battalion_in_report.dButtons.CockpitHatch	= (t / 256) % 3 == 1;
	gp->steel_battalion_in_report.dButtons.Ignition		= (t / 256) % 3 == 2;
	gp->steel_battalion_in_report.dButtons.Start		= (t / 256) % 3 == 0;

    gp->steel_battalion_in_report.dButtons.Comm1        = (t / 256) % 3 == 0;
    gp->steel_battalion_in_report.dButtons.Comm2        = (t / 256) % 3 == 1;
    gp->steel_battalion_in_report.dButtons.Comm3        = (t / 256) % 3 == 2;
    gp->steel_battalion_in_report.dButtons.Comm4        = (t / 256) % 3 == 0;
    gp->steel_battalion_in_report.dButtons.Comm5        = (t / 256) % 3 == 1;

	gp->steel_battalion_in_report.dButtons.Function1 	= (t / 256) % 3 == 0;
	gp->steel_battalion_in_report.dButtons.Function2 	= (t / 256) % 3 == 1;
	gp->steel_battalion_in_report.dButtons.Function3 	= (t / 256) % 3 == 2;

	gp->steel_battalion_in_report.dButtons.ForecastShootingSystem = (t / 256) % 3 == 2;
	gp->steel_battalion_in_report.dButtons.Manipulator			  = (t / 256) % 3 == 0;
	gp->steel_battalion_in_report.dButtons.LineColorChange 		  = (t / 256) % 3 == 1;

	gp->steel_battalion_in_report.dButtons.TankDetach	= (t / 256) % 3 == 1;
    gp->steel_battalion_in_report.dButtons.Override		= (t / 256) % 3 == 2;
	gp->steel_battalion_in_report.dButtons.NightScope	= (t / 256) % 3 == 0;


	// TODO: Update the LED colors (as necessary):
	//     gp->steel_battalion_out_report
}
