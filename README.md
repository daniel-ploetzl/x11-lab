# x11-lab: Session Behaviour & Input Capture Lab

## Overview

A minimal C-programme using Xlib to take full control of an X11 session from user space.

It creates a fullscreen black window that ignores the window manager, captures all keyboard and mouse input exclusively, hides the cursor and terminates all user processes via a signal-based timer after a configurable duration.

Built to analyse how input control, focus management and process termination can be enforced purely from user space using X11 primitives, without kernel-level access or elevated privileges.

---

## What it does

- Creates a fullscreen `override_redirect` window, bypassing the window manager entirely
- Grabs keyboard and pointer, all input events are redirected exclusively to this window
- Renders an invisible cursor, removing all visual feedback
- Resets the XScreenSaver idle clock on received input events, preventing power-saving states
- Terminates all user processes via `kill(-1, SIGKILL)` after the specified duration,
  triggered by `SIGALRM`
- Non-interruptible from within the session once running

---

## Build

```bash
# Requires libx11-dev
gcc -O2 blackscreen.c -o blackscreen -lX11
```

## Usage

```bash
./blackscreen <hours>   # e.g. ./blackscreen 8
```

---

## Technical details

| Mechanism | API used |
|---|---|
| Fullscreen window | `XCreateWindow` with `override_redirect = True` |
| WM bypass | `override_redirect` - window manager receives no `MapRequest` |
| Keyboard capture | `XGrabKeyboard` - exclusive, async mode |
| Pointer capture | `XGrabPointer` - exclusive, async mode |
| Invisible cursor | `XCreatePixmapCursor` from 1×1 blank pixmap |
| Idle reset | `XResetScreenSaver` + `XFlush` on motion/key/button events |
| Timed exit | `SIGALRM` via `alarm()` -> `kill(-1, SIGKILL)` |
| X11 error handling | `XSetErrorHandler`, grab return-value checks with clean teardown |

---

## Code structure

The program is organised around a single `t_app` state struct that holds the
display, window, geometry and cursor. `main` reads as a linear sequence of
named steps, each delegated to a focused function:

| Function | Responsibility |
|---|---|
| `parse_hours` | Parse and range-check the hours argument |
| `arm_timer` | Install the `SIGALRM` handler and arm `alarm()` |
| `open_display` | Open the display and cache screen geometry |
| `create_window` | Create the fullscreen `override_redirect` window |
| `hide_cursor` | Build and attach the invisible cursor |
| `grab_input` | Grab the keyboard and pointer |
| `event_loop` | Repaint on expose, reset the idle clock on input |
| `cleanup` | Single teardown path for all X resources |

All error exits route through `cleanup`, so every resource is released on one
path regardless of where a failure occurs.

---

## Disclaimer

This project was developed for research and educational purposes to study X11 input handling and session control mechanisms in a controlled environment.
