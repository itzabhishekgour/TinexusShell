/*
 * tools/test_x11_client.c
 * Standalone real X11 test client using Xlib to verify XWayland server,
 * window mapping, scene graph integration, and screencopy.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static volatile int g_running = 1;

static void handle_sig(int sig) {
    (void)sig;
    g_running = 0;
}

int main(int argc, char* argv[]) {
    int timeout_sec = 10;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            timeout_sec = atoi(argv[++i]);
        }
    }

    signal(SIGTERM, handle_sig);
    signal(SIGINT, handle_sig);

    const char* disp_env = getenv("DISPLAY");
    printf("[test_x11_client] Starting... DISPLAY='%s'\n", disp_env ? disp_env : "(null)");
    fflush(stdout);

    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "[test_x11_client] ERROR: Cannot open X11 display (getenv('DISPLAY')='%s')!\n",
                disp_env ? disp_env : "(null)");
        return 1;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    unsigned long border = BlackPixel(dpy, screen);
    unsigned long bg = WhitePixel(dpy, screen);

    // Create a 400x300 window
    Window win = XCreateSimpleWindow(dpy, root, 100, 100, 400, 300, 2, border, bg);
    if (!win) {
        fprintf(stderr, "[test_x11_client] ERROR: Failed to create X11 simple window!\n");
        XCloseDisplay(dpy);
        return 2;
    }

    // Set Window title and class
    XStoreName(dpy, win, "XWayland Verification Window");
    XClassHint class_hint;
    class_hint.res_name = (char*)"x11_test_client";
    class_hint.res_class = (char*)"X11TestClient";
    XSetClassHint(dpy, win, &class_hint);

    // Set WM_PROTOCOLS for graceful close
    Atom wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete_window, 1);

    // Select input events
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask);

    // Create GC with distinct Tinexus Royal Blue (#1f5fe0)
    GC gc = XCreateGC(dpy, win, 0, NULL);
    Colormap cm = DefaultColormap(dpy, screen);
    XColor blue_col;
    blue_col.red = 0x1f1f;
    blue_col.green = 0x5f5f;
    blue_col.blue = 0xe0e0;
    blue_col.flags = DoRed | DoGreen | DoBlue;
    if (XAllocColor(dpy, cm, &blue_col)) {
        XSetForeground(dpy, gc, blue_col.pixel);
    } else {
        XSetForeground(dpy, gc, BlackPixel(dpy, screen));
    }

    // Map the window
    XMapWindow(dpy, win);
    XFlush(dpy);
    printf("[test_x11_client] Window created and mapped successfully (win=0x%lx).\n", (unsigned long)win);
    fflush(stdout);

    int elapsed = 0;
    while (g_running && (timeout_sec <= 0 || elapsed < timeout_sec * 10)) {
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                // Fill inner rect with blue color
                XFillRectangle(dpy, win, gc, 20, 20, 360, 260);
                XFlush(dpy);
            } else if (ev.type == ClientMessage) {
                if ((Atom)ev.xclient.data.l[0] == wm_delete_window) {
                    g_running = 0;
                }
            }
        }
        usleep(100000); // 100ms
        elapsed++;
    }

    printf("[test_x11_client] Exiting cleanly after %d ms.\n", elapsed * 100);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
