/*
Copyright (c) 2014, Tudor Marcu
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#define IN	0x81
#define OUT	0x02
#define VENDOR  0x1941 /* Use to open the device handle */
#define PRODUCT 0x8021

static uint8_t FIRE[] = {0x10, 00, 00, 00, 00, 00, 00, 00};
static uint8_t UP[] = {0x01, 00, 00, 00, 00, 00, 00, 00};
static uint8_t DOWN[] = {0x02, 00, 00, 00, 00, 00, 00, 00};
static uint8_t LEFT[] = {0x04, 00, 00, 00, 00, 00, 00, 00};
static uint8_t RIGHT[] = {0x08, 00, 00, 00, 00, 00, 00, 00};
static uint8_t STOP[] = {00, 00, 00, 00, 00, 00, 00, 00};

/* Function to go through the USB bus and find the Launcher */
libusb_device_handle *findUSB(libusb_context *session, struct libusb_device **list) {
	libusb_device_handle *h_dev = 0;	/* USB device handle */
	int devcount;
	struct libusb_device_descriptor ddesc; /* Device descriptor gives us info abuot USB devices */
	int i;

	/* Get all of the USB devices on the computer */
	devcount = libusb_get_device_list(session, &list);
	if (devcount <= 0) {
		fprintf(stderr, "! Missile Launcher is not connected!\n");
		libusb_free_device_list(list, 1);
		return h_dev;
	}

	/* Iterate through all of the USB devices in the list */
	for (i = 0; i < devcount; i++) {
		if ((libusb_get_device_descriptor(list[i], &ddesc)) < 0) {
			fprintf(stderr, "! Cannot find device descriptor. \n");
			libusb_free_device_list(list, 1);
			return h_dev;
		}
		if ((ddesc.idVendor == VENDOR && ddesc.idProduct == PRODUCT)) {
			printf("* Found Missile Launcher *\n\tUSB#%d\n\tVendor ID: 0x%x\n\tProduct ID: 0x%x\n", i, ddesc.idVendor, ddesc.idProduct);

			h_dev = libusb_open_device_with_vid_pid(session, VENDOR, PRODUCT);
			break;
		}
	}

	if (h_dev == NULL)
		fprintf(stderr, "! Could not get a handle to the Missile Launcher!\n");

	/* Free the list and return the USB device handle */
	libusb_free_device_list(list, 1);
	return h_dev;
}

int claim_interface(libusb_context *session, libusb_device_handle *h_dev, struct libusb_device **list) {
	int ret = 1;

	/* Check if the kernel driver is already attached to the launcher.
	 * If so, attempt to detach it so we can claim it for our driver.
	*/
	if (libusb_kernel_driver_active(h_dev, 0) == 1) {
		printf("\tTrying to remove Kernel driver from USB...\n");
		if (libusb_detach_kernel_driver(h_dev, 0) == 0)
			printf("\tSuccessfully removed Kernel driver!\n");
		else {
			fprintf(stderr, "! Could not detach the kernel driver, exiting.\n");
			return ret;
		}
	}
	/* The configuration should be set since it's not always defined
	 * for all hardware.
	 */
	if ((ret = libusb_set_configuration(h_dev, 1)) < 0) {
		fprintf(stderr, "! Could not set configuration. LIBUSB CODE %d\n", ret);
		return ret;
	}

	/* Finally claim the interface for our driver. */
	if ((ret = libusb_claim_interface(h_dev, 0)) < 0) {
		fprintf(stderr, "! The interface could not be reached.\n");
		return ret;
	}
	printf("\tClaimed interface for launcher\n");

	/* Should clear the endpoints just in case, to make sure nothing is
	 * blocking them from transferring data.
	 */
	if (libusb_clear_halt(h_dev, IN))
		fprintf(stderr, "! Can't clear IN endpoint\n");
	else if (libusb_clear_halt(h_dev, OUT))
		fprintf(stderr, "! Can't clear OUT endpoint\n");

	return ret;
}

void freeDevice(libusb_device_handle *h_dev, libusb_context *session) {
	printf("* Shutting down interface and releasing handles.\n");
	libusb_close(h_dev);
	libusb_exit(session);
}

int missile_usb_sendcommand(libusb_device_handle *handle, uint8_t *seq)
{
	if (libusb_control_transfer(handle, 0x21, 9, 0x00000200, 0x00, seq, 8, 1000) != 0)
		return -1;

	return 0;
}

int main(int argc, char *argv[]) {
	libusb_device_handle *h_dev;
	libusb_context *session;
	libusb_device **list = 0;
	unsigned char c;

	if (getuid() != 0) {
		fprintf(stderr, "! Program can only be run as root.\n");
		exit(EXIT_FAILURE);
	}

	libusb_init(&session);
	libusb_set_debug(session, 3); /* Set debug to the highest level */

	h_dev = findUSB(session, list);

	if (claim_interface(session, h_dev, list)) {
		freeDevice(h_dev, session);
		exit(EXIT_FAILURE);
	}
	/* Control the launcher using the arrow keys as input, space as fire*/
	do {
		c = getchar();
		switch(c) {
			case 65:
				missile_usb_sendcommand(h_dev, UP);
				usleep(500000);
				missile_usb_sendcommand(h_dev, STOP);
			break;
			case 66:
				missile_usb_sendcommand(h_dev, DOWN);
				usleep(500000);
				missile_usb_sendcommand(h_dev, STOP);
			break;
			case 67:
				missile_usb_sendcommand(h_dev, RIGHT);
				usleep(500000);
				missile_usb_sendcommand(h_dev, STOP);
			break;
			case 68:
				missile_usb_sendcommand(h_dev, LEFT);
				usleep(500000);
				missile_usb_sendcommand(h_dev, STOP);
			break;
			case 32:
				missile_usb_sendcommand(h_dev, FIRE);
				sleep(3);
				missile_usb_sendcommand(h_dev, STOP);
			break;
			case 's':
			case 'S':
				missile_usb_sendcommand(h_dev, STOP);
			break;
		}
	} while(c != 'q' || c != 'Q');
	printf("Trying to send command...\n");

	freeDevice(h_dev, session);

	return 0;
}
