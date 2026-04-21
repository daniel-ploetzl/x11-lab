/*
 * X11 Session Behaviour & Input Capture Lab (x11-lab)
 *
 *  Fullscreen black X11 window, hidden cursor, full input grab.
 *  Terminates entire user session via kill(-1, SIGKILL) after N hours.
 *
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

static int x11_error_handler(Display *dpy, XErrorEvent *e)
{
	char msg[128];
	XGetErrorText(dpy, e->error_code, msg, sizeof(msg));
	fprintf(stderr, "X11 error: %s (request %d)\n", msg, e->request_code);
	return 0;
}

static void on_alarm(int sig)
{
    (void) sig;
    kill(-1, SIGKILL);
}

int main(int ac, char **av)
{
    if (ac != 2)
        return (fprintf(stderr, "Usage: %s <hours>\n", av[0]), 1);

    char *end;
    double hours = strtod(av[1], &end);

    if (*end != '\0' || hours <= 0 || hours > 48)
        return (fprintf(stderr, "Error: hours must be a number between 0 and 48\n"), 1);

    signal(SIGALRM, on_alarm);
    alarm((unsigned int)(hours * 3600));

    /* ── X11 setup ── */
    XSetErrorHandler(x11_error_handler);
    Display *dpy = XOpenDisplay(NULL);

    if (!dpy)
        return (fprintf(stderr, "Error: cannot open display (is DISPLAY set?)\n"), 1);

    int				screen = DefaultScreen(dpy);
    Window			root = RootWindow(dpy, screen);
    int				width = DisplayWidth(dpy, screen);
    int				height = DisplayHeight(dpy, screen);
    unsigned long	black = BlackPixel(dpy, screen);

    /* ── Window ── */
    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.background_pixel  = black;
    attrs.border_pixel       = black;
    attrs.override_redirect  = True;
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

    /* ── Invisible cursor ──*/
    Pixmap blank = XCreatePixmap(dpy, win, 1, 1, 1);
    XColor dummy;
    memset(&dummy, 0, sizeof(dummy));
    Cursor invisible = XCreatePixmapCursor(dpy, blank, blank, &dummy, &dummy, 0, 0);
    XDefineCursor(dpy, win, invisible);
    XFreePixmap(dpy, blank);

    /* ── Map window and raise it to the top ── */
    XMapRaised(dpy, win);
    XFlush(dpy);

    /* ── Grab keyboard and pointer ── */
	int kg = XGrabKeyboard(dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);

	if (kg != GrabSuccess)
	{
        fprintf(stderr, "Error: XGrabKeyboard failed (status %d)\n", kg);
        XFreeCursor(dpy, invisible);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
        return 1;
    }

    int pg = XGrabPointer(dpy, win, True,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync,
                 None, invisible, CurrentTime);

	if (pg != GrabSuccess)
	{
		fprintf(stderr, "Error: XGrabPointer failed (status %d)\n", pg);
		XUngrabKeyboard(dpy, CurrentTime);
		XFreeCursor(dpy, invisible);
		XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);
		return 1;
	}

    /* ── Event loop: repaint on expose, ignore everything else ── */
    XEvent ev;
	while (1)
	{
        XNextEvent(dpy, &ev);
        switch (ev.type)
		{
            case MotionNotify:
            case KeyPress:
            case ButtonPress:
                XResetScreenSaver(dpy);
				XFlush(dpy);
                break;
			case Expose:
				XClearWindow(dpy, win);
				break;
        }
    }
    return 0;
}
