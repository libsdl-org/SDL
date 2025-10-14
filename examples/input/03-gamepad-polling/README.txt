This example code looks for the current gamepad state once per frame,
and draws a visual representation of it. See 01-joystick-polling for the
equivalent example code for the lower-level joystick API.

Please note that on the web, gamepads don't show up until you interact with
them, so press a button to "connect" the controller.

Also note that on the web, gamepad triggers are treated as buttons (either
pressed or not) instead of axes (pressed 0 to 100 percent). This is a web
issue, not an SDL limitation.

