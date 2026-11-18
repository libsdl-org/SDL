#include "SDL_internal.h"

#define FLAG_FORCE_FEEDBACK	0x01
#define FLAG_WIRELESS		0x02
#define FLAG_VOICE			0x04
#define FLAG_PLUGIN_MODULES	0x08
#define FLAG_NO_NAVIGATION	0x10

typedef struct {
  uint8_t type;
  uint8_t subType;
  uint16_t flags;
  struct {
	uint16_t  wButtons;
	uint8_t  bLeftTrigger;
	uint8_t  bRightTrigger;
	int16_t sThumbLX;
	int16_t sThumbLY;
	int16_t sThumbRX;
	int16_t sThumbRY;
  } gamepad;

  struct {
	uint16_t wLeftMotorSpeed;
	uint16_t wRightMotorSpeed;
  } vibration;
} SDL_xinput_capabilities;

typedef enum
{
    SDL_XINPUT_SUBTYPE_GAMEPAD = 1,
    SDL_XINPUT_SUBTYPE_WHEEL = 2,
    SDL_XINPUT_SUBTYPE_ARCADE_STICK = 3,
    SDL_XINPUT_SUBTYPE_FLIGHT_STICK = 4,
    SDL_XINPUT_SUBTYPE_DANCE_PAD = 5,
    SDL_XINPUT_SUBTYPE_GUITAR = 6,
    SDL_XINPUT_SUBTYPE_GUITAR_ALTERNATE = 7,
    SDL_XINPUT_SUBTYPE_DRUMS = 8,
    SDL_XINPUT_SUBTYPE_STAGE_KIT = 9,
    SDL_XINPUT_SUBTYPE_GUITAR_BASS = 11,
    SDL_XINPUT_SUBTYPE_PRO_KEYS = 15,
    SDL_XINPUT_SUBTYPE_ARCADE_PAD = 19,
    SDL_XINPUT_SUBTYPE_TURNTABLE = 23,
    SDL_XINPUT_SUBTYPE_PRO_GUITAR = 25,
    SDL_XINPUT_SUBTYPE_DISNEY_INFINITY_AND_LEGO_DIMENSIONS = 33,
    SDL_XINPUT_SUBTYPE_SKYLANDERS = 36
} SDL_XInputSubType;

// Function to return the joystick type for an XInput device based on its subtype
extern SDL_JoystickType SDL_GetJoystickTypeForXInput(SDL_XInputSubType subtype);