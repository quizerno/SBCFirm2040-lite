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

	// TODO: Update the LED colors (as necessary):
	//     gp->steel_battalion_out_report
}
