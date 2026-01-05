/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifdef SDL_SENSOR_OHOS

#include "../SDL_syssensor.h"
#include "sensors/oh_sensor.h"
#include "sensors/oh_sensor_type.h"

typedef struct {
    Sensor_Info *info;
    Sensor_Subscriber *subs;
    Sensor_SubscriptionId *subid;
    Sensor_SubscriptionAttribute *subattr;
    SDL_Sensor *sensor;
    SDL_SensorID id;
} OHSensorMapping;

static uint32_t sensorCount;
static OHSensorMapping *mapping;

static bool SDL_OHOS_SensorInit(void)
{
    Sensor_Info **infobase;
    uint32_t s;
    if (OH_Sensor_GetInfos(NULL, &s) != SENSOR_SUCCESS) {
        return false;
    }
    sensorCount = s;
    
    infobase = OH_Sensor_CreateInfos(s);
    mapping = (OHSensorMapping *)SDL_calloc(sizeof(OHSensorMapping), s);
    
    if (OH_Sensor_GetInfos(infobase, &s) != SENSOR_SUCCESS) {
        return false;
    }

    for (int i = 0; i < s; i++)
    {
        mapping[i].sensor = NULL;
        mapping[i].info = infobase[i];
        mapping[i].id = SDL_GetNextObjectID();
    }

    return true;
}

static int SDL_OHOS_SensorGetCount(void)
{
    return sensorCount;
}

static void SDL_OHOS_SensorDetect(void)
{
}

static const char *SDL_OHOS_SensorGetDeviceName(int device_index)
{
    char *data;
    uint32_t length;
    OH_SensorInfo_GetName(mapping[device_index].info, NULL, &length);
    data = (char*)SDL_malloc(length);
    OH_SensorInfo_GetName(mapping[device_index].info, data, &length);
    
    return data;
}

static SDL_SensorType SDL_OHOS_SensorGetDeviceType(int device_index)
{
    Sensor_Type type;
    if (OH_SensorInfo_GetType(mapping[device_index].info, &type) != SENSOR_SUCCESS) {
        return SDL_SENSOR_INVALID;
    }
    switch (type) {
    case SENSOR_TYPE_ACCELEROMETER:
        return SDL_SENSOR_ACCEL;
    case SENSOR_TYPE_GYROSCOPE:
        return SDL_SENSOR_GYRO;
    default:
        return SDL_SENSOR_UNKNOWN;
    }
}

static int SDL_OHOS_SensorGetDeviceNonPortableType(int device_index)
{
    Sensor_Type type;
    if (OH_SensorInfo_GetType(mapping[device_index].info, &type) != SENSOR_SUCCESS) {
        return -1;
    }
    return type;
}

static SDL_SensorID SDL_OHOS_SensorGetDeviceInstanceID(int device_index)
{
    return mapping[device_index].id;
}

static void SDL_OHOS_EventSub(Sensor_Event *event)
{
    Sensor_Type type;
    if (OH_SensorEvent_GetType(event, &type) != SENSOR_SUCCESS) {
        return;
    }
    int64_t timestamp;
    if (OH_SensorEvent_GetTimestamp(event, &timestamp) != SENSOR_SUCCESS) {
        return;
    }
    
    for (int i = 0; i < sensorCount; i++) {
        if (mapping[i].sensor && SDL_OHOS_SensorGetDeviceNonPortableType(i) == type) {
            uint32_t length;
            if (OH_SensorEvent_GetData(event, NULL, &length) != SENSOR_SUCCESS) {
                return;
            }
            float *data = (float *)SDL_malloc(sizeof(float) * length);
            if (OH_SensorEvent_GetData(event, &data, &length) != SENSOR_SUCCESS) {
                return;
            }
            SDL_SendSensorUpdate(timestamp, mapping[i].sensor, timestamp, data, length);
            SDL_free(data);
            break;
        }
    }
}

static bool SDL_OHOS_SensorOpen(SDL_Sensor *sensor, int device_index)
{
    if (device_index >= sensorCount) {
        return false;
    }
    mapping[device_index].sensor = sensor;
    
    mapping[device_index].subs = OH_Sensor_CreateSubscriber();
    if (OH_SensorSubscriber_SetCallback(mapping[device_index].subs, SDL_OHOS_EventSub) != SENSOR_SUCCESS) {
        return false;
    }
    
    mapping[device_index].subid = OH_Sensor_CreateSubscriptionId();
    if (OH_SensorSubscriptionId_SetType(mapping[device_index].subid, (Sensor_Type)SDL_OHOS_SensorGetDeviceNonPortableType(device_index)) != SENSOR_SUCCESS) {
        return false;
    }
    
    mapping[device_index].subattr = OH_Sensor_CreateSubscriptionAttribute();
    if (OH_SensorSubscriptionAttribute_SetSamplingInterval(mapping[device_index].subattr, 10000000) != SENSOR_SUCCESS) {
        return false;
    }
    
    if (OH_Sensor_Subscribe(mapping[device_index].subid, mapping[device_index].subattr, mapping[device_index].subs) != SENSOR_SUCCESS) {
        return false;
    }
    
    return true;
}

static void SDL_OHOS_SensorUpdate(SDL_Sensor *sensor)
{
}

static void SDL_OHOS_SensorClose(SDL_Sensor *sensor)
{
    for (int i = 0; i < sensorCount; i++) {
        if (mapping[i].sensor == sensor) {
            mapping[i].sensor = NULL;
            OH_Sensor_Unsubscribe(mapping[i].subid, mapping[i].subs);
            if (mapping[i].subattr) {
                OH_Sensor_DestroySubscriptionAttribute(mapping[i].subattr);
                mapping[i].subattr = NULL;
            }
            if (mapping[i].subid) {
                OH_Sensor_DestroySubscriptionId(mapping[i].subid);
                mapping[i].subid = NULL;
            }
            if (mapping[i].subs) {
                OH_Sensor_DestroySubscriber(mapping[i].subs);
                mapping[i].subs = NULL;
            }
            break;
        }
    }
}

static void SDL_OHOS_SensorQuit(void)
{
    for (int i = 0; i < sensorCount; i++) {
        if (mapping[i].sensor) {
            SDL_OHOS_SensorClose(mapping[i].sensor);
        }
    }
    sensorCount = 0;
    SDL_free(mapping);
}

SDL_SensorDriver SDL_OHOS_SensorDriver = {
    SDL_OHOS_SensorInit,
    SDL_OHOS_SensorGetCount,
    SDL_OHOS_SensorDetect,
    SDL_OHOS_SensorGetDeviceName,
    SDL_OHOS_SensorGetDeviceType,
    SDL_OHOS_SensorGetDeviceNonPortableType,
    SDL_OHOS_SensorGetDeviceInstanceID,
    SDL_OHOS_SensorOpen,
    SDL_OHOS_SensorUpdate,
    SDL_OHOS_SensorClose,
    SDL_OHOS_SensorQuit,
};

#endif // SDL_SENSOR_DUMMY || SDL_SENSOR_DISABLED
