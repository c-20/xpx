#if 0
nano $0
exit 0
#endif

#include <assert.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>

#include <cairo.h>
#include <cairo-xlib.h>

#include <chrono>
#include <thread>

#define MSG  printf
#define RT   return
#define IF   if
#define EF   else if
#define EL   else
#define INT  int
#define FP   double

#define XPXSTARTWIDTH    10
#define XPXSTARTHEIGHT   10

#define CLEARWINDOWBORDERWIDTH   10
#define REGIONWIDTHPERCENT       16


#define MILLISECONDS(ms)      \
  std::chrono::milliseconds(ms)
#define MSLEEP(ms)                            \
  std::this_thread::sleep_for(MILLISECONDS(ms))

void clearall(cairo_t *cr) {
  cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
  cairo_paint(cr);
}

void clearsize(cairo_t *cr, int w, int h) {
  cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
  cairo_rectangle(cr, 0, 0, w, h);
  cairo_fill(cr);
//  cairo_paint(cs);
}

void clearwindow(cairo_t *cr, int w, int h, int bw) {
  int tlix = 0 + bw;
  int tliy = 0 + bw;
  int brix = w - bw - 1;
  int briy = h - bw - 1;
  int windowh = briy - tliy;
  int windoww = brix - tlix;
  cairo_set_source_rgb(cr, 1.0, 0.0, 1.0);
  cairo_rectangle(cr,    0,    0, bw, bw); // TL
  cairo_rectangle(cr, brix,    0, bw, bw); // TR
  cairo_rectangle(cr,    0, briy, bw, bw); // BL
  cairo_rectangle(cr, brix, briy, bw, bw); // BR
  cairo_fill(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
  cairo_rectangle(cr, tlix,    0, windoww, bw); // T
  cairo_rectangle(cr,    0, tliy, bw, windowh); // L
  cairo_rectangle(cr, brix, tliy, bw, windowh); // R
  cairo_rectangle(cr, tlix, briy, windoww, bw); // B
  cairo_fill(cr);
//  cairo_paint(cs);
}

const int VisualDepth  = 32;
const int VisualColour = TrueColor;
#define TRUECOLOUR  VisualDepth, VisualColour
//#define TRUECOLOURWINDOW  VisualDepth, \
//                          InputOutput, \
//                          VisualColour
// can also use CopyFromParent for depth and visual
// safer if settruecolour doesn't continue since
// maybe it regresses well in cairo ? idk
//#define TRUECOLOURWINDOW  CopyFromParent, \
//                          InputOutput,    \
//                          CopyFromParent
// OR NOT?!?!?
#define TRUECOLOURDEPTH   VisualDepth
#define TRUECOLOURVISUAL  VisualColour
#define HARDWAREDISPLAYNAME  ":0"

typedef struct _TCDisplay {
  Display *display;
  Window rootwindow;
  int screennumber;
  int depth;
  XVisualInfo visinfo;
} TCDisplay;

TCDisplay truecolourdisplay() {
  TCDisplay tcd;
  tcd.display = XOpenDisplay(HARDWAREDISPLAYNAME);
  tcd.rootwindow = DefaultRootWindow(tcd.display);
  tcd.screennumber = XDefaultScreen(tcd.display);
  tcd.depth = XDefaultDepth(tcd.display, tcd.screennumber);
MSG("TCD DEPTH: %d\n", tcd.depth);
  if (!XMatchVisualInfo(tcd.display, tcd.screennumber,
                        tcd.depth, TrueColor, &(tcd.visinfo)))
    { MSG("failed to set TRUECOLOUR mode\n"); }
  return tcd;
}

void freetruecolourdisplay(TCDisplay *tcd) {
  if (tcd && tcd->display)
    { XCloseDisplay(tcd->display); }
}

typedef struct _AnimAction {
  int startx, starty;
  int startwidth, startheight;
  int targetx, targety;
  int targetwidth, targetheight;
  int actiontime, totaltime;
  struct _AnimAction *next;
} AnimAction;

typedef struct _MoveWindow {
  int startx, starty;
  int startwidth, startheight;
  int cursorstartx, cursorstarty;
  int started;
  int xdiff, ydiff;
  int widthdiff, heightdiff;
} MoveWindow;

typedef struct _WindowState {
  TCDisplay tcdisplay;
//  Display *display;
  Window window;
  Screen *screen;
  int x, y;
  int width, height;
  int maxwidth, maxheight;
  cairo_surface_t *surface;
  cairo_t *context;
  AnimAction *animation;
  MoveWindow movewindow;
  int currentstatus;
} WindowState;

WindowState windowstate(TCDisplay tcd, Window w) {
  XWindowAttributes wa;
  XGetWindowAttributes(tcd.display, w, &wa);
  WindowState ws =
    { tcdisplay: tcd,
      window: w, screen: wa.screen,
      x: wa.x, y: wa.y,
      width: wa.width, height: wa.height,
      maxwidth: wa.width, maxheight: wa.height,
      surface: NULL, context: NULL,
      animation: NULL,
      movewindow: (MoveWindow){ started: 0 }   };
  return ws;
}

WindowState rootwindowstate(TCDisplay tcd) {
  Window scr = tcd.rootwindow;
  WindowState screen = windowstate(tcd, scr);
  if (screen.width < 1 || screen.height < 1)
    { MSG("screen size error\n"); exit(1); }
  return screen;
}

WindowState screenstate(TCDisplay tcd) {
  return rootwindowstate(tcd);
}

WindowState newxpx(WindowState parent) {
  TCDisplay tcd = parent.tcdisplay;
  Display *d = tcd.display;
  Window   w = parent.window;
  Visual  *v = tcd.visinfo.visual;
  // expect parent (screen) x and y to be 0 for now
  // or assume child x and y is relative to parent
  int centrex = parent.width  / 2;
  int centrey = parent.height / 2;
  int width  = XPXSTARTWIDTH;
  int height = XPXSTARTHEIGHT;
  int borderwidth = 0;
  XSetWindowAttributes attribs;
  attribs.override_redirect = true;
  attribs.colormap = XDefaultColormap(d, tcd.screennumber);
//XCreateColormap(d, w, v, AllocNone);

  attribs.backing_store = Always;
  attribs.save_under = True;
//  attribs.background_pixel = None;
  attribs.background_pixmap = ParentRelative; //None;
//MSG("None: %d\n", None);
  attribs.border_pixel = 0;
  unsigned long attribmask = CWOverrideRedirect |
        CWBackingStore | CWSaveUnder |
        CWColormap | CWBackPixmap | CWBorderPixel;
  Window pixel = XCreateWindow(d, w, centrex, centrey,
    width, height, borderwidth, tcd.depth, //TrueColor,
//TRUECOLOURDEPTH,
    InputOutput, v, attribmask, &attribs              );
//  XMapWindow(d, pixel); // ws.window);
  XMapRaised(d, pixel);
  int clearevents = False;
  XSync(d, clearevents);
  WindowState ws = windowstate(tcd, pixel);
  if (ws.window != pixel) { MSG("xpx error\n"); }
  // ^ impossible
  ws.maxwidth  = parent.width;
  ws.maxheight = parent.height;
  ws.surface = cairo_xlib_surface_create(d, pixel, v,
                            ws.maxwidth, ws.maxheight);
  if (!ws.surface) { MSG("cairo surface error\n"); }
  ws.context = cairo_create(ws.surface);
  if (!ws.context) { MSG("cairo context error\n"); }
  return ws;
}

void freexpx(WindowState *xpx) {
  if (xpx) {
    if (xpx->context)
      { cairo_destroy(xpx->context); }
    if (xpx->surface)
      { cairo_surface_destroy(xpx->surface); }
    if (xpx->tcdisplay.display)
      { XUnmapWindow(xpx->tcdisplay.display, xpx->window); }
  }
}

void stretchxpx(WindowState *xpx, int dx, int dy,
                                  int dw, int dh, int dt) {
  AnimAction *aa = (AnimAction *)malloc(sizeof(AnimAction));
  AnimAction *xa = xpx->animation;
  if (!xa) { // inherit geometry from idle size
    aa->startx      = xpx->x;
    aa->starty      = xpx->y;
    aa->startwidth  = xpx->width;
    aa->startheight = xpx->height;
    xpx->animation = aa;
  } else { // inherit geometry from prev animaction
    while (xa->next) { xa = xa->next; }
    aa->startx      = xa->targetx;
    aa->starty      = xa->targety;
    aa->startwidth  = xa->targetwidth;
    aa->startheight = xa->targetheight;
    xa->next = aa;
  }
  aa->targetx      = aa->startx + dx;
  aa->targety      = aa->starty + dy;
  aa->targetwidth  = aa->startwidth + dw;
  aa->targetheight = aa->startheight + dh;
  aa->actiontime = 0;
  aa->totaltime  = dt;
  aa->next = NULL;
}

void stretchxpxud(WindowState *xpx, int height, float t) {
  int heightdiff = height - xpx->height;
  int halfheightdiff = heightdiff / 2;
  int dt = (int)(t * 1000); // to ms
  stretchxpx(xpx, 0, -halfheightdiff, 0, heightdiff, dt);
}

void stretchxpxlr(WindowState *xpx, int width, float t) {
  int widthdiff = width - xpx->width;
  int halfwidthdiff = widthdiff / 2;
  int dt = (int)(t * 1000); // to ms
  stretchxpx(xpx, -halfwidthdiff, 0, widthdiff, 0, dt);
}

typedef struct _PointerState {
  Window root;
  int rootx, rooty;
  Window child;
  int windowx, windowy;
  unsigned int mask;
} PointerState;

PointerState pointerstate(TCDisplay tcd, Window w) {
  Display *d = tcd.display;
  PointerState ps;
  XQueryPointer(d, w, &ps.root, &ps.child, &ps.rootx,
    &ps.rooty, &ps.windowx, &ps.windowy, &ps.mask    );
  // if false returned, pointer is not on
  // the same screen as the specified window
  // no child window,x,y
  return ps;
}

typedef struct _KeyboardState {
  char keymap[32];
  char ascii[256];
} KeyboardState;

KeyboardState keyboardstate(TCDisplay tcd, Window w) {
  Display *d = tcd.display;
  KeyboardState ks;
  XQueryKeymap(d, ks.keymap);
  int ix = -1;
  while (++ix < 256) {
    int keyval = ks.keymap[ix >> 3] & (1 << (ix & 7));
//    int keycode = XKeysymToKeycode(d, ix);
    // e -> 0 -- this is backwards?
    int keycode = XKeysymToKeycode(d, ix);
    if (keycode < 0 || keycode > 255) {
      MSG("keycode for %d out of range\n", ix);
    } else if (keyval != 0) {
      ks.ascii[keycode] = 1;
      MSG("keysym %d keycode down: %d\n", ix, keycode);
    } else { ks.ascii[keycode] = 0; }
  }
  return ks;
}


void setcolour(cairo_t *cr, char code, double alpha) {
  int r = 0, g = 0, b = 0; // black default
  if (code == 'R') { r = 255; g =   0; b =   0; }
  if (code == 'Y') { r = 255; g = 255; b =   0; }
  if (code == 'G') { r =   0; g = 255; b =   0; }
  if (code == 'C') { r =   0; g = 255; b = 255; }
  if (code == 'B') { r =   0; g =   0; b = 255; }
  if (code == 'M') { r = 255; g =   0; b = 255; }
  if (code == 'W') { r = 255; g = 255; b = 255; }
  if (code == 'K') { r =   0; g =   0; b =   0; }
  double red   = ((double)r) / 255.0;
  double green = ((double)g) / 255.0;
  double blue  = ((double)b) / 255.0;
//  double alpha = 255.0;
  cairo_set_source_rgba(cr, red, green, blue, alpha);
}

void fillrectangle(cairo_t *cr, int x, int y, int w, int h) {
  cairo_rectangle(cr, x, y, w, h);
  cairo_fill(cr);
}

void draw(cairo_t *cr, PointerState ps, KeyboardState ks) {
  setcolour(cr, 'R', 0.75);
  fillrectangle(cr, ps.windowx, ps.windowy, 10, 10);
}

#define MOVINGCONTENT    1
#define MOVINGWINDOW     2
#define SIZINGTL         3
#define SIZINGTR         4
#define SIZINGTOP        5
#define SIZINGLEFT       6
#define SIZINGRIGHT      7
#define SIZINGBOTTOM     8
#define SIZINGBL         9
#define SIZINGBR        10

#define UPDATEFRAMETIME 100

void screenbound(TCDisplay tcd, WindowState *xpx) {
  WindowState screen = screenstate(tcd);
// no, these are unbound
  int refx = xpx->x;
  int refy = xpx->y;
  int refwidth  = xpx->width;
  int refheight = xpx->height;
//  if (xpx->movewindow.started) {
//    refwidth  = xpx->movewindow.startwidth
//    refheight = xpx->movewindow.startheight;
//  } // active move uses start size .....

  if (xpx->x < 0) { xpx->x = 0; }
  if (xpx->y < 0) { xpx->y = 0; }
  if (xpx->width + xpx->x > screen.width)
    { xpx->width = screen.width - xpx->x; }
  if (xpx->height + xpx->y > screen.height)
    { xpx->height = screen.height - xpx->y; }
  if (xpx->width  < XPXSTARTWIDTH)
    { xpx->width  = XPXSTARTWIDTH; }
  if (xpx->height < XPXSTARTHEIGHT)
    { xpx->height = XPXSTARTHEIGHT; }
//  XMoveResizeWindow(tcd.display, xpx->window,
//      xpx->x, xpx->y, 1, 1); // redraw beneath
// ^ triggers focus change in window below
  // how to redraw underneath ......
  //XExpose....
//  if (   refwidth  != xpx->width
//      || refheight != xpx->height) {
    XMoveResizeWindow(tcd.display, xpx->window,
        xpx->x, xpx->y, xpx->width, xpx->height);
//  } else if (   refx != xpx->x
//             || refy != xpx->y) {
//    XMoveWindow(tcd.display, xpx->window, xpx->x, xpx->y);
//  } else { // if (xpx->movewindow.started) {
/*
    // redraw while moving/resizing
    if (   xpx->width  != xpx->movewindow.startwidth
        || xpx->height != xpx->movewindow.startheight) {
      XMoveResizeWindow(tcd.display, xpx->window,
          xpx->x, xpx->y, xpx->width, xpx->height);
    } else if (   xpx->x != xpx->movewindow.startx
               || xpx->y != xpx->movewindow.starty) {
      XMoveWindow(tcd.display, xpx->window, xpx->x, xpx->y);
    } else { MSG("STATIC WINDOW\n"); }
  } else if (xpx->animation) {
    MSG("ANIMATION\n");
//    AnimAction *tact = xpx->animation;
//    if (   xpx->width  != tact->startwidth
//        || xpx->height != tact->startheight) {
      XMoveResizeWindow(tcd.display, xpx->window,
          xpx->x, xpx->y, xpx->width, xpx->height);
//    } else if (   xpx->x != tact->startx
//               || xpx->y != tact->starty) {
//      XMoveWindow(tcd.display, xpx->window, xpx->x, xpx->y);
//    } else { MSG("STATIC ANIMATION\n"); }
  } else { MSG("CORRECTLY BOUND WINDOW\n"); }
*/
}

void screenclear(TCDisplay tcd, WindowState *xpx, int bw) {
  int w = xpx->width;
  int h = xpx->height;
  int tlix = 0 + bw;
  int tliy = 0 + bw;
  int brix = w - bw - 1;
  int briy = h - bw - 1;
  int windowh = briy - tliy + 1;
  int windoww = brix - tlix + 1;
  int generateexposeevents = 0;
  XClearArea(tcd.display, xpx->window, tlix, tliy,
             windoww, windowh, generateexposeevents);
  //int discardexcessevents = True; // faster move
  //int discardexcessevents = False;
  //XSync(tcd.display, discardexcessevents);
}

INT shiftbyregion(WindowState *xpx, PointerState ps) {
//  INT pointx = ps.rootx;
//  INT pointy = ps.rooty;
  INT wwidth  = xpx->width;
  INT wheight = xpx->height;
  INT minx = 0; //xpx->x;
  INT miny = 0; //xpx->y;
  INT maxx = minx + xpx->width;
  INT maxy = miny + xpx->height;
  INT rwidthpct = REGIONWIDTHPERCENT;
  INT rwidth  = rwidthpct * wwidth  / 100;
  INT rheight = rwidthpct * wheight / 100;
  INT pointx = ps.windowx;
  INT pointy = ps.windowy;
  IF (pointx < minx || pointx > maxx) { RT MOVINGCONTENT; }
  IF (pointy < miny || pointy > maxy) { RT MOVINGCONTENT; }
  INT minrx = minx + rwidth;
  INT minry = miny + rheight;
  INT maxrx = maxx - rwidth;
  INT maxry = maxy - rheight;
  INT midx = wwidth  / 2;
  INT midy = wheight / 2;
  IF (    minrx      < CLEARWINDOWBORDERWIDTH) { minrx = midx; }
  IF (    minry      < CLEARWINDOWBORDERWIDTH) { minry = midy; }
  IF ((maxx - maxrx) < CLEARWINDOWBORDERWIDTH) { maxrx = midx; }
  IF ((maxy - maxry) < CLEARWINDOWBORDERWIDTH) { maxry = midy; }
  IF (pointx < minrx && pointy < minry) { RT SIZINGTL;     }
  IF (pointx > maxrx && pointy < minry) { RT SIZINGTR;     }
  IF (                  pointy < minry) { RT SIZINGTOP;    }
  IF (pointx < minrx && pointy > maxry) { RT SIZINGBL;     }
  IF (pointx > maxrx && pointy > maxry) { RT SIZINGBR;     }
  IF (                  pointy > maxry) { RT SIZINGBOTTOM; }
  IF (pointx < minrx                  ) { RT SIZINGLEFT;   }
  IF (pointx > maxrx                  ) { RT SIZINGRIGHT;  }
  RT MOVINGWINDOW;
}

void drawregionoverlay(WindowState *xpx, PointerState ps) {
  cairo_t *cr = xpx->context;
  INT status = xpx->currentstatus;
  INT newstatus = shiftbyregion(xpx, ps);
  FP opacity = (newstatus == status) ? 0.6 : 0.3;
//  IF (status == MOVINGCONTENT) { RT; }
  IF (   status == SIZINGTL || status == SIZINGTR
      || status == SIZINGBL || status == SIZINGBR) {
    setcolour(cr, 'R', opacity);
  } EF (   status == SIZINGTOP  || status == SIZINGBOTTOM
        || status == SIZINGLEFT || status == SIZINGRIGHT ) {
    setcolour(cr, 'Y', opacity);
  } EL { setcolour(cr, 'G', opacity); } // presume MOVINGWINDOW
  IF (status == MOVINGCONTENT) {
MSG("MOVINGCONTENT newstatus = %d\n", newstatus);
    status = newstatus; // to follow mouse when unshifted but still
    newstatus = MOVINGCONTENT;
  } else { // follow action
    // status = status, newstatus = newstatus
//    status = newstatus;
//    newstatus = xpx->currentstatus; // compare, swap status vars
  }
//  } // follow mouse (not status) when unshifted
//  INT pointx = ps.rootx;
//  INT pointy = ps.rooty;
  INT wwidth  = xpx->width;
  INT wheight = xpx->height;
  INT minx = 0; // xpx->x; relative to window
  INT miny = 0; // xpx->y; relative to window
  INT maxx = minx + xpx->width - 1;
  INT maxy = miny + xpx->height - 1;
  INT rwidthpct = REGIONWIDTHPERCENT;
  INT rwidth  = rwidthpct * wwidth  / 100;
  INT rheight = rwidthpct * wheight / 100;
//  IF (pointx < minx || pointx > maxx) { RT; } // MOVINGCONTENT; }
//  IF (pointx < miny || pointx > maxy) { RT; } // MOVINGCONTENT; }
  INT minrx = minx + rwidth;
  INT minry = miny + rheight;
  INT maxrx = maxx - rwidth;
  INT maxry = maxy - rheight;
  INT midx = wwidth  / 2;
  INT midy = wheight / 2;
  IF (    minrx      < CLEARWINDOWBORDERWIDTH) { minrx = midx; }
  IF (    minry      < CLEARWINDOWBORDERWIDTH) { minry = midy; }
  IF ((maxx - maxrx) < CLEARWINDOWBORDERWIDTH) { maxrx = midx; }
  IF ((maxy - maxry) < CLEARWINDOWBORDERWIDTH) { maxry = midy; }
  INT cwidth  = maxrx - minrx;
  INT cheight = maxry - minry;
  INT pointx = ps.windowx;
  INT pointy = ps.windowy;
  IF (status == MOVINGWINDOW ) {
    fillrectangle(cr, minrx, minry, cwidth, cheight);
  } EF (status == SIZINGTL   ) {
    IF (status != newstatus && pointx > minrx) { minrx = pointx; }
    IF (status != newstatus && pointy > minry) { minry = pointy; }
    fillrectangle(cr,  minx,  miny, minrx - minx, minry - miny);
  } EF (status == SIZINGTOP  ) {
    IF (status != newstatus && pointy > minry) { minry = pointy; }
    fillrectangle(cr, minrx,  miny, cwidth, minry - miny);
  } EF (status == SIZINGTR   ) {
    IF (status != newstatus && pointx < maxrx) { maxrx = pointx; }
    IF (status != newstatus && pointy > minry) { minry = pointy; }
    fillrectangle(cr, maxrx,  miny, maxx - maxrx, minry - miny);
  } EF (status == SIZINGLEFT ) {
    IF (status != newstatus && pointx > minrx) { minrx = pointx; }
    fillrectangle(cr,  minx, minry, minrx - minx, cheight);
  } EF (status == SIZINGRIGHT) {
    IF (status != newstatus && pointx < maxrx) { maxrx = pointx; }
    fillrectangle(cr, maxrx, minry, maxx - maxrx, cheight);
  } EF (status == SIZINGBL   ) {
    IF (status != newstatus && pointx > minrx) { minrx = pointx; }
    IF (status != newstatus && pointy < maxry) { maxry = pointy; }
    fillrectangle(cr,  minx, maxry, minrx - minx, maxy - maxry);
  } EF (status == SIZINGBOTTOM) {
    IF (status != newstatus && pointy < maxry) { maxry = pointy; }
    fillrectangle(cr, minrx, maxry, cwidth, maxy - maxry);
  } EF (status == SIZINGBR) {
    IF (status != newstatus && pointx < maxrx) { maxrx = pointx; }
    IF (status != newstatus && pointy < maxry) { maxry = pointy; }
    fillrectangle(cr, maxrx, maxry, maxx - maxrx, maxy - maxry);
  } EF (status == MOVINGCONTENT) {
    MSG("MOVINGCONTENT -> MOVINGCONTENT\n");
  } EL { MSG("DRAWREGIONOVERLAY STATUSERROR\n"); }
}

void drawcontent(TCDisplay tcd, WindowState *xpx,
                 PointerState ps, KeyboardState ks) {
  int clearwinborderwidth = CLEARWINDOWBORDERWIDTH;
  clearwindow(xpx->context, xpx->width,
              xpx->height, clearwinborderwidth);
  screenclear(tcd, xpx, clearwinborderwidth);
  draw(xpx->context, ps, ks);
//  int status = shiftbyregion(xpx, ps);
  drawregionoverlay(xpx, ps); // status);
//  XFlush(tcd.display);
}

void updatexpx(WindowState *xpx, PointerState ps,
               KeyboardState ks, int status      ) {
  if (!xpx) { return; }
  TCDisplay tcd = xpx->tcdisplay;
  if (status == MOVINGCONTENT) {
    xpx->movewindow.started = 0; // stop move
    AnimAction *tact = NULL;
    if (xpx->animation) {
      tact = xpx->animation;
      int ttotal = tact->totaltime;
      tact->actiontime += UPDATEFRAMETIME;
      if (tact->actiontime >= ttotal) {
        xpx->x = tact->targetx;
        xpx->y = tact->targety;
        xpx->width  = tact->targetwidth;
        xpx->height = tact->targetheight;
        AnimAction *nact = tact->next;
        free(xpx->animation); // tact
        xpx->animation = nact;
      } else { // ^ handle next action on next update
        int tdiff = tact->actiontime;
        int xdiff = tact->targetx - tact->startx;
        int ydiff = tact->targety - tact->starty;
        int wdiff = tact->targetwidth  - tact->startwidth;
        int hdiff = tact->targetheight - tact->startheight;
        int xoff = xdiff * tdiff / ttotal;
        int yoff = ydiff * tdiff / ttotal;
        int woff = wdiff * tdiff / ttotal;
        int hoff = hdiff * tdiff / ttotal;
        xpx->x = tact->startx + xoff;
        xpx->y = tact->starty + yoff;
        xpx->width  = tact->startwidth  + woff;
        xpx->height = tact->startheight + hoff;
      } // ^ update size
      if (tact) { // action occurred
        screenbound(tcd, xpx);
      } // ^ propagate change to X11
    } // else no change to geometry
//    screenbound(tcd, xpx); // redraw beneath
//    screenclear(tcd, xpx); <-- in drawcontent
    drawcontent(tcd, xpx, ps, ks);
//    clear(xpx->context);
//    draw(xpx->context, ps);
//    XFlush(tcd.display);
    MSLEEP(UPDATEFRAMETIME);
  } else {
//  } else if (status == MOVINGWINDOW
//          || status == BLSIZINGWINDOW
//          || status == TRSIZINGWINDOW) {
    MoveWindow *movew = &(xpx->movewindow);
    // cannot use ps.windowx,x on a moving window
    if (!movew->started) {
      movew->startx = xpx->x; // declared,
      movew->starty = xpx->y; // not measured
      movew->startwidth  = xpx->width;
      movew->startheight = xpx->height;
      movew->cursorstartx = ps.rootx;
      movew->cursorstarty = ps.rooty;
      movew->started = 1;
      movew->xdiff = 0;
      movew->ydiff = 0;
      movew->widthdiff  = 0;
      movew->heightdiff = 0;
    } else {
      movew->xdiff = ps.rootx - movew->cursorstartx;
      movew->ydiff = ps.rooty - movew->cursorstarty;
      if (status == MOVINGWINDOW) {
        xpx->x = movew->startx + movew->xdiff;
        xpx->y = movew->starty + movew->ydiff;
      } else if (   status == SIZINGTL || status == SIZINGTR
                 || status == SIZINGTOP                     ) {
        xpx->y = movew->starty + movew->ydiff;
        xpx->height = movew->startheight - movew->ydiff;
        IF (status == SIZINGTL) {
          xpx->x = movew->startx + movew->xdiff;
          xpx->width  = movew->startwidth - movew->xdiff;
        } EF (status == SIZINGTR) {
  //        { xpx->x = movew->startx - movew->xdiff;
          xpx->width  = movew->startwidth + movew->xdiff;
        }
      } else if (   status == SIZINGBL || status == SIZINGBR
                 || status == SIZINGBOTTOM                  ) {
//        xpx->y = movew->starty - movew->ydiff;
        xpx->height = movew->startheight + movew->ydiff;
        IF (status == SIZINGBL) {
          xpx->x = movew->startx + movew->xdiff;
          xpx->width  = movew->startwidth - movew->xdiff;
        } EF (status == SIZINGBR) {
          // { xpx->x = movew->startx - movew->xdiff;
          xpx->width  = movew->startwidth + movew->xdiff;
        }
      } else if (status == SIZINGLEFT) {
        xpx->x = movew->startx + movew->xdiff;
        xpx->width  = movew->startwidth - movew->xdiff;
      } else if (status == SIZINGRIGHT) {
//        xpx->x = movew->startx + movew->xdiff;
        xpx->width  = movew->startwidth - movew->xdiff;
      } else { MSG("invalid status\n"); }
    }
    // consider move by keyboard
    // where are arrow keys
    screenbound(tcd, xpx);
    drawcontent(tcd, xpx, ps, ks);
  } // no sleep during move/resize
//  int discardexcessevents = True; // faster move
//  XSync(tcd.display, discardexcessevents);
}

INT main() {
MSG("INIT\n");
  TCDisplay tcd = truecolourdisplay();
MSG("TCD\n");
  WindowState screen = screenstate(tcd);
MSG("screenstate\n");
  if (screen.window != tcd.rootwindow)
    { MSG("unexpected tcd root window\n"); }
  printf("w: %d, h: %d\n", screen.width, screen.height);
  WindowState xpx = newxpx(screen);
  clearall(xpx.context);
  stretchxpxlr(&xpx, 400,  0.5);
  stretchxpxud(&xpx, 800,  0.8);
 // stretchxpxlr(&xpx, 200, 2000);
  INT events = ExposureMask
//             | KeyPressMask | KeyReleaseMask
//             | ButtonPressMask | ButtonReleaseMask
//             | EnterWindowMask | LeaveWindowMask
              | PointerMotionMask;
  XSelectInput(tcd.display, xpx.window, events);

  INT status = MOVINGCONTENT;
  xpx.currentstatus = status;
  while (1) {
    XEvent event;
    XNextEvent(tcd.display, &event);
    PointerState ps = pointerstate(tcd, xpx.window);
    // ^ need ps for non-motion events too
    if (event.type == MotionNotify) {
      // should this use xpx.window ?
      // shift = TL, ctrl = BR, ctrl+shift = TLBR
      if (ps.mask & ShiftMask) {
        if (status == MOVINGCONTENT) {
          status = shiftbyregion(&xpx, ps);
          xpx.currentstatus = status;
MSG("NEWSHIFT %d\n", status);
        } // else locked until shift not detected
        else { MSG("NEWSHIFT\n"); }

      } else {
MSG("NOSHIFT\n");
        status = MOVINGCONTENT;
        xpx.currentstatus = status;
      }
//      if (ps.mask & ControlMask) {
/*
    } else if (event.type == KeyPress) {
MSG("KEYPRESS\n");
      XKeyEvent *ke = (XKeyEvent *)&event;
      INT keyindex = 0; // of how many ?
      KeySym ks = XLookupKeysym(ke, keyindex);
      if (ks == XK_Escape) { break; }
*/
    }
//    KeyboardState ks = keyboardstate(tcd, xpx.window);
//    if (ks.ascii[27]) { break; } // ESC to quit
    KeyboardState ks;
    updatexpx(&xpx, ps, ks, status);
  }
  freexpx(&xpx);
  freetruecolourdisplay(&tcd);
  return 0;
}

