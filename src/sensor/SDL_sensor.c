/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

/* This is the sensor API for Simple DirectMedia Layer */

#include "SDL_syssensor.h"

#ifndef SDL_EVENTS_DISABLED
#include "../events/SDL_events_c.h"
#endif
#include "../joystick/SDL_gamepad_c.h"

static SDL_SensorDriver *SDL_sensor_drivers[] = {
#ifdef SDL_SENSOR_ANDROID
    &SDL_ANDROID_SensorDriver,
#endif
#ifdef SDL_SENSOR_COREMOTION
    &SDL_COREMOTION_SensorDriver,
#endif
#ifdef SDL_SENSOR_WINDOWS
    &SDL_WINDOWS_SensorDriver,
#endif
#ifdef SDL_SENSOR_VITA
    &SDL_VITA_SensorDriver,
#endif
#ifdef SDL_SENSOR_N3DS
    &SDL_N3DS_SensorDriver,
#endif
#if defined(SDL_SENSOR_DUMMY) || defined(SDL_SENSOR_DISABLED)
    &SDL_DUMMY_SensorDriver
#endif
};
static SDL_Mutex *SDL_sensor_lock = NULL; /* This needs to support recursive locks */
static SDL_Sensor *SDL_sensors SDL_GUARDED_BY(SDL_sensor_lock) = NULL;
static SDL_AtomicInt SDL_last_sensor_instance_id SDL_GUARDED_BY(SDL_sensor_lock);

void SDL_LockSensors(void) SDL_ACQUIRE(SDL_sensor_lock)
{
    SDL_LockMutex(SDL_sensor_lock);
}

void SDL_UnlockSensors(void) SDL_RELEASE(SDL_sensor_lock)
{
    SDL_UnlockMutex(SDL_sensor_lock);
}

int SDL_InitSensors(void)
{
    int i, status;

    /* Create the sensor list lock */
    if (SDL_sensor_lock == NULL) {
        SDL_sensor_lock = SDL_CreateMutex();
    }

#ifndef SDL_EVENTS_DISABLED
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0) {
        return -1;
    }
#endif /* !SDL_EVENTS_DISABLED */

    status = -1;
    for (i = 0; i < SDL_arraysize(SDL_sensor_drivers); ++i) {
        if (SDL_sensor_drivers[i]->Init() >= 0) {
            status = 0;
        }
    }
    return status;
}

SDL_bool SDL_SensorsOpened(void)
{
    SDL_bool opened;

    SDL_LockSensors();
    {
        if (SDL_sensors != NULL) {
            opened = SDL_TRUE;
        } else {
            opened = SDL_FALSE;
        }
    }
    SDL_UnlockSensors();

    return opened;
}

SDL_SensorID *SDL_GetSensors(int *count)
{
    int i, num_sensors, device_index;
    int sensor_index = 0, total_sensors = 0;
    SDL_SensorID *sensors;

    SDL_LockSensors();
    {
        for (i = 0; i < SDL_arraysize(SDL_sensor_drivers); ++i) {
            total_sensors += SDL_sensor_drivers[i]->GetCount();
        }

        sensors = (SDL_SensorID *)SDL_malloc((total_sensors + 1) * sizeof(*sensors));
        if (sensors) {
            if (count) {
                *count = total_sensors;
            }

            for (i = 0; i < SDL_arraysize(SDL_sensor_drivers); ++i) {
                num_sensors = SDL_sensor_drivers[i]->GetCount();
                for (device_index = 0; device_index < num_sensors; ++device_index) {
                    SDL_assert(sensor_index < total_sensors);
                    sensors[sensor_index] = SDL_sensor_drivers[i]->GetDeviceInstanceID(device_index);
                    SDL_assert(sensors[sensor_index] > 0);
                    ++sensor_index;
                }
            }
            SDL_assert(sensor_index == total_sensors);
            sensors[sensor_index] = 0;
        } else {
            if (count) {
                *count = 0;
            }

            SDL_OutOfMemory();
        }
    }
    SDL_UnlockSensors();

    return sensors;
}

/*
 * Return the next available sensor instance ID
 * This may be called by drivers from multiple threads, unprotected by any locks
 */
SDL_SensorID SDL_GetNextSensorInstanceID(void)
{
    return SDL_AtomicIncRef(&SDL_last_sensor_instance_id) + 1;
}

/*
 * Get the driver and device index for a sensor instance ID
 * This should be called while the sensor lock is held, to prevent another thread from updating the list
 */
static SDL_bool SDL_GetDriverAndSensorIndex(SDL_SensorID instance_id, SDL_SensorDriver **driver, int *driver_index)
{
    int i, num_sensors, device_index;

    if (instance_id > 0) {
        for (i = 0; i < SDL_arraysize(SDL_sensor_drivers); ++i) {
            num_sensors = SDL_sensor_drivers[i]->GetCount();
            for (device_index = 0; device_index < num_sensors; ++device_index) {
                SDL_SensorID sensor_id = SDL_sensor_drivers[i]->GetDeviceInstanceID(device_index);
                if (sensor_id == instance_id) {
                    *driver = SDL_sensor_drivers[i];
                    *driver_index = device_index;
                    return SDL_TRUE;
                }
            }
        }
    }
    SDL_SetError("Sensor %" SDL_PRIs32 " not found", instance_id);
    return SDL_FALSE;
}

/*
 * Get the implementation dependent name of a sensor
 */
const char *SDL_GetSensorInstanceName(SDL_SensorID instance_id)
{
    SDL_SensorDriver *driver;
    int device_index;
    const char *name = NULL;

    SDL_LockSensors();
    if (SDL_GetDriverAndSensorIndex(instance_id, &driver, &device_index)) {
        name = driver->GetDeviceName(device_index);
    }
    SDL_UnlockSensors();

    /* FIXME: Really we should reference count this name so it doesn't go away after unlock */
    return name;
}

SDL_SensorType SDL_GetSensorInstanceType(SDL_SensorID instance_id)
{
    SDL_SensorDriver *driver;
    int device_index;
    SDL_SensorType type = SDL_SENSOR_INVALID;

    SDL_LockSensors();
    if (SDL_GetDriverAndSensorIndex(instance_id, &driver, &device_index)) {
        type = driver->GetDeviceType(device_index);
    }
    SDL_UnlockSensors();

    return type;
}

int SDL_GetSensorInstanceNonPortableType(SDL_SensorID instance_id)
{
    SDL_SensorDriver *driver;
    int device_index;
    int type = -1;

    SDL_LockSensors();
    if (SDL_GetDriverAndSensorIndex(instance_id, &driver, &device_index)) {
        type = driver->GetDeviceNonPortableType(device_index);
    }
    SDL_UnlockSensors();

    return type;
}

/*
 * Open a sensor for use - the index passed as an argument refers to
 * the N'th sensor on the system.  This index is the value which will
 * identify this sensor in future sensor events.
 *
 * This function returns a sensor identifier, or NULL if an error occurred.
 */
SDL_Sensor *SDL_OpenSensor(SDL_SensorID instance_id)
{
    SDL_SensorDriver *driver;
    int device_index;
    SDL_Sensor *sensor;
    SDL_Sensor *sensorlist;
    const char *sensorname = NULL;

    SDL_LockSensors();

    if (!SDL_GetDriverAndSensorIndex(instance_id, &driver, &device_index)) {
        SDL_UnlockSensors();
        return NULL;
    }

    sensorlist = SDL_sensors;
    /* If the sensor is already open, return it
     * it is important that we have a single sensor * for each instance id
     */
    while (sensorlist) {
        if (instance_id == sensorlist->instance_id) {
            sensor = sensorlist;
            ++sensor->ref_count;
            SDL_UnlockSensors();
            return sensor;
        }
        sensorlist = sensorlist->next;
    }

    /* Create and initialize the sensor */
    sensor = (SDL_Sensor *)SDL_calloc(sizeof(*sensor), 1);
    if (sensor == NULL) {
        SDL_OutOfMemory();
        SDL_UnlockSensors();
        return NULL;
    }
    sensor->driver = driver;
    sensor->instance_id = instance_id;
    sensor->type = driver->GetDeviceType(device_index);
    sensor->non_portable_type = driver->GetDeviceNonPortableType(device_index);

    if (driver->Open(sensor, device_index) < 0) {
        SDL_free(sensor);
        SDL_UnlockSensors();
        return NULL;
    }

    sensorname = driver->GetDeviceName(device_index);
    if (sensorname) {
        sensor->name = SDL_strdup(sensorname);
    } else {
        sensor->name = NULL;
    }

    /* Add sensor to list */
    ++sensor->ref_count;
    /* Link the sensor in the list */
    sensor->next = SDL_sensors;
    SDL_sensors = sensor;

    driver->Update(sensor);

    SDL_UnlockSensors();

    return sensor;
}

/*
 * Find the SDL_Sensor that owns this instance id
 */
SDL_Sensor *SDL_GetSensorFromInstanceID(SDL_SensorID instance_id)
{
    SDL_Sensor *sensor;

    SDL_LockSensors();
    for (sensor = SDL_sensors; sensor; sensor = sensor->next) {
        if (sensor->instance_id == instance_id) {
            break;
        }
    }
    SDL_UnlockSensors();
    return sensor;
}

/*
 * Checks to make sure the sensor is valid.
 */
static int SDL_IsSensorValid(SDL_Sensor *sensor)
{
    int valid;

    if (sensor == NULL) {
        SDL_SetError("Sensor hasn't been opened yet");
        valid = 0;
    } else {
        valid = 1;
    }

    return valid;
}

/*
 * Get the friendly name of this sensor
 */
const char *SDL_GetSensorName(SDL_Sensor *sensor)
{
    if (!SDL_IsSensorValid(sensor)) {
        return NULL;
    }

    return sensor->name;
}

/*
 * Get the type of this sensor
 */
SDL_SensorType SDL_GetSensorType(SDL_Sensor *sensor)
{
    if (!SDL_IsSensorValid(sensor)) {
        return SDL_SENSOR_INVALID;
    }

    return sensor->type;
}

/*
 * Get the platform dependent type of this sensor
 */
int SDL_GetSensorNonPortableType(SDL_Sensor *sensor)
{
    if (!SDL_IsSensorValid(sensor)) {
        return -1;
    }

    return sensor->non_portable_type;
}

/*
 * Get the instance id for this opened sensor
 */
SDL_SensorID SDL_GetSensorInstanceID(SDL_Sensor *sensor)
{
    if (!SDL_IsSensorValid(sensor)) {
        return 0;
    }

    return sensor->instance_id;
}

/*
 * Get the current state of this sensor
 */
int SDL_GetSensorData(SDL_Sensor *sensor, float *data, int num_values)
{
    if (!SDL_IsSensorValid(sensor)) {
        return -1;
    }

    num_values = SDL_min(num_values, SDL_arraysize(sensor->data));
    SDL_memcpy(data, sensor->data, num_values * sizeof(*data));
    return 0;
}

/*
 * Close a sensor previously opened with SDL_OpenSensor()
 */
void SDL_CloseSensor(SDL_Sensor *sensor)
{
    SDL_Sensor *sensorlist;
    SDL_Sensor *sensorlistprev;

    if (!SDL_IsSensorValid(sensor)) {
        return;
    }

    SDL_LockSensors();

    /* First decrement ref count */
    if (--sensor->ref_count > 0) {
        SDL_UnlockSensors();
        return;
    }

    sensor->driver->Close(sensor);
    sensor->hwdata = NULL;

    sensorlist = SDL_sensors;
    sensorlistprev = NULL;
    while (sensorlist) {
        if (sensor == sensorlist) {
            if (sensorlistprev) {
                /* unlink this entry */
                sensorlistprev->next = sensorlist->next;
            } else {
                SDL_sensors = sensor->next;
            }
            break;
        }
        sensorlistprev = sensorlist;
        sensorlist = sensorlist->next;
    }

    SDL_free(sensor->name);

    /* Free the data associated with this sensor */
    SDL_free(sensor);

    SDL_UnlockSensors();
}

void SDL_QuitSensors(void)
{
    int i;

    SDL_LockSensors();

    /* Stop the event polling */
    while (SDL_sensors) {
        SDL_sensors->ref_count = 1;
        SDL_CloseSensor(SDL_sensors);
    }

    /* Quit the sensor setup */
    for (i = 0; i < SDL_arraysize(SDL_sensor_drivers); ++i) {
        SDL_sensor_drivers[i]->Quit();
    }

    SDL_UnlockSensors();

#ifndef SDL_EVENTS_DISABLED
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
#endif

    if (SDL_sensor_lock) {
        SDL_DestroyMutex(SDL_sensor_lock);
        SDL_sensor_lock = NULL;
    }
}

/* These are global for SDL_syssensor.c and SDL_events.c */

int SDL_SendSensorUpdate(Uint64 timestamp, SDL_Sensor *sensor, Uint64 sensor_timestamp, float *data, int num_values)
{
    int posted;

    /* Allow duplicate events, for things like steps and heartbeats */

    /* Update internal sensor state */
    num_values = SDL_min(num_values, SDL_arraysize(sensor->data));
    SDL_memcpy(sensor->data, data, num_values * sizeof(*data));

    /* Post the event, if desired */
    posted = 0;
#ifndef SDL_EVENTS_DISABLED
    if (SDL_EventEnabled(SDL_EVENT_SENSOR_UPDATE)) {
        SDL_Event event;
        event.type = SDL_EVENT_SENSOR_UPDATE;
        event.common.timestamp = timestamp;
        event.sensor.which = sensor->instance_id;
        num_values = SDL_min(num_values, SDL_arraysize(event.sensor.data));
        SDL_memset(event.sensor.data, 0, sizeof(event.sensor.data));
        SDL_memcpy(event.sensor.data, data, num_values * sizeof(*data));
        event.sensor.sensor_timestamp = sensor_timestamp;
        posted = SDL_PushEvent(&event) == 1;
    }
#endif /* !SDL_EVENTS_DISABLED */

    SDL_GamepadSensorWatcher(timestamp, sensor->instance_id, sensor_timestamp, data, num_values);

    return posted;
}

void SDL_UpdateSensor(SDL_Sensor *sensor)
{
    sensor->driver->Update(sensor);
}

void SDL_UpdateSensors(void)
{
    int i;
    SDL_Sensor *sensor;

    if (!SDL_WasInit(SDL_INIT_SENSOR)) {
        return;
    }

    SDL_LockSensors();

    for (sensor = SDL_sensors; sensor; sensor = sensor->next) {
        sensor->driver->Update(sensor);
    }

    /* this needs to happen AFTER walking the sensor list above, so that any
       dangling hardware data from removed devices can be free'd
     */
    for (i = 0; i < SDL_arraysize(SDL_sensor_drivers); ++i) {
        SDL_sensor_drivers[i]->Detect();
    }

    SDL_UnlockSensors();
}
