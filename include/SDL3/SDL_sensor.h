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

/**
 *  \file SDL_sensor.h
 *
 *  Include file for SDL sensor event handling
 *
 */

#ifndef SDL_sensor_h_
#define SDL_sensor_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/**
 *  \brief SDL_sensor.h
 *
 *  In order to use these functions, SDL_Init() must have been called
 *  with the ::SDL_INIT_SENSOR flag.  This causes SDL to scan the system
 *  for sensors, and load appropriate drivers.
 */

struct SDL_Sensor;
typedef struct SDL_Sensor SDL_Sensor;

/**
 * This is a unique ID for a sensor for the time it is connected to the system,
 * and is never reused for the lifetime of the application.
 *
 * The ID value starts at 1 and increments from there. The value 0 is an invalid ID.
 */
typedef Uint32 SDL_SensorID;

/* The different sensors defined by SDL
 *
 * Additional sensors may be available, using platform dependent semantics.
 *
 * Hare are the additional Android sensors:
 * https://developer.android.com/reference/android/hardware/SensorEvent.html#values
 */
typedef enum
{
    SDL_SENSOR_INVALID = -1,    /**< Returned for an invalid sensor */
    SDL_SENSOR_UNKNOWN,         /**< Unknown sensor type */
    SDL_SENSOR_ACCEL,           /**< Accelerometer */
    SDL_SENSOR_GYRO,            /**< Gyroscope */
    SDL_SENSOR_ACCEL_L,         /**< Accelerometer for left Joy-Con controller and Wii nunchuk */
    SDL_SENSOR_GYRO_L,          /**< Gyroscope for left Joy-Con controller */
    SDL_SENSOR_ACCEL_R,         /**< Accelerometer for right Joy-Con controller */
    SDL_SENSOR_GYRO_R           /**< Gyroscope for right Joy-Con controller */
} SDL_SensorType;

/**
 * Accelerometer sensor
 *
 * The accelerometer returns the current acceleration in SI meters per
 * second squared. This measurement includes the force of gravity, so
 * a device at rest will have an value of SDL_STANDARD_GRAVITY away
 * from the center of the earth.
 *
 * values[0]: Acceleration on the x axis
 * values[1]: Acceleration on the y axis
 * values[2]: Acceleration on the z axis
 *
 * For phones held in portrait mode and game controllers held in front of you,
 * the axes are defined as follows:
 * -X ... +X : left ... right
 * -Y ... +Y : bottom ... top
 * -Z ... +Z : farther ... closer
 *
 * The axis data is not changed when the phone is rotated.
 *
 * \sa SDL_GetDisplayOrientation()
 */
#define SDL_STANDARD_GRAVITY    9.80665f

/**
 * Gyroscope sensor
 *
 * The gyroscope returns the current rate of rotation in radians per second.
 * The rotation is positive in the counter-clockwise direction. That is,
 * an observer looking from a positive location on one of the axes would
 * see positive rotation on that axis when it appeared to be rotating
 * counter-clockwise.
 *
 * values[0]: Angular speed around the x axis (pitch)
 * values[1]: Angular speed around the y axis (yaw)
 * values[2]: Angular speed around the z axis (roll)
 *
 * For phones held in portrait mode and game controllers held in front of you,
 * the axes are defined as follows:
 * -X ... +X : left ... right
 * -Y ... +Y : bottom ... top
 * -Z ... +Z : farther ... closer
 *
 * The axis data is not changed when the phone or controller is rotated.
 *
 * \sa SDL_GetDisplayOrientation()
 */

/* Function prototypes */

/**
 * Return whether there are sensors connected
 *
 * \returns SDL_TRUE if there are sensors connected, SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetSensors
 */
extern DECLSPEC SDL_bool SDLCALL SDL_HasSensors(void);

/**
 * Get a list of currently connected sensors.
 *
 * \param count a pointer filled in with the number of sensors returned
 * \returns a 0 terminated array of sensor instance IDs which should be freed with SDL_free(), or NULL on error; call SDL_GetError() for more details.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_SensorID *SDLCALL SDL_GetSensors(int *count);

/**
 * Get the implementation dependent name of a sensor.
 *
 * \param instance_id the sensor instance ID
 * \returns the sensor name, or NULL if `instance_id` is not valid
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC const char *SDLCALL SDL_GetSensorInstanceName(SDL_SensorID instance_id);

/**
 * Get the type of a sensor.
 *
 * \param instance_id the sensor instance ID
 * \returns the SDL_SensorType, or `SDL_SENSOR_INVALID` if `instance_id` is not valid
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_SensorType SDLCALL SDL_GetSensorInstanceType(SDL_SensorID instance_id);

/**
 * Get the platform dependent type of a sensor.
 *
 * \param instance_id the sensor instance ID
 * \returns the sensor platform dependent type, or -1 if `instance_id` is not valid
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetSensorInstanceNonPortableType(SDL_SensorID instance_id);

/**
 * Open a sensor for use.
 *
 * \param instance_id the sensor instance ID
 * \returns an SDL_Sensor sensor object, or NULL if an error occurred.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Sensor *SDLCALL SDL_OpenSensor(SDL_SensorID instance_id);

/**
 * Return the SDL_Sensor associated with an instance ID.
 *
 * \param instance_id the sensor instance ID
 * \returns an SDL_Sensor object.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_Sensor *SDLCALL SDL_GetSensorFromInstanceID(SDL_SensorID instance_id);

/**
 * Get the implementation dependent name of a sensor
 *
 * \param sensor The SDL_Sensor object
 * \returns the sensor name, or NULL if `sensor` is NULL.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC const char *SDLCALL SDL_GetSensorName(SDL_Sensor *sensor);

/**
 * Get the type of a sensor.
 *
 * \param sensor The SDL_Sensor object to inspect
 * \returns the SDL_SensorType type, or `SDL_SENSOR_INVALID` if `sensor` is
 *          NULL.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_SensorType SDLCALL SDL_GetSensorType(SDL_Sensor *sensor);

/**
 * Get the platform dependent type of a sensor.
 *
 * \param sensor The SDL_Sensor object to inspect
 * \returns the sensor platform dependent type, or -1 if `sensor` is NULL.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetSensorNonPortableType(SDL_Sensor *sensor);

/**
 * Get the instance ID of a sensor.
 *
 * \param sensor The SDL_Sensor object to inspect
 * \returns the sensor instance ID, or -1 if `sensor` is NULL.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC SDL_SensorID SDLCALL SDL_GetSensorInstanceID(SDL_Sensor *sensor);

/**
 * Get the current state of an opened sensor.
 *
 * The number of values and interpretation of the data is sensor dependent.
 *
 * \param sensor The SDL_Sensor object to query
 * \param data A pointer filled with the current sensor state
 * \param num_values The number of values to write to data
 * \returns 0 or -1 if an error occurred.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC int SDLCALL SDL_GetSensorData(SDL_Sensor *sensor, float *data, int num_values);

/**
 * Close a sensor previously opened with SDL_OpenSensor().
 *
 * \param sensor The SDL_Sensor object to close
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC void SDLCALL SDL_CloseSensor(SDL_Sensor *sensor);

/**
 * Update the current state of the open sensors.
 *
 * This is called automatically by the event loop if sensor events are
 * enabled.
 *
 * This needs to be called from the thread that initialized the sensor
 * subsystem.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern DECLSPEC void SDLCALL SDL_UpdateSensors(void);


/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_sensor_h_ */
