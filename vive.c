/*
 * OpenHMD - Free and Open Source API and drivers for immersive technology.
 * Copyright (C) 2013 Fredrik Hultin.
 * Copyright (C) 2013 Jakob Bornecrantz.
 * Distributed under the Boost 1.0 licence, see LICENSE for full text.
 */

/* HTC Vive Driver */

#define FEATURE_BUFFER_SIZE 256

#define HTC_ID                   0x0bb4
#define VIVE_HMD                 0x2c87

#define VALVE_ID                 0x28de
#define VIVE_WATCHMAN_DONGLE     0x2101
#define VIVE_LIGHTHOUSE_FPGA_RX  0x2000


#include <string.h>
#include <wchar.h>
#include <assert.h>

#include "vive.h"

static int getf(ohmd_device* device, ohmd_float_value type, float* out)
{
	vive_priv* priv = (vive_priv*)device;

	switch(type){
	case OHMD_ROTATION_QUAT: 
		out[0] = out[1] = out[2] = 0;
		out[3] = 1.0f;
		break;

	case OHMD_POSITION_VECTOR:
		out[0] = out[1] = out[2] = 0;
		break;

	case OHMD_DISTORTION_K:
		// TODO this should be set to the equivalent of no distortion
		memset(out, 0, sizeof(float) * 6);
		break;

	default:
		ohmd_set_error(priv->base.ctx, "invalid type given to getf (%ud)", type);
		return -1;
		break;
	}

	return 0;
}

static void close_device(ohmd_device* device)
{
	int hret = 0;
	vive_priv* priv = (vive_priv*)device;

	LOGD("closing HTC Vive device");

	// turn the display off
	hret = hid_send_feature_report(priv->hmd_handle, vive_magic_power_off1, sizeof(vive_magic_power_off1));
	printf("power off magic 1: %d\n", hret);
	
	hret = hid_send_feature_report(priv->hmd_handle, vive_magic_power_off2, sizeof(vive_magic_power_off2));
	printf("power off magic 2: %d\n", hret);

	hid_close(priv->hmd_handle);
	hid_close(priv->imu_handle);

	free(device);
}

static void dump_indexed_string(hid_device* device, int index)
{
	wchar_t wbuffer[512] = {0};
	char buffer[1024] = {0};

	int hret = hid_get_indexed_string(device, index, wbuffer, 511);	

	if(hret == 0){
		wcstombs(buffer, wbuffer, sizeof(buffer));
		printf("indexed string 0x%02x: '%s'\n", index, buffer);
	}
}

static void dump_info_string(int (*fun)(hid_device*, wchar_t*, size_t), const char* what, hid_device* device)
{
	wchar_t wbuffer[512] = {0};
	char buffer[1024] = {0};

	int hret = fun(device, wbuffer, 511);	

	if(hret == 0){
		wcstombs(buffer, wbuffer, sizeof(buffer));
		printf("%s: '%s'\n", what, buffer);
	}
}

static void dumpbin(const char* label, const unsigned char* data, int length)
{
	printf("%s:\n", label);
	for(int i = 0; i < length; i++){
		printf("%02x ", data[i]);
        if((i % 16) == 15)
			printf("\n");
	}
	printf("\n");
}

static hid_device* open_device_idx(int manufacturer, int product, int iface, int iface_tot, int device_index)
{
	struct hid_device_info* devs = hid_enumerate(manufacturer, product);
	struct hid_device_info* cur_dev = devs;

	int idx = 0;
	int iface_cur = 0;
	hid_device* ret = NULL;

    printf("Opening Manufacturer %04x Product %04x\n", manufacturer, product);

	while (cur_dev) {
        printf("Path %s", cur_dev->path);

		if(idx == device_index && iface == iface_cur){
            ret = hid_open_path(cur_dev->path);
            printf(" [open]\n");
        } else {
            printf(" [skip]\n");
        }

		cur_dev = cur_dev->next;

		iface_cur++;

		if(iface_cur >= iface_tot){
			idx++;
			iface_cur = 0;
		}
	}

	hid_free_enumeration(devs);

    printf("Returning last opened device.\n");

	return ret;
}

static ohmd_device* open_device(ohmd_driver* driver, ohmd_device_desc* desc)
{
	vive_priv* priv = ohmd_alloc(driver->ctx, sizeof(vive_priv));

	if(!priv)
		return NULL;

	int hret = 0;
	
	priv->base.ctx = driver->ctx;

	int idx = atoi(desc->path);

	// Open the HMD device
	priv->hmd_handle = open_device_idx(HTC_ID, VIVE_HMD, 0, 1, idx);

	if(!priv->hmd_handle)
		goto cleanup;
	
	if(hid_set_nonblocking(priv->hmd_handle, 1) == -1){
		ohmd_set_error(driver->ctx, "failed to set non-blocking on device");
		goto cleanup;
	}
	
	// Open the lighthouse device
	priv->imu_handle = open_device_idx(VALVE_ID, VIVE_LIGHTHOUSE_FPGA_RX, 0, 2, idx);

	if(!priv->imu_handle)
		goto cleanup;
	
	if(hid_set_nonblocking(priv->imu_handle, 1) == -1){
		ohmd_set_error(driver->ctx, "failed to set non-blocking on device");
		goto cleanup;
	}


    priv->lighthouse_sensor_handle = open_device_idx(VALVE_ID, VIVE_LIGHTHOUSE_FPGA_RX, 1, 2, idx);
    if(!priv->lighthouse_sensor_handle)
        goto cleanup;

    if(hid_set_nonblocking(priv->lighthouse_sensor_handle, 1) == -1){
        ohmd_set_error(driver->ctx, "failed to set non-blocking on lighthouse sensor");
        goto cleanup;
    }



    priv->watchman_dongle_handle = open_device_idx(VALVE_ID, VIVE_WATCHMAN_DONGLE, 1, 2, idx);

    // Open the controller dongle

    if(!priv->watchman_dongle_handle) {
        printf("failed to open dongle!\n");
        ohmd_set_error(driver->ctx, "failed to open watchman dongle");
        goto cleanup;

    } else {
        printf("opened watchman dongle! :)\n");
    }

    if(hid_set_nonblocking(priv->watchman_dongle_handle, 1) == -1){
        ohmd_set_error(driver->ctx, "failed to set non-blocking on device");
        goto cleanup;
    } else {
        printf("set watchman to non blocking.\n");
    }

    vive_controller_command_packet controller_command;
    controller_command.report_id = 255;
    controller_command.command = 0x8f;
    controller_command.length = 7;

    hret = hid_send_feature_report(priv->watchman_dongle_handle, vive_controller_power_off, sizeof(vive_controller_power_off));
    printf("vive_controller_haptic_pulse: %d\n", hret);


	dump_info_string(hid_get_manufacturer_string, "manufacturer", priv->hmd_handle);
	dump_info_string(hid_get_product_string , "product", priv->hmd_handle);
	dump_info_string(hid_get_serial_number_string, "serial number", priv->hmd_handle);

	// turn the display on
	hret = hid_send_feature_report(priv->hmd_handle, vive_magic_power_on, sizeof(vive_magic_power_on));
	printf("power on magic: %d\n", hret);
	
	// enable lighthouse
	//hret = hid_send_feature_report(priv->hmd_handle, vive_magic_enable_lighthouse, sizeof(vive_magic_enable_lighthouse));
    //printf("enable lighthouse magic: %d\n", hret);

	// set up device callbacks
	priv->base.close = close_device;
	priv->base.getf = getf;
	
	return (ohmd_device*)priv;

cleanup:
	if(priv)
		free(priv);

	return NULL;
}

static void get_device_list(ohmd_driver* driver, ohmd_device_list* list)
{
	struct hid_device_info* devs = hid_enumerate(HTC_ID, VIVE_HMD);
	struct hid_device_info* cur_dev = devs;

	int idx = 0;
	while (cur_dev) {
		ohmd_device_desc* desc = &list->devices[list->num_devices++];

		strcpy(desc->driver, "OpenHMD HTC Vive Driver");
		strcpy(desc->vendor, "HTC/Valve");
		strcpy(desc->product, "HTC Vive");

		desc->revision = 0;

		snprintf(desc->path, OHMD_STR_SIZE, "%d", idx);

		desc->driver_ptr = driver;

		cur_dev = cur_dev->next;
		idx++;
	}

	hid_free_enumeration(devs);
}

static void destroy_driver(ohmd_driver* drv)
{
	LOGD("shutting down HTC Vive driver");
	free(drv);
}

ohmd_driver* ohmd_create_htc_vive_drv(ohmd_context* ctx)
{
	ohmd_driver* drv = ohmd_alloc(ctx, sizeof(ohmd_driver));
	
	if(!drv)
		return NULL;

	drv->get_device_list = get_device_list;
	drv->open_device = open_device;
	drv->get_device_list = get_device_list;
	drv->open_device = open_device;
	drv->destroy = destroy_driver;

	return drv;
}

