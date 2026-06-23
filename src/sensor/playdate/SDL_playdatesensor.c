/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#include "SDL_config.h"

#ifdef SDL_SENSOR_PLAYDATE

#include "SDL_error.h"
#include "SDL_sensor.h"
#include "SDL_playdatesensor.h"
#include "../SDL_syssensor.h"

#include "pd_api.h"

typedef struct
{
    SDL_SensorType type;
    SDL_SensorID instance_id;
} SDL_PlaydateSensor;

static SDL_PlaydateSensor *SDL_sensors;
static int SDL_sensors_count;

static int
SDL_PLAYDATE_SensorInit(void)
{
    SDL_sensors_count = 1;
    SDL_sensors = (SDL_PlaydateSensor *)SDL_calloc(1, sizeof(*SDL_sensors));
    SDL_sensors[0].type = SDL_SENSOR_ACCEL;
    SDL_sensors[0].instance_id = SDL_GetNextSensorInstanceID();
    return 0;
}

static int
SDL_PLAYDATE_SensorGetCount(void)
{
    return SDL_sensors_count;
}

static void
SDL_PLAYDATE_SensorDetect(void)
{
}

static const char *
SDL_PLAYDATE_SensorGetDeviceName(int device_index)
{
    switch (SDL_sensors[device_index].type) {
    case SDL_SENSOR_ACCEL:
        return "Accelerometer";
    default:
        return "Unknown";
    }
}

static SDL_SensorType
SDL_PLAYDATE_SensorGetDeviceType(int device_index)
{
    return SDL_sensors[device_index].type;
}

static int
SDL_PLAYDATE_SensorGetDeviceNonPortableType(int device_index)
{
    return SDL_sensors[device_index].type;
}

static SDL_SensorID
SDL_PLAYDATE_SensorGetDeviceInstanceID(int device_index)
{
    return SDL_sensors[device_index].instance_id;
}

static int
SDL_PLAYDATE_SensorOpen(SDL_Sensor *sensor, int device_index)
{
    switch (sensor->type) {
        case SDL_SENSOR_ACCEL:
            pd->system->setPeripheralsEnabled(kAccelerometer);
            break;
        default:
            break;
    }
    return 0;
}
    
static void
SDL_PLAYDATE_SensorUpdate(SDL_Sensor *sensor)
{
    switch (sensor->type) {
        case SDL_SENSOR_ACCEL:
            {
                float data[3];
                pd->system->getAccelerometer(&data[0], &data[1], &data[2]);
                data[0] = data[0] * SDL_STANDARD_GRAVITY;
                data[1] = data[1] * SDL_STANDARD_GRAVITY;
                data[2] = data[2] * SDL_STANDARD_GRAVITY;
                SDL_PrivateSensorUpdate(sensor, data, SDL_arraysize(data));
            }
            break;
        default:
            break;
    }
}

static void
SDL_PLAYDATE_SensorClose(SDL_Sensor *sensor)
{
    switch (sensor->type) {
        case SDL_SENSOR_ACCEL:
            pd->system->setPeripheralsEnabled(kNone);
            break;
        default:
            break;
    }
}

static void
SDL_PLAYDATE_SensorQuit(void)
{
}

SDL_SensorDriver SDL_PLAYDATE_SensorDriver =
{
    SDL_PLAYDATE_SensorInit,
    SDL_PLAYDATE_SensorGetCount,
    SDL_PLAYDATE_SensorDetect,
    SDL_PLAYDATE_SensorGetDeviceName,
    SDL_PLAYDATE_SensorGetDeviceType,
    SDL_PLAYDATE_SensorGetDeviceNonPortableType,
    SDL_PLAYDATE_SensorGetDeviceInstanceID,
    SDL_PLAYDATE_SensorOpen,
    SDL_PLAYDATE_SensorUpdate,
    SDL_PLAYDATE_SensorClose,
    SDL_PLAYDATE_SensorQuit,
};

#endif /* SDL_SENSOR_PLAYDATE */

/* vi: set ts=4 sw=4 expandtab: */
