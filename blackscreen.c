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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define MAX_HOURS 48.0
#define SECONDS_PER_HOUR 3600

typedef struct s_app
{
	Display			*dpy;
	int				screen;
	Window			root;
	Window			win;
	int				width;
	int				height;
	unsigned long	black;
	Cursor			cursor;
} t_app;

static int x11_error_handler(Display *dpy, XErrorEvent *e)
{
	char msg[128];

	assert(dpy != NULL);
	assert(e != NULL);
	XGetErrorText(dpy, e->error_code, msg, sizeof(msg));
	fprintf(stderr, "X11 error: %s (request %d)\n", msg, e->request_code);
	return 0;
}

static void on_alarm(int sig)
{
	(void) sig;
	kill(-1, SIGKILL);
}

/* Parse the hours argument and validate its range. Returns 0 on success. */
static int parse_hours(const char *arg, double *out)
{
	char	*end;
	double	hours;

	assert(arg != NULL);
	assert(out != NULL);
	hours = strtod(arg, &end);
	if (*end != '\0' || hours <= 0 || hours > MAX_HOURS)
	{
		fprintf(stderr, "Error: hours must be a number between 0 and 48\n");
		return 1;
	}
	*out = hours;
	return 0;
}

/* Arm the session kill timer. */
static void arm_timer(double hours)
{
	assert(hours > 0);
	signal(SIGALRM, on_alarm);
	alarm((unsigned int)(hours * SECONDS_PER_HOUR));
}

/* Open the display and cache the screen geometry needed. Returns 0 on success. */
static int open_display(t_app *app)
{
	assert(app != NULL);
	XSetErrorHandler(x11_error_handler);
	app->dpy = XOpenDisplay(NULL);
	if (!app->dpy)
	{
		fprintf(stderr, "Error: cannot open display (is DISPLAY set?)\n");
		return 1;
	}
	app->screen = DefaultScreen(app->dpy);
	app->root = RootWindow(app->dpy, app->screen);
	app->width = DisplayWidth(app->dpy, app->screen);
	app->height = DisplayHeight(app->dpy, app->screen);
	app->black = BlackPixel(app->dpy, app->screen);
	return 0;
}

/* Create the fullscreen override redirect black window. */
static void create_window(t_app *app)
{
	XSetWindowAttributes attrs;

	assert(app != NULL);
	assert(app->dpy != NULL);
	memset(&attrs, 0, sizeof(attrs));
	attrs.background_pixel = app->black;
	attrs.border_pixel = app->black;
	attrs.override_redirect = True;
	attrs.event_mask = KeyPressMask | ButtonPressMask
		| PointerMotionMask | ExposureMask;
	app->win = XCreateWindow(
		app->dpy, app->root,
		0, 0, (unsigned)app->width, (unsigned)app->height, 0,
		CopyFromParent, InputOutput, CopyFromParent,
		CWBackPixel | CWBorderPixel | CWOverrideRedirect | CWEventMask,
		&attrs);
}

/* Build a 1x1 fully transparent cursor and attach it to the window. */
static void hide_cursor(t_app *app)
{
	Pixmap	blank;
	XColor	dummy;

	assert(app != NULL);
	assert(app->dpy != NULL);
	blank = XCreatePixmap(app->dpy, app->win, 1, 1, 1);
	memset(&dummy, 0, sizeof(dummy));
	app->cursor = XCreatePixmapCursor(app->dpy, blank, blank,
			&dummy, &dummy, 0, 0);
	XDefineCursor(app->dpy, app->win, app->cursor);
	XFreePixmap(app->dpy, blank);
}

/* Release every X resource held, then close the display. */
static void cleanup(t_app *app)
{
	assert(app != NULL);
	if (!app->dpy)
		return ;
	if (app->cursor)
		XFreeCursor(app->dpy, app->cursor);
	if (app->win)
		XDestroyWindow(app->dpy, app->win);
	XCloseDisplay(app->dpy);
}

/* Grab the keyboard and pointer so no input reaches anything else. */
static int grab_input(t_app *app)
{
	int	kg;
	int	pg;

	assert(app != NULL);
	assert(app->dpy != NULL);
	kg = XGrabKeyboard(app->dpy, app->win, True,
			GrabModeAsync, GrabModeAsync, CurrentTime);
	if (kg != GrabSuccess)
	{
		fprintf(stderr, "Error: XGrabKeyboard failed (status %d)\n", kg);
		return 1;
	}
	pg = XGrabPointer(app->dpy, app->win, True,
			ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
			GrabModeAsync, GrabModeAsync, None, app->cursor, CurrentTime);
	if (pg != GrabSuccess)
	{
		fprintf(stderr, "Error: XGrabPointer failed (status %d)\n", pg);
		XUngrabKeyboard(app->dpy, CurrentTime);
		return 1;
	}
	return 0;
}

/* Repaint on expose, poke the screensaver on input, ignore the rest. */
static void event_loop(t_app *app)
{
	XEvent	ev;

	assert(app != NULL);
	assert(app->dpy != NULL);
	while (1)
	{
		XNextEvent(app->dpy, &ev);
		switch (ev.type)
		{
			case MotionNotify:
			case KeyPress:
			case ButtonPress:
				XResetScreenSaver(app->dpy);
				XFlush(app->dpy);
				break ;
			case Expose:
				XClearWindow(app->dpy, app->win);
				break ;
		}
	}
}

int main(int ac, char **av)
{
	t_app	app;
	double	hours;

	if (ac != 2)
		return (fprintf(stderr, "Usage: %s <hours>\n", av[0]), 1);

	if (parse_hours(av[1], &hours))
		return 1;

	arm_timer(hours);
	memset(&app, 0, sizeof(app));

	if (open_display(&app))
		return 1;

	create_window(&app);
	hide_cursor(&app);
	XMapRaised(app.dpy, app.win);
	XFlush(app.dpy);

	if (grab_input(&app))
		return (cleanup(&app), 1);

	event_loop(&app);
	cleanup(&app);
	return 0;
}
