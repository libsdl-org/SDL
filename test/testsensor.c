/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple test of the SDL sensor code */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

static const char *GetSensorTypeString(SDL_SensorType type)
{
    static char unknown_type[64];

    switch (type) {
    case SDL_SENSOR_INVALID:
        return "SDL_SENSOR_INVALID";
    case SDL_SENSOR_UNKNOWN:
        return "SDL_SENSOR_UNKNOWN";
    case SDL_SENSOR_ACCEL:
        return "SDL_SENSOR_ACCEL";
    case SDL_SENSOR_GYRO:
        return "SDL_SENSOR_GYRO";
    default:
        (void)SDL_snprintf(unknown_type, sizeof(unknown_type), "UNKNOWN (%d)", type);
        return unknown_type;
    }
}

static void HandleSensorEvent(SDL_SensorEvent *event)
{
    SDL_Sensor *sensor = SDL_GetSensorFromID(event->which);
    if (!sensor) {
        SDL_Log("Couldn't get sensor for sensor event");
        return;
    }

    switch (SDL_GetSensorType(sensor)) {
    case SDL_SENSOR_ACCEL:
        SDL_Log("Accelerometer update: %.2f, %.2f, %.2f", event->data[0], event->data[1], event->data[2]);
        break;
    case SDL_SENSOR_GYRO:
        SDL_Log("Gyro update: %.2f, %.2f, %.2f", event->data[0], event->data[1], event->data[2]);
        break;
    default:
        SDL_Log("Sensor update for sensor type %s", GetSensorTypeString(SDL_GetSensorType(sensor)));
        break;
    }
}

int main(int argc, char **argv)
{
    SDL_SensorID *sensors;
    int i, num_sensors, num_opened;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        SDL_Quit();
        SDLTest_CommonDestroyState(state);
        return 1;
    }

    /* Load the SDL library */
    if (!SDL_Init(SDL_INIT_SENSOR)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        SDL_Quit();
        SDLTest_CommonDestroyState(state);
        return 1;
    }

    sensors = SDL_GetSensors(&num_sensors);
    num_opened = 0;

    SDL_Log("There are %d sensors available", num_sensors);
    if (sensors) {
        for (i = 0; i < num_sensors; ++i) {
            SDL_Log("Sensor %" SDL_PRIu32 ": %s, type %s, platform type %d",
                    sensors[i],
                    SDL_GetSensorNameForID(sensors[i]),
                    GetSensorTypeString(SDL_GetSensorTypeForID(sensors[i])),
                    SDL_GetSensorNonPortableTypeForID(sensors[i]));

            if (SDL_GetSensorTypeForID(sensors[i]) != SDL_SENSOR_UNKNOWN) {
                SDL_Sensor *sensor = SDL_OpenSensor(sensors[i]);
                if (!sensor) {
                    SDL_Log("Couldn't open sensor %" SDL_PRIu32 ": %s", sensors[i], SDL_GetError());
                } else {
                    ++num_opened;
                }
            }
        }
        SDL_free(sensors);
    }
    SDL_Log("Opened %d sensors", num_opened);

    if (num_opened > 0) {
        bool done = false;
        SDL_Event event;

        SDL_CreateWindow("Sensor Test", 0, 0, SDL_WINDOW_FULLSCREEN);
        while (!done) {
            /* Update to get the current event state */
            SDL_PumpEvents();

            /* Process all currently pending events */
            while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) == 1) {
                switch (event.type) {
                case SDL_EVENT_SENSOR_UPDATE:
                    HandleSensorEvent(&event.sensor);
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                case SDL_EVENT_KEY_UP:
                case SDL_EVENT_QUIT:
                    done = true;
                    break;
                default:
                    break;
                }
            }
        }
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
