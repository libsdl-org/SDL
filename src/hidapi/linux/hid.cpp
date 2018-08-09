//=================== Copyright Valve Corporation, All rights reserved. =======
//
// Purpose: A wrapper around both the libusb and hidraw versions of HIDAPI
//
//          The libusb version doesn't support Bluetooth, but not all Linux
//          distributions allow access to /dev/hidraw*
//
//          This merges the two, at a small performance cost, until distributions
//          have granted access to /dev/hidraw*
//
//=============================================================================

#define NAMESPACE HIDRAW
#include "../hidapi/hidapi.h"
#undef NAMESPACE
#undef HIDAPI_H__

#define NAMESPACE HIDUSB
#include "../hidapi/hidapi.h"
#undef NAMESPACE
#undef HIDAPI_H__

#include "../hidapi/hidapi.h"

#include "../../../public/tier1/utlvector.h"
#include "../../../public/tier1/utlhashmap.h"


template <class T>
void CopyHIDDeviceInfo( T *pSrc, struct hid_device_info *pDst )
{
	pDst->path = pSrc->path ? strdup( pSrc->path ) : NULL;
	pDst->vendor_id = pSrc->vendor_id;
	pDst->product_id = pSrc->product_id;
	pDst->serial_number = pSrc->serial_number ? wcsdup( pSrc->serial_number ) : NULL;
	pDst->release_number = pSrc->release_number;
	pDst->manufacturer_string = pSrc->manufacturer_string ? wcsdup( pSrc->manufacturer_string ) : NULL;
	pDst->product_string = pSrc->product_string ? wcsdup( pSrc->product_string ) : NULL;
	pDst->usage_page = pSrc->usage_page;
	pDst->usage = pSrc->usage;
	pDst->interface_number = pSrc->interface_number;
	pDst->next = NULL;
}

extern "C"
{

enum EHIDAPIType
{
	k_EHIDAPIUnknown,
	k_EHIDAPIRAW,
	k_EHIDAPIUSB
};

static CUtlHashMap<uintptr_t, EHIDAPIType> s_hashDeviceToAPI;

static EHIDAPIType GetAPIForDevice( hid_device *pDevice )
{
	int iIndex = s_hashDeviceToAPI.Find( (uintptr_t)pDevice );
	if ( iIndex != -1 )
	{
		return s_hashDeviceToAPI[ iIndex ];
	}
	return k_EHIDAPIUnknown;
}

struct hid_device_info HID_API_EXPORT * HID_API_CALL hid_enumerate(unsigned short vendor_id, unsigned short product_id)
{
	struct HIDUSB::hid_device_info *usb_devs = HIDUSB::hid_enumerate( vendor_id, product_id );
	struct HIDUSB::hid_device_info *usb_dev;
	struct HIDRAW::hid_device_info *raw_devs = HIDRAW::hid_enumerate( vendor_id, product_id );
	struct HIDRAW::hid_device_info *raw_dev;
	struct hid_device_info *devs = NULL, *last = NULL, *new_dev;

	for ( usb_dev = usb_devs; usb_dev; usb_dev = usb_dev->next )
	{
		bool bFound = false;
		for ( raw_dev = raw_devs; raw_dev; raw_dev = raw_dev->next )
		{
			if ( usb_dev->vendor_id == raw_dev->vendor_id && usb_dev->product_id == raw_dev->product_id )
			{
				bFound = true;
				break;
			}
		}

//printf("%s USB device VID/PID 0x%.4x/0x%.4x, %ls %ls\n", bFound ? "Found matching" : "Added new", usb_dev->vendor_id, usb_dev->product_id, usb_dev->manufacturer_string, usb_dev->product_string );

		if ( !bFound )
		{
			new_dev = new struct hid_device_info;
			CopyHIDDeviceInfo( usb_dev, new_dev );

			if ( last )
			{
				last->next = new_dev;
			}
			else
			{
				devs = new_dev;
			}
			last = new_dev;
		}
	}
	HIDUSB::hid_free_enumeration( usb_devs );

	for ( raw_dev = raw_devs; raw_dev; raw_dev = raw_dev->next )
	{
		new_dev = new struct hid_device_info;
		CopyHIDDeviceInfo( raw_dev, new_dev );
		new_dev->next = NULL;

		if ( last )
		{
			last->next = new_dev;
		}
		else
		{
			devs = new_dev;
		}
		last = new_dev;
	}
	HIDRAW::hid_free_enumeration( raw_devs );

	return devs;
}

void  HID_API_EXPORT HID_API_CALL hid_free_enumeration(struct hid_device_info *devs)
{
	while ( devs )
	{
		struct hid_device_info *next = devs->next;
		free( devs->path );
		free( devs->serial_number );
		free( devs->manufacturer_string );
		free( devs->product_string );
		delete devs;
		devs = next;
	}
}

HID_API_EXPORT hid_device * HID_API_CALL hid_open(unsigned short vendor_id, unsigned short product_id, const wchar_t *serial_number)
{
	hid_device *pDevice = NULL;
	if ( ( pDevice = (hid_device *)HIDRAW::hid_open( vendor_id, product_id, serial_number ) ) != NULL )
	{
		s_hashDeviceToAPI.Insert( (uintptr_t)pDevice, k_EHIDAPIRAW );
		return pDevice;
	}
	if ( ( pDevice = (hid_device *)HIDUSB::hid_open( vendor_id, product_id, serial_number ) ) != NULL )
	{
		s_hashDeviceToAPI.Insert( (uintptr_t)pDevice, k_EHIDAPIUSB );
		return pDevice;
	}
	return NULL;
}

HID_API_EXPORT hid_device * HID_API_CALL hid_open_path(const char *path, int bExclusive)
{
	hid_device *pDevice = NULL;
	if ( ( pDevice = (hid_device *)HIDRAW::hid_open_path( path, bExclusive ) ) != NULL )
	{
		s_hashDeviceToAPI.Insert( (uintptr_t)pDevice, k_EHIDAPIRAW );
		return pDevice;
	}
	if ( ( pDevice = (hid_device *)HIDUSB::hid_open_path( path, bExclusive ) ) != NULL )
	{
		s_hashDeviceToAPI.Insert( (uintptr_t)pDevice, k_EHIDAPIUSB );
		return pDevice;
	}
	return NULL;
}

int  HID_API_EXPORT HID_API_CALL hid_write(hid_device *device, const unsigned char *data, size_t length)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_write( (HIDRAW::hid_device*)device, data, length );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_write( (HIDUSB::hid_device*)device, data, length );
	default:
		return -1;
	} 
}

int HID_API_EXPORT HID_API_CALL hid_read_timeout(hid_device *device, unsigned char *data, size_t length, int milliseconds)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_read_timeout( (HIDRAW::hid_device*)device, data, length, milliseconds );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_read_timeout( (HIDUSB::hid_device*)device, data, length, milliseconds );
	default:
		return -1;
	} 
}

int  HID_API_EXPORT HID_API_CALL hid_read(hid_device *device, unsigned char *data, size_t length)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_read( (HIDRAW::hid_device*)device, data, length );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_read( (HIDUSB::hid_device*)device, data, length );
	default:
		return -1;
	} 
}

int  HID_API_EXPORT HID_API_CALL hid_set_nonblocking(hid_device *device, int nonblock)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_set_nonblocking( (HIDRAW::hid_device*)device, nonblock );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_set_nonblocking( (HIDUSB::hid_device*)device, nonblock );
	default:
		return -1;
	} 
}

int HID_API_EXPORT HID_API_CALL hid_send_feature_report(hid_device *device, const unsigned char *data, size_t length)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_send_feature_report( (HIDRAW::hid_device*)device, data, length );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_send_feature_report( (HIDUSB::hid_device*)device, data, length );
	default:
		return -1;
	} 
}

int HID_API_EXPORT HID_API_CALL hid_get_feature_report(hid_device *device, unsigned char *data, size_t length)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_get_feature_report( (HIDRAW::hid_device*)device, data, length );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_get_feature_report( (HIDUSB::hid_device*)device, data, length );
	default:
		return -1;
	} 
}

void HID_API_EXPORT HID_API_CALL hid_close(hid_device *device)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		HIDRAW::hid_close( (HIDRAW::hid_device*)device );
		break;
	case k_EHIDAPIUSB:
		HIDUSB::hid_close( (HIDUSB::hid_device*)device );
		break;
	default:
		break;
	} 
	s_hashDeviceToAPI.Remove( (uintptr_t)device );
}

int HID_API_EXPORT_CALL hid_get_manufacturer_string(hid_device *device, wchar_t *string, size_t maxlen)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_get_manufacturer_string( (HIDRAW::hid_device*)device, string, maxlen );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_get_manufacturer_string( (HIDUSB::hid_device*)device, string, maxlen );
	default:
		return -1;
	} 
}

int HID_API_EXPORT_CALL hid_get_product_string(hid_device *device, wchar_t *string, size_t maxlen)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_get_product_string( (HIDRAW::hid_device*)device, string, maxlen );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_get_product_string( (HIDUSB::hid_device*)device, string, maxlen );
	default:
		return -1;
	}
}

int HID_API_EXPORT_CALL hid_get_serial_number_string(hid_device *device, wchar_t *string, size_t maxlen)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_get_serial_number_string( (HIDRAW::hid_device*)device, string, maxlen );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_get_serial_number_string( (HIDUSB::hid_device*)device, string, maxlen );
	default:
		return -1;
	}
}

int HID_API_EXPORT_CALL hid_get_indexed_string(hid_device *device, int string_index, wchar_t *string, size_t maxlen)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_get_indexed_string( (HIDRAW::hid_device*)device, string_index, string, maxlen );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_get_indexed_string( (HIDUSB::hid_device*)device, string_index, string, maxlen );
	default:
		return -1;
	}
}

HID_API_EXPORT const wchar_t* HID_API_CALL hid_error(hid_device *device)
{
	switch ( GetAPIForDevice( device ) )
	{
	case k_EHIDAPIRAW:
		return HIDRAW::hid_error( (HIDRAW::hid_device*)device );
	case k_EHIDAPIUSB:
		return HIDUSB::hid_error( (HIDUSB::hid_device*)device );
	default:
		return NULL;
	} 
}

}
