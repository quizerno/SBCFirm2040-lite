#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "tusb.h"
#include "bsp/board_api.h"
#include "tusb_gamepad.h"

void update_gamepad(Gamepad *gp);

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
	//     gp->steel_battalion_in_report

	// TODO: Update the LED colors (as necessary):
	//     gp->steel_battalion_out_report
}
