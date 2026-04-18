/*
 * PURPOSE : Fullscreen black X11 window with hidden cursor and input grab.
 *           Terminates all user processes (kill -9 -1) after N hours.
 *           Single self-contained binary — zero Python/runtime dependency.
 *
 * BUILD   : gcc -O2 -o blackscreen blackscreen.c -lX11
 * USAGE   : ./blackscreen <hours>
 * EXAMPLE : ./blackscreen 8
 *
 * HOW     : Opens a fullscreen InputOutput window on the default X display,
 *           overrides the window manager (override_redirect), paints it black,
 *           grabs pointer and keyboard so all input is consumed by this window,
 *           creates an invisible cursor via a 1x1 black Pixmap,
 *           then sits in XNextEvent() until the SIGALRM fires.
 *           SIGALRM is set via alarm() for durations ≤ 2^31 seconds (~68 years).
 *           On alarm: sends SIGKILL to pgid -1 (all user processes).
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* SIGALRM handler — equivalent to shell's kill -9 -1 */
static void on_alarm(int sig)
{
    (void) sig;
    kill(-1, SIGKILL);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hours>\n", argv[0]);
        return 1;
    }

    char *end;
    double hours = strtod(argv[1], &end);
    if (*end != '\0' || hours <= 0 || hours > 48) {
        fprintf(stderr, "Error: hours must be a number between 0 and 48\n");
        return 1;
    }

    /* ── Connect to X display ─────────────────────────────────────────── */
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Error: cannot open display (is DISPLAY set?)\n");
        return 1;
    }

    int screen       = DefaultScreen(dpy);
    Window root      = RootWindow(dpy, screen);
    int width        = DisplayWidth(dpy, screen);
    int height       = DisplayHeight(dpy, screen);
    unsigned long black = BlackPixel(dpy, screen);

    /* ── Create fullscreen black window ──────────────────────────────── */
    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.background_pixel  = black;
    attrs.border_pixel       = black;
    attrs.override_redirect  = True;   /* bypass window manager entirely */
    attrs.event_mask         = KeyPressMask
								| ButtonPressMask
								| PointerMotionMask
								| ExposureMask;

    Window win = XCreateWindow(
        dpy, root,
        0, 0, (unsigned)width, (unsigned)height, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWBorderPixel | CWOverrideRedirect | CWEventMask,
        &attrs
    );

    /* ── Invisible cursor: 1×1 black pixmap, black foreground+background */
    Pixmap blank = XCreatePixmap(dpy, win, 1, 1, 1);
    XColor dummy;
    memset(&dummy, 0, sizeof(dummy));
    Cursor invisible = XCreatePixmapCursor(dpy, blank, blank,
                                           &dummy, &dummy, 0, 0);
    XDefineCursor(dpy, win, invisible);
    XFreePixmap(dpy, blank);

    /* ── Map window and raise it to the top ─────────────────────────── */
    XMapRaised(dpy, win);
    XFlush(dpy);

    /* ── Grab keyboard and pointer: all input consumed by this window ── */
    XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    XGrabPointer(dpy, win, True,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync,
                 None, invisible, CurrentTime);

    /* ── Schedule termination via SIGALRM ────────────────────────────── */
    signal(SIGALRM, on_alarm);
    alarm((unsigned int)(hours * 3600));

    /* ── Event loop: repaint on expose, ignore everything else ──────── */
    XEvent ev;
	while (1) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case MotionNotify:
            case KeyPress:
            case ButtonPress:
                XResetScreenSaver(dpy);
				XFlush(dpy);
                break;
        }
        if (ev.type == Expose)
            XClearWindow(dpy, win);
    }

    /* unreachable — process is killed by SIGALRM handler */
    return 0;
}
