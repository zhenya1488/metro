#ifndef APP_WINDOW_H
#define APP_WINDOW_H

#include <adwaita.h>
#include "../metroapp/pathfinder.h"

typedef struct {
	int selectedStart;
	int selectedEnd;
	Path* currentPath;
	int animStep;
	guint animTimerId;

	double zoomLevel;
	double panX, panY;
	int hoveredStation;
	int dragging;
	double dragStartX, dragStartY;
	double dragPanStartX, dragPanStartY;

	char searchQuery[64];

	GtkWidget* drawArea;
	GtkWidget* routeBox;
	GtkWidget* statusLabel;
	GtkWidget* startEntry;
	GtkWidget* startResults;
	GtkWidget* endEntry;
	GtkWidget* endResults;
	GtkWidget* timeLabel;
	GtkWidget* transfersLabel;
	GtkWidget* findButton;
	GtkWidget* searchEntry;
	GtkWidget* searchResults;
} AppState;

GtkWidget* createAppWindow(AdwApplication* app, AppState* state);

#endif
