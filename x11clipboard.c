/*
 * Xlib Selection test program.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define ATOMNAME(a) a == None ? "None" : XGetAtomName(display,a)

static Display *display;
static Window window;
static XEvent event;
static Atom xa_ae_clipboard_target;
static Atom xa_ae_clipboard_string;
static Atom xa_ae_clipboard_utf8_string;
static Atom xa_ae_clipboard_text;
static Atom xa_targets;
static Atom xa_text;
static Atom xa_utf8_string;

static Time seltime;

void ownselect()
{
    Window owner;
    puts("> set-selection-owner");
    XSetSelectionOwner(display, XA_PRIMARY, window, seltime);
    owner = XGetSelectionOwner(display, XA_PRIMARY);
    if (owner != window)
        puts("failed to own primary");
}

void deselect()
{
    Window owner;
    puts("> unset-selection-owner");
    XSetSelectionOwner(display, XA_PRIMARY, None, seltime);
    owner = XGetSelectionOwner(display, XA_PRIMARY);
    if (owner != None)
        puts("failed to disown primary");
}

void ask_paste()
{
    puts("! deleting properties on window");
    XDeleteProperty(display, window, xa_ae_clipboard_target);
    XDeleteProperty(display, window, xa_ae_clipboard_string);
    XDeleteProperty(display, window, xa_ae_clipboard_utf8_string);
    XDeleteProperty(display, window, xa_ae_clipboard_text);

    puts("> convert-selection TARGETS");
    XConvertSelection(display, XA_PRIMARY, xa_targets, xa_ae_clipboard_target,
                      window, CurrentTime);
    puts("> convert-selection STRING");
    XConvertSelection(display, XA_PRIMARY, XA_STRING, xa_ae_clipboard_string,
                      window, CurrentTime);
    puts("> convert-selection UTF8_STRING");
    XConvertSelection(display, XA_PRIMARY, xa_utf8_string,
                      xa_ae_clipboard_utf8_string,
                      window, CurrentTime);
    puts("> convert-selection TEXT");
    XConvertSelection(display, XA_PRIMARY, xa_text, xa_ae_clipboard_text,
                      window, CurrentTime);
}

void recv_paste(XSelectionEvent *selevent)
{
    Atom typeret;
    int format;
    unsigned long nitems, bytesleft;
    unsigned char *data;

    XGetWindowProperty(display, window, selevent->property,
                       0L, 4096L, False,
                       AnyPropertyType, &typeret,
                       &format, &nitems, &bytesleft,
                       &data);

    printf("< get-prop %s:", ATOMNAME(typeret));

    if (format == 32) {
        Atom* atomp = (Atom*)data;
        int i;
        printf("[");
        for (i=0; i < nitems; i++) {
            printf("%s", ATOMNAME(atomp[i]));
            if (i < nitems-1)
                putchar(',');
        }
        putchar(']');
        putchar('\n');
    }

    else if (format == 8) {
        int i;
        printf(" \"");
        for (i=0; i < nitems; i++) {
            putchar(data[i]);
        }
        putchar('"');
        putchar('\n');
    }

    else {
        printf(" fmt=%d nitems=%d\n", format, nitems);
    }

    XDeleteProperty(display, window, selevent->property);
}

void send_copy(XSelectionRequestEvent *reqevent)
{
    XEvent newevent;
    char buf[256];

    /* support obsolete clients */
    if (reqevent->property == None) {
        reqevent->property = reqevent->target;
    }

    /* prepare notify event */
    newevent.xselection.type = SelectionNotify;
    //newevent.xselection.serial = 0; // ???
    //newevent.xselection.send_event = True;
    //newevent.xselection.display = display;

    newevent.xselection.requestor = reqevent->requestor;
    newevent.xselection.selection = reqevent->selection;
    newevent.xselection.target = reqevent->target;
    newevent.xselection.property = reqevent->property;
    newevent.xselection.time = reqevent->time;

    /* convert to target format... */
    if (reqevent->target == XA_STRING) {
        puts("> sending text data");
        strcpy(buf, "STRING: Hello, world!");

        /* transmit data to property on requestors window */
        XChangeProperty(display, reqevent->requestor,
                        reqevent->property, reqevent->target,
                        8, PropModeReplace,
                        buf, strlen(buf));
    }
    else if (reqevent->target == xa_targets) {
        /* transmit data to property on requestors window */
        puts("> sending targets list");
        Atom alist[2];
        alist[0] = xa_targets;
        alist[1] = XA_STRING;
        XChangeProperty(display, reqevent->requestor,
                        reqevent->property, reqevent->target,
                        32, PropModeReplace,
                        (unsigned char*)alist, 2);
    }
    else {
        /* set property to None if we can't convert */
        puts("> sending none");
        newevent.xselection.property = None;
    }

    /* send a notify event to let requestor know the data is ready */
    XSendEvent(display, reqevent->requestor, False,
               SelectionNotify, &newevent);
}

int main(int argc, char **argv)
{
    display = XOpenDisplay(NULL);
    assert(display);

    xa_targets = XInternAtom(display, "TARGETS", False);
    xa_utf8_string = XInternAtom(display, "UTF8_STRING", False);
    xa_text = XInternAtom(display, "TEXT", False);

    xa_ae_clipboard_target = XInternAtom(display, "AECLIP_TARGET", False);
    xa_ae_clipboard_string = XInternAtom(display, "AECLIP_STRING", False);
    xa_ae_clipboard_utf8_string = XInternAtom(display, "AECLIP_UTF8", False);
    xa_ae_clipboard_text = XInternAtom(display, "AECLIP_TEXT", False);

    window = XCreateSimpleWindow(display,
                                 DefaultRootWindow(display),
                                 10, 10,
                                 200, 200,
                                 1,
                                 CopyFromParent,
                                 CopyFromParent);
    XSelectInput(display, window, ButtonPressMask | PropertyChangeMask);
    XMapWindow(display, window);

    XFlush(display);

    while (1)
    {
        XNextEvent(display, &event);

        switch (event.type)
        {
            case ButtonPress:
                puts("ButtonPress");
                if (event.xbutton.button == 1) {
                    seltime = event.xbutton.time;
                    ownselect();
                }
                else if (event.xbutton.button == 2) {
                    ask_paste();
                }
                else if (event.xbutton.button == 3) {
                    deselect();
                }
                break;

            case ClientMessage:
                puts("ClientMessage");
                break;

            case SelectionClear:
                printf("SelectionClear sel=%s\n",
                       ATOMNAME(event.xselectionclear.selection));
                break;

            case SelectionNotify:
                printf("SelectionNotify sel=%s tgt=%s prop=%s\n",
                       ATOMNAME(event.xselection.selection),
                       ATOMNAME(event.xselection.target),
                       ATOMNAME(event.xselection.property)
                      );
                if (event.xselection.property != None)
                    recv_paste(&event.xselection);
                XFlush(display);
                break;

            case SelectionRequest:
                printf("SelectionRequest sel=%s tgt=%s prop=%s\n",
                       ATOMNAME(event.xselectionrequest.selection),
                       ATOMNAME(event.xselectionrequest.target),
                       ATOMNAME(event.xselectionrequest.property)
                      );
                send_copy(&event.xselectionrequest);
                XFlush(display);
                break;

            case PropertyNotify:
                printf("PropertyNotify atom=%s [%s]\n",
                       ATOMNAME(event.xproperty.atom),
                       event.xproperty.state == PropertyNewValue ?
                       "new" : "delete"
                      );
        }
    }

    return 0;
}
