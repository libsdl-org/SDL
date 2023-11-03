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

#ifndef SDL_THREAD_SAFETY_ANALYSIS
static
#endif
SDL_Mutex *SDL_sensor_lock = NULL; /* This needs to support recursive locks */
static SDL_AtomicInt SDL_sensor_lock_pending;
static int SDL_sensors_locked;
static SDL_bool SDL_sensors_initialized;
static SDL_Sensor *SDL_sensors SDL_GUARDED_BY(SDL_sensor_lock) = NULL;
static char SDL_sensor_magic;

#define CHECK_SENSOR_MAGIC(sensor, retval)              \
    if (!sensor || sensor->magic != &SDL_sensor_magic) { \
        SDL_InvalidParamError("sensor");                \
        SDL_UnlockSensors();                            \
        return retval;                                  \
    }

SDL_bool SDL_SensorsInitialized(void)
{
    return SDL_sensors_initialized;
}

void SDL_LockSensors(void)
{
    (void)SDL_AtomicIncRef(&SDL_sensor_lock_pending);
    SDL_LockMutex(SDL_sensor_lock);
    (void)SDL_AtomicDecRef(&SDL_sensor_lock_pending);

    ++SDL_sensors_locked;
}

void SDL_UnlockSensors(void)
{
    SDL_bool last_unlock = SDL_FALSE;

    --SDL_sensors_locked;

    if (!SDL_sensors_initialized) {
        /* NOTE: There's a small window here where another thread could lock the mutex after we've checked for pending locks */
        if (!SDL_sensors_locked && SDL_AtomicGet(&SDL_sensor_lock_pending) == 0) {
            last_unlock = SDL_TRUE;
        }
    }

    /* The last unlock after sensors are uninitialized will cleanup the mutex,
     * allowing applications to lock sensors while reinitializing the system.
     */
    if (last_unlock) {
        SDL_Mutex *sensor_lock = SDL_sensor_lock;

        SDL_LockMutex(sensor_lock);
        {
            SDL_UnlockMutex(SDL_sensor_lock);

            SDL_sensor_lock = NULL;
        }
        SDL_UnlockMutex(sensor_lock);
        SDL_DestroyMutex(sensor_lock);
    } else {
        SDL_UnlockMutex(SDL_sensor_lock);
    }
}

SDL_bool SDL_SensorsLocked(void)
{
    return (SDL_sensors_locked > 0);
}

void SDL_AssertSensorsLocked(void)
{
    SDL_assert(SDL_SensorsLocked());
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

    SDL_LockSensors();

    SDL_sensors_initialized = SDL_TRUE;

    status = -1;
    for (i = 0; i < SDL_arraysize(SDL_sensor_drivers); ++i) {
        if (SDL_sensor_drivers[i]->Init() >= 0) {
            status = 0;
        }
    }

    SDL_UnlockSensors();

    if (status < 0) {
        SDL_QuitSensors();
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
    sensor->magic = &SDL_sensor_magic;
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
 * Get the properties associated with a sensor.
 */
SDL_PropertiesID SDL_GetSensorProperties(SDL_Sensor *sensor)
{
    SDL_PropertiesID retval;

    SDL_LockSensors();
    {
        CHECK_SENSOR_MAGIC(sensor, 0);

        if (sensor->props == 0) {
            sensor->props = SDL_CreateProperties();
        }
        retval = sensor->props;
    }
    SDL_UnlockSensors();

    return retval;
}

/*
 * Get the friendly name of this sensor
 */
const char *SDL_GetSensorName(SDL_Sensor *sensor)
{
    const char *retval;

    SDL_LockSensors();
    {
        CHECK_SENSOR_MAGIC(sensor, NULL);

        retval = sensor->name;
    }
    SDL_UnlockSensors();

    return retval;
}

/*
 * Get the type of this sensor
 */
SDL_SensorType SDL_GetSensorType(SDL_Sensor *sensor)
{
    SDL_SensorType retval;

    SDL_LockSensors();
    {
        CHECK_SENSOR_MAGIC(sensor, SDL_SENSOR_INVALID);

        retval = sensor->type;
    }
    SDL_UnlockSensors();

    return retval;
}

/*
 * Get the platform dependent type of this sensor
 */
int SDL_GetSensorNonPortableType(SDL_Sensor *sensor)
{
    int retval;

    SDL_LockSensors();
    {
        CHECK_SENSOR_MAGIC(sensor, -1);

        retval = sensor->non_portable_type;
    }
    SDL_UnlockSensors();

    return retval;
}

/*
 * Get the instance id for this opened sensor
 */
SDL_SensorID SDL_GetSensorInstanceID(SDL_Sensor *sensor)
{
    SDL_SensorID retval;

    SDL_LockSensors();
    {
        CHECK_SENSOR_MAGIC(sensor, 0);

        retval = sensor->instance_id;
    }
    SDL_UnlockSensors();

    return retval;
}

/*
 * Get the current state of this sensor
 */
int SDL_GetSensorData(SDL_Sensor *sensor, float *data, int num_values)
{
    SDL_LockSensors();
    {
        CHECK_SENSOR_MAGIC(sensor, -1);

        num_values = SDL_min(num_values, SDL_arraysize(sensor->data));
        SDL_memcpy(data, sensor->data, num_values * sizeof(*data));
    }
    SDL_UnlockSensors();

    return 0;
}

/*
 * Close a sensor previously opened with SDL_OpenSensor()
 */
void SDL_CloseSensor(SDL_Sensor *sensor)
{
    SDL_Sensor *sensorlist;
    SDL_Sensor *sensorlistprev;

    SDL_LockSensors();
    {
        CHECK_SENSOR_MAGIC(sensor,);

        /* First decrement ref count */
        if (--sensor->ref_count > 0) {
            SDL_UnlockSensors();
            return;
        }

        SDL_DestroyProperties(sensor->props);

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

        /* Free the data associated with this sensor */
        SDL_free(sensor->name);
        SDL_free(sensor);
    }
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

#ifndef SDL_EVENTS_DISABLED
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
#endif

    SDL_sensors_initialized = SDL_FALSE;

    SDL_UnlockSensors();
}

/* These are global for SDL_syssensor.c and SDL_events.c */

int SDL_SendSensorUpdate(Uint64 timestamp, SDL_Sensor *sensor, Uint64 sensor_timestamp, float *data, int num_values)
{
    int posted;

    SDL_AssertSensorsLocked();

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
    SDL_LockSensors();
    {
        CHECK_SENSOR_MAGIC(sensor,);

        sensor->driver->Update(sensor);
    }
    SDL_UnlockSensors();
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
