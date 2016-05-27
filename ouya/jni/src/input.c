#include <stdint.h>
#include <unistd.h>

#include "jniapi.h"
#include "logger.h"
#include "input.h"
#include "../../../frontend/ouya.h"


//void _input_poll_callback();
//int16_t _input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id);

static int input_state[10];

void _input_poll_callback()
{
	int *inp_arr;
	inp_arr = JNI_Get_InputState();

	if (inp_arr == NULL)
		return;

	input_state[INPUT_P1_BUTTONS] = 0;
	input_state[INPUT_P1_BUTTONS] |= inp_arr[INPUT_P1_BUTTONS];
	input_state[INPUT_P1_LX] = inp_arr[INPUT_P1_LX];
	input_state[INPUT_P1_LY] = inp_arr[INPUT_P1_LY];
	input_state[INPUT_P1_RX] = inp_arr[INPUT_P1_RX];
	input_state[INPUT_P1_RY] = inp_arr[INPUT_P1_RY];

	input_state[INPUT_P2_BUTTONS] = 0;
	input_state[INPUT_P2_BUTTONS] |= inp_arr[INPUT_P2_BUTTONS];
	input_state[INPUT_P2_LX] = inp_arr[INPUT_P2_LX];
	input_state[INPUT_P2_LY] = inp_arr[INPUT_P2_LY];
	input_state[INPUT_P2_RX] = inp_arr[INPUT_P2_RX];
	input_state[INPUT_P2_RY] = inp_arr[INPUT_P2_RY];


	//~ if (input_state[INPUT_P1_BUTTONS] != 0 || input_state[INPUT_P2_BUTTONS] != 0) {
		//~ LOG_DEBUG("Got input keystate: P1: %d P2: %d", input_state[INPUT_P1_BUTTONS], input_state[INPUT_P2_BUTTONS]);
	//~ }
	
	return;
}

int16_t _input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id)
{
	switch (device) {
		case OUYA_DEVICE_JOYPAD:
			if (port == 0)
				return input_state[INPUT_P1_BUTTONS] & (1 << id);
			else
				return input_state[INPUT_P2_BUTTONS] & (1 << id);
		case OUYA_DEVICE_ANALOG:
			if (port == 0) {
				switch (id) {
					case OUYA_ANALOG_LX:
						return input_state[INPUT_P1_LX];
					case OUYA_ANALOG_LY:
						return input_state[INPUT_P1_LY];
					case OUYA_ANALOG_RX:
						return input_state[INPUT_P1_RX];
					case OUYA_ANALOG_RY:
						return input_state[INPUT_P1_RY];
				}
			} else {
				switch (id) {
					case OUYA_ANALOG_LX:
						return input_state[INPUT_P2_LX];
					case OUYA_ANALOG_LY:
						return input_state[INPUT_P2_LY];
					case OUYA_ANALOG_RX:
						return input_state[INPUT_P2_RX];
					case OUYA_ANALOG_RY:
						return input_state[INPUT_P2_RY];
				}
			}
	}
	return 0;
}

