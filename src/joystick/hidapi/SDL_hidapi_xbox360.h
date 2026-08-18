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