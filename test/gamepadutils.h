/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Gamepad image */

typedef struct GamepadImage GamepadImage;

typedef enum
{
    CONTROLLER_MODE_TESTING,
    CONTROLLER_MODE_BINDING,
} ControllerDisplayMode;

enum
{
    SDL_GAMEPAD_ELEMENT_INVALID = -1,

    /* ... SDL_GamepadButton ... */

    SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_NEGATIVE = SDL_GAMEPAD_BUTTON_MAX,
    SDL_GAMEPAD_ELEMENT_AXIS_LEFTX_POSITIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_NEGATIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_LEFTY_POSITIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_NEGATIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_RIGHTX_POSITIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_NEGATIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_RIGHTY_POSITIVE,
    SDL_GAMEPAD_ELEMENT_AXIS_LEFT_TRIGGER,
    SDL_GAMEPAD_ELEMENT_AXIS_RIGHT_TRIGGER,
    SDL_GAMEPAD_ELEMENT_AXIS_MAX,

    SDL_GAMEPAD_ELEMENT_NAME = SDL_GAMEPAD_ELEMENT_AXIS_MAX,
    SDL_GAMEPAD_ELEMENT_TYPE,
    SDL_GAMEPAD_ELEMENT_MAX,
};

#define HIGHLIGHT_COLOR         224, 255, 255, SDL_ALPHA_OPAQUE
#define HIGHLIGHT_TEXTURE_MOD   224, 255, 255
#define PRESSED_COLOR           175, 238, 238, SDL_ALPHA_OPAQUE
#define PRESSED_TEXTURE_MOD     175, 238, 238
#define SELECTED_COLOR          224, 255, 224, SDL_ALPHA_OPAQUE

/* Gamepad image display */

extern GamepadImage *CreateGamepadImage(SDL_Renderer *renderer);
extern void SetGamepadImagePosition(GamepadImage *ctx, int x, int y);
extern void GetGamepadImageArea(GamepadImage *ctx, SDL_Rect *area);
extern void SetGamepadImageShowingFront(GamepadImage *ctx, SDL_bool showing_front);
extern void SetGamepadImageType(GamepadImage *ctx, SDL_GamepadType type);
extern SDL_GamepadType GetGamepadImageType(GamepadImage *ctx);
extern void SetGamepadImageDisplayMode(GamepadImage *ctx, ControllerDisplayMode display_mode);
extern int GetGamepadImageButtonWidth(GamepadImage *ctx);
extern int GetGamepadImageButtonHeight(GamepadImage *ctx);
extern int GetGamepadImageAxisWidth(GamepadImage *ctx);
extern int GetGamepadImageAxisHeight(GamepadImage *ctx);
extern int GetGamepadImageElementAt(GamepadImage *ctx, float x, float y);

extern void ClearGamepadImage(GamepadImage *ctx);
extern void SetGamepadImageElement(GamepadImage *ctx, int element, SDL_bool active);

extern void UpdateGamepadImageFromGamepad(GamepadImage *ctx, SDL_Gamepad *gamepad);
extern void RenderGamepadImage(GamepadImage *ctx);
extern void DestroyGamepadImage(GamepadImage *ctx);

/* Gamepad element display */

typedef struct GamepadDisplay GamepadDisplay;

extern GamepadDisplay *CreateGamepadDisplay(SDL_Renderer *renderer);
extern void SetGamepadDisplayDisplayMode(GamepadDisplay *ctx, ControllerDisplayMode display_mode);
extern void SetGamepadDisplayArea(GamepadDisplay *ctx, const SDL_Rect *area);
extern int GetGamepadDisplayElementAt(GamepadDisplay *ctx, SDL_Gamepad *gamepad, float x, float y);
extern void SetGamepadDisplayHighlight(GamepadDisplay *ctx, int element, SDL_bool pressed);
extern void SetGamepadDisplaySelected(GamepadDisplay *ctx, int element);
extern void RenderGamepadDisplay(GamepadDisplay *ctx, SDL_Gamepad *gamepad);
extern void DestroyGamepadDisplay(GamepadDisplay *ctx);

/* Gamepad type display */

enum
{
    SDL_GAMEPAD_TYPE_UNSELECTED = -1
};

typedef struct GamepadTypeDisplay GamepadTypeDisplay;

extern GamepadTypeDisplay *CreateGamepadTypeDisplay(SDL_Renderer *renderer);
extern void SetGamepadTypeDisplayArea(GamepadTypeDisplay *ctx, const SDL_Rect *area);
extern int GetGamepadTypeDisplayAt(GamepadTypeDisplay *ctx, float x, float y);
extern void SetGamepadTypeDisplayHighlight(GamepadTypeDisplay *ctx, int type, SDL_bool pressed);
extern void SetGamepadTypeDisplaySelected(GamepadTypeDisplay *ctx, int type);
extern void SetGamepadTypeDisplayRealType(GamepadTypeDisplay *ctx, SDL_GamepadType type);
extern void RenderGamepadTypeDisplay(GamepadTypeDisplay *ctx);
extern void DestroyGamepadTypeDisplay(GamepadTypeDisplay *ctx);

/* Joystick element display */

typedef struct JoystickDisplay JoystickDisplay;

extern JoystickDisplay *CreateJoystickDisplay(SDL_Renderer *renderer);
extern void SetJoystickDisplayArea(JoystickDisplay *ctx, const SDL_Rect *area);
extern char *GetJoystickDisplayElementAt(JoystickDisplay *ctx, SDL_Joystick *joystick, float x, float y);
extern void SetJoystickDisplayHighlight(JoystickDisplay *ctx, const char *element, SDL_bool pressed);
extern void RenderJoystickDisplay(JoystickDisplay *ctx, SDL_Joystick *joystick);
extern void DestroyJoystickDisplay(JoystickDisplay *ctx);

/* Simple buttons */

typedef struct GamepadButton GamepadButton;

extern GamepadButton *CreateGamepadButton(SDL_Renderer *renderer, const char *label);
extern void SetGamepadButtonArea(GamepadButton *ctx, const SDL_Rect *area);
extern void GetGamepadButtonArea(GamepadButton *ctx, SDL_Rect *area);
extern void SetGamepadButtonHighlight(GamepadButton *ctx, SDL_bool highlight, SDL_bool pressed);
extern int GetGamepadButtonLabelWidth(GamepadButton *ctx);
extern int GetGamepadButtonLabelHeight(GamepadButton *ctx);
extern SDL_bool GamepadButtonContains(GamepadButton *ctx, float x, float y);
extern void RenderGamepadButton(GamepadButton *ctx);
extern void DestroyGamepadButton(GamepadButton *ctx);

/* Working with mappings and bindings */

/* Return whether a mapping has any bindings */
extern SDL_bool MappingHasBindings(const char *mapping);

/* Return true if the mapping has a controller name */
extern SDL_bool MappingHasName(const char *mapping);

/* Return the name from a mapping, which should be freed using SDL_free(), or NULL if there is no name specified */
extern char *GetMappingName(const char *mapping);

/* Set the name in a mapping, freeing the mapping passed in and returning a new mapping */
extern char *SetMappingName(char *mapping, const char *name);

/* Get the friendly string for an SDL_GamepadType */
extern const char *GetGamepadTypeString(SDL_GamepadType type);

/* Return the type from a mapping, which should be freed using SDL_free(), or NULL if there is no type specified */
extern SDL_GamepadType GetMappingType(const char *mapping);

/* Set the type in a mapping, freeing the mapping passed in and returning a new mapping */
extern char *SetMappingType(char *mapping, SDL_GamepadType type);

/* Return true if a mapping has this element bound */
extern SDL_bool MappingHasElement(const char *mapping, int element);

/* Get the binding for an element, which should be freed using SDL_free(), or NULL if the element isn't bound */
extern char *GetElementBinding(const char *mapping, int element);

/* Set the binding for an element, or NULL to clear it, freeing the mapping passed in and returning a new mapping */
extern char *SetElementBinding(char *mapping, int element, const char *binding);

/* Get the element for a binding, or SDL_GAMEPAD_ELEMENT_INVALID if that binding isn't used */
extern int GetElementForBinding(char *mapping, const char *binding);

/* Return true if a mapping contains this binding */
extern SDL_bool MappingHasBinding(const char *mapping, const char *binding);

/* Clear any previous binding */
extern char *ClearMappingBinding(char *mapping, const char *binding);
