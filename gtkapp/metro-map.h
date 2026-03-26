#ifndef METRO_MAP_H
#define METRO_MAP_H

#include <adwaita.h>
#include "app-window.h"

void mapDrawFunc(GtkDrawingArea* area, cairo_t* cr,
                 int width, int height, gpointer userData);

void mapClickHandler(GtkGestureClick* gesture, int nPress,
                     double x, double y, gpointer userData);

void mapMotionHandler(GtkEventControllerMotion* ctrl,
                      double x, double y, gpointer userData);

void mapLeaveHandler(GtkEventControllerMotion* ctrl,
                     gpointer userData);

gboolean mapScrollHandler(GtkEventControllerScroll* ctrl,
                          double dx, double dy, gpointer userData);

void mapDragBegin(GtkGestureDrag* gesture,
                  double x, double y, gpointer userData);

void mapDragUpdate(GtkGestureDrag* gesture,
                   double dx, double dy, gpointer userData);

void mapDragEnd(GtkGestureDrag* gesture,
                double dx, double dy, gpointer userData);

gboolean animateStep(gpointer userData);
void startRouteAnimation(AppState* state);
void stopRouteAnimation(AppState* state);

void screenToLogical(const AppState* state, int width, int height,
                     double sx, double sy, double* lx, double* ly);

#endif
