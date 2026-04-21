# x11-lab

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

## Disclaimer

This project was developed for research and educational purposes to study X11 input handling and session control mechanisms in a controlled environment.
