#include <adwaita.h>
#include <string.h>
#include "../metroapp/pathfinder.h"
#include "app-window.h"

#define STATIONS_DAT "data/stations.dat"
#define STATIONS_DAT_SIG "data/stations.dat.sig"
#define EDGES_DAT "data/edges.dat"
#define EDGES_DAT_SIG "data/edges.dat.sig"
#define LINES_DAT "data/lines.dat"
#define LINES_DAT_SIG "data/lines.dat.sig"

static AppState appState;

static int loadMetroData() {
	if (get_stations(STATIONS_DAT, STATIONS_DAT_SIG) != 0) {
		fprintf(stderr, "Ошибка загрузки станций.\n");
		return 1;
	}
	if (get_edges(EDGES_DAT, EDGES_DAT_SIG) != 0) {
		fprintf(stderr, "Ошибка загрузки рёбер графа.\n");
		return 1;
	}
	if (get_lines(LINES_DAT, LINES_DAT_SIG) != 0) {
		fprintf(stderr, "Ошибка загрузки линий.\n");
		return 1;
	}
	return 0;
}

static void onActivate(AdwApplication* app, gpointer userData) {
	AppState* state = (AppState*)userData;

	state->selectedStart = -1;
	state->selectedEnd = -1;
	state->currentPath = NULL;
	state->animStep = -1;
	state->animTimerId = 0;
	state->zoomLevel = 1.0;
	state->panX = 0;
	state->panY = 0;
	state->hoveredStation = -1;
	state->dragging = 0;
	memset(state->searchQuery, 0, sizeof(state->searchQuery));

	GtkWidget* window = createAppWindow(app, state);
	gtk_window_present(GTK_WINDOW(window));
}

static void onShutdown(AdwApplication* app, gpointer userData) {
	(void)app;
	AppState* state = (AppState*)userData;

	if (state->animTimerId > 0)
		g_source_remove(state->animTimerId);
	if (state->currentPath)
		freePath(state->currentPath);
	free_all();
}

int main(int argc, char** argv) {
	if (loadMetroData() != 0)
		return 1;

	AdwApplication* app = adw_application_new(
		"com.metro.spb", G_APPLICATION_DEFAULT_FLAGS);

	g_signal_connect(app, "activate", G_CALLBACK(onActivate), &appState);
	g_signal_connect(app, "shutdown", G_CALLBACK(onShutdown), &appState);

	int status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);
	return status;
}
