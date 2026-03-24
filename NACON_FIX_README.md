# Add support for Nacon Compact PS4 Wired Controller (0x146b:0x0603)

Tested and confirmed working on macOS (Apple Silicon) via
`SDL_JOYSTICK_HIDAPI_XBOX_360` driver over libusb. All inputs verified:
A/B/X/Y, LB/RB, LT/RT (full analog), both sticks, d-pad, Share/Options.
Center touchpad is inert — XInput protocol has no touchpad data channel.

## Problem

This controller uses XInput over a vendor-specific USB interface (no HID).
Platform HID backends (IOHIDManager, hidraw) can't see it — only libusb can.
SDL3 already supports the protocol; two guards blocked the device from reaching
the driver.

## Fix

**`src/hidapi/SDL_hidapi.c`** — Added `0x146b:0x0603` to `SDL_libusb_required[]`.
The libusb whitelist only had Nintendo devices. Affects macOS and Linux.

**`src/joystick/hidapi/SDL_hidapi_xbox360.c`** — Added `else if` branch in
the macOS MFI guard to let libusb-required devices through instead of deferring
to GCController (which can't see them). Generic — future `SDL_libusb_required[]`
entries benefit automatically. Not needed on Linux (no MFI guard).

## Other Nacon XInput controllers

Only `0x0603` was hardware-tested. Five other Nacon XInput PIDs in
`controller_list.h` likely need the same fix:

- `0x0601` — BigBen Interactive XBOX 360 Controller
- `0x0604` — NACON Daija Arcade Stick
- `0x0605` — NACON PS4 controller in Xbox mode
- `0x0606` — NACON Unknown Controller
- `0x0609` — NACON Wireless Controller for PS4

## Runtime Requirement

Requires libusb (`brew install libusb` on macOS). SDL3 loads it dynamically
via `dlopen("libusb-1.0.dylib")`.
