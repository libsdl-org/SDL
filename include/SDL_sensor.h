/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

/**
 *  \file SDL_sensor.h
 *
 *  Include file for SDL sensor event handling
 *
 */

#ifndef _SDL_sensor_h
#define _SDL_sensor_h

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/**
 *  \file SDL_sensor.h
 *
 *  In order to use these functions, SDL_Init() must have been called
 *  with the ::SDL_INIT_SENSOR flag.  This causes SDL to scan the system
 *  for sensors, and load appropriate drivers.
 */

struct _SDL_Sensor;
typedef struct _SDL_Sensor SDL_Sensor;

/**
 * This is a unique ID for a sensor for the time it is connected to the system,
 * and is never reused for the lifetime of the application.
 *
 * The ID value starts at 0 and increments from there. The value -1 is an invalid ID.
 */
typedef Sint32 SDL_JoystickID;
typedef int SDL_SensorID;

/* The different sensor types */
typedef enum
{
    SDL_SENSOR_INVALID = -1,    /**< Returned for an invalid sensor */
    SDL_SENSOR_UNKNOWN,         /**< Unknown sensor type */
    SDL_SENSOR_ACCEL,           /**< Accelerometer */
    SDL_SENSOR_GYRO,            /**< Gyroscope */
} SDL_SensorType;

/* Function prototypes */

/**
 *  \brief Count the number of sensors attached to the system right now
 */
extern DECLSPEC int SDLCALL SDL_NumSensors(void);

/**
 *  \brief Get the implementation dependent name of a sensor.
 *
 *  This can be called before any sensors are opened.
 * 
 *  \return The sensor name, or NULL if device_index is out of range.
 */
extern DECLSPEC const char *SDLCALL SDL_SensorGetDeviceName(int device_index);

/**
 *  \brief Get the type of a sensor.
 *
 *  This can be called before any sensors are opened.
 *
 *  \return The sensor type, or SDL_SENSOR_INVALID if device_index is out of range.
 */
extern DECLSPEC SDL_SensorType SDLCALL SDL_SensorGetDeviceType(int device_index);

/**
 *  \brief Get the platform dependent type of a sensor.
 *
 *  This can be called before any sensors are opened.
 *
 *  \return The sensor platform dependent type, or -1 if device_index is out of range.
 */
extern DECLSPEC int SDLCALL SDL_SensorGetDeviceNonPortableType(int device_index);

/**
 *  \brief Get the instance ID of a sensor.
 *
 *  This can be called before any sensors are opened.
 *
 *  \return The sensor instance ID, or -1 if device_index is out of range.
 */
extern DECLSPEC SDL_SensorID SDLCALL SDL_SensorGetDeviceInstanceID(int device_index);

/**
 *  \brief Open a sensor for use.
 *
 *  The index passed as an argument refers to the N'th sensor on the system.
 *
 *  \return A sensor identifier, or NULL if an error occurred.
 */
extern DECLSPEC SDL_Sensor *SDLCALL SDL_SensorOpen(int device_index);

/**
 * Return the SDL_Sensor associated with an instance id.
 */
extern DECLSPEC SDL_Sensor *SDLCALL SDL_SensorFromInstanceID(SDL_SensorID instance_id);

/**
 *  \brief Get the implementation dependent name of a sensor.
 *
 *  \return The sensor name, or NULL if the sensor is NULL.
 */
extern DECLSPEC const char *SDLCALL SDL_SensorGetName(SDL_Sensor *sensor);

/**
 *  \brief Get the type of a sensor.
 *
 *  This can be called before any sensors are opened.
 *
 *  \return The sensor type, or SDL_SENSOR_INVALID if the sensor is NULL.
 */
extern DECLSPEC SDL_SensorType SDLCALL SDL_SensorGetType(SDL_Sensor *sensor);

/**
 *  \brief Get the platform dependent type of a sensor.
 *
 *  This can be called before any sensors are opened.
 *
 *  \return The sensor platform dependent type, or -1 if the sensor is NULL.
 */
extern DECLSPEC int SDLCALL SDL_SensorGetNonPortableType(SDL_Sensor *sensor);

/**
 *  \brief Get the instance ID of a sensor.
 *
 *  This can be called before any sensors are opened.
 *
 *  \return The sensor instance ID, or -1 if the sensor is NULL.
 */
extern DECLSPEC SDL_SensorID SDLCALL SDL_SensorGetInstanceID(SDL_Sensor *sensor);

/**
 *  Get the current state of an opened sensor.
 *
 *  The number of values and interpretation of the data is sensor dependent.
 *
 *  \param sensor The sensor to query
 *  \param data A pointer filled with the current sensor state
 *  \param num_values The number of values to write to data
 *
 *  \return 0 or -1 if an error occurred.
 */
extern DECLSPEC int SDLCALL SDL_SensorGetData(SDL_Sensor * sensor, float *data, int num_values);

/**
 *  Close a sensor previously opened with SDL_SensorOpen()
 */
extern DECLSPEC void SDLCALL SDL_SensorClose(SDL_Sensor * sensor);

/**
 *  Update the current state of the open sensors.
 *
 *  This is called automatically by the event loop if sensor events are enabled.
 *
 *  This needs to be called from the thread that initialized the sensor subsystem.
 */
extern DECLSPEC void SDLCALL SDL_SensorUpdate(void);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_sensor_h */

/* vi: set ts=4 sw=4 expandtab: */
