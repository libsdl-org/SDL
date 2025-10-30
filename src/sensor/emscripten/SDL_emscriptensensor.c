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

#ifdef SDL_SENSOR_EMSCRIPTEN

#include "../SDL_syssensor.h"
#include "SDL_emscriptensensor.h"
#include <emscripten/html5.h>

#define EMSCRIPTEN_SENSOR_COUNT 2

typedef struct
{
    SDL_SensorType type;
    SDL_SensorID instance_id;
    float data[3];
    bool new_data;
} SDL_EmscriptenSensor;

static SDL_EmscriptenSensor SDL_sensors[EMSCRIPTEN_SENSOR_COUNT];

static void SDL_EMSCRIPTEN_AccelerometerCallback(const EmscriptenDeviceMotionEvent *event)
{
    double total_gravity;
    double gravity[3];

    // Convert from browser specific gravity constant to SDL_STANDARD_GRAVITY.
    total_gravity = 0.0;
    total_gravity += fabs(event->accelerationIncludingGravityX - event->accelerationX);
    total_gravity += fabs(event->accelerationIncludingGravityY - event->accelerationY);
    total_gravity += fabs(event->accelerationIncludingGravityZ - event->accelerationZ);

    gravity[0] = (event->accelerationIncludingGravityX - event->accelerationX) / total_gravity;
    gravity[1] = (event->accelerationIncludingGravityY - event->accelerationY) / total_gravity;
    gravity[2] = (event->accelerationIncludingGravityZ - event->accelerationZ) / total_gravity;

    SDL_sensors[0].data[0] = (float)(event->accelerationX + gravity[0] * SDL_STANDARD_GRAVITY);
    SDL_sensors[0].data[1] = (float)(event->accelerationY + gravity[1] * SDL_STANDARD_GRAVITY);
    SDL_sensors[0].data[2] = (float)(event->accelerationZ + gravity[2] * SDL_STANDARD_GRAVITY);
    SDL_sensors[0].new_data = true;
}

static void SDL_EMSCRIPTEN_GyroscopeCallback(const EmscriptenDeviceMotionEvent *event)
{
    SDL_sensors[1].data[0] = (float)event->rotationRateAlpha * SDL_PI_F / 180.0f;
    SDL_sensors[1].data[1] = (float)event->rotationRateBeta * SDL_PI_F / 180.0f;
    SDL_sensors[1].data[2] = (float)event->rotationRateGamma * SDL_PI_F / 180.0f;
    SDL_sensors[1].new_data = true;
}

static EM_BOOL SDL_EMSCRIPTEN_SensorCallback(int event_type, const EmscriptenDeviceMotionEvent *event, void *user_data)
{
    SDL_EMSCRIPTEN_AccelerometerCallback(event);
    SDL_EMSCRIPTEN_GyroscopeCallback(event);

    return true;
}

static bool SDL_EMSCRIPTEN_SensorInit(void)
{
    emscripten_set_devicemotion_callback((void *)0, false, &SDL_EMSCRIPTEN_SensorCallback);

    SDL_sensors[0].type = SDL_SENSOR_ACCEL;
    SDL_sensors[0].instance_id = SDL_GetNextObjectID();
    SDL_sensors[0].new_data = false;
    SDL_sensors[1].type = SDL_SENSOR_GYRO;
    SDL_sensors[1].instance_id = SDL_GetNextObjectID();
    SDL_sensors[1].new_data = false;

    return true;
}

static int SDL_EMSCRIPTEN_SensorGetCount(void)
{
    return EMSCRIPTEN_SENSOR_COUNT;
}

static void SDL_EMSCRIPTEN_SensorDetect(void)
{
}

static const char *SDL_EMSCRIPTEN_SensorGetDeviceName(int device_index)
{
    if (device_index < EMSCRIPTEN_SENSOR_COUNT) {
        switch (SDL_sensors[device_index].type) {
        case SDL_SENSOR_ACCEL:
            return "Accelerometer";
        case SDL_SENSOR_GYRO:
            return "Gyroscope";
        default:
            return "Unknown";
        }
    }

    return NULL;
}

static SDL_SensorType SDL_EMSCRIPTEN_SensorGetDeviceType(int device_index)
{
    if (device_index < EMSCRIPTEN_SENSOR_COUNT) {
        return SDL_sensors[device_index].type;
    }

    return SDL_SENSOR_INVALID;
}

static int SDL_EMSCRIPTEN_SensorGetDeviceNonPortableType(int device_index)
{
    if (device_index < EMSCRIPTEN_SENSOR_COUNT) {
        return SDL_sensors[device_index].type;
    }

    return -1;
}

static SDL_SensorID SDL_EMSCRIPTEN_SensorGetDeviceInstanceID(int device_index)
{
    if (device_index < EMSCRIPTEN_SENSOR_COUNT) {
        return SDL_sensors[device_index].instance_id;
    }

    return -1;
}

static bool SDL_EMSCRIPTEN_SensorOpen(SDL_Sensor *sensor, int device_index)
{
    return true;
}

static void SDL_EMSCRIPTEN_SensorUpdate(SDL_Sensor *sensor)
{
    Uint64 timestamp;

    switch (sensor->type) {
    case SDL_SENSOR_ACCEL:
        if (SDL_sensors[0].new_data) {
            SDL_sensors[0].new_data = false;
            timestamp = SDL_GetTicksNS();
            SDL_SendSensorUpdate(timestamp, sensor, timestamp, SDL_sensors[0].data, SDL_arraysize(SDL_sensors[0].data));
        }
        break;
    case SDL_SENSOR_GYRO:
        if (SDL_sensors[1].new_data) {
            SDL_sensors[1].new_data = false;
            timestamp = SDL_GetTicksNS();
            SDL_SendSensorUpdate(timestamp, sensor, timestamp, SDL_sensors[1].data, SDL_arraysize(SDL_sensors[1].data));
        }
        break;
    default:
        break;
    }
}

static void SDL_EMSCRIPTEN_SensorClose(SDL_Sensor *sensor)
{
}

static void SDL_EMSCRIPTEN_SensorQuit(void)
{
}

SDL_SensorDriver SDL_EMSCRIPTEN_SensorDriver = {
    SDL_EMSCRIPTEN_SensorInit,
    SDL_EMSCRIPTEN_SensorGetCount,
    SDL_EMSCRIPTEN_SensorDetect,
    SDL_EMSCRIPTEN_SensorGetDeviceName,
    SDL_EMSCRIPTEN_SensorGetDeviceType,
    SDL_EMSCRIPTEN_SensorGetDeviceNonPortableType,
    SDL_EMSCRIPTEN_SensorGetDeviceInstanceID,
    SDL_EMSCRIPTEN_SensorOpen,
    SDL_EMSCRIPTEN_SensorUpdate,
    SDL_EMSCRIPTEN_SensorClose,
    SDL_EMSCRIPTEN_SensorQuit,
};

#endif // SDL_SENSOR_EMSCRIPTEN
