/*******************************************************
 HIDAPI - Multi-Platform library for
 communication with HID devices.

 Alan Ott
 Signal 11 Software

 libusb/hidapi Team

 Copyright 2022, All Rights Reserved.

 At the discretion of the user of this library,
 this software may be licensed under the terms of the
 GNU General Public License v3, a BSD-Style license, or the
 original HIDAPI license as outlined in the LICENSE.txt,
 LICENSE-gpl3.txt, LICENSE-bsd.txt, and LICENSE-orig.txt
 files located at the root of the source distribution.
 These files may also be found in the public source
 code repository located at:
        https://github.com/libusb/hidapi .
********************************************************/

#include "../hidapi/hidapi.h"
#include <stdio.h>
#include <emscripten/html5.h>
#include <emscripten/emscripten.h>

EM_JS_DEPS(hidapi, "$stringToUTF32");

#if 0
#define HIDAPI_WEBHID_DEBUG
#endif

struct hid_device_ {
    int device_id;
    unsigned char *last_report;
    size_t last_report_length;
    struct hid_device_info* device_info;
};

static struct hid_api_version api_version = {
    .major = HID_API_VERSION_MAJOR,
    .minor = HID_API_VERSION_MINOR,
    .patch = HID_API_VERSION_PATCH
};

static int initialized = 0;
static int webhid_supported = 0;

static hid_device *new_hid_device(void)
{
    hid_device *dev = (hid_device*) calloc(1, sizeof(hid_device));
    if (dev == NULL) {
        return NULL;
    }

    dev->device_id = -1;
    dev->last_report = NULL;
    dev->last_report_length = 0;

    return dev;
}

HID_API_EXPORT const struct hid_api_version* HID_API_CALL hid_version(void)
{
    return &api_version;
}

HID_API_EXPORT const char* HID_API_CALL hid_version_str(void)
{
    return HID_API_VERSION_STR;
}

int HID_API_EXPORT hid_init(void)
{
    if (!initialized) {
        webhid_supported = MAIN_THREAD_EM_ASM_INT({
            return "hid" in navigator;
        });
    }
    return webhid_supported ? 0 : -1;
}

int HID_API_EXPORT hid_exit(void)
{
    return 0;
}

static struct hid_device_info * create_device_info_for_device(int device_id)
{
    struct hid_device_info *root = NULL;
    struct hid_device_info *cur_dev = NULL;
    char path[16];
    wchar_t product_string[128];
    int ignore = 0;
    int input_report_count;

    ignore = MAIN_THREAD_EM_ASM_INT({
        let device = window._hidDeviceList[$0];
        if (!device) {
            return true;
        }
        return false;
    }, device_id);

    if (ignore)
        goto end;

    /* Create the record. */
    root = (struct hid_device_info*) calloc(1, sizeof(struct hid_device_info));
    if (!root)
        goto end;

    cur_dev = root;

    snprintf(path, sizeof(path), "hid%d", device_id);
    cur_dev->path = strdup(path);

    cur_dev->vendor_id = MAIN_THREAD_EM_ASM_INT({
        return window._hidDeviceList[$0].vendorId;
    }, device_id);
    cur_dev->product_id = MAIN_THREAD_EM_ASM_INT({
        return window._hidDeviceList[$0].productId;
    }, device_id);

    cur_dev->serial_number = NULL;
    cur_dev->release_number = 0;
    cur_dev->manufacturer_string = NULL;

    MAIN_THREAD_EM_ASM({
        stringToUTF32(window._hidDeviceList[$0].productName, $1, Number($2));
    }, device_id, product_string, sizeof(product_string));

    cur_dev->product_string = wcsdup(product_string);

    cur_dev->usage_page = MAIN_THREAD_EM_ASM_INT({
        return window._hidDeviceList[$0].collections[0].usagePage;
    }, device_id);
    cur_dev->usage = MAIN_THREAD_EM_ASM_INT({
        return window._hidDeviceList[$0].collections[0].usage;
    }, device_id);
    cur_dev->interface_number = 0;

    input_report_count = MAIN_THREAD_EM_ASM_INT({
        return window._hidDeviceList[$0].collections[0].inputReports.length;
    }, device_id);

    /* WebHID doesn't provide the bus type, so we have to guess it ourselves */
    if (input_report_count >= 5) {
        cur_dev->bus_type = HID_API_BUS_BLUETOOTH;
    } else {
        cur_dev->bus_type = HID_API_BUS_USB;
    }

    cur_dev->next = NULL;

#ifdef HIDAPI_WEBHID_DEBUG
    printf("HIDAPI: New device found: %d %d\n", cur_dev->vendor_id, cur_dev->product_id);
#endif
end:
    return root;
}

EM_ASYNC_JS(int, hid_js_get_device_count, (), {
    window._hidDeviceList = await navigator.hid.getDevices();
    return window._hidDeviceList.length;
});

struct hid_device_info HID_API_EXPORT *hid_enumerate(unsigned short vendor_id, unsigned short product_id)
{
    int device_count;
    struct hid_device_info *root = NULL; /* return object */
    struct hid_device_info *cur_dev = NULL;

    if (hid_init() < 0) {
        return NULL;
    }

    device_count = hid_js_get_device_count();

#ifdef HIDAPI_WEBHID_DEBUG
    printf("hid_enumerate, device_count=%d\n", device_count);
#endif

    int device_id;
    for (device_id = 0; device_id < device_count; device_id++) {
		struct hid_device_info * tmp;
        /* TODO: handle vendor_id and product_id */
        tmp = create_device_info_for_device(device_id);

        if (tmp) {
            if (cur_dev) {
                cur_dev->next = tmp;
            }
            else {
                root = tmp;
            }
            cur_dev = tmp;

            /* move the pointer to the tail of returned list */
            while (cur_dev->next != NULL) {
                cur_dev = cur_dev->next;
            }
        }
    }

    return root;
}

void HID_API_EXPORT hid_free_enumeration(struct hid_device_info *devs)
{
    struct hid_device_info *d = devs;
    while (d) {
        struct hid_device_info *next = d->next;
        free(d->path);
        free(d->product_string);
        free(d);
        d = next;
    }
}

hid_device * hid_open(unsigned short vendor_id, unsigned short product_id, const wchar_t *serial_number)
{
    struct hid_device_info *devs, *cur_dev;
    const char *path_to_open = NULL;
    hid_device *handle = NULL;

    /* register_global_error: global error is reset by hid_enumerate/hid_init */
    devs = hid_enumerate(vendor_id, product_id);
    if (devs == NULL) {
        /* register_global_error: global error is already set by hid_enumerate */
        return NULL;
    }

    cur_dev = devs;
    while (cur_dev) {
        if (cur_dev->vendor_id == vendor_id &&
            cur_dev->product_id == product_id) {
            if (serial_number) {
                if (wcscmp(serial_number, cur_dev->serial_number) == 0) {
                    path_to_open = cur_dev->path;
                    break;
                }
            } else {
                path_to_open = cur_dev->path;
                break;
            }
        }
        cur_dev = cur_dev->next;
    }

    if (path_to_open) {
        /* Open the device */
        handle = hid_open_path(path_to_open);
    }

    hid_free_enumeration(devs);

    return handle;
}

EM_ASYNC_JS(void, hid_js_open, (int device_id, hid_device *dev, unsigned char **last_report_out, size_t *last_report_length_out), {
    let device = window._hidDeviceList[device_id];
    if (device) {
        try {
            await device.open();
            device.addEventListener("inputreport", function (event) {
                const { data, device, reportId } = event;
                
                let dataLength = data['byteLength']+1;
                let pointer = _malloc(dataLength);
                HEAPU8[pointer] = reportId;
                for (let i = 0; i < data['byteLength']; i++) {
                    HEAPU8[pointer + i + 1] = data['getUint8'](i);
                }

                if (HEAP32[last_report_out >> 2] != 0) {
                    _free(HEAP32[last_report_out >> 2]);
                }
                HEAP32[last_report_out >> 2] = pointer;
                HEAP32[last_report_length_out >> 2] = dataLength;
            });
        } catch (e) {
            // Pass?
        }
    }
});

hid_device * HID_API_EXPORT hid_open_path(const char *path)
{
    hid_device *dev = NULL;
    int device_id = 0;

    if (hid_init() < 0) {
        return NULL;
    }
    /* register_global_error: global error is reset by hid_init */

#ifdef HIDAPI_WEBHID_DEBUG
    printf("hid_open_path: %s\n", path);
#endif
    dev = new_hid_device();
    if (!dev) {
#ifdef HIDAPI_WEBHID_DEBUG
        printf("hid_open_path: no memory\n");
#endif
        return NULL;
    }

    if (sscanf(path, "hid%d", &device_id) != 1) {
        free(dev);
#ifdef HIDAPI_WEBHID_DEBUG
        printf("hid_open_path: invalid path\n");
#endif
        return NULL;
    }

    dev->device_id = device_id;
    hid_js_open(device_id, dev, &dev->last_report, &dev->last_report_length);

    return dev;
}

EM_ASYNC_JS(int, hid_js_write, (int device_id, int report_id, const unsigned char *data, size_t length), {
    let device = window._hidDeviceList[device_id];
    if (device) {
        let dataArray = new Uint8Array(length);
        for (let i = 0; i < length; i++) {
            dataArray[i] = HEAPU8[data+i];
        }
        await device.sendReport(report_id, dataArray);
    }
});

int HID_API_EXPORT hid_write(hid_device *dev, const unsigned char *data, size_t length)
{
    if (!webhid_supported || length < 1)
        return 0;
    hid_js_write(dev->device_id, data[0], data+1, length-1);
    return length;
}

EM_ASYNC_JS(int, hid_js_read_timeout, (int device_id, unsigned char *data, size_t length), {
    let device = window._hidDeviceList[device_id];
    if (device) {
        let [report_id, dataView] = await new Promise(function(resolve, reject) {
            device.addEventListener("inputreport", function (event) {
                const { data, device, report_id } = event;
                resolve([report_id, data]); // done
            }, {once: true});
        });
        HEAPU8[data] = report_id;
        for (let i = 0; i < dataView['byteLength'] && i < (length-1); i++) {
            HEAPU8[data + i + 1] = dataView['getUint8'](i);
        }
        return dataView['byteLength']+1;
    }
    return 0;
});

int HID_API_EXPORT hid_read_timeout(hid_device *dev, unsigned char *data, size_t length, int milliseconds)
{
    /* TODO: timeout */
    if (!webhid_supported || length < 1)
        return -1;
    if (milliseconds == 0) {
        if (dev->last_report) {
            size_t return_size = length;
            if (dev->last_report_length < length) {
                return_size = dev->last_report_length;
            }
            memcpy(data, dev->last_report, return_size);
            free(dev->last_report);
            dev->last_report = NULL;
            return return_size;
        } else {
            return 0;
        }
    }
    return hid_js_read_timeout(dev->device_id, data, length);
}

int HID_API_EXPORT hid_read(hid_device *dev, unsigned char *data, size_t length)
{
    /* TODO: blocking */
    return hid_read_timeout(dev, data, length, -1);
}

int HID_API_EXPORT hid_set_nonblocking(hid_device *dev, int nonblock)
{
    /* TODO */
    return 0;
}

int HID_API_EXPORT hid_send_feature_report(hid_device *dev, const unsigned char *data, size_t length)
{
    return 0;
}

EM_ASYNC_JS(void, hid_js_get_feature_report, (int device_id, int report_id, unsigned char *data, size_t length), {
    let device = window._hidDeviceList[device_id];
    if (device) {
        let dataView = await device['receiveFeatureReport'](report_id);
        for (let i = 0; i < dataView['byteLength']; i++) {
            HEAPU8[data + i] = dataView['getUint8'](i);
        }
    }
});

int HID_API_EXPORT hid_get_feature_report(hid_device *dev, unsigned char *data, size_t length)
{
    if (!webhid_supported || length < 1)
        return -1;
    int report_id = (int)data[0];
    hid_js_get_feature_report(dev->device_id, report_id, data, length);
    return 0;
}

int HID_API_EXPORT HID_API_CALL hid_get_input_report(hid_device *dev, unsigned char *data, size_t length)
{
    return 0;
}

EM_ASYNC_JS(int, hid_js_close, (int device_id), {
    let device = window._hidDeviceList[device_id];
    if (device) {
        await device.close();
    }
});

void HID_API_EXPORT hid_close(hid_device *dev)
{
    if (!dev)
        return;

    if (dev->last_report) {
        free(dev->last_report);
    }
    
    hid_js_close(dev->device_id);
    hid_free_enumeration(dev->device_info);

    free(dev);
}

int HID_API_EXPORT_CALL hid_get_manufacturer_string(hid_device *dev, wchar_t *string, size_t maxlen)
{
    return 0;
}

int HID_API_EXPORT_CALL hid_get_product_string(hid_device *dev, wchar_t *string, size_t maxlen)
{
    return 0;
}

int HID_API_EXPORT_CALL hid_get_serial_number_string(hid_device *dev, wchar_t *string, size_t maxlen)
{
    return 0;
}

HID_API_EXPORT struct hid_device_info *HID_API_CALL hid_get_device_info(hid_device *dev)
{
    return NULL;
}

int HID_API_EXPORT_CALL hid_get_indexed_string(hid_device *dev, int string_index, wchar_t *string, size_t maxlen)
{
    return -1;
}

int HID_API_EXPORT_CALL hid_get_report_descriptor(hid_device *dev, unsigned char *buf, size_t buf_size)
{
    return 0;
}

HID_API_EXPORT const wchar_t * HID_API_CALL hid_error(hid_device *dev)
{
    return L"";
}