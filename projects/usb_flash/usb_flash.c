/*
 * plf_inst_extract.c
 *
 * Copyright (c) 2011 scorp2kk, All rights reserved
 *
 * Description:
 *  This program flashes a new firmware via USB to the drone.
 *
 * License:
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <usb.h>
#include <getopt.h>
#include "plf.h"

#define VERSION_MAJOR  0
#define VERSION_MINOR  0
#define VERSION_BUGFIX 1

#define PARROT_VENDOR_ID 0x19CF
#define PARROT_PRODUCT_ID 0x1000

#define MAX_RECONNECT_TRY 10

#if defined(__WIN32__)
#  define sleep(x) Sleep(1000*(x))
#endif

static const struct option long_options[] =
{
        { "help", no_argument, 0, 'h' },
        { "kernel-test", required_argument, 0, 't' },
        { 0, 0, 0, 0 }
};

typedef struct s_command_args_tag
{
    u32 mode;
#define MODE_FLASH 1
#define MODE_TEST 2
    const char* bootloader_file;
    const char* installer_file;
    const char* payload_file;
} s_command_args;

s_command_args command_args =
{
        .mode = MODE_FLASH,
        .bootloader_file = "ardrone_usb_bootloader.bin",
        .installer_file = "ardrone_installer.plf",
        .payload_file = "ardrone_update.plf"
};

int start_usb(int* p_interface, usb_dev_handle** p_dev_handle)
{
	struct usb_bus *busses, *bus;
	struct usb_device *dev;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	busses = usb_get_busses();

	for (bus = busses; bus; bus=bus->next)
	{
		for (dev = bus->devices; dev; dev=dev->next)
		{
			if (dev->descriptor.idVendor == PARROT_VENDOR_ID &&
					dev->descriptor.idProduct == PARROT_PRODUCT_ID)
			{
					char buffer[256];

					printf("Found a possible device:\n");
					*p_dev_handle = usb_open(dev);
					if (*p_dev_handle == 0)
					{
						printf("usb_open failed\n");
						return -2;
					}

					usb_get_string_simple(*p_dev_handle, dev->descriptor.iManufacturer, buffer, sizeof(buffer));
					printf("- Manufacturer: %s\n", buffer);

					usb_get_string_simple(*p_dev_handle, dev->descriptor.iProduct, buffer, sizeof(buffer));
					printf("- Product: %s\n", buffer);

					usb_get_string_simple(*p_dev_handle, dev->descriptor.iSerialNumber, buffer, sizeof(buffer));
					printf("- Serialnumber: %s\n", buffer);

					printf("- Number of configurations: %d\n", dev->descriptor.bNumConfigurations );

#if defined(__WIN32__)
					/* Linux sets the configuration when probing a device. As the installer does not like
					 * to be configured twice, this call is removed for linux.
					 */
					usb_set_configuration(*p_dev_handle, 1);
#endif
					*p_interface = usb_claim_interface(*p_dev_handle, 0);



					if (*p_interface < 0)
					{
						usb_close(*p_dev_handle);
						printf("usb_claim_interface failed!\n");
						*p_interface = -1;
						return -2;
					}

					return 0;

			}
		}
	}

	*p_interface = -1;
	*p_dev_handle = 0;

	return -1;
}

/* Reconnect to the usb device */
int reconnect_usb(int* p_interface, usb_dev_handle** p_dev_handle)
{
    int i;
    if (p_interface == 0 || p_dev_handle == 0)
        return -2;

    if (*p_dev_handle != 0)
    {
        printf("Closing usb connection...\n");
        usb_release_interface(*p_dev_handle, *p_interface);
        usb_close(*p_dev_handle);

        *p_interface = -1;
        *p_dev_handle = 0;
    }

    for (i = 0; i < MAX_RECONNECT_TRY; ++i)
    {
        sleep(1);
        printf("Try [%02d/%02d] to connect to VID: 0x%04x PID: 0x%04x\n", i, MAX_RECONNECT_TRY, PARROT_VENDOR_ID, PARROT_PRODUCT_ID);
        if (start_usb(p_interface, p_dev_handle) == 0)
        {
            return 0;
        }
    }

    return -1;
}

/* Upload a file over usb */
int upload_file (usb_dev_handle* devhandle, int interface, const char* filename, int timeout)
{
	FILE* fp;
	char* f_buffer;
	int bytes_read;
	unsigned int resp;

	printf("Uploading file: %s\n", filename);
	fp = fopen(filename, "rb");

	if (!fp)
	{
		printf("  unable to open file %s ==> FAILED!\n", filename);
		return -1;
	}

	f_buffer = (char*)malloc(0x10000);

	do
	{
		bytes_read = fread(f_buffer, 1, 0x10000, fp);

		if (bytes_read <= 0)
			break;

		if (usb_bulk_write(devhandle, 0x01, f_buffer, bytes_read, timeout) < 0)
		{
			printf("  usb_bulk_write failed ==> FAILED!\n");
			fclose(fp);
			free(f_buffer);
			return -2;
		}
	}
	while (bytes_read > 0);

	fclose(fp);
	free(f_buffer);

	usb_bulk_read(devhandle, 0x81, (char*)(&resp), 4, timeout);
	printf("Error Code returned: 0x%08x ", resp );

	if (resp == 0x0000)
	{
		printf(" ==> OK\n");
		return 0;
	}
	else
	{
		printf(" ==> FAILED\n");
		return -3;
	}

}


int parse_options(int argc, char** argv)
{

    while(1)
    {
        int option_index;
        int result = getopt_long(argc, argv, "t:h", long_options, &option_index);

        if (result < 0)
            return 0;


        switch(result)
        {

        case 'h':
            return -1;
            break;

        case 't':
            command_args.mode = MODE_TEST;
            command_args.installer_file = optarg;
            break;

        }


    }

    return 0;
}

int do_flash()
{
    char data[] = { 0xA3, 0x00, 0x00, 0x00 };
    usb_dev_handle* devhandle = 0;
    int interface = -1;


    printf("\n\n*** DOWNLOADING USB BOOTLOADER ***\n");
    /* Start USB connection */
    if (reconnect_usb(&interface, &devhandle) < 0)
    {
        printf("!!! Unable to connect to USB device!\n");
        return -1;
    }

    /* *** FLASH THE BOOTLOADER ***/

    /* 1. Open the file */
    FILE* fp = fopen(command_args.bootloader_file, "rb");
    if (!fp)
    {
        printf("!!! Unable to open file: ardrone_usb_bootloader.bin\n");
        usb_release_interface(devhandle, interface);
        usb_close(devhandle);
        return -1;
    }

    /* 2. Get the file size (quite dirty...) */
    fseek(fp, 0, SEEK_END);
    int filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("Sending bootloader (0x%04x bytes)\n", filesize);

    /* 3. Send Hello P6 (0xA3 <filesize>)*/
    printf(" - Send Hello P6 (0xA3)\n");

    data[0] = 0xA3;
    data[1] = (char)(filesize);
    data[2] = (char)(filesize>>8);

    if ( usb_bulk_write(devhandle, 0x01, (char*)&data[0], 3, 5000 ) < 0)
    {
        printf("!!! usb_bulk_write failed..\n");
        usb_release_interface(devhandle, interface);
        usb_close(devhandle);
        fclose(fp);
        return -1;
    }

    /* 4. Read back the answer of Hello P6 (should be 0xa8) */
    if ( usb_bulk_read(devhandle, 0x81, (char*)&data[0], 1, 5000) < 0)
    {
        printf("!!! usb_bulk_read failed..\n");
        usb_release_interface(devhandle, interface);
        usb_close(devhandle);
        fclose(fp);
        return -1;
    }
    if (data[0] == (char)0xa8)
        printf("Success!!!\n\n");

    fflush(stdout);


    /* 5. Write down the content of the bootloader file */
    char* f_buffer = (char*)malloc(0x10000);

    int bytes_read = 0;
    unsigned int checksum = 0; /* CheckSUM */

    do
    {
        int jj;
        bytes_read = fread(f_buffer, 1, 0x10000, fp);

        if (bytes_read <= 0)
            break;

        /* Calculate the checksum */
        for (jj = 0; jj < bytes_read; ++jj)
            checksum += (unsigned char)f_buffer[jj];

        if (usb_bulk_write(devhandle, 0x01, f_buffer, bytes_read, 5000) < 0)
        {
            printf("!!! usb_bulk_write failed..\n");
            usb_release_interface(devhandle, interface);
            usb_close(devhandle);
            free(f_buffer);
            fclose(fp);
            return -1;
        }
    }
    while (bytes_read > 0);

    free(f_buffer);
    fclose(fp);


    /* 6. Read back the answer. Answer is the checksum*/
    short usb_checksum = 0;
    if (usb_bulk_read(devhandle, 0x81, (char*)&usb_checksum, 2, 5000) < 0)
    {
        printf("!!! usb_bulk_read failed..\n");
        usb_release_interface(devhandle, interface);
        usb_close(devhandle);
        return -1;
    }

    /* 7. Verify checksums */
    printf("Checksum returned: 0x%04x Expected: 0x%04x ", usb_checksum, (short)checksum);

    if (usb_checksum == (short)checksum)
    {
        printf("=> OK\n");
    }
    else
    {
        printf("=> FAILED\n");
        usb_release_interface(devhandle, interface);
        usb_close(devhandle);
        return -1;
    }


    /* 8. Start the bootloader (send 0x55) */
    printf("Starting the bootloader...");
    data[0] = 0x55;
    if (usb_bulk_write(devhandle, 0x01, (char*)&data[0], 1, 5000) < 0)
    {
        printf("!!! usb_bulk_write failed..\n");
        usb_release_interface(devhandle, interface);
        usb_close(devhandle);
        return -1;
    }


    printf("\n\n*** INSTALLER DOWNLOAD ***\n");
    /* *** RECONNECT *** */
    if (reconnect_usb(&interface, &devhandle) < 0)
    {
        printf("!!! Unable to re-connect to USB device!\n");
        return -1;
    }

    /* *** LOAD THE INSTALLER *** */
    printf("loading installer\n");
    if (upload_file(devhandle, interface, command_args.installer_file, 5000) < 0)
    {
        printf("!!!upload_file failed...\n");
        usb_release_interface(devhandle, interface);
        usb_close(devhandle);
        return -1;
    }

    if (command_args.mode == MODE_FLASH)
    {
        printf("\n\n*** FIRMWARE INSTALLATION ***\n");
        /* *** RECONNECT *** */
        if (reconnect_usb(&interface, &devhandle) < 0)
        {
            printf("!!! Unable to re-connect to USB device!\n");
            return -1;
        }

        /* *** LOAD THE FIRMWARE *** */
        printf ("loading firmware\n");
        if (upload_file(devhandle, interface, command_args.payload_file, 1000000) < 0)
        {
            printf("!!!upload_file failed...\n");
            usb_release_interface(devhandle, interface);
            usb_close(devhandle);
            return -1;
        }
    }

    /* *** CLOSE CONNECTION *** */
    usb_release_interface(devhandle, interface);
    usb_close(devhandle);
    fflush(stdout);

    return 0;
}

int verify_file(const char* filename, int plf_type)
{
    if (!filename)
    {
        return -1;
    }

    printf("Verifying %s\n", filename);
    if (plf_type != 0)
    {
        int ret_val;
        int fileIdx = plf_open_file(filename);

        if (fileIdx < 0)
        {
            printf("!!! unable to open %s\nPlease verify that this file is available! (%d)\n", filename, fileIdx);
            return -1;
        }
        ret_val = plf_verify(fileIdx);

        if (ret_val < 0)
        {
            printf("!!! %s is not a valid plf file (%d)\n", filename, ret_val);
            return -1;
        }

        /* Check file header */
        s_plf_file* hdr = plf_get_file_header(fileIdx);
        if (hdr->dwHdrVersion < 10)
        {
            printf("!!! %s has wrong header version (%d) minimum 10 is required", filename,  hdr->dwHdrVersion);
            return -1;
        }

        if (hdr->dwFileType != plf_type)
        {
            printf("!!! %s has wrong file type (%d), %d is required", filename, hdr->dwFileType, plf_type);
            return -1;
        }

        plf_close(fileIdx);
    }
    else
    {
        FILE* fp = fopen(filename, "rb");
        if (!fp)
        {
            printf("!!! unable to open %s\nPlease verify that this file is available!\n", filename);
            return -1;
        }

        fclose(fp);
    }

    return 0;

}

int main(int argc, char** argv) {
    int ret_val = 0;

    ret_val = parse_options(argc, argv);

	printf("A.R. Drone Flasher\n");
	printf("(c) scorpion2k\n");

	printf("!!USE AT YOUR OWN RISK!!\n");

	printf("\n\n");
	printf("Version: %d.%d.%d\n\n", VERSION_MAJOR, VERSION_MINOR, VERSION_BUGFIX);

	printf("1. Connect your drone via USB to your PC\n");
	printf("2. Connect the Battery to your Drone\n");
	printf("3. Ensure the drivers are installed and the device shows up in devmgr\n");
	printf("4. Press any key to continue\n");
	printf("\n\nMode: %s\n", command_args.mode==MODE_FLASH?"flash":"kernel test");

#if defined(__WIN32__)
	system("PAUSE");
#endif

	printf("\n\n*** VERIFICATION ***\n");
	ret_val += verify_file(command_args.bootloader_file, 0);
	ret_val += verify_file(command_args.installer_file, 1);

	if (command_args.mode == MODE_FLASH)
	    ret_val += verify_file(command_args.payload_file, 2);


	if (ret_val != 0)
	{
	    printf("!!! Verification failed\n Please check your files!!!\n\n");
	}
	else
	{
	    ret_val = do_flash();
	}

	if (ret_val == 0)
	{
	    printf(" *** INSTALLATION DONE ***\n");
	}
	else
	{
        printf(" *** INSTALLATION FAILED ***\n");
	}

#if defined(__WIN32__)
	system("PAUSE");
#endif

	return ret_val;
}
