
#define SDL_OFFSETOF(type, member)  ((int)(uintptr_t)(&((type *)0)->member))

static SDL_bool ValidatePadding(void)
{
    SDL_bool result = SDL_TRUE;

    /* SDL_CommonEvent */

    if (sizeof(SDL_CommonEvent_pack8) != sizeof(SDL_CommonEvent_pack1)) {
        SDL_Log("SDL_CommonEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_CommonEvent_pack1), (int)sizeof(SDL_CommonEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CommonEvent_pack8, type) != SDL_OFFSETOF(SDL_CommonEvent_pack1, type)) {
        SDL_Log("SDL_CommonEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CommonEvent_pack1, type), SDL_OFFSETOF(SDL_CommonEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CommonEvent_pack8, reserved) != SDL_OFFSETOF(SDL_CommonEvent_pack1, reserved)) {
        SDL_Log("SDL_CommonEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CommonEvent_pack1, reserved), SDL_OFFSETOF(SDL_CommonEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CommonEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_CommonEvent_pack1, timestamp)) {
        SDL_Log("SDL_CommonEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CommonEvent_pack1, timestamp), SDL_OFFSETOF(SDL_CommonEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_CommonEvent_pack4) != sizeof(SDL_CommonEvent_pack1)) {
        SDL_Log("SDL_CommonEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_CommonEvent_pack1), (int)sizeof(SDL_CommonEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CommonEvent_pack4, type) != SDL_OFFSETOF(SDL_CommonEvent_pack1, type)) {
        SDL_Log("SDL_CommonEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CommonEvent_pack1, type), SDL_OFFSETOF(SDL_CommonEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CommonEvent_pack4, reserved) != SDL_OFFSETOF(SDL_CommonEvent_pack1, reserved)) {
        SDL_Log("SDL_CommonEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CommonEvent_pack1, reserved), SDL_OFFSETOF(SDL_CommonEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CommonEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_CommonEvent_pack1, timestamp)) {
        SDL_Log("SDL_CommonEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CommonEvent_pack1, timestamp), SDL_OFFSETOF(SDL_CommonEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    /* SDL_DisplayEvent */

    if (sizeof(SDL_DisplayEvent_pack8) != sizeof(SDL_DisplayEvent_pack1)) {
        SDL_Log("SDL_DisplayEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_DisplayEvent_pack1), (int)sizeof(SDL_DisplayEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack8, type) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, type)) {
        SDL_Log("SDL_DisplayEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, type), SDL_OFFSETOF(SDL_DisplayEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack8, reserved) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, reserved)) {
        SDL_Log("SDL_DisplayEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, reserved), SDL_OFFSETOF(SDL_DisplayEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, timestamp)) {
        SDL_Log("SDL_DisplayEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, timestamp), SDL_OFFSETOF(SDL_DisplayEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack8, displayID) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, displayID)) {
        SDL_Log("SDL_DisplayEvent.displayID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, displayID), SDL_OFFSETOF(SDL_DisplayEvent_pack8, displayID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack8, data1) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, data1)) {
        SDL_Log("SDL_DisplayEvent.data1 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, data1), SDL_OFFSETOF(SDL_DisplayEvent_pack8, data1));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack8, data2) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, data2)) {
        SDL_Log("SDL_DisplayEvent.data2 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, data2), SDL_OFFSETOF(SDL_DisplayEvent_pack8, data2));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, padding_end)) {
        SDL_Log("SDL_DisplayEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, padding_end), SDL_OFFSETOF(SDL_DisplayEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_DisplayEvent_pack4) != sizeof(SDL_DisplayEvent_pack1)) {
        SDL_Log("SDL_DisplayEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_DisplayEvent_pack1), (int)sizeof(SDL_DisplayEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack4, type) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, type)) {
        SDL_Log("SDL_DisplayEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, type), SDL_OFFSETOF(SDL_DisplayEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack4, reserved) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, reserved)) {
        SDL_Log("SDL_DisplayEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, reserved), SDL_OFFSETOF(SDL_DisplayEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, timestamp)) {
        SDL_Log("SDL_DisplayEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, timestamp), SDL_OFFSETOF(SDL_DisplayEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack4, displayID) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, displayID)) {
        SDL_Log("SDL_DisplayEvent.displayID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, displayID), SDL_OFFSETOF(SDL_DisplayEvent_pack4, displayID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack4, data1) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, data1)) {
        SDL_Log("SDL_DisplayEvent.data1 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, data1), SDL_OFFSETOF(SDL_DisplayEvent_pack4, data1));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack4, data2) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, data2)) {
        SDL_Log("SDL_DisplayEvent.data2 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, data2), SDL_OFFSETOF(SDL_DisplayEvent_pack4, data2));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_DisplayEvent_pack1, padding_end)) {
        SDL_Log("SDL_DisplayEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayEvent_pack1, padding_end), SDL_OFFSETOF(SDL_DisplayEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_WindowEvent */

    if (sizeof(SDL_WindowEvent_pack8) != sizeof(SDL_WindowEvent_pack1)) {
        SDL_Log("SDL_WindowEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_WindowEvent_pack1), (int)sizeof(SDL_WindowEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack8, type) != SDL_OFFSETOF(SDL_WindowEvent_pack1, type)) {
        SDL_Log("SDL_WindowEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, type), SDL_OFFSETOF(SDL_WindowEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack8, reserved) != SDL_OFFSETOF(SDL_WindowEvent_pack1, reserved)) {
        SDL_Log("SDL_WindowEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, reserved), SDL_OFFSETOF(SDL_WindowEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_WindowEvent_pack1, timestamp)) {
        SDL_Log("SDL_WindowEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, timestamp), SDL_OFFSETOF(SDL_WindowEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack8, windowID) != SDL_OFFSETOF(SDL_WindowEvent_pack1, windowID)) {
        SDL_Log("SDL_WindowEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, windowID), SDL_OFFSETOF(SDL_WindowEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack8, data1) != SDL_OFFSETOF(SDL_WindowEvent_pack1, data1)) {
        SDL_Log("SDL_WindowEvent.data1 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, data1), SDL_OFFSETOF(SDL_WindowEvent_pack8, data1));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack8, data2) != SDL_OFFSETOF(SDL_WindowEvent_pack1, data2)) {
        SDL_Log("SDL_WindowEvent.data2 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, data2), SDL_OFFSETOF(SDL_WindowEvent_pack8, data2));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_WindowEvent_pack1, padding_end)) {
        SDL_Log("SDL_WindowEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, padding_end), SDL_OFFSETOF(SDL_WindowEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_WindowEvent_pack4) != sizeof(SDL_WindowEvent_pack1)) {
        SDL_Log("SDL_WindowEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_WindowEvent_pack1), (int)sizeof(SDL_WindowEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack4, type) != SDL_OFFSETOF(SDL_WindowEvent_pack1, type)) {
        SDL_Log("SDL_WindowEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, type), SDL_OFFSETOF(SDL_WindowEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack4, reserved) != SDL_OFFSETOF(SDL_WindowEvent_pack1, reserved)) {
        SDL_Log("SDL_WindowEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, reserved), SDL_OFFSETOF(SDL_WindowEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_WindowEvent_pack1, timestamp)) {
        SDL_Log("SDL_WindowEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, timestamp), SDL_OFFSETOF(SDL_WindowEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack4, windowID) != SDL_OFFSETOF(SDL_WindowEvent_pack1, windowID)) {
        SDL_Log("SDL_WindowEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, windowID), SDL_OFFSETOF(SDL_WindowEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack4, data1) != SDL_OFFSETOF(SDL_WindowEvent_pack1, data1)) {
        SDL_Log("SDL_WindowEvent.data1 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, data1), SDL_OFFSETOF(SDL_WindowEvent_pack4, data1));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack4, data2) != SDL_OFFSETOF(SDL_WindowEvent_pack1, data2)) {
        SDL_Log("SDL_WindowEvent.data2 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, data2), SDL_OFFSETOF(SDL_WindowEvent_pack4, data2));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_WindowEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_WindowEvent_pack1, padding_end)) {
        SDL_Log("SDL_WindowEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_WindowEvent_pack1, padding_end), SDL_OFFSETOF(SDL_WindowEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_KeyboardDeviceEvent */

    if (sizeof(SDL_KeyboardDeviceEvent_pack8) != sizeof(SDL_KeyboardDeviceEvent_pack1)) {
        SDL_Log("SDL_KeyboardDeviceEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_KeyboardDeviceEvent_pack1), (int)sizeof(SDL_KeyboardDeviceEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack8, type) != SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, type)) {
        SDL_Log("SDL_KeyboardDeviceEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack8, reserved) != SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_KeyboardDeviceEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_KeyboardDeviceEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack8, which) != SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, which)) {
        SDL_Log("SDL_KeyboardDeviceEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, padding_end)) {
        SDL_Log("SDL_KeyboardDeviceEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, padding_end), SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_KeyboardDeviceEvent_pack4) != sizeof(SDL_KeyboardDeviceEvent_pack1)) {
        SDL_Log("SDL_KeyboardDeviceEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_KeyboardDeviceEvent_pack1), (int)sizeof(SDL_KeyboardDeviceEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack4, type) != SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, type)) {
        SDL_Log("SDL_KeyboardDeviceEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack4, reserved) != SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_KeyboardDeviceEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_KeyboardDeviceEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack4, which) != SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, which)) {
        SDL_Log("SDL_KeyboardDeviceEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, padding_end)) {
        SDL_Log("SDL_KeyboardDeviceEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack1, padding_end), SDL_OFFSETOF(SDL_KeyboardDeviceEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_KeyboardEvent */

    if (sizeof(SDL_KeyboardEvent_pack8) != sizeof(SDL_KeyboardEvent_pack1)) {
        SDL_Log("SDL_KeyboardEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_KeyboardEvent_pack1), (int)sizeof(SDL_KeyboardEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, type) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, type)) {
        SDL_Log("SDL_KeyboardEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, type), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, reserved) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, reserved)) {
        SDL_Log("SDL_KeyboardEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, reserved), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, timestamp)) {
        SDL_Log("SDL_KeyboardEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, timestamp), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, windowID) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, windowID)) {
        SDL_Log("SDL_KeyboardEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, windowID), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, which) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, which)) {
        SDL_Log("SDL_KeyboardEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, which), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, scancode) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, scancode)) {
        SDL_Log("SDL_KeyboardEvent.scancode has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, scancode), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, scancode));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, key) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, key)) {
        SDL_Log("SDL_KeyboardEvent.key has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, key), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, key));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, mod) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, mod)) {
        SDL_Log("SDL_KeyboardEvent.mod has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, mod), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, mod));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, raw) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, raw)) {
        SDL_Log("SDL_KeyboardEvent.raw has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, raw), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, raw));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, padding16) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, padding16)) {
        SDL_Log("SDL_KeyboardEvent.padding16 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, padding16), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, state) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, state)) {
        SDL_Log("SDL_KeyboardEvent.state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, state), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack8, repeat) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, repeat)) {
        SDL_Log("SDL_KeyboardEvent.repeat has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, repeat), SDL_OFFSETOF(SDL_KeyboardEvent_pack8, repeat));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_KeyboardEvent_pack4) != sizeof(SDL_KeyboardEvent_pack1)) {
        SDL_Log("SDL_KeyboardEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_KeyboardEvent_pack1), (int)sizeof(SDL_KeyboardEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, type) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, type)) {
        SDL_Log("SDL_KeyboardEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, type), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, reserved) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, reserved)) {
        SDL_Log("SDL_KeyboardEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, reserved), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, timestamp)) {
        SDL_Log("SDL_KeyboardEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, timestamp), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, windowID) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, windowID)) {
        SDL_Log("SDL_KeyboardEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, windowID), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, which) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, which)) {
        SDL_Log("SDL_KeyboardEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, which), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, scancode) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, scancode)) {
        SDL_Log("SDL_KeyboardEvent.scancode has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, scancode), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, scancode));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, key) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, key)) {
        SDL_Log("SDL_KeyboardEvent.key has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, key), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, key));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, mod) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, mod)) {
        SDL_Log("SDL_KeyboardEvent.mod has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, mod), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, mod));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, raw) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, raw)) {
        SDL_Log("SDL_KeyboardEvent.raw has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, raw), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, raw));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, padding16) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, padding16)) {
        SDL_Log("SDL_KeyboardEvent.padding16 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, padding16), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, state) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, state)) {
        SDL_Log("SDL_KeyboardEvent.state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, state), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_KeyboardEvent_pack4, repeat) != SDL_OFFSETOF(SDL_KeyboardEvent_pack1, repeat)) {
        SDL_Log("SDL_KeyboardEvent.repeat has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_KeyboardEvent_pack1, repeat), SDL_OFFSETOF(SDL_KeyboardEvent_pack4, repeat));
        result = SDL_FALSE;
    }

    /* SDL_TextEditingEvent */

    if (sizeof(SDL_TextEditingEvent_pack8) != sizeof(SDL_TextEditingEvent_pack1)) {
        SDL_Log("SDL_TextEditingEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_TextEditingEvent_pack1), (int)sizeof(SDL_TextEditingEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack8, type) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, type)) {
        SDL_Log("SDL_TextEditingEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, type), SDL_OFFSETOF(SDL_TextEditingEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack8, reserved) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, reserved)) {
        SDL_Log("SDL_TextEditingEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, reserved), SDL_OFFSETOF(SDL_TextEditingEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, timestamp)) {
        SDL_Log("SDL_TextEditingEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, timestamp), SDL_OFFSETOF(SDL_TextEditingEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack8, windowID) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, windowID)) {
        SDL_Log("SDL_TextEditingEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, windowID), SDL_OFFSETOF(SDL_TextEditingEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack8, padding32) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, padding32)) {
        SDL_Log("SDL_TextEditingEvent.padding32 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, padding32), SDL_OFFSETOF(SDL_TextEditingEvent_pack8, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack8, text) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, text)) {
        SDL_Log("SDL_TextEditingEvent.text has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, text), SDL_OFFSETOF(SDL_TextEditingEvent_pack8, text));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack8, start) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, start)) {
        SDL_Log("SDL_TextEditingEvent.start has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, start), SDL_OFFSETOF(SDL_TextEditingEvent_pack8, start));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack8, length) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, length)) {
        SDL_Log("SDL_TextEditingEvent.length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, length), SDL_OFFSETOF(SDL_TextEditingEvent_pack8, length));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_TextEditingEvent_pack4) != sizeof(SDL_TextEditingEvent_pack1)) {
        SDL_Log("SDL_TextEditingEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_TextEditingEvent_pack1), (int)sizeof(SDL_TextEditingEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack4, type) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, type)) {
        SDL_Log("SDL_TextEditingEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, type), SDL_OFFSETOF(SDL_TextEditingEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack4, reserved) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, reserved)) {
        SDL_Log("SDL_TextEditingEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, reserved), SDL_OFFSETOF(SDL_TextEditingEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, timestamp)) {
        SDL_Log("SDL_TextEditingEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, timestamp), SDL_OFFSETOF(SDL_TextEditingEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack4, windowID) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, windowID)) {
        SDL_Log("SDL_TextEditingEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, windowID), SDL_OFFSETOF(SDL_TextEditingEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack4, padding32) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, padding32)) {
        SDL_Log("SDL_TextEditingEvent.padding32 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, padding32), SDL_OFFSETOF(SDL_TextEditingEvent_pack4, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack4, text) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, text)) {
        SDL_Log("SDL_TextEditingEvent.text has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, text), SDL_OFFSETOF(SDL_TextEditingEvent_pack4, text));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack4, start) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, start)) {
        SDL_Log("SDL_TextEditingEvent.start has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, start), SDL_OFFSETOF(SDL_TextEditingEvent_pack4, start));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingEvent_pack4, length) != SDL_OFFSETOF(SDL_TextEditingEvent_pack1, length)) {
        SDL_Log("SDL_TextEditingEvent.length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingEvent_pack1, length), SDL_OFFSETOF(SDL_TextEditingEvent_pack4, length));
        result = SDL_FALSE;
    }

    /* SDL_TextEditingCandidatesEvent */

    if (sizeof(SDL_TextEditingCandidatesEvent_pack8) != sizeof(SDL_TextEditingCandidatesEvent_pack1)) {
        SDL_Log("SDL_TextEditingCandidatesEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_TextEditingCandidatesEvent_pack1), (int)sizeof(SDL_TextEditingCandidatesEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, type) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, type)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, type), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, reserved) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, reserved)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, reserved), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, timestamp)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, timestamp), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, windowID) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, windowID)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, windowID), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, padding32) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding32)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.padding32 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding32), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, candidates) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, candidates)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.candidates has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, candidates), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, candidates));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, num_candidates) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, num_candidates)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.num_candidates has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, num_candidates), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, num_candidates));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, selected_candidate) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, selected_candidate)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.selected_candidate has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, selected_candidate), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, selected_candidate));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, horizontal) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, horizontal)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.horizontal has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, horizontal), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, horizontal));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, padding8[0]) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding8[0])) {
        SDL_Log("SDL_TextEditingCandidatesEvent.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding_end)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding_end), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_TextEditingCandidatesEvent_pack4) != sizeof(SDL_TextEditingCandidatesEvent_pack1)) {
        SDL_Log("SDL_TextEditingCandidatesEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_TextEditingCandidatesEvent_pack1), (int)sizeof(SDL_TextEditingCandidatesEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, type) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, type)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, type), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, reserved) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, reserved)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, reserved), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, timestamp)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, timestamp), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, windowID) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, windowID)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, windowID), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, padding32) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding32)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.padding32 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding32), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, candidates) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, candidates)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.candidates has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, candidates), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, candidates));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, num_candidates) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, num_candidates)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.num_candidates has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, num_candidates), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, num_candidates));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, selected_candidate) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, selected_candidate)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.selected_candidate has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, selected_candidate), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, selected_candidate));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, horizontal) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, horizontal)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.horizontal has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, horizontal), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, horizontal));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, padding8[0]) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding8[0])) {
        SDL_Log("SDL_TextEditingCandidatesEvent.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding_end)) {
        SDL_Log("SDL_TextEditingCandidatesEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack1, padding_end), SDL_OFFSETOF(SDL_TextEditingCandidatesEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_TextInputEvent */

    if (sizeof(SDL_TextInputEvent_pack8) != sizeof(SDL_TextInputEvent_pack1)) {
        SDL_Log("SDL_TextInputEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_TextInputEvent_pack1), (int)sizeof(SDL_TextInputEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack8, type) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, type)) {
        SDL_Log("SDL_TextInputEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, type), SDL_OFFSETOF(SDL_TextInputEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack8, reserved) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, reserved)) {
        SDL_Log("SDL_TextInputEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, reserved), SDL_OFFSETOF(SDL_TextInputEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, timestamp)) {
        SDL_Log("SDL_TextInputEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, timestamp), SDL_OFFSETOF(SDL_TextInputEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack8, windowID) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, windowID)) {
        SDL_Log("SDL_TextInputEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, windowID), SDL_OFFSETOF(SDL_TextInputEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack8, padding32) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, padding32)) {
        SDL_Log("SDL_TextInputEvent.padding32 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, padding32), SDL_OFFSETOF(SDL_TextInputEvent_pack8, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack8, text) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, text)) {
        SDL_Log("SDL_TextInputEvent.text has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, text), SDL_OFFSETOF(SDL_TextInputEvent_pack8, text));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_TextInputEvent_pack4) != sizeof(SDL_TextInputEvent_pack1)) {
        SDL_Log("SDL_TextInputEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_TextInputEvent_pack1), (int)sizeof(SDL_TextInputEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack4, type) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, type)) {
        SDL_Log("SDL_TextInputEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, type), SDL_OFFSETOF(SDL_TextInputEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack4, reserved) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, reserved)) {
        SDL_Log("SDL_TextInputEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, reserved), SDL_OFFSETOF(SDL_TextInputEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, timestamp)) {
        SDL_Log("SDL_TextInputEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, timestamp), SDL_OFFSETOF(SDL_TextInputEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack4, windowID) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, windowID)) {
        SDL_Log("SDL_TextInputEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, windowID), SDL_OFFSETOF(SDL_TextInputEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack4, padding32) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, padding32)) {
        SDL_Log("SDL_TextInputEvent.padding32 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, padding32), SDL_OFFSETOF(SDL_TextInputEvent_pack4, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TextInputEvent_pack4, text) != SDL_OFFSETOF(SDL_TextInputEvent_pack1, text)) {
        SDL_Log("SDL_TextInputEvent.text has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TextInputEvent_pack1, text), SDL_OFFSETOF(SDL_TextInputEvent_pack4, text));
        result = SDL_FALSE;
    }

    /* SDL_MouseDeviceEvent */

    if (sizeof(SDL_MouseDeviceEvent_pack8) != sizeof(SDL_MouseDeviceEvent_pack1)) {
        SDL_Log("SDL_MouseDeviceEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MouseDeviceEvent_pack1), (int)sizeof(SDL_MouseDeviceEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseDeviceEvent_pack8, type) != SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, type)) {
        SDL_Log("SDL_MouseDeviceEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_MouseDeviceEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseDeviceEvent_pack8, reserved) != SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_MouseDeviceEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_MouseDeviceEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseDeviceEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_MouseDeviceEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_MouseDeviceEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseDeviceEvent_pack8, which) != SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, which)) {
        SDL_Log("SDL_MouseDeviceEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_MouseDeviceEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseDeviceEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, padding_end)) {
        SDL_Log("SDL_MouseDeviceEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, padding_end), SDL_OFFSETOF(SDL_MouseDeviceEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_MouseDeviceEvent_pack4) != sizeof(SDL_MouseDeviceEvent_pack1)) {
        SDL_Log("SDL_MouseDeviceEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MouseDeviceEvent_pack1), (int)sizeof(SDL_MouseDeviceEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseDeviceEvent_pack4, type) != SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, type)) {
        SDL_Log("SDL_MouseDeviceEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_MouseDeviceEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseDeviceEvent_pack4, reserved) != SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_MouseDeviceEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_MouseDeviceEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseDeviceEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_MouseDeviceEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_MouseDeviceEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseDeviceEvent_pack4, which) != SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, which)) {
        SDL_Log("SDL_MouseDeviceEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_MouseDeviceEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseDeviceEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, padding_end)) {
        SDL_Log("SDL_MouseDeviceEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseDeviceEvent_pack1, padding_end), SDL_OFFSETOF(SDL_MouseDeviceEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_MouseMotionEvent */

    if (sizeof(SDL_MouseMotionEvent_pack8) != sizeof(SDL_MouseMotionEvent_pack1)) {
        SDL_Log("SDL_MouseMotionEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MouseMotionEvent_pack1), (int)sizeof(SDL_MouseMotionEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, type) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, type)) {
        SDL_Log("SDL_MouseMotionEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, type), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, reserved) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, reserved)) {
        SDL_Log("SDL_MouseMotionEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, reserved), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, timestamp)) {
        SDL_Log("SDL_MouseMotionEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, timestamp), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, windowID) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, windowID)) {
        SDL_Log("SDL_MouseMotionEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, windowID), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, which) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, which)) {
        SDL_Log("SDL_MouseMotionEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, which), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, state) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, state)) {
        SDL_Log("SDL_MouseMotionEvent.state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, state), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, x) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, x)) {
        SDL_Log("SDL_MouseMotionEvent.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, x), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, y) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, y)) {
        SDL_Log("SDL_MouseMotionEvent.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, y), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, xrel) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, xrel)) {
        SDL_Log("SDL_MouseMotionEvent.xrel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, xrel), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, xrel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, yrel) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, yrel)) {
        SDL_Log("SDL_MouseMotionEvent.yrel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, yrel), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, yrel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, padding_end)) {
        SDL_Log("SDL_MouseMotionEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, padding_end), SDL_OFFSETOF(SDL_MouseMotionEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_MouseMotionEvent_pack4) != sizeof(SDL_MouseMotionEvent_pack1)) {
        SDL_Log("SDL_MouseMotionEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MouseMotionEvent_pack1), (int)sizeof(SDL_MouseMotionEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, type) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, type)) {
        SDL_Log("SDL_MouseMotionEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, type), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, reserved) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, reserved)) {
        SDL_Log("SDL_MouseMotionEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, reserved), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, timestamp)) {
        SDL_Log("SDL_MouseMotionEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, timestamp), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, windowID) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, windowID)) {
        SDL_Log("SDL_MouseMotionEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, windowID), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, which) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, which)) {
        SDL_Log("SDL_MouseMotionEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, which), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, state) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, state)) {
        SDL_Log("SDL_MouseMotionEvent.state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, state), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, x) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, x)) {
        SDL_Log("SDL_MouseMotionEvent.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, x), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, y) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, y)) {
        SDL_Log("SDL_MouseMotionEvent.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, y), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, xrel) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, xrel)) {
        SDL_Log("SDL_MouseMotionEvent.xrel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, xrel), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, xrel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, yrel) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, yrel)) {
        SDL_Log("SDL_MouseMotionEvent.yrel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, yrel), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, yrel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, padding_end)) {
        SDL_Log("SDL_MouseMotionEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseMotionEvent_pack1, padding_end), SDL_OFFSETOF(SDL_MouseMotionEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_MouseButtonEvent */

    if (sizeof(SDL_MouseButtonEvent_pack8) != sizeof(SDL_MouseButtonEvent_pack1)) {
        SDL_Log("SDL_MouseButtonEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MouseButtonEvent_pack1), (int)sizeof(SDL_MouseButtonEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, type) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, type)) {
        SDL_Log("SDL_MouseButtonEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, type), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, reserved) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, reserved)) {
        SDL_Log("SDL_MouseButtonEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, reserved), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, timestamp)) {
        SDL_Log("SDL_MouseButtonEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, timestamp), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, windowID) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, windowID)) {
        SDL_Log("SDL_MouseButtonEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, windowID), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, which) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, which)) {
        SDL_Log("SDL_MouseButtonEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, which), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, button) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, button)) {
        SDL_Log("SDL_MouseButtonEvent.button has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, button), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, state) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, state)) {
        SDL_Log("SDL_MouseButtonEvent.state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, state), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, clicks) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, clicks)) {
        SDL_Log("SDL_MouseButtonEvent.clicks has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, clicks), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, clicks));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, padding8) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, padding8)) {
        SDL_Log("SDL_MouseButtonEvent.padding8 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, padding8), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, padding8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, x) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, x)) {
        SDL_Log("SDL_MouseButtonEvent.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, x), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, y) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, y)) {
        SDL_Log("SDL_MouseButtonEvent.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, y), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, padding_end)) {
        SDL_Log("SDL_MouseButtonEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, padding_end), SDL_OFFSETOF(SDL_MouseButtonEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_MouseButtonEvent_pack4) != sizeof(SDL_MouseButtonEvent_pack1)) {
        SDL_Log("SDL_MouseButtonEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MouseButtonEvent_pack1), (int)sizeof(SDL_MouseButtonEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, type) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, type)) {
        SDL_Log("SDL_MouseButtonEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, type), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, reserved) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, reserved)) {
        SDL_Log("SDL_MouseButtonEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, reserved), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, timestamp)) {
        SDL_Log("SDL_MouseButtonEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, timestamp), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, windowID) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, windowID)) {
        SDL_Log("SDL_MouseButtonEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, windowID), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, which) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, which)) {
        SDL_Log("SDL_MouseButtonEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, which), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, button) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, button)) {
        SDL_Log("SDL_MouseButtonEvent.button has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, button), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, state) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, state)) {
        SDL_Log("SDL_MouseButtonEvent.state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, state), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, clicks) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, clicks)) {
        SDL_Log("SDL_MouseButtonEvent.clicks has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, clicks), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, clicks));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, padding8) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, padding8)) {
        SDL_Log("SDL_MouseButtonEvent.padding8 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, padding8), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, padding8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, x) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, x)) {
        SDL_Log("SDL_MouseButtonEvent.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, x), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, y) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, y)) {
        SDL_Log("SDL_MouseButtonEvent.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, y), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, padding_end)) {
        SDL_Log("SDL_MouseButtonEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseButtonEvent_pack1, padding_end), SDL_OFFSETOF(SDL_MouseButtonEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_MouseWheelEvent */

    if (sizeof(SDL_MouseWheelEvent_pack8) != sizeof(SDL_MouseWheelEvent_pack1)) {
        SDL_Log("SDL_MouseWheelEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MouseWheelEvent_pack1), (int)sizeof(SDL_MouseWheelEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, type) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, type)) {
        SDL_Log("SDL_MouseWheelEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, type), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, reserved) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, reserved)) {
        SDL_Log("SDL_MouseWheelEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, reserved), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, timestamp)) {
        SDL_Log("SDL_MouseWheelEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, timestamp), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, windowID) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, windowID)) {
        SDL_Log("SDL_MouseWheelEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, windowID), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, which) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, which)) {
        SDL_Log("SDL_MouseWheelEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, which), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, x) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, x)) {
        SDL_Log("SDL_MouseWheelEvent.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, x), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, y) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, y)) {
        SDL_Log("SDL_MouseWheelEvent.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, y), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, direction) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, direction)) {
        SDL_Log("SDL_MouseWheelEvent.direction has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, direction), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, mouse_x) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, mouse_x)) {
        SDL_Log("SDL_MouseWheelEvent.mouse_x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, mouse_x), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, mouse_x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, mouse_y) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, mouse_y)) {
        SDL_Log("SDL_MouseWheelEvent.mouse_y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, mouse_y), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, mouse_y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, padding_end)) {
        SDL_Log("SDL_MouseWheelEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, padding_end), SDL_OFFSETOF(SDL_MouseWheelEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_MouseWheelEvent_pack4) != sizeof(SDL_MouseWheelEvent_pack1)) {
        SDL_Log("SDL_MouseWheelEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MouseWheelEvent_pack1), (int)sizeof(SDL_MouseWheelEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, type) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, type)) {
        SDL_Log("SDL_MouseWheelEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, type), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, reserved) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, reserved)) {
        SDL_Log("SDL_MouseWheelEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, reserved), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, timestamp)) {
        SDL_Log("SDL_MouseWheelEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, timestamp), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, windowID) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, windowID)) {
        SDL_Log("SDL_MouseWheelEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, windowID), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, which) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, which)) {
        SDL_Log("SDL_MouseWheelEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, which), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, x) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, x)) {
        SDL_Log("SDL_MouseWheelEvent.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, x), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, y) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, y)) {
        SDL_Log("SDL_MouseWheelEvent.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, y), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, direction) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, direction)) {
        SDL_Log("SDL_MouseWheelEvent.direction has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, direction), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, mouse_x) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, mouse_x)) {
        SDL_Log("SDL_MouseWheelEvent.mouse_x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, mouse_x), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, mouse_x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, mouse_y) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, mouse_y)) {
        SDL_Log("SDL_MouseWheelEvent.mouse_y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, mouse_y), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, mouse_y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, padding_end)) {
        SDL_Log("SDL_MouseWheelEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MouseWheelEvent_pack1, padding_end), SDL_OFFSETOF(SDL_MouseWheelEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_JoyAxisEvent */

    if (sizeof(SDL_JoyAxisEvent_pack8) != sizeof(SDL_JoyAxisEvent_pack1)) {
        SDL_Log("SDL_JoyAxisEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyAxisEvent_pack1), (int)sizeof(SDL_JoyAxisEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, type) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, type)) {
        SDL_Log("SDL_JoyAxisEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, type), SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, reserved) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyAxisEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyAxisEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, which) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, which)) {
        SDL_Log("SDL_JoyAxisEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, which), SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, axis) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, axis)) {
        SDL_Log("SDL_JoyAxisEvent.axis has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, axis), SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, axis));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, padding8[0]) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding8[0])) {
        SDL_Log("SDL_JoyAxisEvent.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, value) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, value)) {
        SDL_Log("SDL_JoyAxisEvent.value has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, value), SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, value));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, padding16) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding16)) {
        SDL_Log("SDL_JoyAxisEvent.padding16 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding16), SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding_end)) {
        SDL_Log("SDL_JoyAxisEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding_end), SDL_OFFSETOF(SDL_JoyAxisEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_JoyAxisEvent_pack4) != sizeof(SDL_JoyAxisEvent_pack1)) {
        SDL_Log("SDL_JoyAxisEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyAxisEvent_pack1), (int)sizeof(SDL_JoyAxisEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, type) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, type)) {
        SDL_Log("SDL_JoyAxisEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, type), SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, reserved) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyAxisEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyAxisEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, which) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, which)) {
        SDL_Log("SDL_JoyAxisEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, which), SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, axis) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, axis)) {
        SDL_Log("SDL_JoyAxisEvent.axis has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, axis), SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, axis));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, padding8[0]) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding8[0])) {
        SDL_Log("SDL_JoyAxisEvent.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, value) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, value)) {
        SDL_Log("SDL_JoyAxisEvent.value has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, value), SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, value));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, padding16) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding16)) {
        SDL_Log("SDL_JoyAxisEvent.padding16 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding16), SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding_end)) {
        SDL_Log("SDL_JoyAxisEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyAxisEvent_pack1, padding_end), SDL_OFFSETOF(SDL_JoyAxisEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_JoyBallEvent */

    if (sizeof(SDL_JoyBallEvent_pack8) != sizeof(SDL_JoyBallEvent_pack1)) {
        SDL_Log("SDL_JoyBallEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyBallEvent_pack1), (int)sizeof(SDL_JoyBallEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack8, type) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, type)) {
        SDL_Log("SDL_JoyBallEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, type), SDL_OFFSETOF(SDL_JoyBallEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack8, reserved) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyBallEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyBallEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyBallEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyBallEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack8, which) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, which)) {
        SDL_Log("SDL_JoyBallEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, which), SDL_OFFSETOF(SDL_JoyBallEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack8, ball) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, ball)) {
        SDL_Log("SDL_JoyBallEvent.ball has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, ball), SDL_OFFSETOF(SDL_JoyBallEvent_pack8, ball));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack8, padding8[0]) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, padding8[0])) {
        SDL_Log("SDL_JoyBallEvent.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_JoyBallEvent_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack8, xrel) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, xrel)) {
        SDL_Log("SDL_JoyBallEvent.xrel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, xrel), SDL_OFFSETOF(SDL_JoyBallEvent_pack8, xrel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack8, yrel) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, yrel)) {
        SDL_Log("SDL_JoyBallEvent.yrel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, yrel), SDL_OFFSETOF(SDL_JoyBallEvent_pack8, yrel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, padding_end)) {
        SDL_Log("SDL_JoyBallEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, padding_end), SDL_OFFSETOF(SDL_JoyBallEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_JoyBallEvent_pack4) != sizeof(SDL_JoyBallEvent_pack1)) {
        SDL_Log("SDL_JoyBallEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyBallEvent_pack1), (int)sizeof(SDL_JoyBallEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack4, type) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, type)) {
        SDL_Log("SDL_JoyBallEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, type), SDL_OFFSETOF(SDL_JoyBallEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack4, reserved) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyBallEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyBallEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyBallEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyBallEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack4, which) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, which)) {
        SDL_Log("SDL_JoyBallEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, which), SDL_OFFSETOF(SDL_JoyBallEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack4, ball) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, ball)) {
        SDL_Log("SDL_JoyBallEvent.ball has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, ball), SDL_OFFSETOF(SDL_JoyBallEvent_pack4, ball));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack4, padding8[0]) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, padding8[0])) {
        SDL_Log("SDL_JoyBallEvent.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_JoyBallEvent_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack4, xrel) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, xrel)) {
        SDL_Log("SDL_JoyBallEvent.xrel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, xrel), SDL_OFFSETOF(SDL_JoyBallEvent_pack4, xrel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack4, yrel) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, yrel)) {
        SDL_Log("SDL_JoyBallEvent.yrel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, yrel), SDL_OFFSETOF(SDL_JoyBallEvent_pack4, yrel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBallEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_JoyBallEvent_pack1, padding_end)) {
        SDL_Log("SDL_JoyBallEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBallEvent_pack1, padding_end), SDL_OFFSETOF(SDL_JoyBallEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_JoyHatEvent */

    if (sizeof(SDL_JoyHatEvent_pack8) != sizeof(SDL_JoyHatEvent_pack1)) {
        SDL_Log("SDL_JoyHatEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyHatEvent_pack1), (int)sizeof(SDL_JoyHatEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack8, type) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, type)) {
        SDL_Log("SDL_JoyHatEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, type), SDL_OFFSETOF(SDL_JoyHatEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack8, reserved) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyHatEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyHatEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyHatEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyHatEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack8, which) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, which)) {
        SDL_Log("SDL_JoyHatEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, which), SDL_OFFSETOF(SDL_JoyHatEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack8, hat) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, hat)) {
        SDL_Log("SDL_JoyHatEvent.hat has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, hat), SDL_OFFSETOF(SDL_JoyHatEvent_pack8, hat));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack8, value) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, value)) {
        SDL_Log("SDL_JoyHatEvent.value has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, value), SDL_OFFSETOF(SDL_JoyHatEvent_pack8, value));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack8, padding8[0]) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, padding8[0])) {
        SDL_Log("SDL_JoyHatEvent.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_JoyHatEvent_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_JoyHatEvent_pack4) != sizeof(SDL_JoyHatEvent_pack1)) {
        SDL_Log("SDL_JoyHatEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyHatEvent_pack1), (int)sizeof(SDL_JoyHatEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack4, type) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, type)) {
        SDL_Log("SDL_JoyHatEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, type), SDL_OFFSETOF(SDL_JoyHatEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack4, reserved) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyHatEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyHatEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyHatEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyHatEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack4, which) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, which)) {
        SDL_Log("SDL_JoyHatEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, which), SDL_OFFSETOF(SDL_JoyHatEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack4, hat) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, hat)) {
        SDL_Log("SDL_JoyHatEvent.hat has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, hat), SDL_OFFSETOF(SDL_JoyHatEvent_pack4, hat));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack4, value) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, value)) {
        SDL_Log("SDL_JoyHatEvent.value has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, value), SDL_OFFSETOF(SDL_JoyHatEvent_pack4, value));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyHatEvent_pack4, padding8[0]) != SDL_OFFSETOF(SDL_JoyHatEvent_pack1, padding8[0])) {
        SDL_Log("SDL_JoyHatEvent.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyHatEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_JoyHatEvent_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    /* SDL_JoyButtonEvent */

    if (sizeof(SDL_JoyButtonEvent_pack8) != sizeof(SDL_JoyButtonEvent_pack1)) {
        SDL_Log("SDL_JoyButtonEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyButtonEvent_pack1), (int)sizeof(SDL_JoyButtonEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, type) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, type)) {
        SDL_Log("SDL_JoyButtonEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, type), SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, reserved) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyButtonEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyButtonEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, which) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, which)) {
        SDL_Log("SDL_JoyButtonEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, which), SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, button) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, button)) {
        SDL_Log("SDL_JoyButtonEvent.button has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, button), SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, state) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, state)) {
        SDL_Log("SDL_JoyButtonEvent.state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, state), SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, padding8[0]) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, padding8[0])) {
        SDL_Log("SDL_JoyButtonEvent.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_JoyButtonEvent_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_JoyButtonEvent_pack4) != sizeof(SDL_JoyButtonEvent_pack1)) {
        SDL_Log("SDL_JoyButtonEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyButtonEvent_pack1), (int)sizeof(SDL_JoyButtonEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, type) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, type)) {
        SDL_Log("SDL_JoyButtonEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, type), SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, reserved) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyButtonEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyButtonEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, which) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, which)) {
        SDL_Log("SDL_JoyButtonEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, which), SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, button) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, button)) {
        SDL_Log("SDL_JoyButtonEvent.button has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, button), SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, state) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, state)) {
        SDL_Log("SDL_JoyButtonEvent.state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, state), SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, padding8[0]) != SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, padding8[0])) {
        SDL_Log("SDL_JoyButtonEvent.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyButtonEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_JoyButtonEvent_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    /* SDL_JoyDeviceEvent */

    if (sizeof(SDL_JoyDeviceEvent_pack8) != sizeof(SDL_JoyDeviceEvent_pack1)) {
        SDL_Log("SDL_JoyDeviceEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyDeviceEvent_pack1), (int)sizeof(SDL_JoyDeviceEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyDeviceEvent_pack8, type) != SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, type)) {
        SDL_Log("SDL_JoyDeviceEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_JoyDeviceEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyDeviceEvent_pack8, reserved) != SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyDeviceEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyDeviceEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyDeviceEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyDeviceEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyDeviceEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyDeviceEvent_pack8, which) != SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, which)) {
        SDL_Log("SDL_JoyDeviceEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_JoyDeviceEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyDeviceEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, padding_end)) {
        SDL_Log("SDL_JoyDeviceEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, padding_end), SDL_OFFSETOF(SDL_JoyDeviceEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_JoyDeviceEvent_pack4) != sizeof(SDL_JoyDeviceEvent_pack1)) {
        SDL_Log("SDL_JoyDeviceEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyDeviceEvent_pack1), (int)sizeof(SDL_JoyDeviceEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyDeviceEvent_pack4, type) != SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, type)) {
        SDL_Log("SDL_JoyDeviceEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_JoyDeviceEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyDeviceEvent_pack4, reserved) != SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyDeviceEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyDeviceEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyDeviceEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyDeviceEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyDeviceEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyDeviceEvent_pack4, which) != SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, which)) {
        SDL_Log("SDL_JoyDeviceEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_JoyDeviceEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyDeviceEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, padding_end)) {
        SDL_Log("SDL_JoyDeviceEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyDeviceEvent_pack1, padding_end), SDL_OFFSETOF(SDL_JoyDeviceEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_JoyBatteryEvent */

    if (sizeof(SDL_JoyBatteryEvent_pack8) != sizeof(SDL_JoyBatteryEvent_pack1)) {
        SDL_Log("SDL_JoyBatteryEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyBatteryEvent_pack1), (int)sizeof(SDL_JoyBatteryEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, type) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, type)) {
        SDL_Log("SDL_JoyBatteryEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, type), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, reserved) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyBatteryEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyBatteryEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, which) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, which)) {
        SDL_Log("SDL_JoyBatteryEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, which), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, state) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, state)) {
        SDL_Log("SDL_JoyBatteryEvent.state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, state), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, percent) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, percent)) {
        SDL_Log("SDL_JoyBatteryEvent.percent has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, percent), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, percent));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, padding_end)) {
        SDL_Log("SDL_JoyBatteryEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, padding_end), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_JoyBatteryEvent_pack4) != sizeof(SDL_JoyBatteryEvent_pack1)) {
        SDL_Log("SDL_JoyBatteryEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_JoyBatteryEvent_pack1), (int)sizeof(SDL_JoyBatteryEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, type) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, type)) {
        SDL_Log("SDL_JoyBatteryEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, type), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, reserved) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, reserved)) {
        SDL_Log("SDL_JoyBatteryEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, reserved), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, timestamp)) {
        SDL_Log("SDL_JoyBatteryEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, timestamp), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, which) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, which)) {
        SDL_Log("SDL_JoyBatteryEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, which), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, state) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, state)) {
        SDL_Log("SDL_JoyBatteryEvent.state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, state), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, percent) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, percent)) {
        SDL_Log("SDL_JoyBatteryEvent.percent has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, percent), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, percent));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, padding_end)) {
        SDL_Log("SDL_JoyBatteryEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_JoyBatteryEvent_pack1, padding_end), SDL_OFFSETOF(SDL_JoyBatteryEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GamepadAxisEvent */

    if (sizeof(SDL_GamepadAxisEvent_pack8) != sizeof(SDL_GamepadAxisEvent_pack1)) {
        SDL_Log("SDL_GamepadAxisEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadAxisEvent_pack1), (int)sizeof(SDL_GamepadAxisEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, type) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, type)) {
        SDL_Log("SDL_GamepadAxisEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, type), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, reserved) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, reserved)) {
        SDL_Log("SDL_GamepadAxisEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, reserved), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, timestamp)) {
        SDL_Log("SDL_GamepadAxisEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, timestamp), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, which) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, which)) {
        SDL_Log("SDL_GamepadAxisEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, which), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, axis) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, axis)) {
        SDL_Log("SDL_GamepadAxisEvent.axis has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, axis), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, axis));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding8[0])) {
        SDL_Log("SDL_GamepadAxisEvent.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, value) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, value)) {
        SDL_Log("SDL_GamepadAxisEvent.value has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, value), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, value));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, padding16) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding16)) {
        SDL_Log("SDL_GamepadAxisEvent.padding16 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding16), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding_end)) {
        SDL_Log("SDL_GamepadAxisEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding_end), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GamepadAxisEvent_pack4) != sizeof(SDL_GamepadAxisEvent_pack1)) {
        SDL_Log("SDL_GamepadAxisEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadAxisEvent_pack1), (int)sizeof(SDL_GamepadAxisEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, type) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, type)) {
        SDL_Log("SDL_GamepadAxisEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, type), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, reserved) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, reserved)) {
        SDL_Log("SDL_GamepadAxisEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, reserved), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, timestamp)) {
        SDL_Log("SDL_GamepadAxisEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, timestamp), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, which) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, which)) {
        SDL_Log("SDL_GamepadAxisEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, which), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, axis) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, axis)) {
        SDL_Log("SDL_GamepadAxisEvent.axis has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, axis), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, axis));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding8[0])) {
        SDL_Log("SDL_GamepadAxisEvent.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, value) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, value)) {
        SDL_Log("SDL_GamepadAxisEvent.value has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, value), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, value));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, padding16) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding16)) {
        SDL_Log("SDL_GamepadAxisEvent.padding16 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding16), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding_end)) {
        SDL_Log("SDL_GamepadAxisEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadAxisEvent_pack1, padding_end), SDL_OFFSETOF(SDL_GamepadAxisEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GamepadButtonEvent */

    if (sizeof(SDL_GamepadButtonEvent_pack8) != sizeof(SDL_GamepadButtonEvent_pack1)) {
        SDL_Log("SDL_GamepadButtonEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadButtonEvent_pack1), (int)sizeof(SDL_GamepadButtonEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, type) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, type)) {
        SDL_Log("SDL_GamepadButtonEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, type), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, reserved) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, reserved)) {
        SDL_Log("SDL_GamepadButtonEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, reserved), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, timestamp)) {
        SDL_Log("SDL_GamepadButtonEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, timestamp), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, which) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, which)) {
        SDL_Log("SDL_GamepadButtonEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, which), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, button) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, button)) {
        SDL_Log("SDL_GamepadButtonEvent.button has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, button), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, state) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, state)) {
        SDL_Log("SDL_GamepadButtonEvent.state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, state), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, padding8[0])) {
        SDL_Log("SDL_GamepadButtonEvent.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GamepadButtonEvent_pack4) != sizeof(SDL_GamepadButtonEvent_pack1)) {
        SDL_Log("SDL_GamepadButtonEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadButtonEvent_pack1), (int)sizeof(SDL_GamepadButtonEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, type) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, type)) {
        SDL_Log("SDL_GamepadButtonEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, type), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, reserved) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, reserved)) {
        SDL_Log("SDL_GamepadButtonEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, reserved), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, timestamp)) {
        SDL_Log("SDL_GamepadButtonEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, timestamp), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, which) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, which)) {
        SDL_Log("SDL_GamepadButtonEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, which), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, button) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, button)) {
        SDL_Log("SDL_GamepadButtonEvent.button has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, button), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, state) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, state)) {
        SDL_Log("SDL_GamepadButtonEvent.state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, state), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, padding8[0])) {
        SDL_Log("SDL_GamepadButtonEvent.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadButtonEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_GamepadButtonEvent_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    /* SDL_GamepadDeviceEvent */

    if (sizeof(SDL_GamepadDeviceEvent_pack8) != sizeof(SDL_GamepadDeviceEvent_pack1)) {
        SDL_Log("SDL_GamepadDeviceEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadDeviceEvent_pack1), (int)sizeof(SDL_GamepadDeviceEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack8, type) != SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, type)) {
        SDL_Log("SDL_GamepadDeviceEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack8, reserved) != SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_GamepadDeviceEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_GamepadDeviceEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack8, which) != SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, which)) {
        SDL_Log("SDL_GamepadDeviceEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, padding_end)) {
        SDL_Log("SDL_GamepadDeviceEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, padding_end), SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GamepadDeviceEvent_pack4) != sizeof(SDL_GamepadDeviceEvent_pack1)) {
        SDL_Log("SDL_GamepadDeviceEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadDeviceEvent_pack1), (int)sizeof(SDL_GamepadDeviceEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack4, type) != SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, type)) {
        SDL_Log("SDL_GamepadDeviceEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack4, reserved) != SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_GamepadDeviceEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_GamepadDeviceEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack4, which) != SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, which)) {
        SDL_Log("SDL_GamepadDeviceEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, padding_end)) {
        SDL_Log("SDL_GamepadDeviceEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack1, padding_end), SDL_OFFSETOF(SDL_GamepadDeviceEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GamepadTouchpadEvent */

    if (sizeof(SDL_GamepadTouchpadEvent_pack8) != sizeof(SDL_GamepadTouchpadEvent_pack1)) {
        SDL_Log("SDL_GamepadTouchpadEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadTouchpadEvent_pack1), (int)sizeof(SDL_GamepadTouchpadEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, type) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, type)) {
        SDL_Log("SDL_GamepadTouchpadEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, type), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, reserved) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, reserved)) {
        SDL_Log("SDL_GamepadTouchpadEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, reserved), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, timestamp)) {
        SDL_Log("SDL_GamepadTouchpadEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, timestamp), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, which) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, which)) {
        SDL_Log("SDL_GamepadTouchpadEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, which), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, touchpad) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, touchpad)) {
        SDL_Log("SDL_GamepadTouchpadEvent.touchpad has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, touchpad), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, touchpad));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, finger) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, finger)) {
        SDL_Log("SDL_GamepadTouchpadEvent.finger has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, finger), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, finger));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, x) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, x)) {
        SDL_Log("SDL_GamepadTouchpadEvent.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, x), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, y) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, y)) {
        SDL_Log("SDL_GamepadTouchpadEvent.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, y), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, pressure) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, pressure)) {
        SDL_Log("SDL_GamepadTouchpadEvent.pressure has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, pressure), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack8, pressure));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GamepadTouchpadEvent_pack4) != sizeof(SDL_GamepadTouchpadEvent_pack1)) {
        SDL_Log("SDL_GamepadTouchpadEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadTouchpadEvent_pack1), (int)sizeof(SDL_GamepadTouchpadEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, type) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, type)) {
        SDL_Log("SDL_GamepadTouchpadEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, type), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, reserved) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, reserved)) {
        SDL_Log("SDL_GamepadTouchpadEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, reserved), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, timestamp)) {
        SDL_Log("SDL_GamepadTouchpadEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, timestamp), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, which) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, which)) {
        SDL_Log("SDL_GamepadTouchpadEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, which), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, touchpad) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, touchpad)) {
        SDL_Log("SDL_GamepadTouchpadEvent.touchpad has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, touchpad), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, touchpad));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, finger) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, finger)) {
        SDL_Log("SDL_GamepadTouchpadEvent.finger has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, finger), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, finger));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, x) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, x)) {
        SDL_Log("SDL_GamepadTouchpadEvent.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, x), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, y) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, y)) {
        SDL_Log("SDL_GamepadTouchpadEvent.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, y), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, pressure) != SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, pressure)) {
        SDL_Log("SDL_GamepadTouchpadEvent.pressure has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack1, pressure), SDL_OFFSETOF(SDL_GamepadTouchpadEvent_pack4, pressure));
        result = SDL_FALSE;
    }

    /* SDL_GamepadSensorEvent */

    if (sizeof(SDL_GamepadSensorEvent_pack8) != sizeof(SDL_GamepadSensorEvent_pack1)) {
        SDL_Log("SDL_GamepadSensorEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadSensorEvent_pack1), (int)sizeof(SDL_GamepadSensorEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, type) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, type)) {
        SDL_Log("SDL_GamepadSensorEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, type), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, reserved) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, reserved)) {
        SDL_Log("SDL_GamepadSensorEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, reserved), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, timestamp)) {
        SDL_Log("SDL_GamepadSensorEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, timestamp), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, which) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, which)) {
        SDL_Log("SDL_GamepadSensorEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, which), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, sensor) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, sensor)) {
        SDL_Log("SDL_GamepadSensorEvent.sensor has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, sensor), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, sensor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, data[0]) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, data[0])) {
        SDL_Log("SDL_GamepadSensorEvent.data[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, data[0]), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, data[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, padding32) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, padding32)) {
        SDL_Log("SDL_GamepadSensorEvent.padding32 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, padding32), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, sensor_timestamp) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, sensor_timestamp)) {
        SDL_Log("SDL_GamepadSensorEvent.sensor_timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, sensor_timestamp), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack8, sensor_timestamp));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GamepadSensorEvent_pack4) != sizeof(SDL_GamepadSensorEvent_pack1)) {
        SDL_Log("SDL_GamepadSensorEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadSensorEvent_pack1), (int)sizeof(SDL_GamepadSensorEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, type) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, type)) {
        SDL_Log("SDL_GamepadSensorEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, type), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, reserved) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, reserved)) {
        SDL_Log("SDL_GamepadSensorEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, reserved), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, timestamp)) {
        SDL_Log("SDL_GamepadSensorEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, timestamp), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, which) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, which)) {
        SDL_Log("SDL_GamepadSensorEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, which), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, sensor) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, sensor)) {
        SDL_Log("SDL_GamepadSensorEvent.sensor has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, sensor), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, sensor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, data[0]) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, data[0])) {
        SDL_Log("SDL_GamepadSensorEvent.data[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, data[0]), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, data[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, padding32) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, padding32)) {
        SDL_Log("SDL_GamepadSensorEvent.padding32 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, padding32), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, sensor_timestamp) != SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, sensor_timestamp)) {
        SDL_Log("SDL_GamepadSensorEvent.sensor_timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GamepadSensorEvent_pack1, sensor_timestamp), SDL_OFFSETOF(SDL_GamepadSensorEvent_pack4, sensor_timestamp));
        result = SDL_FALSE;
    }

    /* SDL_AudioDeviceEvent */

    if (sizeof(SDL_AudioDeviceEvent_pack8) != sizeof(SDL_AudioDeviceEvent_pack1)) {
        SDL_Log("SDL_AudioDeviceEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_AudioDeviceEvent_pack1), (int)sizeof(SDL_AudioDeviceEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, type) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, type)) {
        SDL_Log("SDL_AudioDeviceEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, reserved) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_AudioDeviceEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_AudioDeviceEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, which) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, which)) {
        SDL_Log("SDL_AudioDeviceEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, recording) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, recording)) {
        SDL_Log("SDL_AudioDeviceEvent.recording has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, recording), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, recording));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, padding8[0]) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, padding8[0])) {
        SDL_Log("SDL_AudioDeviceEvent.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_AudioDeviceEvent_pack4) != sizeof(SDL_AudioDeviceEvent_pack1)) {
        SDL_Log("SDL_AudioDeviceEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_AudioDeviceEvent_pack1), (int)sizeof(SDL_AudioDeviceEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, type) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, type)) {
        SDL_Log("SDL_AudioDeviceEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, reserved) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_AudioDeviceEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_AudioDeviceEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, which) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, which)) {
        SDL_Log("SDL_AudioDeviceEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, recording) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, recording)) {
        SDL_Log("SDL_AudioDeviceEvent.recording has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, recording), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, recording));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, padding8[0]) != SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, padding8[0])) {
        SDL_Log("SDL_AudioDeviceEvent.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioDeviceEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_AudioDeviceEvent_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    /* SDL_CameraDeviceEvent */

    if (sizeof(SDL_CameraDeviceEvent_pack8) != sizeof(SDL_CameraDeviceEvent_pack1)) {
        SDL_Log("SDL_CameraDeviceEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_CameraDeviceEvent_pack1), (int)sizeof(SDL_CameraDeviceEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraDeviceEvent_pack8, type) != SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, type)) {
        SDL_Log("SDL_CameraDeviceEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_CameraDeviceEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraDeviceEvent_pack8, reserved) != SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_CameraDeviceEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_CameraDeviceEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraDeviceEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_CameraDeviceEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_CameraDeviceEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraDeviceEvent_pack8, which) != SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, which)) {
        SDL_Log("SDL_CameraDeviceEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_CameraDeviceEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraDeviceEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, padding_end)) {
        SDL_Log("SDL_CameraDeviceEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, padding_end), SDL_OFFSETOF(SDL_CameraDeviceEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_CameraDeviceEvent_pack4) != sizeof(SDL_CameraDeviceEvent_pack1)) {
        SDL_Log("SDL_CameraDeviceEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_CameraDeviceEvent_pack1), (int)sizeof(SDL_CameraDeviceEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraDeviceEvent_pack4, type) != SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, type)) {
        SDL_Log("SDL_CameraDeviceEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, type), SDL_OFFSETOF(SDL_CameraDeviceEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraDeviceEvent_pack4, reserved) != SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, reserved)) {
        SDL_Log("SDL_CameraDeviceEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, reserved), SDL_OFFSETOF(SDL_CameraDeviceEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraDeviceEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, timestamp)) {
        SDL_Log("SDL_CameraDeviceEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, timestamp), SDL_OFFSETOF(SDL_CameraDeviceEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraDeviceEvent_pack4, which) != SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, which)) {
        SDL_Log("SDL_CameraDeviceEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, which), SDL_OFFSETOF(SDL_CameraDeviceEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraDeviceEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, padding_end)) {
        SDL_Log("SDL_CameraDeviceEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraDeviceEvent_pack1, padding_end), SDL_OFFSETOF(SDL_CameraDeviceEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_TouchFingerEvent */

    if (sizeof(SDL_TouchFingerEvent_pack8) != sizeof(SDL_TouchFingerEvent_pack1)) {
        SDL_Log("SDL_TouchFingerEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_TouchFingerEvent_pack1), (int)sizeof(SDL_TouchFingerEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, type) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, type)) {
        SDL_Log("SDL_TouchFingerEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, type), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, reserved) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, reserved)) {
        SDL_Log("SDL_TouchFingerEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, reserved), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, timestamp)) {
        SDL_Log("SDL_TouchFingerEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, timestamp), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, touchID) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, touchID)) {
        SDL_Log("SDL_TouchFingerEvent.touchID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, touchID), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, touchID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, fingerID) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, fingerID)) {
        SDL_Log("SDL_TouchFingerEvent.fingerID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, fingerID), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, fingerID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, x) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, x)) {
        SDL_Log("SDL_TouchFingerEvent.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, x), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, y) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, y)) {
        SDL_Log("SDL_TouchFingerEvent.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, y), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, dx) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, dx)) {
        SDL_Log("SDL_TouchFingerEvent.dx has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, dx), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, dx));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, dy) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, dy)) {
        SDL_Log("SDL_TouchFingerEvent.dy has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, dy), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, dy));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, pressure) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, pressure)) {
        SDL_Log("SDL_TouchFingerEvent.pressure has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, pressure), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, pressure));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, windowID) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, windowID)) {
        SDL_Log("SDL_TouchFingerEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, windowID), SDL_OFFSETOF(SDL_TouchFingerEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_TouchFingerEvent_pack4) != sizeof(SDL_TouchFingerEvent_pack1)) {
        SDL_Log("SDL_TouchFingerEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_TouchFingerEvent_pack1), (int)sizeof(SDL_TouchFingerEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, type) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, type)) {
        SDL_Log("SDL_TouchFingerEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, type), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, reserved) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, reserved)) {
        SDL_Log("SDL_TouchFingerEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, reserved), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, timestamp)) {
        SDL_Log("SDL_TouchFingerEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, timestamp), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, touchID) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, touchID)) {
        SDL_Log("SDL_TouchFingerEvent.touchID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, touchID), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, touchID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, fingerID) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, fingerID)) {
        SDL_Log("SDL_TouchFingerEvent.fingerID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, fingerID), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, fingerID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, x) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, x)) {
        SDL_Log("SDL_TouchFingerEvent.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, x), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, y) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, y)) {
        SDL_Log("SDL_TouchFingerEvent.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, y), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, dx) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, dx)) {
        SDL_Log("SDL_TouchFingerEvent.dx has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, dx), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, dx));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, dy) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, dy)) {
        SDL_Log("SDL_TouchFingerEvent.dy has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, dy), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, dy));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, pressure) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, pressure)) {
        SDL_Log("SDL_TouchFingerEvent.pressure has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, pressure), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, pressure));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, windowID) != SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, windowID)) {
        SDL_Log("SDL_TouchFingerEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_TouchFingerEvent_pack1, windowID), SDL_OFFSETOF(SDL_TouchFingerEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    /* SDL_PenProximityEvent */

    if (sizeof(SDL_PenProximityEvent_pack8) != sizeof(SDL_PenProximityEvent_pack1)) {
        SDL_Log("SDL_PenProximityEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PenProximityEvent_pack1), (int)sizeof(SDL_PenProximityEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenProximityEvent_pack8, type) != SDL_OFFSETOF(SDL_PenProximityEvent_pack1, type)) {
        SDL_Log("SDL_PenProximityEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenProximityEvent_pack1, type), SDL_OFFSETOF(SDL_PenProximityEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenProximityEvent_pack8, reserved) != SDL_OFFSETOF(SDL_PenProximityEvent_pack1, reserved)) {
        SDL_Log("SDL_PenProximityEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenProximityEvent_pack1, reserved), SDL_OFFSETOF(SDL_PenProximityEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenProximityEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_PenProximityEvent_pack1, timestamp)) {
        SDL_Log("SDL_PenProximityEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenProximityEvent_pack1, timestamp), SDL_OFFSETOF(SDL_PenProximityEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenProximityEvent_pack8, windowID) != SDL_OFFSETOF(SDL_PenProximityEvent_pack1, windowID)) {
        SDL_Log("SDL_PenProximityEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenProximityEvent_pack1, windowID), SDL_OFFSETOF(SDL_PenProximityEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenProximityEvent_pack8, which) != SDL_OFFSETOF(SDL_PenProximityEvent_pack1, which)) {
        SDL_Log("SDL_PenProximityEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenProximityEvent_pack1, which), SDL_OFFSETOF(SDL_PenProximityEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_PenProximityEvent_pack4) != sizeof(SDL_PenProximityEvent_pack1)) {
        SDL_Log("SDL_PenProximityEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PenProximityEvent_pack1), (int)sizeof(SDL_PenProximityEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenProximityEvent_pack4, type) != SDL_OFFSETOF(SDL_PenProximityEvent_pack1, type)) {
        SDL_Log("SDL_PenProximityEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenProximityEvent_pack1, type), SDL_OFFSETOF(SDL_PenProximityEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenProximityEvent_pack4, reserved) != SDL_OFFSETOF(SDL_PenProximityEvent_pack1, reserved)) {
        SDL_Log("SDL_PenProximityEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenProximityEvent_pack1, reserved), SDL_OFFSETOF(SDL_PenProximityEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenProximityEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_PenProximityEvent_pack1, timestamp)) {
        SDL_Log("SDL_PenProximityEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenProximityEvent_pack1, timestamp), SDL_OFFSETOF(SDL_PenProximityEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenProximityEvent_pack4, windowID) != SDL_OFFSETOF(SDL_PenProximityEvent_pack1, windowID)) {
        SDL_Log("SDL_PenProximityEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenProximityEvent_pack1, windowID), SDL_OFFSETOF(SDL_PenProximityEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenProximityEvent_pack4, which) != SDL_OFFSETOF(SDL_PenProximityEvent_pack1, which)) {
        SDL_Log("SDL_PenProximityEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenProximityEvent_pack1, which), SDL_OFFSETOF(SDL_PenProximityEvent_pack4, which));
        result = SDL_FALSE;
    }

    /* SDL_PenMotionEvent */

    if (sizeof(SDL_PenMotionEvent_pack8) != sizeof(SDL_PenMotionEvent_pack1)) {
        SDL_Log("SDL_PenMotionEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PenMotionEvent_pack1), (int)sizeof(SDL_PenMotionEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack8, type) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, type)) {
        SDL_Log("SDL_PenMotionEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, type), SDL_OFFSETOF(SDL_PenMotionEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack8, reserved) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, reserved)) {
        SDL_Log("SDL_PenMotionEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, reserved), SDL_OFFSETOF(SDL_PenMotionEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, timestamp)) {
        SDL_Log("SDL_PenMotionEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, timestamp), SDL_OFFSETOF(SDL_PenMotionEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack8, windowID) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, windowID)) {
        SDL_Log("SDL_PenMotionEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, windowID), SDL_OFFSETOF(SDL_PenMotionEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack8, which) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, which)) {
        SDL_Log("SDL_PenMotionEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, which), SDL_OFFSETOF(SDL_PenMotionEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack8, pen_state) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, pen_state)) {
        SDL_Log("SDL_PenMotionEvent.pen_state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, pen_state), SDL_OFFSETOF(SDL_PenMotionEvent_pack8, pen_state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack8, x) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, x)) {
        SDL_Log("SDL_PenMotionEvent.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, x), SDL_OFFSETOF(SDL_PenMotionEvent_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack8, y) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, y)) {
        SDL_Log("SDL_PenMotionEvent.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, y), SDL_OFFSETOF(SDL_PenMotionEvent_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, padding_end)) {
        SDL_Log("SDL_PenMotionEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, padding_end), SDL_OFFSETOF(SDL_PenMotionEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_PenMotionEvent_pack4) != sizeof(SDL_PenMotionEvent_pack1)) {
        SDL_Log("SDL_PenMotionEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PenMotionEvent_pack1), (int)sizeof(SDL_PenMotionEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack4, type) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, type)) {
        SDL_Log("SDL_PenMotionEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, type), SDL_OFFSETOF(SDL_PenMotionEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack4, reserved) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, reserved)) {
        SDL_Log("SDL_PenMotionEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, reserved), SDL_OFFSETOF(SDL_PenMotionEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, timestamp)) {
        SDL_Log("SDL_PenMotionEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, timestamp), SDL_OFFSETOF(SDL_PenMotionEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack4, windowID) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, windowID)) {
        SDL_Log("SDL_PenMotionEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, windowID), SDL_OFFSETOF(SDL_PenMotionEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack4, which) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, which)) {
        SDL_Log("SDL_PenMotionEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, which), SDL_OFFSETOF(SDL_PenMotionEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack4, pen_state) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, pen_state)) {
        SDL_Log("SDL_PenMotionEvent.pen_state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, pen_state), SDL_OFFSETOF(SDL_PenMotionEvent_pack4, pen_state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack4, x) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, x)) {
        SDL_Log("SDL_PenMotionEvent.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, x), SDL_OFFSETOF(SDL_PenMotionEvent_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack4, y) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, y)) {
        SDL_Log("SDL_PenMotionEvent.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, y), SDL_OFFSETOF(SDL_PenMotionEvent_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenMotionEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_PenMotionEvent_pack1, padding_end)) {
        SDL_Log("SDL_PenMotionEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenMotionEvent_pack1, padding_end), SDL_OFFSETOF(SDL_PenMotionEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_PenTouchEvent */

    if (sizeof(SDL_PenTouchEvent_pack8) != sizeof(SDL_PenTouchEvent_pack1)) {
        SDL_Log("SDL_PenTouchEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PenTouchEvent_pack1), (int)sizeof(SDL_PenTouchEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, type) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, type)) {
        SDL_Log("SDL_PenTouchEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, type), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, reserved) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, reserved)) {
        SDL_Log("SDL_PenTouchEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, reserved), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, timestamp)) {
        SDL_Log("SDL_PenTouchEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, timestamp), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, windowID) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, windowID)) {
        SDL_Log("SDL_PenTouchEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, windowID), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, which) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, which)) {
        SDL_Log("SDL_PenTouchEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, which), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, pen_state) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, pen_state)) {
        SDL_Log("SDL_PenTouchEvent.pen_state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, pen_state), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, pen_state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, x) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, x)) {
        SDL_Log("SDL_PenTouchEvent.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, x), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, y) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, y)) {
        SDL_Log("SDL_PenTouchEvent.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, y), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, eraser) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, eraser)) {
        SDL_Log("SDL_PenTouchEvent.eraser has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, eraser), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, eraser));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, state) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, state)) {
        SDL_Log("SDL_PenTouchEvent.state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, state), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack8, padding8[0]) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, padding8[0])) {
        SDL_Log("SDL_PenTouchEvent.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_PenTouchEvent_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_PenTouchEvent_pack4) != sizeof(SDL_PenTouchEvent_pack1)) {
        SDL_Log("SDL_PenTouchEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PenTouchEvent_pack1), (int)sizeof(SDL_PenTouchEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, type) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, type)) {
        SDL_Log("SDL_PenTouchEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, type), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, reserved) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, reserved)) {
        SDL_Log("SDL_PenTouchEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, reserved), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, timestamp)) {
        SDL_Log("SDL_PenTouchEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, timestamp), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, windowID) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, windowID)) {
        SDL_Log("SDL_PenTouchEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, windowID), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, which) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, which)) {
        SDL_Log("SDL_PenTouchEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, which), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, pen_state) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, pen_state)) {
        SDL_Log("SDL_PenTouchEvent.pen_state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, pen_state), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, pen_state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, x) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, x)) {
        SDL_Log("SDL_PenTouchEvent.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, x), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, y) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, y)) {
        SDL_Log("SDL_PenTouchEvent.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, y), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, eraser) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, eraser)) {
        SDL_Log("SDL_PenTouchEvent.eraser has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, eraser), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, eraser));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, state) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, state)) {
        SDL_Log("SDL_PenTouchEvent.state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, state), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenTouchEvent_pack4, padding8[0]) != SDL_OFFSETOF(SDL_PenTouchEvent_pack1, padding8[0])) {
        SDL_Log("SDL_PenTouchEvent.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenTouchEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_PenTouchEvent_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    /* SDL_PenButtonEvent */

    if (sizeof(SDL_PenButtonEvent_pack8) != sizeof(SDL_PenButtonEvent_pack1)) {
        SDL_Log("SDL_PenButtonEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PenButtonEvent_pack1), (int)sizeof(SDL_PenButtonEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, type) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, type)) {
        SDL_Log("SDL_PenButtonEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, type), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, reserved) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, reserved)) {
        SDL_Log("SDL_PenButtonEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, reserved), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, timestamp)) {
        SDL_Log("SDL_PenButtonEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, timestamp), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, windowID) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, windowID)) {
        SDL_Log("SDL_PenButtonEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, windowID), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, which) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, which)) {
        SDL_Log("SDL_PenButtonEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, which), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, pen_state) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, pen_state)) {
        SDL_Log("SDL_PenButtonEvent.pen_state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, pen_state), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, pen_state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, x) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, x)) {
        SDL_Log("SDL_PenButtonEvent.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, x), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, y) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, y)) {
        SDL_Log("SDL_PenButtonEvent.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, y), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, button) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, button)) {
        SDL_Log("SDL_PenButtonEvent.button has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, button), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, state) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, state)) {
        SDL_Log("SDL_PenButtonEvent.state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, state), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack8, padding8[0]) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, padding8[0])) {
        SDL_Log("SDL_PenButtonEvent.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_PenButtonEvent_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_PenButtonEvent_pack4) != sizeof(SDL_PenButtonEvent_pack1)) {
        SDL_Log("SDL_PenButtonEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PenButtonEvent_pack1), (int)sizeof(SDL_PenButtonEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, type) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, type)) {
        SDL_Log("SDL_PenButtonEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, type), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, reserved) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, reserved)) {
        SDL_Log("SDL_PenButtonEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, reserved), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, timestamp)) {
        SDL_Log("SDL_PenButtonEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, timestamp), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, windowID) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, windowID)) {
        SDL_Log("SDL_PenButtonEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, windowID), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, which) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, which)) {
        SDL_Log("SDL_PenButtonEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, which), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, pen_state) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, pen_state)) {
        SDL_Log("SDL_PenButtonEvent.pen_state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, pen_state), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, pen_state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, x) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, x)) {
        SDL_Log("SDL_PenButtonEvent.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, x), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, y) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, y)) {
        SDL_Log("SDL_PenButtonEvent.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, y), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, button) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, button)) {
        SDL_Log("SDL_PenButtonEvent.button has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, button), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, state) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, state)) {
        SDL_Log("SDL_PenButtonEvent.state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, state), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenButtonEvent_pack4, padding8[0]) != SDL_OFFSETOF(SDL_PenButtonEvent_pack1, padding8[0])) {
        SDL_Log("SDL_PenButtonEvent.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenButtonEvent_pack1, padding8[0]), SDL_OFFSETOF(SDL_PenButtonEvent_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    /* SDL_PenAxisEvent */

    if (sizeof(SDL_PenAxisEvent_pack8) != sizeof(SDL_PenAxisEvent_pack1)) {
        SDL_Log("SDL_PenAxisEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PenAxisEvent_pack1), (int)sizeof(SDL_PenAxisEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, type) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, type)) {
        SDL_Log("SDL_PenAxisEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, type), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, reserved) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, reserved)) {
        SDL_Log("SDL_PenAxisEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, reserved), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, timestamp)) {
        SDL_Log("SDL_PenAxisEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, timestamp), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, windowID) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, windowID)) {
        SDL_Log("SDL_PenAxisEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, windowID), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, which) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, which)) {
        SDL_Log("SDL_PenAxisEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, which), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, pen_state) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, pen_state)) {
        SDL_Log("SDL_PenAxisEvent.pen_state has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, pen_state), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, pen_state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, x) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, x)) {
        SDL_Log("SDL_PenAxisEvent.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, x), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, y) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, y)) {
        SDL_Log("SDL_PenAxisEvent.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, y), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, axis) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, axis)) {
        SDL_Log("SDL_PenAxisEvent.axis has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, axis), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, axis));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, value) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, value)) {
        SDL_Log("SDL_PenAxisEvent.value has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, value), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, value));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack8, padding_end) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, padding_end)) {
        SDL_Log("SDL_PenAxisEvent.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, padding_end), SDL_OFFSETOF(SDL_PenAxisEvent_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_PenAxisEvent_pack4) != sizeof(SDL_PenAxisEvent_pack1)) {
        SDL_Log("SDL_PenAxisEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PenAxisEvent_pack1), (int)sizeof(SDL_PenAxisEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, type) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, type)) {
        SDL_Log("SDL_PenAxisEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, type), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, reserved) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, reserved)) {
        SDL_Log("SDL_PenAxisEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, reserved), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, timestamp)) {
        SDL_Log("SDL_PenAxisEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, timestamp), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, windowID) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, windowID)) {
        SDL_Log("SDL_PenAxisEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, windowID), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, which) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, which)) {
        SDL_Log("SDL_PenAxisEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, which), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, pen_state) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, pen_state)) {
        SDL_Log("SDL_PenAxisEvent.pen_state has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, pen_state), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, pen_state));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, x) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, x)) {
        SDL_Log("SDL_PenAxisEvent.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, x), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, y) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, y)) {
        SDL_Log("SDL_PenAxisEvent.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, y), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, axis) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, axis)) {
        SDL_Log("SDL_PenAxisEvent.axis has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, axis), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, axis));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, value) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, value)) {
        SDL_Log("SDL_PenAxisEvent.value has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, value), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, value));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PenAxisEvent_pack4, padding_end) != SDL_OFFSETOF(SDL_PenAxisEvent_pack1, padding_end)) {
        SDL_Log("SDL_PenAxisEvent.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PenAxisEvent_pack1, padding_end), SDL_OFFSETOF(SDL_PenAxisEvent_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_DropEvent */

    if (sizeof(SDL_DropEvent_pack8) != sizeof(SDL_DropEvent_pack1)) {
        SDL_Log("SDL_DropEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_DropEvent_pack1), (int)sizeof(SDL_DropEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack8, type) != SDL_OFFSETOF(SDL_DropEvent_pack1, type)) {
        SDL_Log("SDL_DropEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, type), SDL_OFFSETOF(SDL_DropEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack8, reserved) != SDL_OFFSETOF(SDL_DropEvent_pack1, reserved)) {
        SDL_Log("SDL_DropEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, reserved), SDL_OFFSETOF(SDL_DropEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_DropEvent_pack1, timestamp)) {
        SDL_Log("SDL_DropEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, timestamp), SDL_OFFSETOF(SDL_DropEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack8, windowID) != SDL_OFFSETOF(SDL_DropEvent_pack1, windowID)) {
        SDL_Log("SDL_DropEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, windowID), SDL_OFFSETOF(SDL_DropEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack8, x) != SDL_OFFSETOF(SDL_DropEvent_pack1, x)) {
        SDL_Log("SDL_DropEvent.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, x), SDL_OFFSETOF(SDL_DropEvent_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack8, y) != SDL_OFFSETOF(SDL_DropEvent_pack1, y)) {
        SDL_Log("SDL_DropEvent.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, y), SDL_OFFSETOF(SDL_DropEvent_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack8, padding32) != SDL_OFFSETOF(SDL_DropEvent_pack1, padding32)) {
        SDL_Log("SDL_DropEvent.padding32 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, padding32), SDL_OFFSETOF(SDL_DropEvent_pack8, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack8, source) != SDL_OFFSETOF(SDL_DropEvent_pack1, source)) {
        SDL_Log("SDL_DropEvent.source has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, source), SDL_OFFSETOF(SDL_DropEvent_pack8, source));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack8, data) != SDL_OFFSETOF(SDL_DropEvent_pack1, data)) {
        SDL_Log("SDL_DropEvent.data has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, data), SDL_OFFSETOF(SDL_DropEvent_pack8, data));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_DropEvent_pack4) != sizeof(SDL_DropEvent_pack1)) {
        SDL_Log("SDL_DropEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_DropEvent_pack1), (int)sizeof(SDL_DropEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack4, type) != SDL_OFFSETOF(SDL_DropEvent_pack1, type)) {
        SDL_Log("SDL_DropEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, type), SDL_OFFSETOF(SDL_DropEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack4, reserved) != SDL_OFFSETOF(SDL_DropEvent_pack1, reserved)) {
        SDL_Log("SDL_DropEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, reserved), SDL_OFFSETOF(SDL_DropEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_DropEvent_pack1, timestamp)) {
        SDL_Log("SDL_DropEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, timestamp), SDL_OFFSETOF(SDL_DropEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack4, windowID) != SDL_OFFSETOF(SDL_DropEvent_pack1, windowID)) {
        SDL_Log("SDL_DropEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, windowID), SDL_OFFSETOF(SDL_DropEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack4, x) != SDL_OFFSETOF(SDL_DropEvent_pack1, x)) {
        SDL_Log("SDL_DropEvent.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, x), SDL_OFFSETOF(SDL_DropEvent_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack4, y) != SDL_OFFSETOF(SDL_DropEvent_pack1, y)) {
        SDL_Log("SDL_DropEvent.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, y), SDL_OFFSETOF(SDL_DropEvent_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack4, padding32) != SDL_OFFSETOF(SDL_DropEvent_pack1, padding32)) {
        SDL_Log("SDL_DropEvent.padding32 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, padding32), SDL_OFFSETOF(SDL_DropEvent_pack4, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack4, source) != SDL_OFFSETOF(SDL_DropEvent_pack1, source)) {
        SDL_Log("SDL_DropEvent.source has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, source), SDL_OFFSETOF(SDL_DropEvent_pack4, source));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DropEvent_pack4, data) != SDL_OFFSETOF(SDL_DropEvent_pack1, data)) {
        SDL_Log("SDL_DropEvent.data has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DropEvent_pack1, data), SDL_OFFSETOF(SDL_DropEvent_pack4, data));
        result = SDL_FALSE;
    }

    /* SDL_ClipboardEvent */

    if (sizeof(SDL_ClipboardEvent_pack8) != sizeof(SDL_ClipboardEvent_pack1)) {
        SDL_Log("SDL_ClipboardEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_ClipboardEvent_pack1), (int)sizeof(SDL_ClipboardEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_ClipboardEvent_pack8, type) != SDL_OFFSETOF(SDL_ClipboardEvent_pack1, type)) {
        SDL_Log("SDL_ClipboardEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_ClipboardEvent_pack1, type), SDL_OFFSETOF(SDL_ClipboardEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_ClipboardEvent_pack8, reserved) != SDL_OFFSETOF(SDL_ClipboardEvent_pack1, reserved)) {
        SDL_Log("SDL_ClipboardEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_ClipboardEvent_pack1, reserved), SDL_OFFSETOF(SDL_ClipboardEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_ClipboardEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_ClipboardEvent_pack1, timestamp)) {
        SDL_Log("SDL_ClipboardEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_ClipboardEvent_pack1, timestamp), SDL_OFFSETOF(SDL_ClipboardEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_ClipboardEvent_pack4) != sizeof(SDL_ClipboardEvent_pack1)) {
        SDL_Log("SDL_ClipboardEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_ClipboardEvent_pack1), (int)sizeof(SDL_ClipboardEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_ClipboardEvent_pack4, type) != SDL_OFFSETOF(SDL_ClipboardEvent_pack1, type)) {
        SDL_Log("SDL_ClipboardEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_ClipboardEvent_pack1, type), SDL_OFFSETOF(SDL_ClipboardEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_ClipboardEvent_pack4, reserved) != SDL_OFFSETOF(SDL_ClipboardEvent_pack1, reserved)) {
        SDL_Log("SDL_ClipboardEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_ClipboardEvent_pack1, reserved), SDL_OFFSETOF(SDL_ClipboardEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_ClipboardEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_ClipboardEvent_pack1, timestamp)) {
        SDL_Log("SDL_ClipboardEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_ClipboardEvent_pack1, timestamp), SDL_OFFSETOF(SDL_ClipboardEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    /* SDL_SensorEvent */

    if (sizeof(SDL_SensorEvent_pack8) != sizeof(SDL_SensorEvent_pack1)) {
        SDL_Log("SDL_SensorEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_SensorEvent_pack1), (int)sizeof(SDL_SensorEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack8, type) != SDL_OFFSETOF(SDL_SensorEvent_pack1, type)) {
        SDL_Log("SDL_SensorEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, type), SDL_OFFSETOF(SDL_SensorEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack8, reserved) != SDL_OFFSETOF(SDL_SensorEvent_pack1, reserved)) {
        SDL_Log("SDL_SensorEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, reserved), SDL_OFFSETOF(SDL_SensorEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_SensorEvent_pack1, timestamp)) {
        SDL_Log("SDL_SensorEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, timestamp), SDL_OFFSETOF(SDL_SensorEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack8, which) != SDL_OFFSETOF(SDL_SensorEvent_pack1, which)) {
        SDL_Log("SDL_SensorEvent.which has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, which), SDL_OFFSETOF(SDL_SensorEvent_pack8, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack8, data[0]) != SDL_OFFSETOF(SDL_SensorEvent_pack1, data[0])) {
        SDL_Log("SDL_SensorEvent.data[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, data[0]), SDL_OFFSETOF(SDL_SensorEvent_pack8, data[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack8, padding32) != SDL_OFFSETOF(SDL_SensorEvent_pack1, padding32)) {
        SDL_Log("SDL_SensorEvent.padding32 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, padding32), SDL_OFFSETOF(SDL_SensorEvent_pack8, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack8, sensor_timestamp) != SDL_OFFSETOF(SDL_SensorEvent_pack1, sensor_timestamp)) {
        SDL_Log("SDL_SensorEvent.sensor_timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, sensor_timestamp), SDL_OFFSETOF(SDL_SensorEvent_pack8, sensor_timestamp));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_SensorEvent_pack4) != sizeof(SDL_SensorEvent_pack1)) {
        SDL_Log("SDL_SensorEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_SensorEvent_pack1), (int)sizeof(SDL_SensorEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack4, type) != SDL_OFFSETOF(SDL_SensorEvent_pack1, type)) {
        SDL_Log("SDL_SensorEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, type), SDL_OFFSETOF(SDL_SensorEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack4, reserved) != SDL_OFFSETOF(SDL_SensorEvent_pack1, reserved)) {
        SDL_Log("SDL_SensorEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, reserved), SDL_OFFSETOF(SDL_SensorEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_SensorEvent_pack1, timestamp)) {
        SDL_Log("SDL_SensorEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, timestamp), SDL_OFFSETOF(SDL_SensorEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack4, which) != SDL_OFFSETOF(SDL_SensorEvent_pack1, which)) {
        SDL_Log("SDL_SensorEvent.which has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, which), SDL_OFFSETOF(SDL_SensorEvent_pack4, which));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack4, data[0]) != SDL_OFFSETOF(SDL_SensorEvent_pack1, data[0])) {
        SDL_Log("SDL_SensorEvent.data[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, data[0]), SDL_OFFSETOF(SDL_SensorEvent_pack4, data[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack4, padding32) != SDL_OFFSETOF(SDL_SensorEvent_pack1, padding32)) {
        SDL_Log("SDL_SensorEvent.padding32 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, padding32), SDL_OFFSETOF(SDL_SensorEvent_pack4, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_SensorEvent_pack4, sensor_timestamp) != SDL_OFFSETOF(SDL_SensorEvent_pack1, sensor_timestamp)) {
        SDL_Log("SDL_SensorEvent.sensor_timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_SensorEvent_pack1, sensor_timestamp), SDL_OFFSETOF(SDL_SensorEvent_pack4, sensor_timestamp));
        result = SDL_FALSE;
    }

    /* SDL_QuitEvent */

    if (sizeof(SDL_QuitEvent_pack8) != sizeof(SDL_QuitEvent_pack1)) {
        SDL_Log("SDL_QuitEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_QuitEvent_pack1), (int)sizeof(SDL_QuitEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_QuitEvent_pack8, type) != SDL_OFFSETOF(SDL_QuitEvent_pack1, type)) {
        SDL_Log("SDL_QuitEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_QuitEvent_pack1, type), SDL_OFFSETOF(SDL_QuitEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_QuitEvent_pack8, reserved) != SDL_OFFSETOF(SDL_QuitEvent_pack1, reserved)) {
        SDL_Log("SDL_QuitEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_QuitEvent_pack1, reserved), SDL_OFFSETOF(SDL_QuitEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_QuitEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_QuitEvent_pack1, timestamp)) {
        SDL_Log("SDL_QuitEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_QuitEvent_pack1, timestamp), SDL_OFFSETOF(SDL_QuitEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_QuitEvent_pack4) != sizeof(SDL_QuitEvent_pack1)) {
        SDL_Log("SDL_QuitEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_QuitEvent_pack1), (int)sizeof(SDL_QuitEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_QuitEvent_pack4, type) != SDL_OFFSETOF(SDL_QuitEvent_pack1, type)) {
        SDL_Log("SDL_QuitEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_QuitEvent_pack1, type), SDL_OFFSETOF(SDL_QuitEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_QuitEvent_pack4, reserved) != SDL_OFFSETOF(SDL_QuitEvent_pack1, reserved)) {
        SDL_Log("SDL_QuitEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_QuitEvent_pack1, reserved), SDL_OFFSETOF(SDL_QuitEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_QuitEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_QuitEvent_pack1, timestamp)) {
        SDL_Log("SDL_QuitEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_QuitEvent_pack1, timestamp), SDL_OFFSETOF(SDL_QuitEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    /* SDL_UserEvent */

    if (sizeof(SDL_UserEvent_pack8) != sizeof(SDL_UserEvent_pack1)) {
        SDL_Log("SDL_UserEvent has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_UserEvent_pack1), (int)sizeof(SDL_UserEvent_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack8, type) != SDL_OFFSETOF(SDL_UserEvent_pack1, type)) {
        SDL_Log("SDL_UserEvent.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, type), SDL_OFFSETOF(SDL_UserEvent_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack8, reserved) != SDL_OFFSETOF(SDL_UserEvent_pack1, reserved)) {
        SDL_Log("SDL_UserEvent.reserved has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, reserved), SDL_OFFSETOF(SDL_UserEvent_pack8, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack8, timestamp) != SDL_OFFSETOF(SDL_UserEvent_pack1, timestamp)) {
        SDL_Log("SDL_UserEvent.timestamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, timestamp), SDL_OFFSETOF(SDL_UserEvent_pack8, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack8, windowID) != SDL_OFFSETOF(SDL_UserEvent_pack1, windowID)) {
        SDL_Log("SDL_UserEvent.windowID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, windowID), SDL_OFFSETOF(SDL_UserEvent_pack8, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack8, code) != SDL_OFFSETOF(SDL_UserEvent_pack1, code)) {
        SDL_Log("SDL_UserEvent.code has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, code), SDL_OFFSETOF(SDL_UserEvent_pack8, code));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack8, data1) != SDL_OFFSETOF(SDL_UserEvent_pack1, data1)) {
        SDL_Log("SDL_UserEvent.data1 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, data1), SDL_OFFSETOF(SDL_UserEvent_pack8, data1));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack8, data2) != SDL_OFFSETOF(SDL_UserEvent_pack1, data2)) {
        SDL_Log("SDL_UserEvent.data2 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, data2), SDL_OFFSETOF(SDL_UserEvent_pack8, data2));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_UserEvent_pack4) != sizeof(SDL_UserEvent_pack1)) {
        SDL_Log("SDL_UserEvent has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_UserEvent_pack1), (int)sizeof(SDL_UserEvent_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack4, type) != SDL_OFFSETOF(SDL_UserEvent_pack1, type)) {
        SDL_Log("SDL_UserEvent.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, type), SDL_OFFSETOF(SDL_UserEvent_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack4, reserved) != SDL_OFFSETOF(SDL_UserEvent_pack1, reserved)) {
        SDL_Log("SDL_UserEvent.reserved has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, reserved), SDL_OFFSETOF(SDL_UserEvent_pack4, reserved));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack4, timestamp) != SDL_OFFSETOF(SDL_UserEvent_pack1, timestamp)) {
        SDL_Log("SDL_UserEvent.timestamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, timestamp), SDL_OFFSETOF(SDL_UserEvent_pack4, timestamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack4, windowID) != SDL_OFFSETOF(SDL_UserEvent_pack1, windowID)) {
        SDL_Log("SDL_UserEvent.windowID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, windowID), SDL_OFFSETOF(SDL_UserEvent_pack4, windowID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack4, code) != SDL_OFFSETOF(SDL_UserEvent_pack1, code)) {
        SDL_Log("SDL_UserEvent.code has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, code), SDL_OFFSETOF(SDL_UserEvent_pack4, code));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack4, data1) != SDL_OFFSETOF(SDL_UserEvent_pack1, data1)) {
        SDL_Log("SDL_UserEvent.data1 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, data1), SDL_OFFSETOF(SDL_UserEvent_pack4, data1));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_UserEvent_pack4, data2) != SDL_OFFSETOF(SDL_UserEvent_pack1, data2)) {
        SDL_Log("SDL_UserEvent.data2 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_UserEvent_pack1, data2), SDL_OFFSETOF(SDL_UserEvent_pack4, data2));
        result = SDL_FALSE;
    }

    /* SDL_Event */

    if (sizeof(SDL_Event_pack8) != sizeof(SDL_Event_pack1)) {
        SDL_Log("SDL_Event has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Event_pack1), (int)sizeof(SDL_Event_pack8));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_Event_pack4) != sizeof(SDL_Event_pack1)) {
        SDL_Log("SDL_Event has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Event_pack1), (int)sizeof(SDL_Event_pack4));
        result = SDL_FALSE;
    }

    /* SDL_CameraSpec */

    if (sizeof(SDL_CameraSpec_pack8) != sizeof(SDL_CameraSpec_pack1)) {
        SDL_Log("SDL_CameraSpec has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_CameraSpec_pack1), (int)sizeof(SDL_CameraSpec_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack8, format) != SDL_OFFSETOF(SDL_CameraSpec_pack1, format)) {
        SDL_Log("SDL_CameraSpec.format has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, format), SDL_OFFSETOF(SDL_CameraSpec_pack8, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack8, colorspace) != SDL_OFFSETOF(SDL_CameraSpec_pack1, colorspace)) {
        SDL_Log("SDL_CameraSpec.colorspace has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, colorspace), SDL_OFFSETOF(SDL_CameraSpec_pack8, colorspace));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack8, width) != SDL_OFFSETOF(SDL_CameraSpec_pack1, width)) {
        SDL_Log("SDL_CameraSpec.width has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, width), SDL_OFFSETOF(SDL_CameraSpec_pack8, width));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack8, height) != SDL_OFFSETOF(SDL_CameraSpec_pack1, height)) {
        SDL_Log("SDL_CameraSpec.height has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, height), SDL_OFFSETOF(SDL_CameraSpec_pack8, height));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack8, framerate_numerator) != SDL_OFFSETOF(SDL_CameraSpec_pack1, framerate_numerator)) {
        SDL_Log("SDL_CameraSpec.framerate_numerator has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, framerate_numerator), SDL_OFFSETOF(SDL_CameraSpec_pack8, framerate_numerator));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack8, framerate_denominator) != SDL_OFFSETOF(SDL_CameraSpec_pack1, framerate_denominator)) {
        SDL_Log("SDL_CameraSpec.framerate_denominator has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, framerate_denominator), SDL_OFFSETOF(SDL_CameraSpec_pack8, framerate_denominator));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_CameraSpec_pack4) != sizeof(SDL_CameraSpec_pack1)) {
        SDL_Log("SDL_CameraSpec has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_CameraSpec_pack1), (int)sizeof(SDL_CameraSpec_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack4, format) != SDL_OFFSETOF(SDL_CameraSpec_pack1, format)) {
        SDL_Log("SDL_CameraSpec.format has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, format), SDL_OFFSETOF(SDL_CameraSpec_pack4, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack4, colorspace) != SDL_OFFSETOF(SDL_CameraSpec_pack1, colorspace)) {
        SDL_Log("SDL_CameraSpec.colorspace has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, colorspace), SDL_OFFSETOF(SDL_CameraSpec_pack4, colorspace));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack4, width) != SDL_OFFSETOF(SDL_CameraSpec_pack1, width)) {
        SDL_Log("SDL_CameraSpec.width has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, width), SDL_OFFSETOF(SDL_CameraSpec_pack4, width));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack4, height) != SDL_OFFSETOF(SDL_CameraSpec_pack1, height)) {
        SDL_Log("SDL_CameraSpec.height has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, height), SDL_OFFSETOF(SDL_CameraSpec_pack4, height));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack4, framerate_numerator) != SDL_OFFSETOF(SDL_CameraSpec_pack1, framerate_numerator)) {
        SDL_Log("SDL_CameraSpec.framerate_numerator has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, framerate_numerator), SDL_OFFSETOF(SDL_CameraSpec_pack4, framerate_numerator));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_CameraSpec_pack4, framerate_denominator) != SDL_OFFSETOF(SDL_CameraSpec_pack1, framerate_denominator)) {
        SDL_Log("SDL_CameraSpec.framerate_denominator has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_CameraSpec_pack1, framerate_denominator), SDL_OFFSETOF(SDL_CameraSpec_pack4, framerate_denominator));
        result = SDL_FALSE;
    }

    /* SDL_HapticDirection */

    if (sizeof(SDL_HapticDirection_pack8) != sizeof(SDL_HapticDirection_pack1)) {
        SDL_Log("SDL_HapticDirection has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticDirection_pack1), (int)sizeof(SDL_HapticDirection_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticDirection_pack8, type) != SDL_OFFSETOF(SDL_HapticDirection_pack1, type)) {
        SDL_Log("SDL_HapticDirection.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticDirection_pack1, type), SDL_OFFSETOF(SDL_HapticDirection_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticDirection_pack8, padding8[0]) != SDL_OFFSETOF(SDL_HapticDirection_pack1, padding8[0])) {
        SDL_Log("SDL_HapticDirection.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticDirection_pack1, padding8[0]), SDL_OFFSETOF(SDL_HapticDirection_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticDirection_pack8, dir[0]) != SDL_OFFSETOF(SDL_HapticDirection_pack1, dir[0])) {
        SDL_Log("SDL_HapticDirection.dir[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticDirection_pack1, dir[0]), SDL_OFFSETOF(SDL_HapticDirection_pack8, dir[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_HapticDirection_pack4) != sizeof(SDL_HapticDirection_pack1)) {
        SDL_Log("SDL_HapticDirection has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticDirection_pack1), (int)sizeof(SDL_HapticDirection_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticDirection_pack4, type) != SDL_OFFSETOF(SDL_HapticDirection_pack1, type)) {
        SDL_Log("SDL_HapticDirection.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticDirection_pack1, type), SDL_OFFSETOF(SDL_HapticDirection_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticDirection_pack4, padding8[0]) != SDL_OFFSETOF(SDL_HapticDirection_pack1, padding8[0])) {
        SDL_Log("SDL_HapticDirection.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticDirection_pack1, padding8[0]), SDL_OFFSETOF(SDL_HapticDirection_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticDirection_pack4, dir[0]) != SDL_OFFSETOF(SDL_HapticDirection_pack1, dir[0])) {
        SDL_Log("SDL_HapticDirection.dir[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticDirection_pack1, dir[0]), SDL_OFFSETOF(SDL_HapticDirection_pack4, dir[0]));
        result = SDL_FALSE;
    }

    /* SDL_HapticConstant */

    if (sizeof(SDL_HapticConstant_pack8) != sizeof(SDL_HapticConstant_pack1)) {
        SDL_Log("SDL_HapticConstant has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticConstant_pack1), (int)sizeof(SDL_HapticConstant_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, type) != SDL_OFFSETOF(SDL_HapticConstant_pack1, type)) {
        SDL_Log("SDL_HapticConstant.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, type), SDL_OFFSETOF(SDL_HapticConstant_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, padding16) != SDL_OFFSETOF(SDL_HapticConstant_pack1, padding16)) {
        SDL_Log("SDL_HapticConstant.padding16 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, padding16), SDL_OFFSETOF(SDL_HapticConstant_pack8, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, direction) != SDL_OFFSETOF(SDL_HapticConstant_pack1, direction)) {
        SDL_Log("SDL_HapticConstant.direction has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, direction), SDL_OFFSETOF(SDL_HapticConstant_pack8, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, length) != SDL_OFFSETOF(SDL_HapticConstant_pack1, length)) {
        SDL_Log("SDL_HapticConstant.length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, length), SDL_OFFSETOF(SDL_HapticConstant_pack8, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, delay) != SDL_OFFSETOF(SDL_HapticConstant_pack1, delay)) {
        SDL_Log("SDL_HapticConstant.delay has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, delay), SDL_OFFSETOF(SDL_HapticConstant_pack8, delay));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, button) != SDL_OFFSETOF(SDL_HapticConstant_pack1, button)) {
        SDL_Log("SDL_HapticConstant.button has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, button), SDL_OFFSETOF(SDL_HapticConstant_pack8, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, interval) != SDL_OFFSETOF(SDL_HapticConstant_pack1, interval)) {
        SDL_Log("SDL_HapticConstant.interval has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, interval), SDL_OFFSETOF(SDL_HapticConstant_pack8, interval));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, level) != SDL_OFFSETOF(SDL_HapticConstant_pack1, level)) {
        SDL_Log("SDL_HapticConstant.level has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, level), SDL_OFFSETOF(SDL_HapticConstant_pack8, level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, attack_length) != SDL_OFFSETOF(SDL_HapticConstant_pack1, attack_length)) {
        SDL_Log("SDL_HapticConstant.attack_length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, attack_length), SDL_OFFSETOF(SDL_HapticConstant_pack8, attack_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, attack_level) != SDL_OFFSETOF(SDL_HapticConstant_pack1, attack_level)) {
        SDL_Log("SDL_HapticConstant.attack_level has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, attack_level), SDL_OFFSETOF(SDL_HapticConstant_pack8, attack_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, fade_length) != SDL_OFFSETOF(SDL_HapticConstant_pack1, fade_length)) {
        SDL_Log("SDL_HapticConstant.fade_length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, fade_length), SDL_OFFSETOF(SDL_HapticConstant_pack8, fade_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack8, fade_level) != SDL_OFFSETOF(SDL_HapticConstant_pack1, fade_level)) {
        SDL_Log("SDL_HapticConstant.fade_level has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, fade_level), SDL_OFFSETOF(SDL_HapticConstant_pack8, fade_level));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_HapticConstant_pack4) != sizeof(SDL_HapticConstant_pack1)) {
        SDL_Log("SDL_HapticConstant has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticConstant_pack1), (int)sizeof(SDL_HapticConstant_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, type) != SDL_OFFSETOF(SDL_HapticConstant_pack1, type)) {
        SDL_Log("SDL_HapticConstant.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, type), SDL_OFFSETOF(SDL_HapticConstant_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, padding16) != SDL_OFFSETOF(SDL_HapticConstant_pack1, padding16)) {
        SDL_Log("SDL_HapticConstant.padding16 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, padding16), SDL_OFFSETOF(SDL_HapticConstant_pack4, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, direction) != SDL_OFFSETOF(SDL_HapticConstant_pack1, direction)) {
        SDL_Log("SDL_HapticConstant.direction has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, direction), SDL_OFFSETOF(SDL_HapticConstant_pack4, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, length) != SDL_OFFSETOF(SDL_HapticConstant_pack1, length)) {
        SDL_Log("SDL_HapticConstant.length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, length), SDL_OFFSETOF(SDL_HapticConstant_pack4, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, delay) != SDL_OFFSETOF(SDL_HapticConstant_pack1, delay)) {
        SDL_Log("SDL_HapticConstant.delay has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, delay), SDL_OFFSETOF(SDL_HapticConstant_pack4, delay));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, button) != SDL_OFFSETOF(SDL_HapticConstant_pack1, button)) {
        SDL_Log("SDL_HapticConstant.button has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, button), SDL_OFFSETOF(SDL_HapticConstant_pack4, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, interval) != SDL_OFFSETOF(SDL_HapticConstant_pack1, interval)) {
        SDL_Log("SDL_HapticConstant.interval has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, interval), SDL_OFFSETOF(SDL_HapticConstant_pack4, interval));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, level) != SDL_OFFSETOF(SDL_HapticConstant_pack1, level)) {
        SDL_Log("SDL_HapticConstant.level has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, level), SDL_OFFSETOF(SDL_HapticConstant_pack4, level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, attack_length) != SDL_OFFSETOF(SDL_HapticConstant_pack1, attack_length)) {
        SDL_Log("SDL_HapticConstant.attack_length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, attack_length), SDL_OFFSETOF(SDL_HapticConstant_pack4, attack_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, attack_level) != SDL_OFFSETOF(SDL_HapticConstant_pack1, attack_level)) {
        SDL_Log("SDL_HapticConstant.attack_level has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, attack_level), SDL_OFFSETOF(SDL_HapticConstant_pack4, attack_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, fade_length) != SDL_OFFSETOF(SDL_HapticConstant_pack1, fade_length)) {
        SDL_Log("SDL_HapticConstant.fade_length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, fade_length), SDL_OFFSETOF(SDL_HapticConstant_pack4, fade_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticConstant_pack4, fade_level) != SDL_OFFSETOF(SDL_HapticConstant_pack1, fade_level)) {
        SDL_Log("SDL_HapticConstant.fade_level has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticConstant_pack1, fade_level), SDL_OFFSETOF(SDL_HapticConstant_pack4, fade_level));
        result = SDL_FALSE;
    }

    /* SDL_HapticPeriodic */

    if (sizeof(SDL_HapticPeriodic_pack8) != sizeof(SDL_HapticPeriodic_pack1)) {
        SDL_Log("SDL_HapticPeriodic has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticPeriodic_pack1), (int)sizeof(SDL_HapticPeriodic_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, type) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, type)) {
        SDL_Log("SDL_HapticPeriodic.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, type), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, direction) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, direction)) {
        SDL_Log("SDL_HapticPeriodic.direction has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, direction), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, length) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, length)) {
        SDL_Log("SDL_HapticPeriodic.length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, length), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, delay) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, delay)) {
        SDL_Log("SDL_HapticPeriodic.delay has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, delay), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, delay));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, button) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, button)) {
        SDL_Log("SDL_HapticPeriodic.button has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, button), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, interval) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, interval)) {
        SDL_Log("SDL_HapticPeriodic.interval has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, interval), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, interval));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, period) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, period)) {
        SDL_Log("SDL_HapticPeriodic.period has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, period), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, period));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, magnitude) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, magnitude)) {
        SDL_Log("SDL_HapticPeriodic.magnitude has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, magnitude), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, magnitude));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, offset) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, offset)) {
        SDL_Log("SDL_HapticPeriodic.offset has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, offset), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, phase) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, phase)) {
        SDL_Log("SDL_HapticPeriodic.phase has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, phase), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, phase));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, attack_length) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, attack_length)) {
        SDL_Log("SDL_HapticPeriodic.attack_length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, attack_length), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, attack_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, attack_level) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, attack_level)) {
        SDL_Log("SDL_HapticPeriodic.attack_level has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, attack_level), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, attack_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, fade_length) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, fade_length)) {
        SDL_Log("SDL_HapticPeriodic.fade_length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, fade_length), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, fade_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, fade_level) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, fade_level)) {
        SDL_Log("SDL_HapticPeriodic.fade_level has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, fade_level), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, fade_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack8, padding_end) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, padding_end)) {
        SDL_Log("SDL_HapticPeriodic.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, padding_end), SDL_OFFSETOF(SDL_HapticPeriodic_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_HapticPeriodic_pack4) != sizeof(SDL_HapticPeriodic_pack1)) {
        SDL_Log("SDL_HapticPeriodic has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticPeriodic_pack1), (int)sizeof(SDL_HapticPeriodic_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, type) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, type)) {
        SDL_Log("SDL_HapticPeriodic.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, type), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, direction) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, direction)) {
        SDL_Log("SDL_HapticPeriodic.direction has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, direction), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, length) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, length)) {
        SDL_Log("SDL_HapticPeriodic.length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, length), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, delay) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, delay)) {
        SDL_Log("SDL_HapticPeriodic.delay has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, delay), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, delay));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, button) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, button)) {
        SDL_Log("SDL_HapticPeriodic.button has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, button), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, interval) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, interval)) {
        SDL_Log("SDL_HapticPeriodic.interval has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, interval), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, interval));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, period) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, period)) {
        SDL_Log("SDL_HapticPeriodic.period has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, period), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, period));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, magnitude) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, magnitude)) {
        SDL_Log("SDL_HapticPeriodic.magnitude has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, magnitude), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, magnitude));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, offset) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, offset)) {
        SDL_Log("SDL_HapticPeriodic.offset has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, offset), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, phase) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, phase)) {
        SDL_Log("SDL_HapticPeriodic.phase has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, phase), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, phase));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, attack_length) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, attack_length)) {
        SDL_Log("SDL_HapticPeriodic.attack_length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, attack_length), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, attack_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, attack_level) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, attack_level)) {
        SDL_Log("SDL_HapticPeriodic.attack_level has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, attack_level), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, attack_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, fade_length) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, fade_length)) {
        SDL_Log("SDL_HapticPeriodic.fade_length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, fade_length), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, fade_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, fade_level) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, fade_level)) {
        SDL_Log("SDL_HapticPeriodic.fade_level has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, fade_level), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, fade_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticPeriodic_pack4, padding_end) != SDL_OFFSETOF(SDL_HapticPeriodic_pack1, padding_end)) {
        SDL_Log("SDL_HapticPeriodic.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticPeriodic_pack1, padding_end), SDL_OFFSETOF(SDL_HapticPeriodic_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_HapticCondition */

    if (sizeof(SDL_HapticCondition_pack8) != sizeof(SDL_HapticCondition_pack1)) {
        SDL_Log("SDL_HapticCondition has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticCondition_pack1), (int)sizeof(SDL_HapticCondition_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, type) != SDL_OFFSETOF(SDL_HapticCondition_pack1, type)) {
        SDL_Log("SDL_HapticCondition.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, type), SDL_OFFSETOF(SDL_HapticCondition_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, direction) != SDL_OFFSETOF(SDL_HapticCondition_pack1, direction)) {
        SDL_Log("SDL_HapticCondition.direction has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, direction), SDL_OFFSETOF(SDL_HapticCondition_pack8, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, length) != SDL_OFFSETOF(SDL_HapticCondition_pack1, length)) {
        SDL_Log("SDL_HapticCondition.length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, length), SDL_OFFSETOF(SDL_HapticCondition_pack8, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, delay) != SDL_OFFSETOF(SDL_HapticCondition_pack1, delay)) {
        SDL_Log("SDL_HapticCondition.delay has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, delay), SDL_OFFSETOF(SDL_HapticCondition_pack8, delay));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, button) != SDL_OFFSETOF(SDL_HapticCondition_pack1, button)) {
        SDL_Log("SDL_HapticCondition.button has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, button), SDL_OFFSETOF(SDL_HapticCondition_pack8, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, interval) != SDL_OFFSETOF(SDL_HapticCondition_pack1, interval)) {
        SDL_Log("SDL_HapticCondition.interval has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, interval), SDL_OFFSETOF(SDL_HapticCondition_pack8, interval));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, right_sat[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, right_sat[0])) {
        SDL_Log("SDL_HapticCondition.right_sat[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, right_sat[0]), SDL_OFFSETOF(SDL_HapticCondition_pack8, right_sat[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, left_sat[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, left_sat[0])) {
        SDL_Log("SDL_HapticCondition.left_sat[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, left_sat[0]), SDL_OFFSETOF(SDL_HapticCondition_pack8, left_sat[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, right_coeff[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, right_coeff[0])) {
        SDL_Log("SDL_HapticCondition.right_coeff[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, right_coeff[0]), SDL_OFFSETOF(SDL_HapticCondition_pack8, right_coeff[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, left_coeff[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, left_coeff[0])) {
        SDL_Log("SDL_HapticCondition.left_coeff[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, left_coeff[0]), SDL_OFFSETOF(SDL_HapticCondition_pack8, left_coeff[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, deadband[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, deadband[0])) {
        SDL_Log("SDL_HapticCondition.deadband[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, deadband[0]), SDL_OFFSETOF(SDL_HapticCondition_pack8, deadband[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, center[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, center[0])) {
        SDL_Log("SDL_HapticCondition.center[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, center[0]), SDL_OFFSETOF(SDL_HapticCondition_pack8, center[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack8, padding_end) != SDL_OFFSETOF(SDL_HapticCondition_pack1, padding_end)) {
        SDL_Log("SDL_HapticCondition.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, padding_end), SDL_OFFSETOF(SDL_HapticCondition_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_HapticCondition_pack4) != sizeof(SDL_HapticCondition_pack1)) {
        SDL_Log("SDL_HapticCondition has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticCondition_pack1), (int)sizeof(SDL_HapticCondition_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, type) != SDL_OFFSETOF(SDL_HapticCondition_pack1, type)) {
        SDL_Log("SDL_HapticCondition.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, type), SDL_OFFSETOF(SDL_HapticCondition_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, direction) != SDL_OFFSETOF(SDL_HapticCondition_pack1, direction)) {
        SDL_Log("SDL_HapticCondition.direction has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, direction), SDL_OFFSETOF(SDL_HapticCondition_pack4, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, length) != SDL_OFFSETOF(SDL_HapticCondition_pack1, length)) {
        SDL_Log("SDL_HapticCondition.length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, length), SDL_OFFSETOF(SDL_HapticCondition_pack4, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, delay) != SDL_OFFSETOF(SDL_HapticCondition_pack1, delay)) {
        SDL_Log("SDL_HapticCondition.delay has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, delay), SDL_OFFSETOF(SDL_HapticCondition_pack4, delay));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, button) != SDL_OFFSETOF(SDL_HapticCondition_pack1, button)) {
        SDL_Log("SDL_HapticCondition.button has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, button), SDL_OFFSETOF(SDL_HapticCondition_pack4, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, interval) != SDL_OFFSETOF(SDL_HapticCondition_pack1, interval)) {
        SDL_Log("SDL_HapticCondition.interval has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, interval), SDL_OFFSETOF(SDL_HapticCondition_pack4, interval));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, right_sat[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, right_sat[0])) {
        SDL_Log("SDL_HapticCondition.right_sat[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, right_sat[0]), SDL_OFFSETOF(SDL_HapticCondition_pack4, right_sat[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, left_sat[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, left_sat[0])) {
        SDL_Log("SDL_HapticCondition.left_sat[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, left_sat[0]), SDL_OFFSETOF(SDL_HapticCondition_pack4, left_sat[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, right_coeff[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, right_coeff[0])) {
        SDL_Log("SDL_HapticCondition.right_coeff[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, right_coeff[0]), SDL_OFFSETOF(SDL_HapticCondition_pack4, right_coeff[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, left_coeff[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, left_coeff[0])) {
        SDL_Log("SDL_HapticCondition.left_coeff[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, left_coeff[0]), SDL_OFFSETOF(SDL_HapticCondition_pack4, left_coeff[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, deadband[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, deadband[0])) {
        SDL_Log("SDL_HapticCondition.deadband[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, deadband[0]), SDL_OFFSETOF(SDL_HapticCondition_pack4, deadband[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, center[0]) != SDL_OFFSETOF(SDL_HapticCondition_pack1, center[0])) {
        SDL_Log("SDL_HapticCondition.center[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, center[0]), SDL_OFFSETOF(SDL_HapticCondition_pack4, center[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCondition_pack4, padding_end) != SDL_OFFSETOF(SDL_HapticCondition_pack1, padding_end)) {
        SDL_Log("SDL_HapticCondition.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCondition_pack1, padding_end), SDL_OFFSETOF(SDL_HapticCondition_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_HapticRamp */

    if (sizeof(SDL_HapticRamp_pack8) != sizeof(SDL_HapticRamp_pack1)) {
        SDL_Log("SDL_HapticRamp has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticRamp_pack1), (int)sizeof(SDL_HapticRamp_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, type) != SDL_OFFSETOF(SDL_HapticRamp_pack1, type)) {
        SDL_Log("SDL_HapticRamp.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, type), SDL_OFFSETOF(SDL_HapticRamp_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, direction) != SDL_OFFSETOF(SDL_HapticRamp_pack1, direction)) {
        SDL_Log("SDL_HapticRamp.direction has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, direction), SDL_OFFSETOF(SDL_HapticRamp_pack8, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, length) != SDL_OFFSETOF(SDL_HapticRamp_pack1, length)) {
        SDL_Log("SDL_HapticRamp.length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, length), SDL_OFFSETOF(SDL_HapticRamp_pack8, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, delay) != SDL_OFFSETOF(SDL_HapticRamp_pack1, delay)) {
        SDL_Log("SDL_HapticRamp.delay has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, delay), SDL_OFFSETOF(SDL_HapticRamp_pack8, delay));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, button) != SDL_OFFSETOF(SDL_HapticRamp_pack1, button)) {
        SDL_Log("SDL_HapticRamp.button has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, button), SDL_OFFSETOF(SDL_HapticRamp_pack8, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, interval) != SDL_OFFSETOF(SDL_HapticRamp_pack1, interval)) {
        SDL_Log("SDL_HapticRamp.interval has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, interval), SDL_OFFSETOF(SDL_HapticRamp_pack8, interval));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, start) != SDL_OFFSETOF(SDL_HapticRamp_pack1, start)) {
        SDL_Log("SDL_HapticRamp.start has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, start), SDL_OFFSETOF(SDL_HapticRamp_pack8, start));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, end) != SDL_OFFSETOF(SDL_HapticRamp_pack1, end)) {
        SDL_Log("SDL_HapticRamp.end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, end), SDL_OFFSETOF(SDL_HapticRamp_pack8, end));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, attack_length) != SDL_OFFSETOF(SDL_HapticRamp_pack1, attack_length)) {
        SDL_Log("SDL_HapticRamp.attack_length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, attack_length), SDL_OFFSETOF(SDL_HapticRamp_pack8, attack_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, attack_level) != SDL_OFFSETOF(SDL_HapticRamp_pack1, attack_level)) {
        SDL_Log("SDL_HapticRamp.attack_level has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, attack_level), SDL_OFFSETOF(SDL_HapticRamp_pack8, attack_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, fade_length) != SDL_OFFSETOF(SDL_HapticRamp_pack1, fade_length)) {
        SDL_Log("SDL_HapticRamp.fade_length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, fade_length), SDL_OFFSETOF(SDL_HapticRamp_pack8, fade_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, fade_level) != SDL_OFFSETOF(SDL_HapticRamp_pack1, fade_level)) {
        SDL_Log("SDL_HapticRamp.fade_level has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, fade_level), SDL_OFFSETOF(SDL_HapticRamp_pack8, fade_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack8, padding_end) != SDL_OFFSETOF(SDL_HapticRamp_pack1, padding_end)) {
        SDL_Log("SDL_HapticRamp.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, padding_end), SDL_OFFSETOF(SDL_HapticRamp_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_HapticRamp_pack4) != sizeof(SDL_HapticRamp_pack1)) {
        SDL_Log("SDL_HapticRamp has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticRamp_pack1), (int)sizeof(SDL_HapticRamp_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, type) != SDL_OFFSETOF(SDL_HapticRamp_pack1, type)) {
        SDL_Log("SDL_HapticRamp.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, type), SDL_OFFSETOF(SDL_HapticRamp_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, direction) != SDL_OFFSETOF(SDL_HapticRamp_pack1, direction)) {
        SDL_Log("SDL_HapticRamp.direction has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, direction), SDL_OFFSETOF(SDL_HapticRamp_pack4, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, length) != SDL_OFFSETOF(SDL_HapticRamp_pack1, length)) {
        SDL_Log("SDL_HapticRamp.length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, length), SDL_OFFSETOF(SDL_HapticRamp_pack4, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, delay) != SDL_OFFSETOF(SDL_HapticRamp_pack1, delay)) {
        SDL_Log("SDL_HapticRamp.delay has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, delay), SDL_OFFSETOF(SDL_HapticRamp_pack4, delay));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, button) != SDL_OFFSETOF(SDL_HapticRamp_pack1, button)) {
        SDL_Log("SDL_HapticRamp.button has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, button), SDL_OFFSETOF(SDL_HapticRamp_pack4, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, interval) != SDL_OFFSETOF(SDL_HapticRamp_pack1, interval)) {
        SDL_Log("SDL_HapticRamp.interval has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, interval), SDL_OFFSETOF(SDL_HapticRamp_pack4, interval));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, start) != SDL_OFFSETOF(SDL_HapticRamp_pack1, start)) {
        SDL_Log("SDL_HapticRamp.start has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, start), SDL_OFFSETOF(SDL_HapticRamp_pack4, start));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, end) != SDL_OFFSETOF(SDL_HapticRamp_pack1, end)) {
        SDL_Log("SDL_HapticRamp.end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, end), SDL_OFFSETOF(SDL_HapticRamp_pack4, end));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, attack_length) != SDL_OFFSETOF(SDL_HapticRamp_pack1, attack_length)) {
        SDL_Log("SDL_HapticRamp.attack_length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, attack_length), SDL_OFFSETOF(SDL_HapticRamp_pack4, attack_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, attack_level) != SDL_OFFSETOF(SDL_HapticRamp_pack1, attack_level)) {
        SDL_Log("SDL_HapticRamp.attack_level has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, attack_level), SDL_OFFSETOF(SDL_HapticRamp_pack4, attack_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, fade_length) != SDL_OFFSETOF(SDL_HapticRamp_pack1, fade_length)) {
        SDL_Log("SDL_HapticRamp.fade_length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, fade_length), SDL_OFFSETOF(SDL_HapticRamp_pack4, fade_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, fade_level) != SDL_OFFSETOF(SDL_HapticRamp_pack1, fade_level)) {
        SDL_Log("SDL_HapticRamp.fade_level has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, fade_level), SDL_OFFSETOF(SDL_HapticRamp_pack4, fade_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticRamp_pack4, padding_end) != SDL_OFFSETOF(SDL_HapticRamp_pack1, padding_end)) {
        SDL_Log("SDL_HapticRamp.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticRamp_pack1, padding_end), SDL_OFFSETOF(SDL_HapticRamp_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_HapticLeftRight */

    if (sizeof(SDL_HapticLeftRight_pack8) != sizeof(SDL_HapticLeftRight_pack1)) {
        SDL_Log("SDL_HapticLeftRight has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticLeftRight_pack1), (int)sizeof(SDL_HapticLeftRight_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticLeftRight_pack8, type) != SDL_OFFSETOF(SDL_HapticLeftRight_pack1, type)) {
        SDL_Log("SDL_HapticLeftRight.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticLeftRight_pack1, type), SDL_OFFSETOF(SDL_HapticLeftRight_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticLeftRight_pack8, length) != SDL_OFFSETOF(SDL_HapticLeftRight_pack1, length)) {
        SDL_Log("SDL_HapticLeftRight.length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticLeftRight_pack1, length), SDL_OFFSETOF(SDL_HapticLeftRight_pack8, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticLeftRight_pack8, large_magnitude) != SDL_OFFSETOF(SDL_HapticLeftRight_pack1, large_magnitude)) {
        SDL_Log("SDL_HapticLeftRight.large_magnitude has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticLeftRight_pack1, large_magnitude), SDL_OFFSETOF(SDL_HapticLeftRight_pack8, large_magnitude));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticLeftRight_pack8, small_magnitude) != SDL_OFFSETOF(SDL_HapticLeftRight_pack1, small_magnitude)) {
        SDL_Log("SDL_HapticLeftRight.small_magnitude has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticLeftRight_pack1, small_magnitude), SDL_OFFSETOF(SDL_HapticLeftRight_pack8, small_magnitude));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_HapticLeftRight_pack4) != sizeof(SDL_HapticLeftRight_pack1)) {
        SDL_Log("SDL_HapticLeftRight has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticLeftRight_pack1), (int)sizeof(SDL_HapticLeftRight_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticLeftRight_pack4, type) != SDL_OFFSETOF(SDL_HapticLeftRight_pack1, type)) {
        SDL_Log("SDL_HapticLeftRight.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticLeftRight_pack1, type), SDL_OFFSETOF(SDL_HapticLeftRight_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticLeftRight_pack4, length) != SDL_OFFSETOF(SDL_HapticLeftRight_pack1, length)) {
        SDL_Log("SDL_HapticLeftRight.length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticLeftRight_pack1, length), SDL_OFFSETOF(SDL_HapticLeftRight_pack4, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticLeftRight_pack4, large_magnitude) != SDL_OFFSETOF(SDL_HapticLeftRight_pack1, large_magnitude)) {
        SDL_Log("SDL_HapticLeftRight.large_magnitude has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticLeftRight_pack1, large_magnitude), SDL_OFFSETOF(SDL_HapticLeftRight_pack4, large_magnitude));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticLeftRight_pack4, small_magnitude) != SDL_OFFSETOF(SDL_HapticLeftRight_pack1, small_magnitude)) {
        SDL_Log("SDL_HapticLeftRight.small_magnitude has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticLeftRight_pack1, small_magnitude), SDL_OFFSETOF(SDL_HapticLeftRight_pack4, small_magnitude));
        result = SDL_FALSE;
    }

    /* SDL_HapticCustom */

    if (sizeof(SDL_HapticCustom_pack8) != sizeof(SDL_HapticCustom_pack1)) {
        SDL_Log("SDL_HapticCustom has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticCustom_pack1), (int)sizeof(SDL_HapticCustom_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, type) != SDL_OFFSETOF(SDL_HapticCustom_pack1, type)) {
        SDL_Log("SDL_HapticCustom.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, type), SDL_OFFSETOF(SDL_HapticCustom_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, direction) != SDL_OFFSETOF(SDL_HapticCustom_pack1, direction)) {
        SDL_Log("SDL_HapticCustom.direction has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, direction), SDL_OFFSETOF(SDL_HapticCustom_pack8, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, length) != SDL_OFFSETOF(SDL_HapticCustom_pack1, length)) {
        SDL_Log("SDL_HapticCustom.length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, length), SDL_OFFSETOF(SDL_HapticCustom_pack8, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, delay) != SDL_OFFSETOF(SDL_HapticCustom_pack1, delay)) {
        SDL_Log("SDL_HapticCustom.delay has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, delay), SDL_OFFSETOF(SDL_HapticCustom_pack8, delay));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, button) != SDL_OFFSETOF(SDL_HapticCustom_pack1, button)) {
        SDL_Log("SDL_HapticCustom.button has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, button), SDL_OFFSETOF(SDL_HapticCustom_pack8, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, interval) != SDL_OFFSETOF(SDL_HapticCustom_pack1, interval)) {
        SDL_Log("SDL_HapticCustom.interval has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, interval), SDL_OFFSETOF(SDL_HapticCustom_pack8, interval));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, channels) != SDL_OFFSETOF(SDL_HapticCustom_pack1, channels)) {
        SDL_Log("SDL_HapticCustom.channels has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, channels), SDL_OFFSETOF(SDL_HapticCustom_pack8, channels));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, padding8[0]) != SDL_OFFSETOF(SDL_HapticCustom_pack1, padding8[0])) {
        SDL_Log("SDL_HapticCustom.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, padding8[0]), SDL_OFFSETOF(SDL_HapticCustom_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, period) != SDL_OFFSETOF(SDL_HapticCustom_pack1, period)) {
        SDL_Log("SDL_HapticCustom.period has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, period), SDL_OFFSETOF(SDL_HapticCustom_pack8, period));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, samples) != SDL_OFFSETOF(SDL_HapticCustom_pack1, samples)) {
        SDL_Log("SDL_HapticCustom.samples has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, samples), SDL_OFFSETOF(SDL_HapticCustom_pack8, samples));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, padding16) != SDL_OFFSETOF(SDL_HapticCustom_pack1, padding16)) {
        SDL_Log("SDL_HapticCustom.padding16 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, padding16), SDL_OFFSETOF(SDL_HapticCustom_pack8, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, data) != SDL_OFFSETOF(SDL_HapticCustom_pack1, data)) {
        SDL_Log("SDL_HapticCustom.data has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, data), SDL_OFFSETOF(SDL_HapticCustom_pack8, data));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, attack_length) != SDL_OFFSETOF(SDL_HapticCustom_pack1, attack_length)) {
        SDL_Log("SDL_HapticCustom.attack_length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, attack_length), SDL_OFFSETOF(SDL_HapticCustom_pack8, attack_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, attack_level) != SDL_OFFSETOF(SDL_HapticCustom_pack1, attack_level)) {
        SDL_Log("SDL_HapticCustom.attack_level has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, attack_level), SDL_OFFSETOF(SDL_HapticCustom_pack8, attack_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, fade_length) != SDL_OFFSETOF(SDL_HapticCustom_pack1, fade_length)) {
        SDL_Log("SDL_HapticCustom.fade_length has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, fade_length), SDL_OFFSETOF(SDL_HapticCustom_pack8, fade_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack8, fade_level) != SDL_OFFSETOF(SDL_HapticCustom_pack1, fade_level)) {
        SDL_Log("SDL_HapticCustom.fade_level has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, fade_level), SDL_OFFSETOF(SDL_HapticCustom_pack8, fade_level));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_HapticCustom_pack4) != sizeof(SDL_HapticCustom_pack1)) {
        SDL_Log("SDL_HapticCustom has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticCustom_pack1), (int)sizeof(SDL_HapticCustom_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, type) != SDL_OFFSETOF(SDL_HapticCustom_pack1, type)) {
        SDL_Log("SDL_HapticCustom.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, type), SDL_OFFSETOF(SDL_HapticCustom_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, direction) != SDL_OFFSETOF(SDL_HapticCustom_pack1, direction)) {
        SDL_Log("SDL_HapticCustom.direction has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, direction), SDL_OFFSETOF(SDL_HapticCustom_pack4, direction));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, length) != SDL_OFFSETOF(SDL_HapticCustom_pack1, length)) {
        SDL_Log("SDL_HapticCustom.length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, length), SDL_OFFSETOF(SDL_HapticCustom_pack4, length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, delay) != SDL_OFFSETOF(SDL_HapticCustom_pack1, delay)) {
        SDL_Log("SDL_HapticCustom.delay has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, delay), SDL_OFFSETOF(SDL_HapticCustom_pack4, delay));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, button) != SDL_OFFSETOF(SDL_HapticCustom_pack1, button)) {
        SDL_Log("SDL_HapticCustom.button has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, button), SDL_OFFSETOF(SDL_HapticCustom_pack4, button));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, interval) != SDL_OFFSETOF(SDL_HapticCustom_pack1, interval)) {
        SDL_Log("SDL_HapticCustom.interval has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, interval), SDL_OFFSETOF(SDL_HapticCustom_pack4, interval));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, channels) != SDL_OFFSETOF(SDL_HapticCustom_pack1, channels)) {
        SDL_Log("SDL_HapticCustom.channels has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, channels), SDL_OFFSETOF(SDL_HapticCustom_pack4, channels));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, padding8[0]) != SDL_OFFSETOF(SDL_HapticCustom_pack1, padding8[0])) {
        SDL_Log("SDL_HapticCustom.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, padding8[0]), SDL_OFFSETOF(SDL_HapticCustom_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, period) != SDL_OFFSETOF(SDL_HapticCustom_pack1, period)) {
        SDL_Log("SDL_HapticCustom.period has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, period), SDL_OFFSETOF(SDL_HapticCustom_pack4, period));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, samples) != SDL_OFFSETOF(SDL_HapticCustom_pack1, samples)) {
        SDL_Log("SDL_HapticCustom.samples has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, samples), SDL_OFFSETOF(SDL_HapticCustom_pack4, samples));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, padding16) != SDL_OFFSETOF(SDL_HapticCustom_pack1, padding16)) {
        SDL_Log("SDL_HapticCustom.padding16 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, padding16), SDL_OFFSETOF(SDL_HapticCustom_pack4, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, data) != SDL_OFFSETOF(SDL_HapticCustom_pack1, data)) {
        SDL_Log("SDL_HapticCustom.data has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, data), SDL_OFFSETOF(SDL_HapticCustom_pack4, data));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, attack_length) != SDL_OFFSETOF(SDL_HapticCustom_pack1, attack_length)) {
        SDL_Log("SDL_HapticCustom.attack_length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, attack_length), SDL_OFFSETOF(SDL_HapticCustom_pack4, attack_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, attack_level) != SDL_OFFSETOF(SDL_HapticCustom_pack1, attack_level)) {
        SDL_Log("SDL_HapticCustom.attack_level has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, attack_level), SDL_OFFSETOF(SDL_HapticCustom_pack4, attack_level));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, fade_length) != SDL_OFFSETOF(SDL_HapticCustom_pack1, fade_length)) {
        SDL_Log("SDL_HapticCustom.fade_length has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, fade_length), SDL_OFFSETOF(SDL_HapticCustom_pack4, fade_length));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_HapticCustom_pack4, fade_level) != SDL_OFFSETOF(SDL_HapticCustom_pack1, fade_level)) {
        SDL_Log("SDL_HapticCustom.fade_level has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_HapticCustom_pack1, fade_level), SDL_OFFSETOF(SDL_HapticCustom_pack4, fade_level));
        result = SDL_FALSE;
    }

    /* SDL_HapticEffect */

    if (sizeof(SDL_HapticEffect_pack8) != sizeof(SDL_HapticEffect_pack1)) {
        SDL_Log("SDL_HapticEffect has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticEffect_pack1), (int)sizeof(SDL_HapticEffect_pack8));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_HapticEffect_pack4) != sizeof(SDL_HapticEffect_pack1)) {
        SDL_Log("SDL_HapticEffect has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_HapticEffect_pack1), (int)sizeof(SDL_HapticEffect_pack4));
        result = SDL_FALSE;
    }

    /* SDL_StorageInterface */

    if (sizeof(SDL_StorageInterface_pack8) != sizeof(SDL_StorageInterface_pack1)) {
        SDL_Log("SDL_StorageInterface has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_StorageInterface_pack1), (int)sizeof(SDL_StorageInterface_pack8));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_StorageInterface_pack4) != sizeof(SDL_StorageInterface_pack1)) {
        SDL_Log("SDL_StorageInterface has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_StorageInterface_pack1), (int)sizeof(SDL_StorageInterface_pack4));
        result = SDL_FALSE;
    }

    /* SDL_DateTime */

    if (sizeof(SDL_DateTime_pack8) != sizeof(SDL_DateTime_pack1)) {
        SDL_Log("SDL_DateTime has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_DateTime_pack1), (int)sizeof(SDL_DateTime_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack8, year) != SDL_OFFSETOF(SDL_DateTime_pack1, year)) {
        SDL_Log("SDL_DateTime.year has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, year), SDL_OFFSETOF(SDL_DateTime_pack8, year));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack8, month) != SDL_OFFSETOF(SDL_DateTime_pack1, month)) {
        SDL_Log("SDL_DateTime.month has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, month), SDL_OFFSETOF(SDL_DateTime_pack8, month));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack8, day) != SDL_OFFSETOF(SDL_DateTime_pack1, day)) {
        SDL_Log("SDL_DateTime.day has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, day), SDL_OFFSETOF(SDL_DateTime_pack8, day));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack8, hour) != SDL_OFFSETOF(SDL_DateTime_pack1, hour)) {
        SDL_Log("SDL_DateTime.hour has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, hour), SDL_OFFSETOF(SDL_DateTime_pack8, hour));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack8, minute) != SDL_OFFSETOF(SDL_DateTime_pack1, minute)) {
        SDL_Log("SDL_DateTime.minute has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, minute), SDL_OFFSETOF(SDL_DateTime_pack8, minute));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack8, second) != SDL_OFFSETOF(SDL_DateTime_pack1, second)) {
        SDL_Log("SDL_DateTime.second has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, second), SDL_OFFSETOF(SDL_DateTime_pack8, second));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack8, nanosecond) != SDL_OFFSETOF(SDL_DateTime_pack1, nanosecond)) {
        SDL_Log("SDL_DateTime.nanosecond has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, nanosecond), SDL_OFFSETOF(SDL_DateTime_pack8, nanosecond));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack8, day_of_week) != SDL_OFFSETOF(SDL_DateTime_pack1, day_of_week)) {
        SDL_Log("SDL_DateTime.day_of_week has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, day_of_week), SDL_OFFSETOF(SDL_DateTime_pack8, day_of_week));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack8, utc_offset) != SDL_OFFSETOF(SDL_DateTime_pack1, utc_offset)) {
        SDL_Log("SDL_DateTime.utc_offset has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, utc_offset), SDL_OFFSETOF(SDL_DateTime_pack8, utc_offset));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_DateTime_pack4) != sizeof(SDL_DateTime_pack1)) {
        SDL_Log("SDL_DateTime has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_DateTime_pack1), (int)sizeof(SDL_DateTime_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack4, year) != SDL_OFFSETOF(SDL_DateTime_pack1, year)) {
        SDL_Log("SDL_DateTime.year has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, year), SDL_OFFSETOF(SDL_DateTime_pack4, year));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack4, month) != SDL_OFFSETOF(SDL_DateTime_pack1, month)) {
        SDL_Log("SDL_DateTime.month has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, month), SDL_OFFSETOF(SDL_DateTime_pack4, month));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack4, day) != SDL_OFFSETOF(SDL_DateTime_pack1, day)) {
        SDL_Log("SDL_DateTime.day has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, day), SDL_OFFSETOF(SDL_DateTime_pack4, day));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack4, hour) != SDL_OFFSETOF(SDL_DateTime_pack1, hour)) {
        SDL_Log("SDL_DateTime.hour has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, hour), SDL_OFFSETOF(SDL_DateTime_pack4, hour));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack4, minute) != SDL_OFFSETOF(SDL_DateTime_pack1, minute)) {
        SDL_Log("SDL_DateTime.minute has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, minute), SDL_OFFSETOF(SDL_DateTime_pack4, minute));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack4, second) != SDL_OFFSETOF(SDL_DateTime_pack1, second)) {
        SDL_Log("SDL_DateTime.second has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, second), SDL_OFFSETOF(SDL_DateTime_pack4, second));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack4, nanosecond) != SDL_OFFSETOF(SDL_DateTime_pack1, nanosecond)) {
        SDL_Log("SDL_DateTime.nanosecond has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, nanosecond), SDL_OFFSETOF(SDL_DateTime_pack4, nanosecond));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack4, day_of_week) != SDL_OFFSETOF(SDL_DateTime_pack1, day_of_week)) {
        SDL_Log("SDL_DateTime.day_of_week has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, day_of_week), SDL_OFFSETOF(SDL_DateTime_pack4, day_of_week));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DateTime_pack4, utc_offset) != SDL_OFFSETOF(SDL_DateTime_pack1, utc_offset)) {
        SDL_Log("SDL_DateTime.utc_offset has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DateTime_pack1, utc_offset), SDL_OFFSETOF(SDL_DateTime_pack4, utc_offset));
        result = SDL_FALSE;
    }

    /* SDL_Finger */

    if (sizeof(SDL_Finger_pack8) != sizeof(SDL_Finger_pack1)) {
        SDL_Log("SDL_Finger has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Finger_pack1), (int)sizeof(SDL_Finger_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Finger_pack8, id) != SDL_OFFSETOF(SDL_Finger_pack1, id)) {
        SDL_Log("SDL_Finger.id has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Finger_pack1, id), SDL_OFFSETOF(SDL_Finger_pack8, id));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Finger_pack8, x) != SDL_OFFSETOF(SDL_Finger_pack1, x)) {
        SDL_Log("SDL_Finger.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Finger_pack1, x), SDL_OFFSETOF(SDL_Finger_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Finger_pack8, y) != SDL_OFFSETOF(SDL_Finger_pack1, y)) {
        SDL_Log("SDL_Finger.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Finger_pack1, y), SDL_OFFSETOF(SDL_Finger_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Finger_pack8, pressure) != SDL_OFFSETOF(SDL_Finger_pack1, pressure)) {
        SDL_Log("SDL_Finger.pressure has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Finger_pack1, pressure), SDL_OFFSETOF(SDL_Finger_pack8, pressure));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Finger_pack8, padding_end) != SDL_OFFSETOF(SDL_Finger_pack1, padding_end)) {
        SDL_Log("SDL_Finger.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Finger_pack1, padding_end), SDL_OFFSETOF(SDL_Finger_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_Finger_pack4) != sizeof(SDL_Finger_pack1)) {
        SDL_Log("SDL_Finger has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Finger_pack1), (int)sizeof(SDL_Finger_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Finger_pack4, id) != SDL_OFFSETOF(SDL_Finger_pack1, id)) {
        SDL_Log("SDL_Finger.id has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Finger_pack1, id), SDL_OFFSETOF(SDL_Finger_pack4, id));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Finger_pack4, x) != SDL_OFFSETOF(SDL_Finger_pack1, x)) {
        SDL_Log("SDL_Finger.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Finger_pack1, x), SDL_OFFSETOF(SDL_Finger_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Finger_pack4, y) != SDL_OFFSETOF(SDL_Finger_pack1, y)) {
        SDL_Log("SDL_Finger.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Finger_pack1, y), SDL_OFFSETOF(SDL_Finger_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Finger_pack4, pressure) != SDL_OFFSETOF(SDL_Finger_pack1, pressure)) {
        SDL_Log("SDL_Finger.pressure has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Finger_pack1, pressure), SDL_OFFSETOF(SDL_Finger_pack4, pressure));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Finger_pack4, padding_end) != SDL_OFFSETOF(SDL_Finger_pack1, padding_end)) {
        SDL_Log("SDL_Finger.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Finger_pack1, padding_end), SDL_OFFSETOF(SDL_Finger_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GamepadBinding */

    if (sizeof(SDL_GamepadBinding_pack8) != sizeof(SDL_GamepadBinding_pack1)) {
        SDL_Log("SDL_GamepadBinding has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadBinding_pack1), (int)sizeof(SDL_GamepadBinding_pack8));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GamepadBinding_pack4) != sizeof(SDL_GamepadBinding_pack1)) {
        SDL_Log("SDL_GamepadBinding has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GamepadBinding_pack1), (int)sizeof(SDL_GamepadBinding_pack4));
        result = SDL_FALSE;
    }

    /* SDL_Locale */

    if (sizeof(SDL_Locale_pack8) != sizeof(SDL_Locale_pack1)) {
        SDL_Log("SDL_Locale has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Locale_pack1), (int)sizeof(SDL_Locale_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Locale_pack8, language) != SDL_OFFSETOF(SDL_Locale_pack1, language)) {
        SDL_Log("SDL_Locale.language has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Locale_pack1, language), SDL_OFFSETOF(SDL_Locale_pack8, language));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Locale_pack8, country) != SDL_OFFSETOF(SDL_Locale_pack1, country)) {
        SDL_Log("SDL_Locale.country has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Locale_pack1, country), SDL_OFFSETOF(SDL_Locale_pack8, country));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_Locale_pack4) != sizeof(SDL_Locale_pack1)) {
        SDL_Log("SDL_Locale has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Locale_pack1), (int)sizeof(SDL_Locale_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Locale_pack4, language) != SDL_OFFSETOF(SDL_Locale_pack1, language)) {
        SDL_Log("SDL_Locale.language has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Locale_pack1, language), SDL_OFFSETOF(SDL_Locale_pack4, language));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Locale_pack4, country) != SDL_OFFSETOF(SDL_Locale_pack1, country)) {
        SDL_Log("SDL_Locale.country has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Locale_pack1, country), SDL_OFFSETOF(SDL_Locale_pack4, country));
        result = SDL_FALSE;
    }

    /* SDL_AudioSpec */

    if (sizeof(SDL_AudioSpec_pack8) != sizeof(SDL_AudioSpec_pack1)) {
        SDL_Log("SDL_AudioSpec has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_AudioSpec_pack1), (int)sizeof(SDL_AudioSpec_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioSpec_pack8, format) != SDL_OFFSETOF(SDL_AudioSpec_pack1, format)) {
        SDL_Log("SDL_AudioSpec.format has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioSpec_pack1, format), SDL_OFFSETOF(SDL_AudioSpec_pack8, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioSpec_pack8, channels) != SDL_OFFSETOF(SDL_AudioSpec_pack1, channels)) {
        SDL_Log("SDL_AudioSpec.channels has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioSpec_pack1, channels), SDL_OFFSETOF(SDL_AudioSpec_pack8, channels));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioSpec_pack8, freq) != SDL_OFFSETOF(SDL_AudioSpec_pack1, freq)) {
        SDL_Log("SDL_AudioSpec.freq has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioSpec_pack1, freq), SDL_OFFSETOF(SDL_AudioSpec_pack8, freq));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_AudioSpec_pack4) != sizeof(SDL_AudioSpec_pack1)) {
        SDL_Log("SDL_AudioSpec has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_AudioSpec_pack1), (int)sizeof(SDL_AudioSpec_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioSpec_pack4, format) != SDL_OFFSETOF(SDL_AudioSpec_pack1, format)) {
        SDL_Log("SDL_AudioSpec.format has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioSpec_pack1, format), SDL_OFFSETOF(SDL_AudioSpec_pack4, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioSpec_pack4, channels) != SDL_OFFSETOF(SDL_AudioSpec_pack1, channels)) {
        SDL_Log("SDL_AudioSpec.channels has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioSpec_pack1, channels), SDL_OFFSETOF(SDL_AudioSpec_pack4, channels));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_AudioSpec_pack4, freq) != SDL_OFFSETOF(SDL_AudioSpec_pack1, freq)) {
        SDL_Log("SDL_AudioSpec.freq has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_AudioSpec_pack1, freq), SDL_OFFSETOF(SDL_AudioSpec_pack4, freq));
        result = SDL_FALSE;
    }

    /* SDL_DialogFileFilter */

    if (sizeof(SDL_DialogFileFilter_pack8) != sizeof(SDL_DialogFileFilter_pack1)) {
        SDL_Log("SDL_DialogFileFilter has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_DialogFileFilter_pack1), (int)sizeof(SDL_DialogFileFilter_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DialogFileFilter_pack8, name) != SDL_OFFSETOF(SDL_DialogFileFilter_pack1, name)) {
        SDL_Log("SDL_DialogFileFilter.name has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DialogFileFilter_pack1, name), SDL_OFFSETOF(SDL_DialogFileFilter_pack8, name));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DialogFileFilter_pack8, pattern) != SDL_OFFSETOF(SDL_DialogFileFilter_pack1, pattern)) {
        SDL_Log("SDL_DialogFileFilter.pattern has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DialogFileFilter_pack1, pattern), SDL_OFFSETOF(SDL_DialogFileFilter_pack8, pattern));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_DialogFileFilter_pack4) != sizeof(SDL_DialogFileFilter_pack1)) {
        SDL_Log("SDL_DialogFileFilter has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_DialogFileFilter_pack1), (int)sizeof(SDL_DialogFileFilter_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DialogFileFilter_pack4, name) != SDL_OFFSETOF(SDL_DialogFileFilter_pack1, name)) {
        SDL_Log("SDL_DialogFileFilter.name has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DialogFileFilter_pack1, name), SDL_OFFSETOF(SDL_DialogFileFilter_pack4, name));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DialogFileFilter_pack4, pattern) != SDL_OFFSETOF(SDL_DialogFileFilter_pack1, pattern)) {
        SDL_Log("SDL_DialogFileFilter.pattern has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DialogFileFilter_pack1, pattern), SDL_OFFSETOF(SDL_DialogFileFilter_pack4, pattern));
        result = SDL_FALSE;
    }

    /* SDL_IOStreamInterface */

    if (sizeof(SDL_IOStreamInterface_pack8) != sizeof(SDL_IOStreamInterface_pack1)) {
        SDL_Log("SDL_IOStreamInterface has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_IOStreamInterface_pack1), (int)sizeof(SDL_IOStreamInterface_pack8));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_IOStreamInterface_pack4) != sizeof(SDL_IOStreamInterface_pack1)) {
        SDL_Log("SDL_IOStreamInterface has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_IOStreamInterface_pack1), (int)sizeof(SDL_IOStreamInterface_pack4));
        result = SDL_FALSE;
    }

    /* SDL_GPUDepthStencilValue */

    if (sizeof(SDL_GPUDepthStencilValue_pack8) != sizeof(SDL_GPUDepthStencilValue_pack1)) {
        SDL_Log("SDL_GPUDepthStencilValue has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUDepthStencilValue_pack1), (int)sizeof(SDL_GPUDepthStencilValue_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack8, depth) != SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, depth)) {
        SDL_Log("SDL_GPUDepthStencilValue.depth has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, depth), SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack8, depth));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack8, stencil) != SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, stencil)) {
        SDL_Log("SDL_GPUDepthStencilValue.stencil has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, stencil), SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack8, stencil));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, padding8[0])) {
        SDL_Log("SDL_GPUDepthStencilValue.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUDepthStencilValue_pack4) != sizeof(SDL_GPUDepthStencilValue_pack1)) {
        SDL_Log("SDL_GPUDepthStencilValue has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUDepthStencilValue_pack1), (int)sizeof(SDL_GPUDepthStencilValue_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack4, depth) != SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, depth)) {
        SDL_Log("SDL_GPUDepthStencilValue.depth has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, depth), SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack4, depth));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack4, stencil) != SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, stencil)) {
        SDL_Log("SDL_GPUDepthStencilValue.stencil has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, stencil), SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack4, stencil));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, padding8[0])) {
        SDL_Log("SDL_GPUDepthStencilValue.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUDepthStencilValue_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    /* SDL_GPUViewport */

    if (sizeof(SDL_GPUViewport_pack8) != sizeof(SDL_GPUViewport_pack1)) {
        SDL_Log("SDL_GPUViewport has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUViewport_pack1), (int)sizeof(SDL_GPUViewport_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack8, x) != SDL_OFFSETOF(SDL_GPUViewport_pack1, x)) {
        SDL_Log("SDL_GPUViewport.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, x), SDL_OFFSETOF(SDL_GPUViewport_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack8, y) != SDL_OFFSETOF(SDL_GPUViewport_pack1, y)) {
        SDL_Log("SDL_GPUViewport.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, y), SDL_OFFSETOF(SDL_GPUViewport_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack8, w) != SDL_OFFSETOF(SDL_GPUViewport_pack1, w)) {
        SDL_Log("SDL_GPUViewport.w has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, w), SDL_OFFSETOF(SDL_GPUViewport_pack8, w));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack8, h) != SDL_OFFSETOF(SDL_GPUViewport_pack1, h)) {
        SDL_Log("SDL_GPUViewport.h has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, h), SDL_OFFSETOF(SDL_GPUViewport_pack8, h));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack8, minDepth) != SDL_OFFSETOF(SDL_GPUViewport_pack1, minDepth)) {
        SDL_Log("SDL_GPUViewport.minDepth has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, minDepth), SDL_OFFSETOF(SDL_GPUViewport_pack8, minDepth));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack8, maxDepth) != SDL_OFFSETOF(SDL_GPUViewport_pack1, maxDepth)) {
        SDL_Log("SDL_GPUViewport.maxDepth has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, maxDepth), SDL_OFFSETOF(SDL_GPUViewport_pack8, maxDepth));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUViewport_pack4) != sizeof(SDL_GPUViewport_pack1)) {
        SDL_Log("SDL_GPUViewport has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUViewport_pack1), (int)sizeof(SDL_GPUViewport_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack4, x) != SDL_OFFSETOF(SDL_GPUViewport_pack1, x)) {
        SDL_Log("SDL_GPUViewport.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, x), SDL_OFFSETOF(SDL_GPUViewport_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack4, y) != SDL_OFFSETOF(SDL_GPUViewport_pack1, y)) {
        SDL_Log("SDL_GPUViewport.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, y), SDL_OFFSETOF(SDL_GPUViewport_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack4, w) != SDL_OFFSETOF(SDL_GPUViewport_pack1, w)) {
        SDL_Log("SDL_GPUViewport.w has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, w), SDL_OFFSETOF(SDL_GPUViewport_pack4, w));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack4, h) != SDL_OFFSETOF(SDL_GPUViewport_pack1, h)) {
        SDL_Log("SDL_GPUViewport.h has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, h), SDL_OFFSETOF(SDL_GPUViewport_pack4, h));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack4, minDepth) != SDL_OFFSETOF(SDL_GPUViewport_pack1, minDepth)) {
        SDL_Log("SDL_GPUViewport.minDepth has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, minDepth), SDL_OFFSETOF(SDL_GPUViewport_pack4, minDepth));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUViewport_pack4, maxDepth) != SDL_OFFSETOF(SDL_GPUViewport_pack1, maxDepth)) {
        SDL_Log("SDL_GPUViewport.maxDepth has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUViewport_pack1, maxDepth), SDL_OFFSETOF(SDL_GPUViewport_pack4, maxDepth));
        result = SDL_FALSE;
    }

    /* SDL_GPUTextureTransferInfo */

    if (sizeof(SDL_GPUTextureTransferInfo_pack8) != sizeof(SDL_GPUTextureTransferInfo_pack1)) {
        SDL_Log("SDL_GPUTextureTransferInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTextureTransferInfo_pack1), (int)sizeof(SDL_GPUTextureTransferInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack8, transferBuffer) != SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, transferBuffer)) {
        SDL_Log("SDL_GPUTextureTransferInfo.transferBuffer has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, transferBuffer), SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack8, transferBuffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack8, offset) != SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, offset)) {
        SDL_Log("SDL_GPUTextureTransferInfo.offset has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, offset), SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack8, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack8, imagePitch) != SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, imagePitch)) {
        SDL_Log("SDL_GPUTextureTransferInfo.imagePitch has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, imagePitch), SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack8, imagePitch));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack8, imageHeight) != SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, imageHeight)) {
        SDL_Log("SDL_GPUTextureTransferInfo.imageHeight has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, imageHeight), SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack8, imageHeight));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUTextureTransferInfo.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUTextureTransferInfo_pack4) != sizeof(SDL_GPUTextureTransferInfo_pack1)) {
        SDL_Log("SDL_GPUTextureTransferInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTextureTransferInfo_pack1), (int)sizeof(SDL_GPUTextureTransferInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack4, transferBuffer) != SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, transferBuffer)) {
        SDL_Log("SDL_GPUTextureTransferInfo.transferBuffer has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, transferBuffer), SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack4, transferBuffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack4, offset) != SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, offset)) {
        SDL_Log("SDL_GPUTextureTransferInfo.offset has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, offset), SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack4, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack4, imagePitch) != SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, imagePitch)) {
        SDL_Log("SDL_GPUTextureTransferInfo.imagePitch has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, imagePitch), SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack4, imagePitch));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack4, imageHeight) != SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, imageHeight)) {
        SDL_Log("SDL_GPUTextureTransferInfo.imageHeight has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, imageHeight), SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack4, imageHeight));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUTextureTransferInfo.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUTextureTransferInfo_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUTransferBufferLocation */

    if (sizeof(SDL_GPUTransferBufferLocation_pack8) != sizeof(SDL_GPUTransferBufferLocation_pack1)) {
        SDL_Log("SDL_GPUTransferBufferLocation has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTransferBufferLocation_pack1), (int)sizeof(SDL_GPUTransferBufferLocation_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack8, transferBuffer) != SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, transferBuffer)) {
        SDL_Log("SDL_GPUTransferBufferLocation.transferBuffer has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, transferBuffer), SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack8, transferBuffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack8, offset) != SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, offset)) {
        SDL_Log("SDL_GPUTransferBufferLocation.offset has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, offset), SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack8, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, padding_end)) {
        SDL_Log("SDL_GPUTransferBufferLocation.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, padding_end), SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUTransferBufferLocation_pack4) != sizeof(SDL_GPUTransferBufferLocation_pack1)) {
        SDL_Log("SDL_GPUTransferBufferLocation has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTransferBufferLocation_pack1), (int)sizeof(SDL_GPUTransferBufferLocation_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack4, transferBuffer) != SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, transferBuffer)) {
        SDL_Log("SDL_GPUTransferBufferLocation.transferBuffer has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, transferBuffer), SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack4, transferBuffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack4, offset) != SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, offset)) {
        SDL_Log("SDL_GPUTransferBufferLocation.offset has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, offset), SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack4, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, padding_end)) {
        SDL_Log("SDL_GPUTransferBufferLocation.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack1, padding_end), SDL_OFFSETOF(SDL_GPUTransferBufferLocation_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUTextureLocation */

    if (sizeof(SDL_GPUTextureLocation_pack8) != sizeof(SDL_GPUTextureLocation_pack1)) {
        SDL_Log("SDL_GPUTextureLocation has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTextureLocation_pack1), (int)sizeof(SDL_GPUTextureLocation_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, texture) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, texture)) {
        SDL_Log("SDL_GPUTextureLocation.texture has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, texture), SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, mipLevel) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, mipLevel)) {
        SDL_Log("SDL_GPUTextureLocation.mipLevel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, mipLevel), SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, mipLevel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, layer) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, layer)) {
        SDL_Log("SDL_GPUTextureLocation.layer has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, layer), SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, layer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, x) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, x)) {
        SDL_Log("SDL_GPUTextureLocation.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, x), SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, y) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, y)) {
        SDL_Log("SDL_GPUTextureLocation.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, y), SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, z) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, z)) {
        SDL_Log("SDL_GPUTextureLocation.z has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, z), SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, z));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, padding_end)) {
        SDL_Log("SDL_GPUTextureLocation.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, padding_end), SDL_OFFSETOF(SDL_GPUTextureLocation_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUTextureLocation_pack4) != sizeof(SDL_GPUTextureLocation_pack1)) {
        SDL_Log("SDL_GPUTextureLocation has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTextureLocation_pack1), (int)sizeof(SDL_GPUTextureLocation_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, texture) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, texture)) {
        SDL_Log("SDL_GPUTextureLocation.texture has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, texture), SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, mipLevel) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, mipLevel)) {
        SDL_Log("SDL_GPUTextureLocation.mipLevel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, mipLevel), SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, mipLevel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, layer) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, layer)) {
        SDL_Log("SDL_GPUTextureLocation.layer has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, layer), SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, layer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, x) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, x)) {
        SDL_Log("SDL_GPUTextureLocation.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, x), SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, y) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, y)) {
        SDL_Log("SDL_GPUTextureLocation.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, y), SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, z) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, z)) {
        SDL_Log("SDL_GPUTextureLocation.z has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, z), SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, z));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, padding_end)) {
        SDL_Log("SDL_GPUTextureLocation.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureLocation_pack1, padding_end), SDL_OFFSETOF(SDL_GPUTextureLocation_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUTextureRegion */

    if (sizeof(SDL_GPUTextureRegion_pack8) != sizeof(SDL_GPUTextureRegion_pack1)) {
        SDL_Log("SDL_GPUTextureRegion has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTextureRegion_pack1), (int)sizeof(SDL_GPUTextureRegion_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, texture) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, texture)) {
        SDL_Log("SDL_GPUTextureRegion.texture has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, texture), SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, mipLevel) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, mipLevel)) {
        SDL_Log("SDL_GPUTextureRegion.mipLevel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, mipLevel), SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, mipLevel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, layer) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, layer)) {
        SDL_Log("SDL_GPUTextureRegion.layer has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, layer), SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, layer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, x) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, x)) {
        SDL_Log("SDL_GPUTextureRegion.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, x), SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, y) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, y)) {
        SDL_Log("SDL_GPUTextureRegion.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, y), SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, z) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, z)) {
        SDL_Log("SDL_GPUTextureRegion.z has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, z), SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, z));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, w) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, w)) {
        SDL_Log("SDL_GPUTextureRegion.w has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, w), SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, w));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, h) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, h)) {
        SDL_Log("SDL_GPUTextureRegion.h has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, h), SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, h));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, d) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, d)) {
        SDL_Log("SDL_GPUTextureRegion.d has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, d), SDL_OFFSETOF(SDL_GPUTextureRegion_pack8, d));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUTextureRegion_pack4) != sizeof(SDL_GPUTextureRegion_pack1)) {
        SDL_Log("SDL_GPUTextureRegion has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTextureRegion_pack1), (int)sizeof(SDL_GPUTextureRegion_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, texture) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, texture)) {
        SDL_Log("SDL_GPUTextureRegion.texture has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, texture), SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, mipLevel) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, mipLevel)) {
        SDL_Log("SDL_GPUTextureRegion.mipLevel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, mipLevel), SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, mipLevel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, layer) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, layer)) {
        SDL_Log("SDL_GPUTextureRegion.layer has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, layer), SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, layer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, x) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, x)) {
        SDL_Log("SDL_GPUTextureRegion.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, x), SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, y) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, y)) {
        SDL_Log("SDL_GPUTextureRegion.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, y), SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, z) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, z)) {
        SDL_Log("SDL_GPUTextureRegion.z has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, z), SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, z));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, w) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, w)) {
        SDL_Log("SDL_GPUTextureRegion.w has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, w), SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, w));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, h) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, h)) {
        SDL_Log("SDL_GPUTextureRegion.h has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, h), SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, h));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, d) != SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, d)) {
        SDL_Log("SDL_GPUTextureRegion.d has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureRegion_pack1, d), SDL_OFFSETOF(SDL_GPUTextureRegion_pack4, d));
        result = SDL_FALSE;
    }

    /* SDL_GPUBlitRegion */

    if (sizeof(SDL_GPUBlitRegion_pack8) != sizeof(SDL_GPUBlitRegion_pack1)) {
        SDL_Log("SDL_GPUBlitRegion has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUBlitRegion_pack1), (int)sizeof(SDL_GPUBlitRegion_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, texture) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, texture)) {
        SDL_Log("SDL_GPUBlitRegion.texture has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, texture), SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, mipLevel) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, mipLevel)) {
        SDL_Log("SDL_GPUBlitRegion.mipLevel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, mipLevel), SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, mipLevel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, layerOrDepthPlane) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, layerOrDepthPlane)) {
        SDL_Log("SDL_GPUBlitRegion.layerOrDepthPlane has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, layerOrDepthPlane), SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, layerOrDepthPlane));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, x) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, x)) {
        SDL_Log("SDL_GPUBlitRegion.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, x), SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, y) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, y)) {
        SDL_Log("SDL_GPUBlitRegion.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, y), SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, w) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, w)) {
        SDL_Log("SDL_GPUBlitRegion.w has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, w), SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, w));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, h) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, h)) {
        SDL_Log("SDL_GPUBlitRegion.h has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, h), SDL_OFFSETOF(SDL_GPUBlitRegion_pack8, h));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUBlitRegion_pack4) != sizeof(SDL_GPUBlitRegion_pack1)) {
        SDL_Log("SDL_GPUBlitRegion has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUBlitRegion_pack1), (int)sizeof(SDL_GPUBlitRegion_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, texture) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, texture)) {
        SDL_Log("SDL_GPUBlitRegion.texture has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, texture), SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, mipLevel) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, mipLevel)) {
        SDL_Log("SDL_GPUBlitRegion.mipLevel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, mipLevel), SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, mipLevel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, layerOrDepthPlane) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, layerOrDepthPlane)) {
        SDL_Log("SDL_GPUBlitRegion.layerOrDepthPlane has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, layerOrDepthPlane), SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, layerOrDepthPlane));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, x) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, x)) {
        SDL_Log("SDL_GPUBlitRegion.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, x), SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, y) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, y)) {
        SDL_Log("SDL_GPUBlitRegion.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, y), SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, w) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, w)) {
        SDL_Log("SDL_GPUBlitRegion.w has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, w), SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, w));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, h) != SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, h)) {
        SDL_Log("SDL_GPUBlitRegion.h has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBlitRegion_pack1, h), SDL_OFFSETOF(SDL_GPUBlitRegion_pack4, h));
        result = SDL_FALSE;
    }

    /* SDL_GPUBufferLocation */

    if (sizeof(SDL_GPUBufferLocation_pack8) != sizeof(SDL_GPUBufferLocation_pack1)) {
        SDL_Log("SDL_GPUBufferLocation has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUBufferLocation_pack1), (int)sizeof(SDL_GPUBufferLocation_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferLocation_pack8, buffer) != SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, buffer)) {
        SDL_Log("SDL_GPUBufferLocation.buffer has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, buffer), SDL_OFFSETOF(SDL_GPUBufferLocation_pack8, buffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferLocation_pack8, offset) != SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, offset)) {
        SDL_Log("SDL_GPUBufferLocation.offset has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, offset), SDL_OFFSETOF(SDL_GPUBufferLocation_pack8, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferLocation_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, padding_end)) {
        SDL_Log("SDL_GPUBufferLocation.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, padding_end), SDL_OFFSETOF(SDL_GPUBufferLocation_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUBufferLocation_pack4) != sizeof(SDL_GPUBufferLocation_pack1)) {
        SDL_Log("SDL_GPUBufferLocation has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUBufferLocation_pack1), (int)sizeof(SDL_GPUBufferLocation_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferLocation_pack4, buffer) != SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, buffer)) {
        SDL_Log("SDL_GPUBufferLocation.buffer has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, buffer), SDL_OFFSETOF(SDL_GPUBufferLocation_pack4, buffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferLocation_pack4, offset) != SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, offset)) {
        SDL_Log("SDL_GPUBufferLocation.offset has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, offset), SDL_OFFSETOF(SDL_GPUBufferLocation_pack4, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferLocation_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, padding_end)) {
        SDL_Log("SDL_GPUBufferLocation.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferLocation_pack1, padding_end), SDL_OFFSETOF(SDL_GPUBufferLocation_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUBufferRegion */

    if (sizeof(SDL_GPUBufferRegion_pack8) != sizeof(SDL_GPUBufferRegion_pack1)) {
        SDL_Log("SDL_GPUBufferRegion has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUBufferRegion_pack1), (int)sizeof(SDL_GPUBufferRegion_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferRegion_pack8, buffer) != SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, buffer)) {
        SDL_Log("SDL_GPUBufferRegion.buffer has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, buffer), SDL_OFFSETOF(SDL_GPUBufferRegion_pack8, buffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferRegion_pack8, offset) != SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, offset)) {
        SDL_Log("SDL_GPUBufferRegion.offset has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, offset), SDL_OFFSETOF(SDL_GPUBufferRegion_pack8, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferRegion_pack8, size) != SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, size)) {
        SDL_Log("SDL_GPUBufferRegion.size has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, size), SDL_OFFSETOF(SDL_GPUBufferRegion_pack8, size));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUBufferRegion_pack4) != sizeof(SDL_GPUBufferRegion_pack1)) {
        SDL_Log("SDL_GPUBufferRegion has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUBufferRegion_pack1), (int)sizeof(SDL_GPUBufferRegion_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferRegion_pack4, buffer) != SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, buffer)) {
        SDL_Log("SDL_GPUBufferRegion.buffer has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, buffer), SDL_OFFSETOF(SDL_GPUBufferRegion_pack4, buffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferRegion_pack4, offset) != SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, offset)) {
        SDL_Log("SDL_GPUBufferRegion.offset has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, offset), SDL_OFFSETOF(SDL_GPUBufferRegion_pack4, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferRegion_pack4, size) != SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, size)) {
        SDL_Log("SDL_GPUBufferRegion.size has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferRegion_pack1, size), SDL_OFFSETOF(SDL_GPUBufferRegion_pack4, size));
        result = SDL_FALSE;
    }

    /* SDL_GPUIndirectDrawCommand */

    if (sizeof(SDL_GPUIndirectDrawCommand_pack8) != sizeof(SDL_GPUIndirectDrawCommand_pack1)) {
        SDL_Log("SDL_GPUIndirectDrawCommand has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUIndirectDrawCommand_pack1), (int)sizeof(SDL_GPUIndirectDrawCommand_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack8, vertexCount) != SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, vertexCount)) {
        SDL_Log("SDL_GPUIndirectDrawCommand.vertexCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, vertexCount), SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack8, vertexCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack8, instanceCount) != SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, instanceCount)) {
        SDL_Log("SDL_GPUIndirectDrawCommand.instanceCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, instanceCount), SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack8, instanceCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack8, firstVertex) != SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, firstVertex)) {
        SDL_Log("SDL_GPUIndirectDrawCommand.firstVertex has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, firstVertex), SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack8, firstVertex));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack8, firstInstance) != SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, firstInstance)) {
        SDL_Log("SDL_GPUIndirectDrawCommand.firstInstance has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, firstInstance), SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack8, firstInstance));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUIndirectDrawCommand_pack4) != sizeof(SDL_GPUIndirectDrawCommand_pack1)) {
        SDL_Log("SDL_GPUIndirectDrawCommand has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUIndirectDrawCommand_pack1), (int)sizeof(SDL_GPUIndirectDrawCommand_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack4, vertexCount) != SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, vertexCount)) {
        SDL_Log("SDL_GPUIndirectDrawCommand.vertexCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, vertexCount), SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack4, vertexCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack4, instanceCount) != SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, instanceCount)) {
        SDL_Log("SDL_GPUIndirectDrawCommand.instanceCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, instanceCount), SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack4, instanceCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack4, firstVertex) != SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, firstVertex)) {
        SDL_Log("SDL_GPUIndirectDrawCommand.firstVertex has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, firstVertex), SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack4, firstVertex));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack4, firstInstance) != SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, firstInstance)) {
        SDL_Log("SDL_GPUIndirectDrawCommand.firstInstance has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack1, firstInstance), SDL_OFFSETOF(SDL_GPUIndirectDrawCommand_pack4, firstInstance));
        result = SDL_FALSE;
    }

    /* SDL_GPUIndexedIndirectDrawCommand */

    if (sizeof(SDL_GPUIndexedIndirectDrawCommand_pack8) != sizeof(SDL_GPUIndexedIndirectDrawCommand_pack1)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUIndexedIndirectDrawCommand_pack1), (int)sizeof(SDL_GPUIndexedIndirectDrawCommand_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack8, indexCount) != SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, indexCount)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand.indexCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, indexCount), SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack8, indexCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack8, instanceCount) != SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, instanceCount)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand.instanceCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, instanceCount), SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack8, instanceCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack8, firstIndex) != SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, firstIndex)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand.firstIndex has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, firstIndex), SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack8, firstIndex));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack8, vertexOffset) != SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, vertexOffset)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand.vertexOffset has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, vertexOffset), SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack8, vertexOffset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack8, firstInstance) != SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, firstInstance)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand.firstInstance has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, firstInstance), SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack8, firstInstance));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUIndexedIndirectDrawCommand_pack4) != sizeof(SDL_GPUIndexedIndirectDrawCommand_pack1)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUIndexedIndirectDrawCommand_pack1), (int)sizeof(SDL_GPUIndexedIndirectDrawCommand_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack4, indexCount) != SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, indexCount)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand.indexCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, indexCount), SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack4, indexCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack4, instanceCount) != SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, instanceCount)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand.instanceCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, instanceCount), SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack4, instanceCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack4, firstIndex) != SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, firstIndex)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand.firstIndex has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, firstIndex), SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack4, firstIndex));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack4, vertexOffset) != SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, vertexOffset)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand.vertexOffset has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, vertexOffset), SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack4, vertexOffset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack4, firstInstance) != SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, firstInstance)) {
        SDL_Log("SDL_GPUIndexedIndirectDrawCommand.firstInstance has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack1, firstInstance), SDL_OFFSETOF(SDL_GPUIndexedIndirectDrawCommand_pack4, firstInstance));
        result = SDL_FALSE;
    }

    /* SDL_GPUIndirectDispatchCommand */

    if (sizeof(SDL_GPUIndirectDispatchCommand_pack8) != sizeof(SDL_GPUIndirectDispatchCommand_pack1)) {
        SDL_Log("SDL_GPUIndirectDispatchCommand has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUIndirectDispatchCommand_pack1), (int)sizeof(SDL_GPUIndirectDispatchCommand_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack8, groupCountX) != SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountX)) {
        SDL_Log("SDL_GPUIndirectDispatchCommand.groupCountX has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountX), SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack8, groupCountX));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack8, groupCountY) != SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountY)) {
        SDL_Log("SDL_GPUIndirectDispatchCommand.groupCountY has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountY), SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack8, groupCountY));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack8, groupCountZ) != SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountZ)) {
        SDL_Log("SDL_GPUIndirectDispatchCommand.groupCountZ has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountZ), SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack8, groupCountZ));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUIndirectDispatchCommand_pack4) != sizeof(SDL_GPUIndirectDispatchCommand_pack1)) {
        SDL_Log("SDL_GPUIndirectDispatchCommand has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUIndirectDispatchCommand_pack1), (int)sizeof(SDL_GPUIndirectDispatchCommand_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack4, groupCountX) != SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountX)) {
        SDL_Log("SDL_GPUIndirectDispatchCommand.groupCountX has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountX), SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack4, groupCountX));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack4, groupCountY) != SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountY)) {
        SDL_Log("SDL_GPUIndirectDispatchCommand.groupCountY has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountY), SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack4, groupCountY));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack4, groupCountZ) != SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountZ)) {
        SDL_Log("SDL_GPUIndirectDispatchCommand.groupCountZ has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack1, groupCountZ), SDL_OFFSETOF(SDL_GPUIndirectDispatchCommand_pack4, groupCountZ));
        result = SDL_FALSE;
    }

    /* SDL_GPUSamplerCreateInfo */

    if (sizeof(SDL_GPUSamplerCreateInfo_pack8) != sizeof(SDL_GPUSamplerCreateInfo_pack1)) {
        SDL_Log("SDL_GPUSamplerCreateInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUSamplerCreateInfo_pack1), (int)sizeof(SDL_GPUSamplerCreateInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, minFilter) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, minFilter)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.minFilter has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, minFilter), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, minFilter));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, magFilter) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, magFilter)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.magFilter has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, magFilter), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, magFilter));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, mipmapMode) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, mipmapMode)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.mipmapMode has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, mipmapMode), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, mipmapMode));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, addressModeU) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeU)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.addressModeU has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeU), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, addressModeU));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, addressModeV) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeV)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.addressModeV has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeV), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, addressModeV));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, addressModeW) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeW)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.addressModeW has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeW), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, addressModeW));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, mipLodBias) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, mipLodBias)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.mipLodBias has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, mipLodBias), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, mipLodBias));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, maxAnisotropy) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, maxAnisotropy)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.maxAnisotropy has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, maxAnisotropy), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, maxAnisotropy));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, anisotropyEnable) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, anisotropyEnable)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.anisotropyEnable has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, anisotropyEnable), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, anisotropyEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, compareEnable) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, compareEnable)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.compareEnable has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, compareEnable), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, compareEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, padding8[0])) {
        SDL_Log("SDL_GPUSamplerCreateInfo.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, compareOp) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, compareOp)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.compareOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, compareOp), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, compareOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, minLod) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, minLod)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.minLod has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, minLod), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, minLod));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, maxLod) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, maxLod)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.maxLod has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, maxLod), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, maxLod));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, props) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.props has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack8, props));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUSamplerCreateInfo_pack4) != sizeof(SDL_GPUSamplerCreateInfo_pack1)) {
        SDL_Log("SDL_GPUSamplerCreateInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUSamplerCreateInfo_pack1), (int)sizeof(SDL_GPUSamplerCreateInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, minFilter) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, minFilter)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.minFilter has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, minFilter), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, minFilter));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, magFilter) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, magFilter)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.magFilter has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, magFilter), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, magFilter));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, mipmapMode) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, mipmapMode)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.mipmapMode has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, mipmapMode), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, mipmapMode));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, addressModeU) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeU)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.addressModeU has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeU), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, addressModeU));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, addressModeV) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeV)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.addressModeV has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeV), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, addressModeV));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, addressModeW) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeW)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.addressModeW has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, addressModeW), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, addressModeW));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, mipLodBias) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, mipLodBias)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.mipLodBias has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, mipLodBias), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, mipLodBias));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, maxAnisotropy) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, maxAnisotropy)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.maxAnisotropy has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, maxAnisotropy), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, maxAnisotropy));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, anisotropyEnable) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, anisotropyEnable)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.anisotropyEnable has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, anisotropyEnable), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, anisotropyEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, compareEnable) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, compareEnable)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.compareEnable has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, compareEnable), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, compareEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, padding8[0])) {
        SDL_Log("SDL_GPUSamplerCreateInfo.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, compareOp) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, compareOp)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.compareOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, compareOp), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, compareOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, minLod) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, minLod)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.minLod has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, minLod), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, minLod));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, maxLod) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, maxLod)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.maxLod has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, maxLod), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, maxLod));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, props) != SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUSamplerCreateInfo.props has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUSamplerCreateInfo_pack4, props));
        result = SDL_FALSE;
    }

    /* SDL_GPUVertexBinding */

    if (sizeof(SDL_GPUVertexBinding_pack8) != sizeof(SDL_GPUVertexBinding_pack1)) {
        SDL_Log("SDL_GPUVertexBinding has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUVertexBinding_pack1), (int)sizeof(SDL_GPUVertexBinding_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexBinding_pack8, binding) != SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, binding)) {
        SDL_Log("SDL_GPUVertexBinding.binding has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, binding), SDL_OFFSETOF(SDL_GPUVertexBinding_pack8, binding));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexBinding_pack8, stride) != SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, stride)) {
        SDL_Log("SDL_GPUVertexBinding.stride has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, stride), SDL_OFFSETOF(SDL_GPUVertexBinding_pack8, stride));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexBinding_pack8, inputRate) != SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, inputRate)) {
        SDL_Log("SDL_GPUVertexBinding.inputRate has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, inputRate), SDL_OFFSETOF(SDL_GPUVertexBinding_pack8, inputRate));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexBinding_pack8, instanceStepRate) != SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, instanceStepRate)) {
        SDL_Log("SDL_GPUVertexBinding.instanceStepRate has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, instanceStepRate), SDL_OFFSETOF(SDL_GPUVertexBinding_pack8, instanceStepRate));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUVertexBinding_pack4) != sizeof(SDL_GPUVertexBinding_pack1)) {
        SDL_Log("SDL_GPUVertexBinding has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUVertexBinding_pack1), (int)sizeof(SDL_GPUVertexBinding_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexBinding_pack4, binding) != SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, binding)) {
        SDL_Log("SDL_GPUVertexBinding.binding has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, binding), SDL_OFFSETOF(SDL_GPUVertexBinding_pack4, binding));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexBinding_pack4, stride) != SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, stride)) {
        SDL_Log("SDL_GPUVertexBinding.stride has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, stride), SDL_OFFSETOF(SDL_GPUVertexBinding_pack4, stride));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexBinding_pack4, inputRate) != SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, inputRate)) {
        SDL_Log("SDL_GPUVertexBinding.inputRate has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, inputRate), SDL_OFFSETOF(SDL_GPUVertexBinding_pack4, inputRate));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexBinding_pack4, instanceStepRate) != SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, instanceStepRate)) {
        SDL_Log("SDL_GPUVertexBinding.instanceStepRate has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexBinding_pack1, instanceStepRate), SDL_OFFSETOF(SDL_GPUVertexBinding_pack4, instanceStepRate));
        result = SDL_FALSE;
    }

    /* SDL_GPUVertexAttribute */

    if (sizeof(SDL_GPUVertexAttribute_pack8) != sizeof(SDL_GPUVertexAttribute_pack1)) {
        SDL_Log("SDL_GPUVertexAttribute has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUVertexAttribute_pack1), (int)sizeof(SDL_GPUVertexAttribute_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexAttribute_pack8, location) != SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, location)) {
        SDL_Log("SDL_GPUVertexAttribute.location has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, location), SDL_OFFSETOF(SDL_GPUVertexAttribute_pack8, location));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexAttribute_pack8, binding) != SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, binding)) {
        SDL_Log("SDL_GPUVertexAttribute.binding has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, binding), SDL_OFFSETOF(SDL_GPUVertexAttribute_pack8, binding));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexAttribute_pack8, format) != SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, format)) {
        SDL_Log("SDL_GPUVertexAttribute.format has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, format), SDL_OFFSETOF(SDL_GPUVertexAttribute_pack8, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexAttribute_pack8, offset) != SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, offset)) {
        SDL_Log("SDL_GPUVertexAttribute.offset has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, offset), SDL_OFFSETOF(SDL_GPUVertexAttribute_pack8, offset));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUVertexAttribute_pack4) != sizeof(SDL_GPUVertexAttribute_pack1)) {
        SDL_Log("SDL_GPUVertexAttribute has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUVertexAttribute_pack1), (int)sizeof(SDL_GPUVertexAttribute_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexAttribute_pack4, location) != SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, location)) {
        SDL_Log("SDL_GPUVertexAttribute.location has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, location), SDL_OFFSETOF(SDL_GPUVertexAttribute_pack4, location));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexAttribute_pack4, binding) != SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, binding)) {
        SDL_Log("SDL_GPUVertexAttribute.binding has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, binding), SDL_OFFSETOF(SDL_GPUVertexAttribute_pack4, binding));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexAttribute_pack4, format) != SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, format)) {
        SDL_Log("SDL_GPUVertexAttribute.format has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, format), SDL_OFFSETOF(SDL_GPUVertexAttribute_pack4, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexAttribute_pack4, offset) != SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, offset)) {
        SDL_Log("SDL_GPUVertexAttribute.offset has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexAttribute_pack1, offset), SDL_OFFSETOF(SDL_GPUVertexAttribute_pack4, offset));
        result = SDL_FALSE;
    }

    /* SDL_GPUVertexInputState */

    if (sizeof(SDL_GPUVertexInputState_pack8) != sizeof(SDL_GPUVertexInputState_pack1)) {
        SDL_Log("SDL_GPUVertexInputState has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUVertexInputState_pack1), (int)sizeof(SDL_GPUVertexInputState_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexInputState_pack8, vertexBindings) != SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexBindings)) {
        SDL_Log("SDL_GPUVertexInputState.vertexBindings has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexBindings), SDL_OFFSETOF(SDL_GPUVertexInputState_pack8, vertexBindings));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexInputState_pack8, vertexAttributes) != SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexAttributes)) {
        SDL_Log("SDL_GPUVertexInputState.vertexAttributes has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexAttributes), SDL_OFFSETOF(SDL_GPUVertexInputState_pack8, vertexAttributes));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexInputState_pack8, vertexBindingCount) != SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexBindingCount)) {
        SDL_Log("SDL_GPUVertexInputState.vertexBindingCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexBindingCount), SDL_OFFSETOF(SDL_GPUVertexInputState_pack8, vertexBindingCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexInputState_pack8, vertexAttributeCount) != SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexAttributeCount)) {
        SDL_Log("SDL_GPUVertexInputState.vertexAttributeCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexAttributeCount), SDL_OFFSETOF(SDL_GPUVertexInputState_pack8, vertexAttributeCount));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUVertexInputState_pack4) != sizeof(SDL_GPUVertexInputState_pack1)) {
        SDL_Log("SDL_GPUVertexInputState has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUVertexInputState_pack1), (int)sizeof(SDL_GPUVertexInputState_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexInputState_pack4, vertexBindings) != SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexBindings)) {
        SDL_Log("SDL_GPUVertexInputState.vertexBindings has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexBindings), SDL_OFFSETOF(SDL_GPUVertexInputState_pack4, vertexBindings));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexInputState_pack4, vertexAttributes) != SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexAttributes)) {
        SDL_Log("SDL_GPUVertexInputState.vertexAttributes has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexAttributes), SDL_OFFSETOF(SDL_GPUVertexInputState_pack4, vertexAttributes));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexInputState_pack4, vertexBindingCount) != SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexBindingCount)) {
        SDL_Log("SDL_GPUVertexInputState.vertexBindingCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexBindingCount), SDL_OFFSETOF(SDL_GPUVertexInputState_pack4, vertexBindingCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUVertexInputState_pack4, vertexAttributeCount) != SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexAttributeCount)) {
        SDL_Log("SDL_GPUVertexInputState.vertexAttributeCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUVertexInputState_pack1, vertexAttributeCount), SDL_OFFSETOF(SDL_GPUVertexInputState_pack4, vertexAttributeCount));
        result = SDL_FALSE;
    }

    /* SDL_GPUStencilOpState */

    if (sizeof(SDL_GPUStencilOpState_pack8) != sizeof(SDL_GPUStencilOpState_pack1)) {
        SDL_Log("SDL_GPUStencilOpState has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUStencilOpState_pack1), (int)sizeof(SDL_GPUStencilOpState_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStencilOpState_pack8, failOp) != SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, failOp)) {
        SDL_Log("SDL_GPUStencilOpState.failOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, failOp), SDL_OFFSETOF(SDL_GPUStencilOpState_pack8, failOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStencilOpState_pack8, passOp) != SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, passOp)) {
        SDL_Log("SDL_GPUStencilOpState.passOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, passOp), SDL_OFFSETOF(SDL_GPUStencilOpState_pack8, passOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStencilOpState_pack8, depthFailOp) != SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, depthFailOp)) {
        SDL_Log("SDL_GPUStencilOpState.depthFailOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, depthFailOp), SDL_OFFSETOF(SDL_GPUStencilOpState_pack8, depthFailOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStencilOpState_pack8, compareOp) != SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, compareOp)) {
        SDL_Log("SDL_GPUStencilOpState.compareOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, compareOp), SDL_OFFSETOF(SDL_GPUStencilOpState_pack8, compareOp));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUStencilOpState_pack4) != sizeof(SDL_GPUStencilOpState_pack1)) {
        SDL_Log("SDL_GPUStencilOpState has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUStencilOpState_pack1), (int)sizeof(SDL_GPUStencilOpState_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStencilOpState_pack4, failOp) != SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, failOp)) {
        SDL_Log("SDL_GPUStencilOpState.failOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, failOp), SDL_OFFSETOF(SDL_GPUStencilOpState_pack4, failOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStencilOpState_pack4, passOp) != SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, passOp)) {
        SDL_Log("SDL_GPUStencilOpState.passOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, passOp), SDL_OFFSETOF(SDL_GPUStencilOpState_pack4, passOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStencilOpState_pack4, depthFailOp) != SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, depthFailOp)) {
        SDL_Log("SDL_GPUStencilOpState.depthFailOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, depthFailOp), SDL_OFFSETOF(SDL_GPUStencilOpState_pack4, depthFailOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStencilOpState_pack4, compareOp) != SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, compareOp)) {
        SDL_Log("SDL_GPUStencilOpState.compareOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStencilOpState_pack1, compareOp), SDL_OFFSETOF(SDL_GPUStencilOpState_pack4, compareOp));
        result = SDL_FALSE;
    }

    /* SDL_GPUColorAttachmentBlendState */

    if (sizeof(SDL_GPUColorAttachmentBlendState_pack8) != sizeof(SDL_GPUColorAttachmentBlendState_pack1)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUColorAttachmentBlendState_pack1), (int)sizeof(SDL_GPUColorAttachmentBlendState_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, blendEnable) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, blendEnable)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.blendEnable has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, blendEnable), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, blendEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, padding8[0])) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, srcColorBlendFactor) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, srcColorBlendFactor)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.srcColorBlendFactor has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, srcColorBlendFactor), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, srcColorBlendFactor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, dstColorBlendFactor) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, dstColorBlendFactor)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.dstColorBlendFactor has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, dstColorBlendFactor), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, dstColorBlendFactor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, colorBlendOp) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, colorBlendOp)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.colorBlendOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, colorBlendOp), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, colorBlendOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, srcAlphaBlendFactor) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, srcAlphaBlendFactor)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.srcAlphaBlendFactor has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, srcAlphaBlendFactor), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, srcAlphaBlendFactor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, dstAlphaBlendFactor) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, dstAlphaBlendFactor)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.dstAlphaBlendFactor has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, dstAlphaBlendFactor), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, dstAlphaBlendFactor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, alphaBlendOp) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, alphaBlendOp)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.alphaBlendOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, alphaBlendOp), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, alphaBlendOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, colorWriteMask) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, colorWriteMask)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.colorWriteMask has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, colorWriteMask), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, colorWriteMask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, padding_end[0]) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, padding_end[0])) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.padding_end[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, padding_end[0]), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack8, padding_end[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUColorAttachmentBlendState_pack4) != sizeof(SDL_GPUColorAttachmentBlendState_pack1)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUColorAttachmentBlendState_pack1), (int)sizeof(SDL_GPUColorAttachmentBlendState_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, blendEnable) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, blendEnable)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.blendEnable has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, blendEnable), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, blendEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, padding8[0])) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, srcColorBlendFactor) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, srcColorBlendFactor)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.srcColorBlendFactor has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, srcColorBlendFactor), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, srcColorBlendFactor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, dstColorBlendFactor) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, dstColorBlendFactor)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.dstColorBlendFactor has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, dstColorBlendFactor), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, dstColorBlendFactor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, colorBlendOp) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, colorBlendOp)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.colorBlendOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, colorBlendOp), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, colorBlendOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, srcAlphaBlendFactor) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, srcAlphaBlendFactor)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.srcAlphaBlendFactor has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, srcAlphaBlendFactor), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, srcAlphaBlendFactor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, dstAlphaBlendFactor) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, dstAlphaBlendFactor)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.dstAlphaBlendFactor has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, dstAlphaBlendFactor), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, dstAlphaBlendFactor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, alphaBlendOp) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, alphaBlendOp)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.alphaBlendOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, alphaBlendOp), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, alphaBlendOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, colorWriteMask) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, colorWriteMask)) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.colorWriteMask has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, colorWriteMask), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, colorWriteMask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, padding_end[0]) != SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, padding_end[0])) {
        SDL_Log("SDL_GPUColorAttachmentBlendState.padding_end[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack1, padding_end[0]), SDL_OFFSETOF(SDL_GPUColorAttachmentBlendState_pack4, padding_end[0]));
        result = SDL_FALSE;
    }

    /* SDL_GPUShaderCreateInfo */

    if (sizeof(SDL_GPUShaderCreateInfo_pack8) != sizeof(SDL_GPUShaderCreateInfo_pack1)) {
        SDL_Log("SDL_GPUShaderCreateInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUShaderCreateInfo_pack1), (int)sizeof(SDL_GPUShaderCreateInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, codeSize) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, codeSize)) {
        SDL_Log("SDL_GPUShaderCreateInfo.codeSize has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, codeSize), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, codeSize));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, padding32) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, padding32)) {
        SDL_Log("SDL_GPUShaderCreateInfo.padding32 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, padding32), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, code) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, code)) {
        SDL_Log("SDL_GPUShaderCreateInfo.code has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, code), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, code));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, entryPointName) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, entryPointName)) {
        SDL_Log("SDL_GPUShaderCreateInfo.entryPointName has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, entryPointName), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, entryPointName));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, format) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, format)) {
        SDL_Log("SDL_GPUShaderCreateInfo.format has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, format), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, stage) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, stage)) {
        SDL_Log("SDL_GPUShaderCreateInfo.stage has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, stage), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, stage));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, samplerCount) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, samplerCount)) {
        SDL_Log("SDL_GPUShaderCreateInfo.samplerCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, samplerCount), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, samplerCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, storageTextureCount) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, storageTextureCount)) {
        SDL_Log("SDL_GPUShaderCreateInfo.storageTextureCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, storageTextureCount), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, storageTextureCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, storageBufferCount) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, storageBufferCount)) {
        SDL_Log("SDL_GPUShaderCreateInfo.storageBufferCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, storageBufferCount), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, storageBufferCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, uniformBufferCount) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, uniformBufferCount)) {
        SDL_Log("SDL_GPUShaderCreateInfo.uniformBufferCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, uniformBufferCount), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, uniformBufferCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, props) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUShaderCreateInfo.props has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, props));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUShaderCreateInfo.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUShaderCreateInfo_pack4) != sizeof(SDL_GPUShaderCreateInfo_pack1)) {
        SDL_Log("SDL_GPUShaderCreateInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUShaderCreateInfo_pack1), (int)sizeof(SDL_GPUShaderCreateInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, codeSize) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, codeSize)) {
        SDL_Log("SDL_GPUShaderCreateInfo.codeSize has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, codeSize), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, codeSize));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, padding32) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, padding32)) {
        SDL_Log("SDL_GPUShaderCreateInfo.padding32 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, padding32), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, code) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, code)) {
        SDL_Log("SDL_GPUShaderCreateInfo.code has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, code), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, code));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, entryPointName) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, entryPointName)) {
        SDL_Log("SDL_GPUShaderCreateInfo.entryPointName has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, entryPointName), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, entryPointName));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, format) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, format)) {
        SDL_Log("SDL_GPUShaderCreateInfo.format has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, format), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, stage) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, stage)) {
        SDL_Log("SDL_GPUShaderCreateInfo.stage has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, stage), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, stage));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, samplerCount) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, samplerCount)) {
        SDL_Log("SDL_GPUShaderCreateInfo.samplerCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, samplerCount), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, samplerCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, storageTextureCount) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, storageTextureCount)) {
        SDL_Log("SDL_GPUShaderCreateInfo.storageTextureCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, storageTextureCount), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, storageTextureCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, storageBufferCount) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, storageBufferCount)) {
        SDL_Log("SDL_GPUShaderCreateInfo.storageBufferCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, storageBufferCount), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, storageBufferCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, uniformBufferCount) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, uniformBufferCount)) {
        SDL_Log("SDL_GPUShaderCreateInfo.uniformBufferCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, uniformBufferCount), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, uniformBufferCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, props) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUShaderCreateInfo.props has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, props));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUShaderCreateInfo.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUShaderCreateInfo_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUTextureCreateInfo */

    if (sizeof(SDL_GPUTextureCreateInfo_pack8) != sizeof(SDL_GPUTextureCreateInfo_pack1)) {
        SDL_Log("SDL_GPUTextureCreateInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTextureCreateInfo_pack1), (int)sizeof(SDL_GPUTextureCreateInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, type) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, type)) {
        SDL_Log("SDL_GPUTextureCreateInfo.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, type), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, format) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, format)) {
        SDL_Log("SDL_GPUTextureCreateInfo.format has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, format), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, usageFlags) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, usageFlags)) {
        SDL_Log("SDL_GPUTextureCreateInfo.usageFlags has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, usageFlags), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, usageFlags));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, width) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, width)) {
        SDL_Log("SDL_GPUTextureCreateInfo.width has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, width), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, width));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, height) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, height)) {
        SDL_Log("SDL_GPUTextureCreateInfo.height has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, height), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, height));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, layerCountOrDepth) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, layerCountOrDepth)) {
        SDL_Log("SDL_GPUTextureCreateInfo.layerCountOrDepth has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, layerCountOrDepth), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, layerCountOrDepth));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, levelCount) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, levelCount)) {
        SDL_Log("SDL_GPUTextureCreateInfo.levelCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, levelCount), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, levelCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, sampleCount) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, sampleCount)) {
        SDL_Log("SDL_GPUTextureCreateInfo.sampleCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, sampleCount), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, sampleCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, props) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUTextureCreateInfo.props has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack8, props));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUTextureCreateInfo_pack4) != sizeof(SDL_GPUTextureCreateInfo_pack1)) {
        SDL_Log("SDL_GPUTextureCreateInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTextureCreateInfo_pack1), (int)sizeof(SDL_GPUTextureCreateInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, type) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, type)) {
        SDL_Log("SDL_GPUTextureCreateInfo.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, type), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, format) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, format)) {
        SDL_Log("SDL_GPUTextureCreateInfo.format has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, format), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, usageFlags) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, usageFlags)) {
        SDL_Log("SDL_GPUTextureCreateInfo.usageFlags has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, usageFlags), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, usageFlags));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, width) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, width)) {
        SDL_Log("SDL_GPUTextureCreateInfo.width has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, width), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, width));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, height) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, height)) {
        SDL_Log("SDL_GPUTextureCreateInfo.height has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, height), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, height));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, layerCountOrDepth) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, layerCountOrDepth)) {
        SDL_Log("SDL_GPUTextureCreateInfo.layerCountOrDepth has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, layerCountOrDepth), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, layerCountOrDepth));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, levelCount) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, levelCount)) {
        SDL_Log("SDL_GPUTextureCreateInfo.levelCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, levelCount), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, levelCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, sampleCount) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, sampleCount)) {
        SDL_Log("SDL_GPUTextureCreateInfo.sampleCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, sampleCount), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, sampleCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, props) != SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUTextureCreateInfo.props has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUTextureCreateInfo_pack4, props));
        result = SDL_FALSE;
    }

    /* SDL_GPUBufferCreateInfo */

    if (sizeof(SDL_GPUBufferCreateInfo_pack8) != sizeof(SDL_GPUBufferCreateInfo_pack1)) {
        SDL_Log("SDL_GPUBufferCreateInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUBufferCreateInfo_pack1), (int)sizeof(SDL_GPUBufferCreateInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack8, usageFlags) != SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, usageFlags)) {
        SDL_Log("SDL_GPUBufferCreateInfo.usageFlags has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, usageFlags), SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack8, usageFlags));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack8, sizeInBytes) != SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, sizeInBytes)) {
        SDL_Log("SDL_GPUBufferCreateInfo.sizeInBytes has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, sizeInBytes), SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack8, sizeInBytes));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack8, props) != SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUBufferCreateInfo.props has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack8, props));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUBufferCreateInfo_pack4) != sizeof(SDL_GPUBufferCreateInfo_pack1)) {
        SDL_Log("SDL_GPUBufferCreateInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUBufferCreateInfo_pack1), (int)sizeof(SDL_GPUBufferCreateInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack4, usageFlags) != SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, usageFlags)) {
        SDL_Log("SDL_GPUBufferCreateInfo.usageFlags has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, usageFlags), SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack4, usageFlags));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack4, sizeInBytes) != SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, sizeInBytes)) {
        SDL_Log("SDL_GPUBufferCreateInfo.sizeInBytes has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, sizeInBytes), SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack4, sizeInBytes));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack4, props) != SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUBufferCreateInfo.props has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUBufferCreateInfo_pack4, props));
        result = SDL_FALSE;
    }

    /* SDL_GPUTransferBufferCreateInfo */

    if (sizeof(SDL_GPUTransferBufferCreateInfo_pack8) != sizeof(SDL_GPUTransferBufferCreateInfo_pack1)) {
        SDL_Log("SDL_GPUTransferBufferCreateInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTransferBufferCreateInfo_pack1), (int)sizeof(SDL_GPUTransferBufferCreateInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack8, usage) != SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, usage)) {
        SDL_Log("SDL_GPUTransferBufferCreateInfo.usage has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, usage), SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack8, usage));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack8, sizeInBytes) != SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, sizeInBytes)) {
        SDL_Log("SDL_GPUTransferBufferCreateInfo.sizeInBytes has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, sizeInBytes), SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack8, sizeInBytes));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack8, props) != SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUTransferBufferCreateInfo.props has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack8, props));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUTransferBufferCreateInfo_pack4) != sizeof(SDL_GPUTransferBufferCreateInfo_pack1)) {
        SDL_Log("SDL_GPUTransferBufferCreateInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTransferBufferCreateInfo_pack1), (int)sizeof(SDL_GPUTransferBufferCreateInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack4, usage) != SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, usage)) {
        SDL_Log("SDL_GPUTransferBufferCreateInfo.usage has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, usage), SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack4, usage));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack4, sizeInBytes) != SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, sizeInBytes)) {
        SDL_Log("SDL_GPUTransferBufferCreateInfo.sizeInBytes has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, sizeInBytes), SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack4, sizeInBytes));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack4, props) != SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUTransferBufferCreateInfo.props has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUTransferBufferCreateInfo_pack4, props));
        result = SDL_FALSE;
    }

    /* SDL_GPURasterizerState */

    if (sizeof(SDL_GPURasterizerState_pack8) != sizeof(SDL_GPURasterizerState_pack1)) {
        SDL_Log("SDL_GPURasterizerState has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPURasterizerState_pack1), (int)sizeof(SDL_GPURasterizerState_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack8, fillMode) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, fillMode)) {
        SDL_Log("SDL_GPURasterizerState.fillMode has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, fillMode), SDL_OFFSETOF(SDL_GPURasterizerState_pack8, fillMode));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack8, cullMode) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, cullMode)) {
        SDL_Log("SDL_GPURasterizerState.cullMode has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, cullMode), SDL_OFFSETOF(SDL_GPURasterizerState_pack8, cullMode));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack8, frontFace) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, frontFace)) {
        SDL_Log("SDL_GPURasterizerState.frontFace has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, frontFace), SDL_OFFSETOF(SDL_GPURasterizerState_pack8, frontFace));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack8, depthBiasEnable) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasEnable)) {
        SDL_Log("SDL_GPURasterizerState.depthBiasEnable has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasEnable), SDL_OFFSETOF(SDL_GPURasterizerState_pack8, depthBiasEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, padding8[0])) {
        SDL_Log("SDL_GPURasterizerState.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPURasterizerState_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack8, depthBiasConstantFactor) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasConstantFactor)) {
        SDL_Log("SDL_GPURasterizerState.depthBiasConstantFactor has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasConstantFactor), SDL_OFFSETOF(SDL_GPURasterizerState_pack8, depthBiasConstantFactor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack8, depthBiasClamp) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasClamp)) {
        SDL_Log("SDL_GPURasterizerState.depthBiasClamp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasClamp), SDL_OFFSETOF(SDL_GPURasterizerState_pack8, depthBiasClamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack8, depthBiasSlopeFactor) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasSlopeFactor)) {
        SDL_Log("SDL_GPURasterizerState.depthBiasSlopeFactor has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasSlopeFactor), SDL_OFFSETOF(SDL_GPURasterizerState_pack8, depthBiasSlopeFactor));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPURasterizerState_pack4) != sizeof(SDL_GPURasterizerState_pack1)) {
        SDL_Log("SDL_GPURasterizerState has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPURasterizerState_pack1), (int)sizeof(SDL_GPURasterizerState_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack4, fillMode) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, fillMode)) {
        SDL_Log("SDL_GPURasterizerState.fillMode has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, fillMode), SDL_OFFSETOF(SDL_GPURasterizerState_pack4, fillMode));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack4, cullMode) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, cullMode)) {
        SDL_Log("SDL_GPURasterizerState.cullMode has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, cullMode), SDL_OFFSETOF(SDL_GPURasterizerState_pack4, cullMode));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack4, frontFace) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, frontFace)) {
        SDL_Log("SDL_GPURasterizerState.frontFace has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, frontFace), SDL_OFFSETOF(SDL_GPURasterizerState_pack4, frontFace));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack4, depthBiasEnable) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasEnable)) {
        SDL_Log("SDL_GPURasterizerState.depthBiasEnable has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasEnable), SDL_OFFSETOF(SDL_GPURasterizerState_pack4, depthBiasEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, padding8[0])) {
        SDL_Log("SDL_GPURasterizerState.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPURasterizerState_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack4, depthBiasConstantFactor) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasConstantFactor)) {
        SDL_Log("SDL_GPURasterizerState.depthBiasConstantFactor has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasConstantFactor), SDL_OFFSETOF(SDL_GPURasterizerState_pack4, depthBiasConstantFactor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack4, depthBiasClamp) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasClamp)) {
        SDL_Log("SDL_GPURasterizerState.depthBiasClamp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasClamp), SDL_OFFSETOF(SDL_GPURasterizerState_pack4, depthBiasClamp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPURasterizerState_pack4, depthBiasSlopeFactor) != SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasSlopeFactor)) {
        SDL_Log("SDL_GPURasterizerState.depthBiasSlopeFactor has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPURasterizerState_pack1, depthBiasSlopeFactor), SDL_OFFSETOF(SDL_GPURasterizerState_pack4, depthBiasSlopeFactor));
        result = SDL_FALSE;
    }

    /* SDL_GPUMultisampleState */

    if (sizeof(SDL_GPUMultisampleState_pack8) != sizeof(SDL_GPUMultisampleState_pack1)) {
        SDL_Log("SDL_GPUMultisampleState has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUMultisampleState_pack1), (int)sizeof(SDL_GPUMultisampleState_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUMultisampleState_pack8, sampleCount) != SDL_OFFSETOF(SDL_GPUMultisampleState_pack1, sampleCount)) {
        SDL_Log("SDL_GPUMultisampleState.sampleCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUMultisampleState_pack1, sampleCount), SDL_OFFSETOF(SDL_GPUMultisampleState_pack8, sampleCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUMultisampleState_pack8, sampleMask) != SDL_OFFSETOF(SDL_GPUMultisampleState_pack1, sampleMask)) {
        SDL_Log("SDL_GPUMultisampleState.sampleMask has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUMultisampleState_pack1, sampleMask), SDL_OFFSETOF(SDL_GPUMultisampleState_pack8, sampleMask));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUMultisampleState_pack4) != sizeof(SDL_GPUMultisampleState_pack1)) {
        SDL_Log("SDL_GPUMultisampleState has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUMultisampleState_pack1), (int)sizeof(SDL_GPUMultisampleState_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUMultisampleState_pack4, sampleCount) != SDL_OFFSETOF(SDL_GPUMultisampleState_pack1, sampleCount)) {
        SDL_Log("SDL_GPUMultisampleState.sampleCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUMultisampleState_pack1, sampleCount), SDL_OFFSETOF(SDL_GPUMultisampleState_pack4, sampleCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUMultisampleState_pack4, sampleMask) != SDL_OFFSETOF(SDL_GPUMultisampleState_pack1, sampleMask)) {
        SDL_Log("SDL_GPUMultisampleState.sampleMask has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUMultisampleState_pack1, sampleMask), SDL_OFFSETOF(SDL_GPUMultisampleState_pack4, sampleMask));
        result = SDL_FALSE;
    }

    /* SDL_GPUDepthStencilState */

    if (sizeof(SDL_GPUDepthStencilState_pack8) != sizeof(SDL_GPUDepthStencilState_pack1)) {
        SDL_Log("SDL_GPUDepthStencilState has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUDepthStencilState_pack1), (int)sizeof(SDL_GPUDepthStencilState_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, depthTestEnable) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, depthTestEnable)) {
        SDL_Log("SDL_GPUDepthStencilState.depthTestEnable has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, depthTestEnable), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, depthTestEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, depthWriteEnable) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, depthWriteEnable)) {
        SDL_Log("SDL_GPUDepthStencilState.depthWriteEnable has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, depthWriteEnable), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, depthWriteEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, stencilTestEnable) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, stencilTestEnable)) {
        SDL_Log("SDL_GPUDepthStencilState.stencilTestEnable has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, stencilTestEnable), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, stencilTestEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, padding8[0])) {
        SDL_Log("SDL_GPUDepthStencilState.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, compareOp) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, compareOp)) {
        SDL_Log("SDL_GPUDepthStencilState.compareOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, compareOp), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, compareOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, backStencilState) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, backStencilState)) {
        SDL_Log("SDL_GPUDepthStencilState.backStencilState has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, backStencilState), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, backStencilState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, frontStencilState) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, frontStencilState)) {
        SDL_Log("SDL_GPUDepthStencilState.frontStencilState has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, frontStencilState), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, frontStencilState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, compareMask) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, compareMask)) {
        SDL_Log("SDL_GPUDepthStencilState.compareMask has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, compareMask), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, compareMask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, writeMask) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, writeMask)) {
        SDL_Log("SDL_GPUDepthStencilState.writeMask has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, writeMask), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, writeMask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, reference) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, reference)) {
        SDL_Log("SDL_GPUDepthStencilState.reference has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, reference), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, reference));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, padding_end)) {
        SDL_Log("SDL_GPUDepthStencilState.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, padding_end), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUDepthStencilState_pack4) != sizeof(SDL_GPUDepthStencilState_pack1)) {
        SDL_Log("SDL_GPUDepthStencilState has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUDepthStencilState_pack1), (int)sizeof(SDL_GPUDepthStencilState_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, depthTestEnable) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, depthTestEnable)) {
        SDL_Log("SDL_GPUDepthStencilState.depthTestEnable has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, depthTestEnable), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, depthTestEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, depthWriteEnable) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, depthWriteEnable)) {
        SDL_Log("SDL_GPUDepthStencilState.depthWriteEnable has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, depthWriteEnable), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, depthWriteEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, stencilTestEnable) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, stencilTestEnable)) {
        SDL_Log("SDL_GPUDepthStencilState.stencilTestEnable has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, stencilTestEnable), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, stencilTestEnable));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, padding8[0])) {
        SDL_Log("SDL_GPUDepthStencilState.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, compareOp) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, compareOp)) {
        SDL_Log("SDL_GPUDepthStencilState.compareOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, compareOp), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, compareOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, backStencilState) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, backStencilState)) {
        SDL_Log("SDL_GPUDepthStencilState.backStencilState has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, backStencilState), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, backStencilState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, frontStencilState) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, frontStencilState)) {
        SDL_Log("SDL_GPUDepthStencilState.frontStencilState has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, frontStencilState), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, frontStencilState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, compareMask) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, compareMask)) {
        SDL_Log("SDL_GPUDepthStencilState.compareMask has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, compareMask), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, compareMask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, writeMask) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, writeMask)) {
        SDL_Log("SDL_GPUDepthStencilState.writeMask has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, writeMask), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, writeMask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, reference) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, reference)) {
        SDL_Log("SDL_GPUDepthStencilState.reference has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, reference), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, reference));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, padding_end)) {
        SDL_Log("SDL_GPUDepthStencilState.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilState_pack1, padding_end), SDL_OFFSETOF(SDL_GPUDepthStencilState_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUColorAttachmentDescription */

    if (sizeof(SDL_GPUColorAttachmentDescription_pack8) != sizeof(SDL_GPUColorAttachmentDescription_pack1)) {
        SDL_Log("SDL_GPUColorAttachmentDescription has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUColorAttachmentDescription_pack1), (int)sizeof(SDL_GPUColorAttachmentDescription_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack8, format) != SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack1, format)) {
        SDL_Log("SDL_GPUColorAttachmentDescription.format has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack1, format), SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack8, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack8, blendState) != SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack1, blendState)) {
        SDL_Log("SDL_GPUColorAttachmentDescription.blendState has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack1, blendState), SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack8, blendState));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUColorAttachmentDescription_pack4) != sizeof(SDL_GPUColorAttachmentDescription_pack1)) {
        SDL_Log("SDL_GPUColorAttachmentDescription has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUColorAttachmentDescription_pack1), (int)sizeof(SDL_GPUColorAttachmentDescription_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack4, format) != SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack1, format)) {
        SDL_Log("SDL_GPUColorAttachmentDescription.format has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack1, format), SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack4, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack4, blendState) != SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack1, blendState)) {
        SDL_Log("SDL_GPUColorAttachmentDescription.blendState has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack1, blendState), SDL_OFFSETOF(SDL_GPUColorAttachmentDescription_pack4, blendState));
        result = SDL_FALSE;
    }

    /* SDL_GPUGraphicsPipelineAttachmentInfo */

    if (sizeof(SDL_GPUGraphicsPipelineAttachmentInfo_pack8) != sizeof(SDL_GPUGraphicsPipelineAttachmentInfo_pack1)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUGraphicsPipelineAttachmentInfo_pack1), (int)sizeof(SDL_GPUGraphicsPipelineAttachmentInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, colorAttachmentDescriptions) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, colorAttachmentDescriptions)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.colorAttachmentDescriptions has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, colorAttachmentDescriptions), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, colorAttachmentDescriptions));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, colorAttachmentCount) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, colorAttachmentCount)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.colorAttachmentCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, colorAttachmentCount), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, colorAttachmentCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, hasDepthStencilAttachment) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, hasDepthStencilAttachment)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.hasDepthStencilAttachment has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, hasDepthStencilAttachment), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, hasDepthStencilAttachment));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, padding8[0])) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, depthStencilFormat) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, depthStencilFormat)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.depthStencilFormat has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, depthStencilFormat), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, depthStencilFormat));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUGraphicsPipelineAttachmentInfo_pack4) != sizeof(SDL_GPUGraphicsPipelineAttachmentInfo_pack1)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUGraphicsPipelineAttachmentInfo_pack1), (int)sizeof(SDL_GPUGraphicsPipelineAttachmentInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, colorAttachmentDescriptions) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, colorAttachmentDescriptions)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.colorAttachmentDescriptions has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, colorAttachmentDescriptions), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, colorAttachmentDescriptions));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, colorAttachmentCount) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, colorAttachmentCount)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.colorAttachmentCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, colorAttachmentCount), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, colorAttachmentCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, hasDepthStencilAttachment) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, hasDepthStencilAttachment)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.hasDepthStencilAttachment has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, hasDepthStencilAttachment), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, hasDepthStencilAttachment));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, padding8[0])) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, depthStencilFormat) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, depthStencilFormat)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.depthStencilFormat has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, depthStencilFormat), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, depthStencilFormat));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUGraphicsPipelineAttachmentInfo.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUGraphicsPipelineAttachmentInfo_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUGraphicsPipelineCreateInfo */

    if (sizeof(SDL_GPUGraphicsPipelineCreateInfo_pack8) != sizeof(SDL_GPUGraphicsPipelineCreateInfo_pack1)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUGraphicsPipelineCreateInfo_pack1), (int)sizeof(SDL_GPUGraphicsPipelineCreateInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, vertexShader) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, vertexShader)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.vertexShader has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, vertexShader), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, vertexShader));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, fragmentShader) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, fragmentShader)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.fragmentShader has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, fragmentShader), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, fragmentShader));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, vertexInputState) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, vertexInputState)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.vertexInputState has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, vertexInputState), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, vertexInputState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, primitiveType) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, primitiveType)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.primitiveType has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, primitiveType), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, primitiveType));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, rasterizerState) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, rasterizerState)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.rasterizerState has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, rasterizerState), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, rasterizerState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, multisampleState) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, multisampleState)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.multisampleState has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, multisampleState), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, multisampleState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, depthStencilState) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, depthStencilState)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.depthStencilState has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, depthStencilState), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, depthStencilState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, padding32) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, padding32)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.padding32 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, padding32), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, attachmentInfo) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, attachmentInfo)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.attachmentInfo has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, attachmentInfo), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, attachmentInfo));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, blendConstants[0]) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, blendConstants[0])) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.blendConstants[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, blendConstants[0]), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, blendConstants[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, props) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.props has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, props));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUGraphicsPipelineCreateInfo_pack4) != sizeof(SDL_GPUGraphicsPipelineCreateInfo_pack1)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUGraphicsPipelineCreateInfo_pack1), (int)sizeof(SDL_GPUGraphicsPipelineCreateInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, vertexShader) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, vertexShader)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.vertexShader has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, vertexShader), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, vertexShader));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, fragmentShader) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, fragmentShader)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.fragmentShader has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, fragmentShader), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, fragmentShader));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, vertexInputState) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, vertexInputState)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.vertexInputState has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, vertexInputState), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, vertexInputState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, primitiveType) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, primitiveType)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.primitiveType has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, primitiveType), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, primitiveType));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, rasterizerState) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, rasterizerState)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.rasterizerState has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, rasterizerState), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, rasterizerState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, multisampleState) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, multisampleState)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.multisampleState has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, multisampleState), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, multisampleState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, depthStencilState) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, depthStencilState)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.depthStencilState has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, depthStencilState), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, depthStencilState));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, padding32) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, padding32)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.padding32 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, padding32), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, attachmentInfo) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, attachmentInfo)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.attachmentInfo has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, attachmentInfo), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, attachmentInfo));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, blendConstants[0]) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, blendConstants[0])) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.blendConstants[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, blendConstants[0]), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, blendConstants[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, props) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.props has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, props));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUGraphicsPipelineCreateInfo.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUGraphicsPipelineCreateInfo_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUComputePipelineCreateInfo */

    if (sizeof(SDL_GPUComputePipelineCreateInfo_pack8) != sizeof(SDL_GPUComputePipelineCreateInfo_pack1)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUComputePipelineCreateInfo_pack1), (int)sizeof(SDL_GPUComputePipelineCreateInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, codeSize) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, codeSize)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.codeSize has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, codeSize), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, codeSize));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, code) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, code)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.code has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, code), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, code));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, entryPointName) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, entryPointName)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.entryPointName has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, entryPointName), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, entryPointName));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, format) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, format)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.format has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, format), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, readOnlyStorageTextureCount) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, readOnlyStorageTextureCount)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.readOnlyStorageTextureCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, readOnlyStorageTextureCount), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, readOnlyStorageTextureCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, readOnlyStorageBufferCount) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, readOnlyStorageBufferCount)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.readOnlyStorageBufferCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, readOnlyStorageBufferCount), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, readOnlyStorageBufferCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, writeOnlyStorageTextureCount) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, writeOnlyStorageTextureCount)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.writeOnlyStorageTextureCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, writeOnlyStorageTextureCount), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, writeOnlyStorageTextureCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, writeOnlyStorageBufferCount) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, writeOnlyStorageBufferCount)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.writeOnlyStorageBufferCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, writeOnlyStorageBufferCount), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, writeOnlyStorageBufferCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, uniformBufferCount) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, uniformBufferCount)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.uniformBufferCount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, uniformBufferCount), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, uniformBufferCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, threadCountX) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountX)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.threadCountX has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountX), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, threadCountX));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, threadCountY) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountY)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.threadCountY has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountY), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, threadCountY));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, threadCountZ) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountZ)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.threadCountZ has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountZ), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, threadCountZ));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, props) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.props has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack8, props));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUComputePipelineCreateInfo_pack4) != sizeof(SDL_GPUComputePipelineCreateInfo_pack1)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUComputePipelineCreateInfo_pack1), (int)sizeof(SDL_GPUComputePipelineCreateInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, codeSize) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, codeSize)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.codeSize has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, codeSize), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, codeSize));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, code) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, code)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.code has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, code), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, code));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, entryPointName) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, entryPointName)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.entryPointName has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, entryPointName), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, entryPointName));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, format) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, format)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.format has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, format), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, readOnlyStorageTextureCount) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, readOnlyStorageTextureCount)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.readOnlyStorageTextureCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, readOnlyStorageTextureCount), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, readOnlyStorageTextureCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, readOnlyStorageBufferCount) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, readOnlyStorageBufferCount)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.readOnlyStorageBufferCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, readOnlyStorageBufferCount), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, readOnlyStorageBufferCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, writeOnlyStorageTextureCount) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, writeOnlyStorageTextureCount)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.writeOnlyStorageTextureCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, writeOnlyStorageTextureCount), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, writeOnlyStorageTextureCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, writeOnlyStorageBufferCount) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, writeOnlyStorageBufferCount)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.writeOnlyStorageBufferCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, writeOnlyStorageBufferCount), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, writeOnlyStorageBufferCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, uniformBufferCount) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, uniformBufferCount)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.uniformBufferCount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, uniformBufferCount), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, uniformBufferCount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, threadCountX) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountX)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.threadCountX has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountX), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, threadCountX));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, threadCountY) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountY)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.threadCountY has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountY), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, threadCountY));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, threadCountZ) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountZ)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.threadCountZ has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, threadCountZ), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, threadCountZ));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, props) != SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, props)) {
        SDL_Log("SDL_GPUComputePipelineCreateInfo.props has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack1, props), SDL_OFFSETOF(SDL_GPUComputePipelineCreateInfo_pack4, props));
        result = SDL_FALSE;
    }

    /* SDL_GPUColorAttachmentInfo */

    if (sizeof(SDL_GPUColorAttachmentInfo_pack8) != sizeof(SDL_GPUColorAttachmentInfo_pack1)) {
        SDL_Log("SDL_GPUColorAttachmentInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUColorAttachmentInfo_pack1), (int)sizeof(SDL_GPUColorAttachmentInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, texture) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, texture)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.texture has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, texture), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, mipLevel) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, mipLevel)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.mipLevel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, mipLevel), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, mipLevel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, layerOrDepthPlane) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, layerOrDepthPlane)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.layerOrDepthPlane has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, layerOrDepthPlane), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, layerOrDepthPlane));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, clearColor) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, clearColor)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.clearColor has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, clearColor), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, clearColor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, loadOp) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, loadOp)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.loadOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, loadOp), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, loadOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, storeOp) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, storeOp)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.storeOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, storeOp), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, storeOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, cycle) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, cycle)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.cycle has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, cycle), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, cycle));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, padding8[0])) {
        SDL_Log("SDL_GPUColorAttachmentInfo.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUColorAttachmentInfo_pack4) != sizeof(SDL_GPUColorAttachmentInfo_pack1)) {
        SDL_Log("SDL_GPUColorAttachmentInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUColorAttachmentInfo_pack1), (int)sizeof(SDL_GPUColorAttachmentInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, texture) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, texture)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.texture has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, texture), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, mipLevel) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, mipLevel)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.mipLevel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, mipLevel), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, mipLevel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, layerOrDepthPlane) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, layerOrDepthPlane)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.layerOrDepthPlane has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, layerOrDepthPlane), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, layerOrDepthPlane));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, clearColor) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, clearColor)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.clearColor has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, clearColor), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, clearColor));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, loadOp) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, loadOp)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.loadOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, loadOp), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, loadOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, storeOp) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, storeOp)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.storeOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, storeOp), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, storeOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, cycle) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, cycle)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.cycle has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, cycle), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, cycle));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, padding8[0])) {
        SDL_Log("SDL_GPUColorAttachmentInfo.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUColorAttachmentInfo.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUColorAttachmentInfo_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUDepthStencilAttachmentInfo */

    if (sizeof(SDL_GPUDepthStencilAttachmentInfo_pack8) != sizeof(SDL_GPUDepthStencilAttachmentInfo_pack1)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUDepthStencilAttachmentInfo_pack1), (int)sizeof(SDL_GPUDepthStencilAttachmentInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, texture) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, texture)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.texture has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, texture), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, depthStencilClearValue) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, depthStencilClearValue)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.depthStencilClearValue has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, depthStencilClearValue), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, depthStencilClearValue));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, loadOp) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, loadOp)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.loadOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, loadOp), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, loadOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, storeOp) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, storeOp)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.storeOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, storeOp), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, storeOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, stencilLoadOp) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, stencilLoadOp)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.stencilLoadOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, stencilLoadOp), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, stencilLoadOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, stencilStoreOp) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, stencilStoreOp)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.stencilStoreOp has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, stencilStoreOp), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, stencilStoreOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, cycle) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, cycle)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.cycle has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, cycle), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, cycle));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, padding8[0])) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUDepthStencilAttachmentInfo_pack4) != sizeof(SDL_GPUDepthStencilAttachmentInfo_pack1)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUDepthStencilAttachmentInfo_pack1), (int)sizeof(SDL_GPUDepthStencilAttachmentInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, texture) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, texture)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.texture has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, texture), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, depthStencilClearValue) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, depthStencilClearValue)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.depthStencilClearValue has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, depthStencilClearValue), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, depthStencilClearValue));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, loadOp) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, loadOp)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.loadOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, loadOp), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, loadOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, storeOp) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, storeOp)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.storeOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, storeOp), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, storeOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, stencilLoadOp) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, stencilLoadOp)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.stencilLoadOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, stencilLoadOp), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, stencilLoadOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, stencilStoreOp) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, stencilStoreOp)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.stencilStoreOp has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, stencilStoreOp), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, stencilStoreOp));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, cycle) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, cycle)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.cycle has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, cycle), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, cycle));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, padding8[0])) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, padding_end)) {
        SDL_Log("SDL_GPUDepthStencilAttachmentInfo.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack1, padding_end), SDL_OFFSETOF(SDL_GPUDepthStencilAttachmentInfo_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUBufferBinding */

    if (sizeof(SDL_GPUBufferBinding_pack8) != sizeof(SDL_GPUBufferBinding_pack1)) {
        SDL_Log("SDL_GPUBufferBinding has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUBufferBinding_pack1), (int)sizeof(SDL_GPUBufferBinding_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferBinding_pack8, buffer) != SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, buffer)) {
        SDL_Log("SDL_GPUBufferBinding.buffer has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, buffer), SDL_OFFSETOF(SDL_GPUBufferBinding_pack8, buffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferBinding_pack8, offset) != SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, offset)) {
        SDL_Log("SDL_GPUBufferBinding.offset has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, offset), SDL_OFFSETOF(SDL_GPUBufferBinding_pack8, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferBinding_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, padding_end)) {
        SDL_Log("SDL_GPUBufferBinding.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, padding_end), SDL_OFFSETOF(SDL_GPUBufferBinding_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUBufferBinding_pack4) != sizeof(SDL_GPUBufferBinding_pack1)) {
        SDL_Log("SDL_GPUBufferBinding has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUBufferBinding_pack1), (int)sizeof(SDL_GPUBufferBinding_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferBinding_pack4, buffer) != SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, buffer)) {
        SDL_Log("SDL_GPUBufferBinding.buffer has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, buffer), SDL_OFFSETOF(SDL_GPUBufferBinding_pack4, buffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferBinding_pack4, offset) != SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, offset)) {
        SDL_Log("SDL_GPUBufferBinding.offset has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, offset), SDL_OFFSETOF(SDL_GPUBufferBinding_pack4, offset));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUBufferBinding_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, padding_end)) {
        SDL_Log("SDL_GPUBufferBinding.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUBufferBinding_pack1, padding_end), SDL_OFFSETOF(SDL_GPUBufferBinding_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUTextureSamplerBinding */

    if (sizeof(SDL_GPUTextureSamplerBinding_pack8) != sizeof(SDL_GPUTextureSamplerBinding_pack1)) {
        SDL_Log("SDL_GPUTextureSamplerBinding has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTextureSamplerBinding_pack1), (int)sizeof(SDL_GPUTextureSamplerBinding_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack8, texture) != SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack1, texture)) {
        SDL_Log("SDL_GPUTextureSamplerBinding.texture has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack1, texture), SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack8, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack8, sampler) != SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack1, sampler)) {
        SDL_Log("SDL_GPUTextureSamplerBinding.sampler has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack1, sampler), SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack8, sampler));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUTextureSamplerBinding_pack4) != sizeof(SDL_GPUTextureSamplerBinding_pack1)) {
        SDL_Log("SDL_GPUTextureSamplerBinding has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUTextureSamplerBinding_pack1), (int)sizeof(SDL_GPUTextureSamplerBinding_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack4, texture) != SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack1, texture)) {
        SDL_Log("SDL_GPUTextureSamplerBinding.texture has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack1, texture), SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack4, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack4, sampler) != SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack1, sampler)) {
        SDL_Log("SDL_GPUTextureSamplerBinding.sampler has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack1, sampler), SDL_OFFSETOF(SDL_GPUTextureSamplerBinding_pack4, sampler));
        result = SDL_FALSE;
    }

    /* SDL_GPUStorageBufferWriteOnlyBinding */

    if (sizeof(SDL_GPUStorageBufferWriteOnlyBinding_pack8) != sizeof(SDL_GPUStorageBufferWriteOnlyBinding_pack1)) {
        SDL_Log("SDL_GPUStorageBufferWriteOnlyBinding has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUStorageBufferWriteOnlyBinding_pack1), (int)sizeof(SDL_GPUStorageBufferWriteOnlyBinding_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack8, buffer) != SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, buffer)) {
        SDL_Log("SDL_GPUStorageBufferWriteOnlyBinding.buffer has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, buffer), SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack8, buffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack8, cycle) != SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, cycle)) {
        SDL_Log("SDL_GPUStorageBufferWriteOnlyBinding.cycle has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, cycle), SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack8, cycle));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, padding8[0])) {
        SDL_Log("SDL_GPUStorageBufferWriteOnlyBinding.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, padding_end)) {
        SDL_Log("SDL_GPUStorageBufferWriteOnlyBinding.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, padding_end), SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUStorageBufferWriteOnlyBinding_pack4) != sizeof(SDL_GPUStorageBufferWriteOnlyBinding_pack1)) {
        SDL_Log("SDL_GPUStorageBufferWriteOnlyBinding has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUStorageBufferWriteOnlyBinding_pack1), (int)sizeof(SDL_GPUStorageBufferWriteOnlyBinding_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack4, buffer) != SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, buffer)) {
        SDL_Log("SDL_GPUStorageBufferWriteOnlyBinding.buffer has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, buffer), SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack4, buffer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack4, cycle) != SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, cycle)) {
        SDL_Log("SDL_GPUStorageBufferWriteOnlyBinding.cycle has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, cycle), SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack4, cycle));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, padding8[0])) {
        SDL_Log("SDL_GPUStorageBufferWriteOnlyBinding.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, padding_end)) {
        SDL_Log("SDL_GPUStorageBufferWriteOnlyBinding.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack1, padding_end), SDL_OFFSETOF(SDL_GPUStorageBufferWriteOnlyBinding_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_GPUStorageTextureWriteOnlyBinding */

    if (sizeof(SDL_GPUStorageTextureWriteOnlyBinding_pack8) != sizeof(SDL_GPUStorageTextureWriteOnlyBinding_pack1)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUStorageTextureWriteOnlyBinding_pack1), (int)sizeof(SDL_GPUStorageTextureWriteOnlyBinding_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, texture) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, texture)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.texture has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, texture), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, mipLevel) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, mipLevel)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.mipLevel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, mipLevel), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, mipLevel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, layer) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, layer)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.layer has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, layer), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, layer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, cycle) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, cycle)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.cycle has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, cycle), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, cycle));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, padding8[0]) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, padding8[0])) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.padding8[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, padding_end) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, padding_end)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, padding_end), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_GPUStorageTextureWriteOnlyBinding_pack4) != sizeof(SDL_GPUStorageTextureWriteOnlyBinding_pack1)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_GPUStorageTextureWriteOnlyBinding_pack1), (int)sizeof(SDL_GPUStorageTextureWriteOnlyBinding_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, texture) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, texture)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.texture has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, texture), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, texture));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, mipLevel) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, mipLevel)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.mipLevel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, mipLevel), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, mipLevel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, layer) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, layer)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.layer has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, layer), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, layer));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, cycle) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, cycle)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.cycle has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, cycle), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, cycle));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, padding8[0]) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, padding8[0])) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.padding8[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, padding8[0]), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, padding8[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, padding_end) != SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, padding_end)) {
        SDL_Log("SDL_GPUStorageTextureWriteOnlyBinding.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack1, padding_end), SDL_OFFSETOF(SDL_GPUStorageTextureWriteOnlyBinding_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_Surface */

    if (sizeof(SDL_Surface_pack8) != sizeof(SDL_Surface_pack1)) {
        SDL_Log("SDL_Surface has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Surface_pack1), (int)sizeof(SDL_Surface_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack8, flags) != SDL_OFFSETOF(SDL_Surface_pack1, flags)) {
        SDL_Log("SDL_Surface.flags has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, flags), SDL_OFFSETOF(SDL_Surface_pack8, flags));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack8, format) != SDL_OFFSETOF(SDL_Surface_pack1, format)) {
        SDL_Log("SDL_Surface.format has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, format), SDL_OFFSETOF(SDL_Surface_pack8, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack8, h) != SDL_OFFSETOF(SDL_Surface_pack1, h)) {
        SDL_Log("SDL_Surface.h has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, h), SDL_OFFSETOF(SDL_Surface_pack8, h));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack8, pitch) != SDL_OFFSETOF(SDL_Surface_pack1, pitch)) {
        SDL_Log("SDL_Surface.pitch has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, pitch), SDL_OFFSETOF(SDL_Surface_pack8, pitch));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack8, refcount) != SDL_OFFSETOF(SDL_Surface_pack1, refcount)) {
        SDL_Log("SDL_Surface.refcount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, refcount), SDL_OFFSETOF(SDL_Surface_pack8, refcount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack8, pixels) != SDL_OFFSETOF(SDL_Surface_pack1, pixels)) {
        SDL_Log("SDL_Surface.pixels has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, pixels), SDL_OFFSETOF(SDL_Surface_pack8, pixels));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack8, internal) != SDL_OFFSETOF(SDL_Surface_pack1, internal)) {
        SDL_Log("SDL_Surface.internal has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, internal), SDL_OFFSETOF(SDL_Surface_pack8, internal));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_Surface_pack4) != sizeof(SDL_Surface_pack1)) {
        SDL_Log("SDL_Surface has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Surface_pack1), (int)sizeof(SDL_Surface_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack4, flags) != SDL_OFFSETOF(SDL_Surface_pack1, flags)) {
        SDL_Log("SDL_Surface.flags has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, flags), SDL_OFFSETOF(SDL_Surface_pack4, flags));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack4, format) != SDL_OFFSETOF(SDL_Surface_pack1, format)) {
        SDL_Log("SDL_Surface.format has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, format), SDL_OFFSETOF(SDL_Surface_pack4, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack4, h) != SDL_OFFSETOF(SDL_Surface_pack1, h)) {
        SDL_Log("SDL_Surface.h has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, h), SDL_OFFSETOF(SDL_Surface_pack4, h));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack4, pitch) != SDL_OFFSETOF(SDL_Surface_pack1, pitch)) {
        SDL_Log("SDL_Surface.pitch has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, pitch), SDL_OFFSETOF(SDL_Surface_pack4, pitch));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack4, refcount) != SDL_OFFSETOF(SDL_Surface_pack1, refcount)) {
        SDL_Log("SDL_Surface.refcount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, refcount), SDL_OFFSETOF(SDL_Surface_pack4, refcount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack4, pixels) != SDL_OFFSETOF(SDL_Surface_pack1, pixels)) {
        SDL_Log("SDL_Surface.pixels has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, pixels), SDL_OFFSETOF(SDL_Surface_pack4, pixels));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Surface_pack4, internal) != SDL_OFFSETOF(SDL_Surface_pack1, internal)) {
        SDL_Log("SDL_Surface.internal has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Surface_pack1, internal), SDL_OFFSETOF(SDL_Surface_pack4, internal));
        result = SDL_FALSE;
    }

    /* SDL_Vertex */

    if (sizeof(SDL_Vertex_pack8) != sizeof(SDL_Vertex_pack1)) {
        SDL_Log("SDL_Vertex has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Vertex_pack1), (int)sizeof(SDL_Vertex_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Vertex_pack8, position) != SDL_OFFSETOF(SDL_Vertex_pack1, position)) {
        SDL_Log("SDL_Vertex.position has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Vertex_pack1, position), SDL_OFFSETOF(SDL_Vertex_pack8, position));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Vertex_pack8, color) != SDL_OFFSETOF(SDL_Vertex_pack1, color)) {
        SDL_Log("SDL_Vertex.color has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Vertex_pack1, color), SDL_OFFSETOF(SDL_Vertex_pack8, color));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Vertex_pack8, tex_coord) != SDL_OFFSETOF(SDL_Vertex_pack1, tex_coord)) {
        SDL_Log("SDL_Vertex.tex_coord has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Vertex_pack1, tex_coord), SDL_OFFSETOF(SDL_Vertex_pack8, tex_coord));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_Vertex_pack4) != sizeof(SDL_Vertex_pack1)) {
        SDL_Log("SDL_Vertex has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Vertex_pack1), (int)sizeof(SDL_Vertex_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Vertex_pack4, position) != SDL_OFFSETOF(SDL_Vertex_pack1, position)) {
        SDL_Log("SDL_Vertex.position has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Vertex_pack1, position), SDL_OFFSETOF(SDL_Vertex_pack4, position));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Vertex_pack4, color) != SDL_OFFSETOF(SDL_Vertex_pack1, color)) {
        SDL_Log("SDL_Vertex.color has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Vertex_pack1, color), SDL_OFFSETOF(SDL_Vertex_pack4, color));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Vertex_pack4, tex_coord) != SDL_OFFSETOF(SDL_Vertex_pack1, tex_coord)) {
        SDL_Log("SDL_Vertex.tex_coord has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Vertex_pack1, tex_coord), SDL_OFFSETOF(SDL_Vertex_pack4, tex_coord));
        result = SDL_FALSE;
    }

    /* SDL_DisplayMode */

    if (sizeof(SDL_DisplayMode_pack8) != sizeof(SDL_DisplayMode_pack1)) {
        SDL_Log("SDL_DisplayMode has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_DisplayMode_pack1), (int)sizeof(SDL_DisplayMode_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack8, displayID) != SDL_OFFSETOF(SDL_DisplayMode_pack1, displayID)) {
        SDL_Log("SDL_DisplayMode.displayID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, displayID), SDL_OFFSETOF(SDL_DisplayMode_pack8, displayID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack8, format) != SDL_OFFSETOF(SDL_DisplayMode_pack1, format)) {
        SDL_Log("SDL_DisplayMode.format has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, format), SDL_OFFSETOF(SDL_DisplayMode_pack8, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack8, w) != SDL_OFFSETOF(SDL_DisplayMode_pack1, w)) {
        SDL_Log("SDL_DisplayMode.w has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, w), SDL_OFFSETOF(SDL_DisplayMode_pack8, w));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack8, h) != SDL_OFFSETOF(SDL_DisplayMode_pack1, h)) {
        SDL_Log("SDL_DisplayMode.h has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, h), SDL_OFFSETOF(SDL_DisplayMode_pack8, h));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack8, pixel_density) != SDL_OFFSETOF(SDL_DisplayMode_pack1, pixel_density)) {
        SDL_Log("SDL_DisplayMode.pixel_density has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, pixel_density), SDL_OFFSETOF(SDL_DisplayMode_pack8, pixel_density));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack8, refresh_rate) != SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate)) {
        SDL_Log("SDL_DisplayMode.refresh_rate has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate), SDL_OFFSETOF(SDL_DisplayMode_pack8, refresh_rate));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack8, refresh_rate_numerator) != SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate_numerator)) {
        SDL_Log("SDL_DisplayMode.refresh_rate_numerator has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate_numerator), SDL_OFFSETOF(SDL_DisplayMode_pack8, refresh_rate_numerator));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack8, refresh_rate_denominator) != SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate_denominator)) {
        SDL_Log("SDL_DisplayMode.refresh_rate_denominator has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate_denominator), SDL_OFFSETOF(SDL_DisplayMode_pack8, refresh_rate_denominator));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack8, internal) != SDL_OFFSETOF(SDL_DisplayMode_pack1, internal)) {
        SDL_Log("SDL_DisplayMode.internal has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, internal), SDL_OFFSETOF(SDL_DisplayMode_pack8, internal));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_DisplayMode_pack4) != sizeof(SDL_DisplayMode_pack1)) {
        SDL_Log("SDL_DisplayMode has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_DisplayMode_pack1), (int)sizeof(SDL_DisplayMode_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack4, displayID) != SDL_OFFSETOF(SDL_DisplayMode_pack1, displayID)) {
        SDL_Log("SDL_DisplayMode.displayID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, displayID), SDL_OFFSETOF(SDL_DisplayMode_pack4, displayID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack4, format) != SDL_OFFSETOF(SDL_DisplayMode_pack1, format)) {
        SDL_Log("SDL_DisplayMode.format has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, format), SDL_OFFSETOF(SDL_DisplayMode_pack4, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack4, w) != SDL_OFFSETOF(SDL_DisplayMode_pack1, w)) {
        SDL_Log("SDL_DisplayMode.w has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, w), SDL_OFFSETOF(SDL_DisplayMode_pack4, w));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack4, h) != SDL_OFFSETOF(SDL_DisplayMode_pack1, h)) {
        SDL_Log("SDL_DisplayMode.h has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, h), SDL_OFFSETOF(SDL_DisplayMode_pack4, h));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack4, pixel_density) != SDL_OFFSETOF(SDL_DisplayMode_pack1, pixel_density)) {
        SDL_Log("SDL_DisplayMode.pixel_density has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, pixel_density), SDL_OFFSETOF(SDL_DisplayMode_pack4, pixel_density));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack4, refresh_rate) != SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate)) {
        SDL_Log("SDL_DisplayMode.refresh_rate has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate), SDL_OFFSETOF(SDL_DisplayMode_pack4, refresh_rate));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack4, refresh_rate_numerator) != SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate_numerator)) {
        SDL_Log("SDL_DisplayMode.refresh_rate_numerator has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate_numerator), SDL_OFFSETOF(SDL_DisplayMode_pack4, refresh_rate_numerator));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack4, refresh_rate_denominator) != SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate_denominator)) {
        SDL_Log("SDL_DisplayMode.refresh_rate_denominator has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, refresh_rate_denominator), SDL_OFFSETOF(SDL_DisplayMode_pack4, refresh_rate_denominator));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_DisplayMode_pack4, internal) != SDL_OFFSETOF(SDL_DisplayMode_pack1, internal)) {
        SDL_Log("SDL_DisplayMode.internal has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_DisplayMode_pack1, internal), SDL_OFFSETOF(SDL_DisplayMode_pack4, internal));
        result = SDL_FALSE;
    }

    /* SDL_hid_device_info */

    if (sizeof(SDL_hid_device_info_pack8) != sizeof(SDL_hid_device_info_pack1)) {
        SDL_Log("SDL_hid_device_info has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_hid_device_info_pack1), (int)sizeof(SDL_hid_device_info_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, path) != SDL_OFFSETOF(SDL_hid_device_info_pack1, path)) {
        SDL_Log("SDL_hid_device_info.path has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, path), SDL_OFFSETOF(SDL_hid_device_info_pack8, path));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, serial_number) != SDL_OFFSETOF(SDL_hid_device_info_pack1, serial_number)) {
        SDL_Log("SDL_hid_device_info.serial_number has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, serial_number), SDL_OFFSETOF(SDL_hid_device_info_pack8, serial_number));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, manufacturer_string) != SDL_OFFSETOF(SDL_hid_device_info_pack1, manufacturer_string)) {
        SDL_Log("SDL_hid_device_info.manufacturer_string has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, manufacturer_string), SDL_OFFSETOF(SDL_hid_device_info_pack8, manufacturer_string));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, product_string) != SDL_OFFSETOF(SDL_hid_device_info_pack1, product_string)) {
        SDL_Log("SDL_hid_device_info.product_string has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, product_string), SDL_OFFSETOF(SDL_hid_device_info_pack8, product_string));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, vendor_id) != SDL_OFFSETOF(SDL_hid_device_info_pack1, vendor_id)) {
        SDL_Log("SDL_hid_device_info.vendor_id has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, vendor_id), SDL_OFFSETOF(SDL_hid_device_info_pack8, vendor_id));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, product_id) != SDL_OFFSETOF(SDL_hid_device_info_pack1, product_id)) {
        SDL_Log("SDL_hid_device_info.product_id has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, product_id), SDL_OFFSETOF(SDL_hid_device_info_pack8, product_id));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, release_number) != SDL_OFFSETOF(SDL_hid_device_info_pack1, release_number)) {
        SDL_Log("SDL_hid_device_info.release_number has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, release_number), SDL_OFFSETOF(SDL_hid_device_info_pack8, release_number));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, usage_page) != SDL_OFFSETOF(SDL_hid_device_info_pack1, usage_page)) {
        SDL_Log("SDL_hid_device_info.usage_page has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, usage_page), SDL_OFFSETOF(SDL_hid_device_info_pack8, usage_page));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, usage) != SDL_OFFSETOF(SDL_hid_device_info_pack1, usage)) {
        SDL_Log("SDL_hid_device_info.usage has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, usage), SDL_OFFSETOF(SDL_hid_device_info_pack8, usage));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, padding16) != SDL_OFFSETOF(SDL_hid_device_info_pack1, padding16)) {
        SDL_Log("SDL_hid_device_info.padding16 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, padding16), SDL_OFFSETOF(SDL_hid_device_info_pack8, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, interface_number) != SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_number)) {
        SDL_Log("SDL_hid_device_info.interface_number has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_number), SDL_OFFSETOF(SDL_hid_device_info_pack8, interface_number));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, interface_class) != SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_class)) {
        SDL_Log("SDL_hid_device_info.interface_class has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_class), SDL_OFFSETOF(SDL_hid_device_info_pack8, interface_class));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, interface_subclass) != SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_subclass)) {
        SDL_Log("SDL_hid_device_info.interface_subclass has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_subclass), SDL_OFFSETOF(SDL_hid_device_info_pack8, interface_subclass));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, interface_protocol) != SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_protocol)) {
        SDL_Log("SDL_hid_device_info.interface_protocol has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_protocol), SDL_OFFSETOF(SDL_hid_device_info_pack8, interface_protocol));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, bus_type) != SDL_OFFSETOF(SDL_hid_device_info_pack1, bus_type)) {
        SDL_Log("SDL_hid_device_info.bus_type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, bus_type), SDL_OFFSETOF(SDL_hid_device_info_pack8, bus_type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack8, next) != SDL_OFFSETOF(SDL_hid_device_info_pack1, next)) {
        SDL_Log("SDL_hid_device_info.next has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, next), SDL_OFFSETOF(SDL_hid_device_info_pack8, next));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_hid_device_info_pack4) != sizeof(SDL_hid_device_info_pack1)) {
        SDL_Log("SDL_hid_device_info has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_hid_device_info_pack1), (int)sizeof(SDL_hid_device_info_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, path) != SDL_OFFSETOF(SDL_hid_device_info_pack1, path)) {
        SDL_Log("SDL_hid_device_info.path has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, path), SDL_OFFSETOF(SDL_hid_device_info_pack4, path));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, serial_number) != SDL_OFFSETOF(SDL_hid_device_info_pack1, serial_number)) {
        SDL_Log("SDL_hid_device_info.serial_number has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, serial_number), SDL_OFFSETOF(SDL_hid_device_info_pack4, serial_number));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, manufacturer_string) != SDL_OFFSETOF(SDL_hid_device_info_pack1, manufacturer_string)) {
        SDL_Log("SDL_hid_device_info.manufacturer_string has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, manufacturer_string), SDL_OFFSETOF(SDL_hid_device_info_pack4, manufacturer_string));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, product_string) != SDL_OFFSETOF(SDL_hid_device_info_pack1, product_string)) {
        SDL_Log("SDL_hid_device_info.product_string has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, product_string), SDL_OFFSETOF(SDL_hid_device_info_pack4, product_string));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, vendor_id) != SDL_OFFSETOF(SDL_hid_device_info_pack1, vendor_id)) {
        SDL_Log("SDL_hid_device_info.vendor_id has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, vendor_id), SDL_OFFSETOF(SDL_hid_device_info_pack4, vendor_id));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, product_id) != SDL_OFFSETOF(SDL_hid_device_info_pack1, product_id)) {
        SDL_Log("SDL_hid_device_info.product_id has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, product_id), SDL_OFFSETOF(SDL_hid_device_info_pack4, product_id));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, release_number) != SDL_OFFSETOF(SDL_hid_device_info_pack1, release_number)) {
        SDL_Log("SDL_hid_device_info.release_number has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, release_number), SDL_OFFSETOF(SDL_hid_device_info_pack4, release_number));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, usage_page) != SDL_OFFSETOF(SDL_hid_device_info_pack1, usage_page)) {
        SDL_Log("SDL_hid_device_info.usage_page has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, usage_page), SDL_OFFSETOF(SDL_hid_device_info_pack4, usage_page));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, usage) != SDL_OFFSETOF(SDL_hid_device_info_pack1, usage)) {
        SDL_Log("SDL_hid_device_info.usage has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, usage), SDL_OFFSETOF(SDL_hid_device_info_pack4, usage));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, padding16) != SDL_OFFSETOF(SDL_hid_device_info_pack1, padding16)) {
        SDL_Log("SDL_hid_device_info.padding16 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, padding16), SDL_OFFSETOF(SDL_hid_device_info_pack4, padding16));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, interface_number) != SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_number)) {
        SDL_Log("SDL_hid_device_info.interface_number has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_number), SDL_OFFSETOF(SDL_hid_device_info_pack4, interface_number));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, interface_class) != SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_class)) {
        SDL_Log("SDL_hid_device_info.interface_class has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_class), SDL_OFFSETOF(SDL_hid_device_info_pack4, interface_class));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, interface_subclass) != SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_subclass)) {
        SDL_Log("SDL_hid_device_info.interface_subclass has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_subclass), SDL_OFFSETOF(SDL_hid_device_info_pack4, interface_subclass));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, interface_protocol) != SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_protocol)) {
        SDL_Log("SDL_hid_device_info.interface_protocol has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, interface_protocol), SDL_OFFSETOF(SDL_hid_device_info_pack4, interface_protocol));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, bus_type) != SDL_OFFSETOF(SDL_hid_device_info_pack1, bus_type)) {
        SDL_Log("SDL_hid_device_info.bus_type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, bus_type), SDL_OFFSETOF(SDL_hid_device_info_pack4, bus_type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_hid_device_info_pack4, next) != SDL_OFFSETOF(SDL_hid_device_info_pack1, next)) {
        SDL_Log("SDL_hid_device_info.next has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_hid_device_info_pack1, next), SDL_OFFSETOF(SDL_hid_device_info_pack4, next));
        result = SDL_FALSE;
    }

    /* SDL_Point */

    if (sizeof(SDL_Point_pack8) != sizeof(SDL_Point_pack1)) {
        SDL_Log("SDL_Point has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Point_pack1), (int)sizeof(SDL_Point_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Point_pack8, x) != SDL_OFFSETOF(SDL_Point_pack1, x)) {
        SDL_Log("SDL_Point.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Point_pack1, x), SDL_OFFSETOF(SDL_Point_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Point_pack8, y) != SDL_OFFSETOF(SDL_Point_pack1, y)) {
        SDL_Log("SDL_Point.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Point_pack1, y), SDL_OFFSETOF(SDL_Point_pack8, y));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_Point_pack4) != sizeof(SDL_Point_pack1)) {
        SDL_Log("SDL_Point has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Point_pack1), (int)sizeof(SDL_Point_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Point_pack4, x) != SDL_OFFSETOF(SDL_Point_pack1, x)) {
        SDL_Log("SDL_Point.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Point_pack1, x), SDL_OFFSETOF(SDL_Point_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Point_pack4, y) != SDL_OFFSETOF(SDL_Point_pack1, y)) {
        SDL_Log("SDL_Point.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Point_pack1, y), SDL_OFFSETOF(SDL_Point_pack4, y));
        result = SDL_FALSE;
    }

    /* SDL_FPoint */

    if (sizeof(SDL_FPoint_pack8) != sizeof(SDL_FPoint_pack1)) {
        SDL_Log("SDL_FPoint has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_FPoint_pack1), (int)sizeof(SDL_FPoint_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FPoint_pack8, x) != SDL_OFFSETOF(SDL_FPoint_pack1, x)) {
        SDL_Log("SDL_FPoint.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FPoint_pack1, x), SDL_OFFSETOF(SDL_FPoint_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FPoint_pack8, y) != SDL_OFFSETOF(SDL_FPoint_pack1, y)) {
        SDL_Log("SDL_FPoint.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FPoint_pack1, y), SDL_OFFSETOF(SDL_FPoint_pack8, y));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_FPoint_pack4) != sizeof(SDL_FPoint_pack1)) {
        SDL_Log("SDL_FPoint has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_FPoint_pack1), (int)sizeof(SDL_FPoint_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FPoint_pack4, x) != SDL_OFFSETOF(SDL_FPoint_pack1, x)) {
        SDL_Log("SDL_FPoint.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FPoint_pack1, x), SDL_OFFSETOF(SDL_FPoint_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FPoint_pack4, y) != SDL_OFFSETOF(SDL_FPoint_pack1, y)) {
        SDL_Log("SDL_FPoint.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FPoint_pack1, y), SDL_OFFSETOF(SDL_FPoint_pack4, y));
        result = SDL_FALSE;
    }

    /* SDL_Rect */

    if (sizeof(SDL_Rect_pack8) != sizeof(SDL_Rect_pack1)) {
        SDL_Log("SDL_Rect has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Rect_pack1), (int)sizeof(SDL_Rect_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Rect_pack8, y) != SDL_OFFSETOF(SDL_Rect_pack1, y)) {
        SDL_Log("SDL_Rect.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Rect_pack1, y), SDL_OFFSETOF(SDL_Rect_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Rect_pack8, h) != SDL_OFFSETOF(SDL_Rect_pack1, h)) {
        SDL_Log("SDL_Rect.h has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Rect_pack1, h), SDL_OFFSETOF(SDL_Rect_pack8, h));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_Rect_pack4) != sizeof(SDL_Rect_pack1)) {
        SDL_Log("SDL_Rect has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Rect_pack1), (int)sizeof(SDL_Rect_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Rect_pack4, y) != SDL_OFFSETOF(SDL_Rect_pack1, y)) {
        SDL_Log("SDL_Rect.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Rect_pack1, y), SDL_OFFSETOF(SDL_Rect_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Rect_pack4, h) != SDL_OFFSETOF(SDL_Rect_pack1, h)) {
        SDL_Log("SDL_Rect.h has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Rect_pack1, h), SDL_OFFSETOF(SDL_Rect_pack4, h));
        result = SDL_FALSE;
    }

    /* SDL_FRect */

    if (sizeof(SDL_FRect_pack8) != sizeof(SDL_FRect_pack1)) {
        SDL_Log("SDL_FRect has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_FRect_pack1), (int)sizeof(SDL_FRect_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FRect_pack8, x) != SDL_OFFSETOF(SDL_FRect_pack1, x)) {
        SDL_Log("SDL_FRect.x has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FRect_pack1, x), SDL_OFFSETOF(SDL_FRect_pack8, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FRect_pack8, y) != SDL_OFFSETOF(SDL_FRect_pack1, y)) {
        SDL_Log("SDL_FRect.y has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FRect_pack1, y), SDL_OFFSETOF(SDL_FRect_pack8, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FRect_pack8, w) != SDL_OFFSETOF(SDL_FRect_pack1, w)) {
        SDL_Log("SDL_FRect.w has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FRect_pack1, w), SDL_OFFSETOF(SDL_FRect_pack8, w));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FRect_pack8, h) != SDL_OFFSETOF(SDL_FRect_pack1, h)) {
        SDL_Log("SDL_FRect.h has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FRect_pack1, h), SDL_OFFSETOF(SDL_FRect_pack8, h));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_FRect_pack4) != sizeof(SDL_FRect_pack1)) {
        SDL_Log("SDL_FRect has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_FRect_pack1), (int)sizeof(SDL_FRect_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FRect_pack4, x) != SDL_OFFSETOF(SDL_FRect_pack1, x)) {
        SDL_Log("SDL_FRect.x has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FRect_pack1, x), SDL_OFFSETOF(SDL_FRect_pack4, x));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FRect_pack4, y) != SDL_OFFSETOF(SDL_FRect_pack1, y)) {
        SDL_Log("SDL_FRect.y has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FRect_pack1, y), SDL_OFFSETOF(SDL_FRect_pack4, y));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FRect_pack4, w) != SDL_OFFSETOF(SDL_FRect_pack1, w)) {
        SDL_Log("SDL_FRect.w has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FRect_pack1, w), SDL_OFFSETOF(SDL_FRect_pack4, w));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FRect_pack4, h) != SDL_OFFSETOF(SDL_FRect_pack1, h)) {
        SDL_Log("SDL_FRect.h has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FRect_pack1, h), SDL_OFFSETOF(SDL_FRect_pack4, h));
        result = SDL_FALSE;
    }

    /* SDL_VirtualJoystickTouchpadDesc */

    if (sizeof(SDL_VirtualJoystickTouchpadDesc_pack8) != sizeof(SDL_VirtualJoystickTouchpadDesc_pack1)) {
        SDL_Log("SDL_VirtualJoystickTouchpadDesc has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_VirtualJoystickTouchpadDesc_pack1), (int)sizeof(SDL_VirtualJoystickTouchpadDesc_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack8, nfingers) != SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack1, nfingers)) {
        SDL_Log("SDL_VirtualJoystickTouchpadDesc.nfingers has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack1, nfingers), SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack8, nfingers));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack8, padding[0]) != SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack1, padding[0])) {
        SDL_Log("SDL_VirtualJoystickTouchpadDesc.padding[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack1, padding[0]), SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack8, padding[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_VirtualJoystickTouchpadDesc_pack4) != sizeof(SDL_VirtualJoystickTouchpadDesc_pack1)) {
        SDL_Log("SDL_VirtualJoystickTouchpadDesc has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_VirtualJoystickTouchpadDesc_pack1), (int)sizeof(SDL_VirtualJoystickTouchpadDesc_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack4, nfingers) != SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack1, nfingers)) {
        SDL_Log("SDL_VirtualJoystickTouchpadDesc.nfingers has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack1, nfingers), SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack4, nfingers));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack4, padding[0]) != SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack1, padding[0])) {
        SDL_Log("SDL_VirtualJoystickTouchpadDesc.padding[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack1, padding[0]), SDL_OFFSETOF(SDL_VirtualJoystickTouchpadDesc_pack4, padding[0]));
        result = SDL_FALSE;
    }

    /* SDL_VirtualJoystickSensorDesc */

    if (sizeof(SDL_VirtualJoystickSensorDesc_pack8) != sizeof(SDL_VirtualJoystickSensorDesc_pack1)) {
        SDL_Log("SDL_VirtualJoystickSensorDesc has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_VirtualJoystickSensorDesc_pack1), (int)sizeof(SDL_VirtualJoystickSensorDesc_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack8, type) != SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack1, type)) {
        SDL_Log("SDL_VirtualJoystickSensorDesc.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack1, type), SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack8, rate) != SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack1, rate)) {
        SDL_Log("SDL_VirtualJoystickSensorDesc.rate has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack1, rate), SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack8, rate));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_VirtualJoystickSensorDesc_pack4) != sizeof(SDL_VirtualJoystickSensorDesc_pack1)) {
        SDL_Log("SDL_VirtualJoystickSensorDesc has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_VirtualJoystickSensorDesc_pack1), (int)sizeof(SDL_VirtualJoystickSensorDesc_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack4, type) != SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack1, type)) {
        SDL_Log("SDL_VirtualJoystickSensorDesc.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack1, type), SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack4, rate) != SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack1, rate)) {
        SDL_Log("SDL_VirtualJoystickSensorDesc.rate has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack1, rate), SDL_OFFSETOF(SDL_VirtualJoystickSensorDesc_pack4, rate));
        result = SDL_FALSE;
    }

    /* SDL_VirtualJoystickDesc */

    if (sizeof(SDL_VirtualJoystickDesc_pack8) != sizeof(SDL_VirtualJoystickDesc_pack1)) {
        SDL_Log("SDL_VirtualJoystickDesc has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_VirtualJoystickDesc_pack1), (int)sizeof(SDL_VirtualJoystickDesc_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, type) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, type)) {
        SDL_Log("SDL_VirtualJoystickDesc.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, type), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, padding) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, padding)) {
        SDL_Log("SDL_VirtualJoystickDesc.padding has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, padding), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, padding));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, vendor_id) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, vendor_id)) {
        SDL_Log("SDL_VirtualJoystickDesc.vendor_id has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, vendor_id), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, vendor_id));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, product_id) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, product_id)) {
        SDL_Log("SDL_VirtualJoystickDesc.product_id has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, product_id), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, product_id));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, naxes) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, naxes)) {
        SDL_Log("SDL_VirtualJoystickDesc.naxes has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, naxes), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, naxes));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, nbuttons) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nbuttons)) {
        SDL_Log("SDL_VirtualJoystickDesc.nbuttons has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nbuttons), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, nbuttons));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, nballs) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nballs)) {
        SDL_Log("SDL_VirtualJoystickDesc.nballs has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nballs), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, nballs));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, nhats) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nhats)) {
        SDL_Log("SDL_VirtualJoystickDesc.nhats has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nhats), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, nhats));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, ntouchpads) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, ntouchpads)) {
        SDL_Log("SDL_VirtualJoystickDesc.ntouchpads has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, ntouchpads), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, ntouchpads));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, nsensors) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nsensors)) {
        SDL_Log("SDL_VirtualJoystickDesc.nsensors has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nsensors), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, nsensors));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, padding2[0]) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, padding2[0])) {
        SDL_Log("SDL_VirtualJoystickDesc.padding2[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, padding2[0]), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, padding2[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, button_mask) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, button_mask)) {
        SDL_Log("SDL_VirtualJoystickDesc.button_mask has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, button_mask), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, button_mask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, axis_mask) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, axis_mask)) {
        SDL_Log("SDL_VirtualJoystickDesc.axis_mask has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, axis_mask), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, axis_mask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, name) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, name)) {
        SDL_Log("SDL_VirtualJoystickDesc.name has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, name), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, name));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, touchpads) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, touchpads)) {
        SDL_Log("SDL_VirtualJoystickDesc.touchpads has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, touchpads), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, touchpads));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, sensors) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, sensors)) {
        SDL_Log("SDL_VirtualJoystickDesc.sensors has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, sensors), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, sensors));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, userdata) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, userdata)) {
        SDL_Log("SDL_VirtualJoystickDesc.userdata has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, userdata), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack8, userdata));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_VirtualJoystickDesc_pack4) != sizeof(SDL_VirtualJoystickDesc_pack1)) {
        SDL_Log("SDL_VirtualJoystickDesc has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_VirtualJoystickDesc_pack1), (int)sizeof(SDL_VirtualJoystickDesc_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, type) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, type)) {
        SDL_Log("SDL_VirtualJoystickDesc.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, type), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, padding) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, padding)) {
        SDL_Log("SDL_VirtualJoystickDesc.padding has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, padding), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, padding));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, vendor_id) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, vendor_id)) {
        SDL_Log("SDL_VirtualJoystickDesc.vendor_id has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, vendor_id), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, vendor_id));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, product_id) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, product_id)) {
        SDL_Log("SDL_VirtualJoystickDesc.product_id has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, product_id), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, product_id));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, naxes) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, naxes)) {
        SDL_Log("SDL_VirtualJoystickDesc.naxes has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, naxes), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, naxes));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, nbuttons) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nbuttons)) {
        SDL_Log("SDL_VirtualJoystickDesc.nbuttons has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nbuttons), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, nbuttons));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, nballs) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nballs)) {
        SDL_Log("SDL_VirtualJoystickDesc.nballs has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nballs), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, nballs));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, nhats) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nhats)) {
        SDL_Log("SDL_VirtualJoystickDesc.nhats has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nhats), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, nhats));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, ntouchpads) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, ntouchpads)) {
        SDL_Log("SDL_VirtualJoystickDesc.ntouchpads has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, ntouchpads), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, ntouchpads));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, nsensors) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nsensors)) {
        SDL_Log("SDL_VirtualJoystickDesc.nsensors has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, nsensors), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, nsensors));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, padding2[0]) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, padding2[0])) {
        SDL_Log("SDL_VirtualJoystickDesc.padding2[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, padding2[0]), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, padding2[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, button_mask) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, button_mask)) {
        SDL_Log("SDL_VirtualJoystickDesc.button_mask has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, button_mask), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, button_mask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, axis_mask) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, axis_mask)) {
        SDL_Log("SDL_VirtualJoystickDesc.axis_mask has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, axis_mask), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, axis_mask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, name) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, name)) {
        SDL_Log("SDL_VirtualJoystickDesc.name has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, name), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, name));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, touchpads) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, touchpads)) {
        SDL_Log("SDL_VirtualJoystickDesc.touchpads has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, touchpads), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, touchpads));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, sensors) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, sensors)) {
        SDL_Log("SDL_VirtualJoystickDesc.sensors has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, sensors), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, sensors));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, userdata) != SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, userdata)) {
        SDL_Log("SDL_VirtualJoystickDesc.userdata has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack1, userdata), SDL_OFFSETOF(SDL_VirtualJoystickDesc_pack4, userdata));
        result = SDL_FALSE;
    }

    /* SDL_PathInfo */

    if (sizeof(SDL_PathInfo_pack8) != sizeof(SDL_PathInfo_pack1)) {
        SDL_Log("SDL_PathInfo has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PathInfo_pack1), (int)sizeof(SDL_PathInfo_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack8, type) != SDL_OFFSETOF(SDL_PathInfo_pack1, type)) {
        SDL_Log("SDL_PathInfo.type has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, type), SDL_OFFSETOF(SDL_PathInfo_pack8, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack8, padding32) != SDL_OFFSETOF(SDL_PathInfo_pack1, padding32)) {
        SDL_Log("SDL_PathInfo.padding32 has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, padding32), SDL_OFFSETOF(SDL_PathInfo_pack8, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack8, size) != SDL_OFFSETOF(SDL_PathInfo_pack1, size)) {
        SDL_Log("SDL_PathInfo.size has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, size), SDL_OFFSETOF(SDL_PathInfo_pack8, size));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack8, create_time) != SDL_OFFSETOF(SDL_PathInfo_pack1, create_time)) {
        SDL_Log("SDL_PathInfo.create_time has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, create_time), SDL_OFFSETOF(SDL_PathInfo_pack8, create_time));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack8, modify_time) != SDL_OFFSETOF(SDL_PathInfo_pack1, modify_time)) {
        SDL_Log("SDL_PathInfo.modify_time has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, modify_time), SDL_OFFSETOF(SDL_PathInfo_pack8, modify_time));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack8, access_time) != SDL_OFFSETOF(SDL_PathInfo_pack1, access_time)) {
        SDL_Log("SDL_PathInfo.access_time has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, access_time), SDL_OFFSETOF(SDL_PathInfo_pack8, access_time));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_PathInfo_pack4) != sizeof(SDL_PathInfo_pack1)) {
        SDL_Log("SDL_PathInfo has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PathInfo_pack1), (int)sizeof(SDL_PathInfo_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack4, type) != SDL_OFFSETOF(SDL_PathInfo_pack1, type)) {
        SDL_Log("SDL_PathInfo.type has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, type), SDL_OFFSETOF(SDL_PathInfo_pack4, type));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack4, padding32) != SDL_OFFSETOF(SDL_PathInfo_pack1, padding32)) {
        SDL_Log("SDL_PathInfo.padding32 has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, padding32), SDL_OFFSETOF(SDL_PathInfo_pack4, padding32));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack4, size) != SDL_OFFSETOF(SDL_PathInfo_pack1, size)) {
        SDL_Log("SDL_PathInfo.size has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, size), SDL_OFFSETOF(SDL_PathInfo_pack4, size));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack4, create_time) != SDL_OFFSETOF(SDL_PathInfo_pack1, create_time)) {
        SDL_Log("SDL_PathInfo.create_time has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, create_time), SDL_OFFSETOF(SDL_PathInfo_pack4, create_time));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack4, modify_time) != SDL_OFFSETOF(SDL_PathInfo_pack1, modify_time)) {
        SDL_Log("SDL_PathInfo.modify_time has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, modify_time), SDL_OFFSETOF(SDL_PathInfo_pack4, modify_time));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PathInfo_pack4, access_time) != SDL_OFFSETOF(SDL_PathInfo_pack1, access_time)) {
        SDL_Log("SDL_PathInfo.access_time has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PathInfo_pack1, access_time), SDL_OFFSETOF(SDL_PathInfo_pack4, access_time));
        result = SDL_FALSE;
    }

    /* SDL_MessageBoxButtonData */

    if (sizeof(SDL_MessageBoxButtonData_pack8) != sizeof(SDL_MessageBoxButtonData_pack1)) {
        SDL_Log("SDL_MessageBoxButtonData has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MessageBoxButtonData_pack1), (int)sizeof(SDL_MessageBoxButtonData_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MessageBoxButtonData_pack8, flags) != SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, flags)) {
        SDL_Log("SDL_MessageBoxButtonData.flags has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, flags), SDL_OFFSETOF(SDL_MessageBoxButtonData_pack8, flags));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MessageBoxButtonData_pack8, buttonID) != SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, buttonID)) {
        SDL_Log("SDL_MessageBoxButtonData.buttonID has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, buttonID), SDL_OFFSETOF(SDL_MessageBoxButtonData_pack8, buttonID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MessageBoxButtonData_pack8, text) != SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, text)) {
        SDL_Log("SDL_MessageBoxButtonData.text has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, text), SDL_OFFSETOF(SDL_MessageBoxButtonData_pack8, text));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_MessageBoxButtonData_pack4) != sizeof(SDL_MessageBoxButtonData_pack1)) {
        SDL_Log("SDL_MessageBoxButtonData has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MessageBoxButtonData_pack1), (int)sizeof(SDL_MessageBoxButtonData_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MessageBoxButtonData_pack4, flags) != SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, flags)) {
        SDL_Log("SDL_MessageBoxButtonData.flags has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, flags), SDL_OFFSETOF(SDL_MessageBoxButtonData_pack4, flags));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MessageBoxButtonData_pack4, buttonID) != SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, buttonID)) {
        SDL_Log("SDL_MessageBoxButtonData.buttonID has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, buttonID), SDL_OFFSETOF(SDL_MessageBoxButtonData_pack4, buttonID));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MessageBoxButtonData_pack4, text) != SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, text)) {
        SDL_Log("SDL_MessageBoxButtonData.text has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MessageBoxButtonData_pack1, text), SDL_OFFSETOF(SDL_MessageBoxButtonData_pack4, text));
        result = SDL_FALSE;
    }

    /* SDL_MessageBoxColor */

    if (sizeof(SDL_MessageBoxColor_pack8) != sizeof(SDL_MessageBoxColor_pack1)) {
        SDL_Log("SDL_MessageBoxColor has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MessageBoxColor_pack1), (int)sizeof(SDL_MessageBoxColor_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MessageBoxColor_pack8, b) != SDL_OFFSETOF(SDL_MessageBoxColor_pack1, b)) {
        SDL_Log("SDL_MessageBoxColor.b has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MessageBoxColor_pack1, b), SDL_OFFSETOF(SDL_MessageBoxColor_pack8, b));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_MessageBoxColor_pack4) != sizeof(SDL_MessageBoxColor_pack1)) {
        SDL_Log("SDL_MessageBoxColor has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MessageBoxColor_pack1), (int)sizeof(SDL_MessageBoxColor_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MessageBoxColor_pack4, b) != SDL_OFFSETOF(SDL_MessageBoxColor_pack1, b)) {
        SDL_Log("SDL_MessageBoxColor.b has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MessageBoxColor_pack1, b), SDL_OFFSETOF(SDL_MessageBoxColor_pack4, b));
        result = SDL_FALSE;
    }

    /* SDL_MessageBoxColorScheme */

    if (sizeof(SDL_MessageBoxColorScheme_pack8) != sizeof(SDL_MessageBoxColorScheme_pack1)) {
        SDL_Log("SDL_MessageBoxColorScheme has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MessageBoxColorScheme_pack1), (int)sizeof(SDL_MessageBoxColorScheme_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MessageBoxColorScheme_pack8, colors[0]) != SDL_OFFSETOF(SDL_MessageBoxColorScheme_pack1, colors[0])) {
        SDL_Log("SDL_MessageBoxColorScheme.colors[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MessageBoxColorScheme_pack1, colors[0]), SDL_OFFSETOF(SDL_MessageBoxColorScheme_pack8, colors[0]));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_MessageBoxColorScheme_pack4) != sizeof(SDL_MessageBoxColorScheme_pack1)) {
        SDL_Log("SDL_MessageBoxColorScheme has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_MessageBoxColorScheme_pack1), (int)sizeof(SDL_MessageBoxColorScheme_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_MessageBoxColorScheme_pack4, colors[0]) != SDL_OFFSETOF(SDL_MessageBoxColorScheme_pack1, colors[0])) {
        SDL_Log("SDL_MessageBoxColorScheme.colors[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_MessageBoxColorScheme_pack1, colors[0]), SDL_OFFSETOF(SDL_MessageBoxColorScheme_pack4, colors[0]));
        result = SDL_FALSE;
    }

    /* SDL_Color */

    if (sizeof(SDL_Color_pack8) != sizeof(SDL_Color_pack1)) {
        SDL_Log("SDL_Color has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Color_pack1), (int)sizeof(SDL_Color_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Color_pack8, r) != SDL_OFFSETOF(SDL_Color_pack1, r)) {
        SDL_Log("SDL_Color.r has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Color_pack1, r), SDL_OFFSETOF(SDL_Color_pack8, r));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Color_pack8, g) != SDL_OFFSETOF(SDL_Color_pack1, g)) {
        SDL_Log("SDL_Color.g has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Color_pack1, g), SDL_OFFSETOF(SDL_Color_pack8, g));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Color_pack8, b) != SDL_OFFSETOF(SDL_Color_pack1, b)) {
        SDL_Log("SDL_Color.b has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Color_pack1, b), SDL_OFFSETOF(SDL_Color_pack8, b));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Color_pack8, a) != SDL_OFFSETOF(SDL_Color_pack1, a)) {
        SDL_Log("SDL_Color.a has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Color_pack1, a), SDL_OFFSETOF(SDL_Color_pack8, a));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_Color_pack4) != sizeof(SDL_Color_pack1)) {
        SDL_Log("SDL_Color has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Color_pack1), (int)sizeof(SDL_Color_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Color_pack4, r) != SDL_OFFSETOF(SDL_Color_pack1, r)) {
        SDL_Log("SDL_Color.r has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Color_pack1, r), SDL_OFFSETOF(SDL_Color_pack4, r));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Color_pack4, g) != SDL_OFFSETOF(SDL_Color_pack1, g)) {
        SDL_Log("SDL_Color.g has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Color_pack1, g), SDL_OFFSETOF(SDL_Color_pack4, g));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Color_pack4, b) != SDL_OFFSETOF(SDL_Color_pack1, b)) {
        SDL_Log("SDL_Color.b has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Color_pack1, b), SDL_OFFSETOF(SDL_Color_pack4, b));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Color_pack4, a) != SDL_OFFSETOF(SDL_Color_pack1, a)) {
        SDL_Log("SDL_Color.a has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Color_pack1, a), SDL_OFFSETOF(SDL_Color_pack4, a));
        result = SDL_FALSE;
    }

    /* SDL_FColor */

    if (sizeof(SDL_FColor_pack8) != sizeof(SDL_FColor_pack1)) {
        SDL_Log("SDL_FColor has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_FColor_pack1), (int)sizeof(SDL_FColor_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FColor_pack8, r) != SDL_OFFSETOF(SDL_FColor_pack1, r)) {
        SDL_Log("SDL_FColor.r has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FColor_pack1, r), SDL_OFFSETOF(SDL_FColor_pack8, r));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FColor_pack8, g) != SDL_OFFSETOF(SDL_FColor_pack1, g)) {
        SDL_Log("SDL_FColor.g has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FColor_pack1, g), SDL_OFFSETOF(SDL_FColor_pack8, g));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FColor_pack8, b) != SDL_OFFSETOF(SDL_FColor_pack1, b)) {
        SDL_Log("SDL_FColor.b has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FColor_pack1, b), SDL_OFFSETOF(SDL_FColor_pack8, b));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FColor_pack8, a) != SDL_OFFSETOF(SDL_FColor_pack1, a)) {
        SDL_Log("SDL_FColor.a has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FColor_pack1, a), SDL_OFFSETOF(SDL_FColor_pack8, a));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_FColor_pack4) != sizeof(SDL_FColor_pack1)) {
        SDL_Log("SDL_FColor has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_FColor_pack1), (int)sizeof(SDL_FColor_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FColor_pack4, r) != SDL_OFFSETOF(SDL_FColor_pack1, r)) {
        SDL_Log("SDL_FColor.r has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FColor_pack1, r), SDL_OFFSETOF(SDL_FColor_pack4, r));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FColor_pack4, g) != SDL_OFFSETOF(SDL_FColor_pack1, g)) {
        SDL_Log("SDL_FColor.g has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FColor_pack1, g), SDL_OFFSETOF(SDL_FColor_pack4, g));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FColor_pack4, b) != SDL_OFFSETOF(SDL_FColor_pack1, b)) {
        SDL_Log("SDL_FColor.b has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FColor_pack1, b), SDL_OFFSETOF(SDL_FColor_pack4, b));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_FColor_pack4, a) != SDL_OFFSETOF(SDL_FColor_pack1, a)) {
        SDL_Log("SDL_FColor.a has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_FColor_pack1, a), SDL_OFFSETOF(SDL_FColor_pack4, a));
        result = SDL_FALSE;
    }

    /* SDL_Palette */

    if (sizeof(SDL_Palette_pack8) != sizeof(SDL_Palette_pack1)) {
        SDL_Log("SDL_Palette has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Palette_pack1), (int)sizeof(SDL_Palette_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Palette_pack8, refcount) != SDL_OFFSETOF(SDL_Palette_pack1, refcount)) {
        SDL_Log("SDL_Palette.refcount has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Palette_pack1, refcount), SDL_OFFSETOF(SDL_Palette_pack8, refcount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Palette_pack8, ncolors) != SDL_OFFSETOF(SDL_Palette_pack1, ncolors)) {
        SDL_Log("SDL_Palette.ncolors has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Palette_pack1, ncolors), SDL_OFFSETOF(SDL_Palette_pack8, ncolors));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Palette_pack8, colors) != SDL_OFFSETOF(SDL_Palette_pack1, colors)) {
        SDL_Log("SDL_Palette.colors has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Palette_pack1, colors), SDL_OFFSETOF(SDL_Palette_pack8, colors));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Palette_pack8, version) != SDL_OFFSETOF(SDL_Palette_pack1, version)) {
        SDL_Log("SDL_Palette.version has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Palette_pack1, version), SDL_OFFSETOF(SDL_Palette_pack8, version));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Palette_pack8, padding_end) != SDL_OFFSETOF(SDL_Palette_pack1, padding_end)) {
        SDL_Log("SDL_Palette.padding_end has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Palette_pack1, padding_end), SDL_OFFSETOF(SDL_Palette_pack8, padding_end));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_Palette_pack4) != sizeof(SDL_Palette_pack1)) {
        SDL_Log("SDL_Palette has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_Palette_pack1), (int)sizeof(SDL_Palette_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Palette_pack4, refcount) != SDL_OFFSETOF(SDL_Palette_pack1, refcount)) {
        SDL_Log("SDL_Palette.refcount has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Palette_pack1, refcount), SDL_OFFSETOF(SDL_Palette_pack4, refcount));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Palette_pack4, ncolors) != SDL_OFFSETOF(SDL_Palette_pack1, ncolors)) {
        SDL_Log("SDL_Palette.ncolors has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Palette_pack1, ncolors), SDL_OFFSETOF(SDL_Palette_pack4, ncolors));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Palette_pack4, colors) != SDL_OFFSETOF(SDL_Palette_pack1, colors)) {
        SDL_Log("SDL_Palette.colors has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Palette_pack1, colors), SDL_OFFSETOF(SDL_Palette_pack4, colors));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Palette_pack4, version) != SDL_OFFSETOF(SDL_Palette_pack1, version)) {
        SDL_Log("SDL_Palette.version has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Palette_pack1, version), SDL_OFFSETOF(SDL_Palette_pack4, version));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_Palette_pack4, padding_end) != SDL_OFFSETOF(SDL_Palette_pack1, padding_end)) {
        SDL_Log("SDL_Palette.padding_end has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_Palette_pack1, padding_end), SDL_OFFSETOF(SDL_Palette_pack4, padding_end));
        result = SDL_FALSE;
    }

    /* SDL_PixelFormatDetails */

    if (sizeof(SDL_PixelFormatDetails_pack8) != sizeof(SDL_PixelFormatDetails_pack1)) {
        SDL_Log("SDL_PixelFormatDetails has incorrect size with 8-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PixelFormatDetails_pack1), (int)sizeof(SDL_PixelFormatDetails_pack8));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, format) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, format)) {
        SDL_Log("SDL_PixelFormatDetails.format has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, format), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, bits_per_pixel) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, bits_per_pixel)) {
        SDL_Log("SDL_PixelFormatDetails.bits_per_pixel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, bits_per_pixel), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, bits_per_pixel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, bytes_per_pixel) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, bytes_per_pixel)) {
        SDL_Log("SDL_PixelFormatDetails.bytes_per_pixel has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, bytes_per_pixel), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, bytes_per_pixel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, padding[0]) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, padding[0])) {
        SDL_Log("SDL_PixelFormatDetails.padding[0] has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, padding[0]), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, padding[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Rmask) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rmask)) {
        SDL_Log("SDL_PixelFormatDetails.Rmask has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rmask), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Rmask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Gmask) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gmask)) {
        SDL_Log("SDL_PixelFormatDetails.Gmask has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gmask), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Gmask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Bmask) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bmask)) {
        SDL_Log("SDL_PixelFormatDetails.Bmask has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bmask), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Bmask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Amask) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Amask)) {
        SDL_Log("SDL_PixelFormatDetails.Amask has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Amask), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Amask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Rbits) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rbits)) {
        SDL_Log("SDL_PixelFormatDetails.Rbits has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rbits), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Rbits));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Gbits) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gbits)) {
        SDL_Log("SDL_PixelFormatDetails.Gbits has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gbits), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Gbits));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Bbits) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bbits)) {
        SDL_Log("SDL_PixelFormatDetails.Bbits has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bbits), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Bbits));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Abits) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Abits)) {
        SDL_Log("SDL_PixelFormatDetails.Abits has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Abits), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Abits));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Rshift) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rshift)) {
        SDL_Log("SDL_PixelFormatDetails.Rshift has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rshift), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Rshift));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Gshift) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gshift)) {
        SDL_Log("SDL_PixelFormatDetails.Gshift has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gshift), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Gshift));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Bshift) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bshift)) {
        SDL_Log("SDL_PixelFormatDetails.Bshift has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bshift), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Bshift));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Ashift) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Ashift)) {
        SDL_Log("SDL_PixelFormatDetails.Ashift has incorrect offset with 8-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Ashift), SDL_OFFSETOF(SDL_PixelFormatDetails_pack8, Ashift));
        result = SDL_FALSE;
    }

    if (sizeof(SDL_PixelFormatDetails_pack4) != sizeof(SDL_PixelFormatDetails_pack1)) {
        SDL_Log("SDL_PixelFormatDetails has incorrect size with 4-byte alignment, expected %d, got %d\n", (int)sizeof(SDL_PixelFormatDetails_pack1), (int)sizeof(SDL_PixelFormatDetails_pack4));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, format) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, format)) {
        SDL_Log("SDL_PixelFormatDetails.format has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, format), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, format));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, bits_per_pixel) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, bits_per_pixel)) {
        SDL_Log("SDL_PixelFormatDetails.bits_per_pixel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, bits_per_pixel), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, bits_per_pixel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, bytes_per_pixel) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, bytes_per_pixel)) {
        SDL_Log("SDL_PixelFormatDetails.bytes_per_pixel has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, bytes_per_pixel), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, bytes_per_pixel));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, padding[0]) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, padding[0])) {
        SDL_Log("SDL_PixelFormatDetails.padding[0] has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, padding[0]), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, padding[0]));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Rmask) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rmask)) {
        SDL_Log("SDL_PixelFormatDetails.Rmask has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rmask), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Rmask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Gmask) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gmask)) {
        SDL_Log("SDL_PixelFormatDetails.Gmask has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gmask), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Gmask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Bmask) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bmask)) {
        SDL_Log("SDL_PixelFormatDetails.Bmask has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bmask), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Bmask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Amask) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Amask)) {
        SDL_Log("SDL_PixelFormatDetails.Amask has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Amask), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Amask));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Rbits) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rbits)) {
        SDL_Log("SDL_PixelFormatDetails.Rbits has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rbits), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Rbits));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Gbits) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gbits)) {
        SDL_Log("SDL_PixelFormatDetails.Gbits has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gbits), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Gbits));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Bbits) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bbits)) {
        SDL_Log("SDL_PixelFormatDetails.Bbits has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bbits), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Bbits));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Abits) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Abits)) {
        SDL_Log("SDL_PixelFormatDetails.Abits has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Abits), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Abits));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Rshift) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rshift)) {
        SDL_Log("SDL_PixelFormatDetails.Rshift has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Rshift), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Rshift));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Gshift) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gshift)) {
        SDL_Log("SDL_PixelFormatDetails.Gshift has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Gshift), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Gshift));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Bshift) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bshift)) {
        SDL_Log("SDL_PixelFormatDetails.Bshift has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Bshift), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Bshift));
        result = SDL_FALSE;
    }

    if (SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Ashift) != SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Ashift)) {
        SDL_Log("SDL_PixelFormatDetails.Ashift has incorrect offset with 4-byte alignment, expected %d, got %d\n", SDL_OFFSETOF(SDL_PixelFormatDetails_pack1, Ashift), SDL_OFFSETOF(SDL_PixelFormatDetails_pack4, Ashift));
        result = SDL_FALSE;
    }

    return result;
}

