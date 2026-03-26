#include <string.h>
#include "app-window.h"
#include "metro-map.h"
#include "route-panel.h"

/* ─── Helpers ─── */

static void clearBox(GtkWidget* box) {
	GtkWidget* child = gtk_widget_get_first_child(box);
	while (child) {
		GtkWidget* next = gtk_widget_get_next_sibling(child);
		gtk_box_remove(GTK_BOX(box), child);
		child = next;
	}
}

static void fillSuggestions(GtkWidget* box, const char* query,
                            GCallback cb, gpointer userData)
{
	clearBox(box);
	if (!query || query[0] == '\0') return;

	const Station* st = getStations();
	int count = 0;
	for (int i = 0; i < STATIONSCOUNT && count < 6; i++) {
		if (strcasestr(st[i].name, query)) {
			GtkWidget* btn = gtk_button_new_with_label(st[i].name);
			gtk_widget_add_css_class(btn, "flat");
			gtk_widget_add_css_class(btn, "suggestion-btn");
			g_object_set_data(G_OBJECT(btn), "idx", GINT_TO_POINTER(i));
			g_signal_connect(btn, "clicked", cb, userData);
			gtk_box_append(GTK_BOX(box), btn);
			count++;
		}
	}
}

/* ─── Callbacks ─── */

static void onFindClicked(GtkButton* btn, gpointer ud) {
	(void)btn;
	AppState* s = (AppState*)ud;

	if (s->selectedStart < 0 || s->selectedEnd < 0) {
		gtk_label_set_text(GTK_LABEL(s->statusLabel),
			"Выберите обе станции");
		return;
	}
	if (s->selectedStart == s->selectedEnd) {
		gtk_label_set_text(GTK_LABEL(s->statusLabel),
			"Станции совпадают");
		return;
	}

	stopRouteAnimation(s);
	if (s->currentPath) { freePath(s->currentPath); s->currentPath = NULL; }

	s->currentPath = fshortpath(s->selectedStart, s->selectedEnd);
	if (!s->currentPath) {
		gtk_label_set_text(GTK_LABEL(s->statusLabel), "Маршрут не найден");
		clearRoutePanel(s);
		return;
	}

	gtk_label_set_text(GTK_LABEL(s->statusLabel), "");
	updateRoutePanel(s);
	startRouteAnimation(s);
}

static void onResetClicked(GtkButton* btn, gpointer ud) {
	(void)btn;
	AppState* s = (AppState*)ud;

	stopRouteAnimation(s);
	if (s->currentPath) { freePath(s->currentPath); s->currentPath = NULL; }
	s->selectedStart = -1;
	s->selectedEnd = -1;
	s->searchQuery[0] = '\0';

	gtk_editable_set_text(GTK_EDITABLE(s->startEntry), "");
	gtk_editable_set_text(GTK_EDITABLE(s->endEntry), "");
	gtk_label_set_text(GTK_LABEL(s->statusLabel), "");
	clearBox(s->startResults);
	clearBox(s->endResults);

	clearRoutePanel(s);
	gtk_widget_queue_draw(s->drawArea);
}

/* Autosuggestion: Откуда */
static void onStartSuggestionClicked(GtkButton* btn, gpointer ud) {
	AppState* s = (AppState*)ud;
	const int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "idx"));
	const Station* st = getStations();

	s->selectedStart = idx;
	g_signal_handlers_block_by_func(s->startEntry,
		G_CALLBACK(gtk_widget_queue_draw), NULL);
	gtk_editable_set_text(GTK_EDITABLE(s->startEntry), st[idx].name);
	clearBox(s->startResults);
	gtk_widget_queue_draw(s->drawArea);
}

static void onStartEntryChanged(GtkEditable* ed, gpointer ud) {
	AppState* s = (AppState*)ud;
	const char* text = gtk_editable_get_text(ed);

	s->selectedStart = -1;
	const Station* st = getStations();
	for (int i = 0; i < STATIONSCOUNT; i++) {
		if (strcmp(st[i].name, text) == 0) {
			s->selectedStart = i;
			break;
		}
	}

	strncpy(s->searchQuery, text ? text : "", sizeof(s->searchQuery) - 1);
	fillSuggestions(s->startResults, text,
		G_CALLBACK(onStartSuggestionClicked), s);
	gtk_widget_queue_draw(s->drawArea);
}

/* Autosuggestion: Куда */
static void onEndSuggestionClicked(GtkButton* btn, gpointer ud) {
	AppState* s = (AppState*)ud;
	const int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "idx"));
	const Station* st = getStations();

	s->selectedEnd = idx;
	gtk_editable_set_text(GTK_EDITABLE(s->endEntry), st[idx].name);
	clearBox(s->endResults);
	gtk_widget_queue_draw(s->drawArea);
}

static void onEndEntryChanged(GtkEditable* ed, gpointer ud) {
	AppState* s = (AppState*)ud;
	const char* text = gtk_editable_get_text(ed);

	s->selectedEnd = -1;
	const Station* st = getStations();
	for (int i = 0; i < STATIONSCOUNT; i++) {
		if (strcmp(st[i].name, text) == 0) {
			s->selectedEnd = i;
			break;
		}
	}

	fillSuggestions(s->endResults, text,
		G_CALLBACK(onEndSuggestionClicked), s);
	gtk_widget_queue_draw(s->drawArea);
}

/* ─── CSS ─── */

static const char* INLINE_CSS =
".sidebar { padding: 10px; }\n"
".field-label { font-size: 12px; font-weight: bold; margin: 6px 0 2px 0; }\n"
".title-label { font-size: 16px; font-weight: bold; margin: 4px 0; }\n"
".status-label { color: @error_color; font-size: 11px; }\n"
".route-info { font-size: 13px; font-weight: bold; margin: 2px 0; }\n"
".route-station { font-size: 11px; padding: 2px 0 2px 6px; }\n"
".route-station-first, .route-station-last {\n"
"  font-size: 12px; font-weight: bold; padding: 3px 0 3px 6px;\n"
"}\n"
".transfer-label {\n"
"  font-size: 10px; font-weight: bold;\n"
"  padding: 4px 6px; margin: 1px 0;\n"
"  background: alpha(currentColor, 0.05); border-radius: 4px;\n"
"}\n"
".suggestion-btn { font-size: 11px; padding: 4px 8px; }\n";

static void loadCss() {
	GtkCssProvider* p = gtk_css_provider_new();
	gtk_css_provider_load_from_string(p, INLINE_CSS);
	gtk_style_context_add_provider_for_display(
		gdk_display_get_default(), GTK_STYLE_PROVIDER(p),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(p);
}

/* ─── Window ─── */

GtkWidget* createAppWindow(AdwApplication* app, AppState* state) {
	loadCss();

	AdwStyleManager* sm = adw_style_manager_get_default();
	adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_FORCE_DARK);

	GtkWidget* win = adw_application_window_new(GTK_APPLICATION(app));
	gtk_window_set_title(GTK_WINDOW(win), "Метро СПб");
	gtk_window_set_default_size(GTK_WINDOW(win), 1200, 800);

	GtkWidget* hdr = adw_header_bar_new();
	adw_header_bar_set_title_widget(ADW_HEADER_BAR(hdr),
		adw_window_title_new("Метро СПб", ""));

	GtkWidget* mainBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget* tbv = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tbv), hdr);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tbv), mainBox);
	adw_application_window_set_content(ADW_APPLICATION_WINDOW(win), tbv);

	/* ─── Карта ─── */
	GtkWidget* drawArea = gtk_drawing_area_new();
	gtk_widget_set_hexpand(drawArea, TRUE);
	gtk_widget_set_vexpand(drawArea, TRUE);
	gtk_drawing_area_set_draw_func(
		GTK_DRAWING_AREA(drawArea), mapDrawFunc, state, NULL);
	state->drawArea = drawArea;

	GtkGesture* click = gtk_gesture_click_new();
	g_signal_connect(click, "pressed", G_CALLBACK(mapClickHandler), state);
	gtk_widget_add_controller(drawArea, GTK_EVENT_CONTROLLER(click));

	GtkEventController* motion = gtk_event_controller_motion_new();
	g_signal_connect(motion, "motion", G_CALLBACK(mapMotionHandler), state);
	g_signal_connect(motion, "leave", G_CALLBACK(mapLeaveHandler), state);
	gtk_widget_add_controller(drawArea, motion);

	GtkEventController* scroll = gtk_event_controller_scroll_new(
		GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
	g_signal_connect(scroll, "scroll", G_CALLBACK(mapScrollHandler), state);
	gtk_widget_add_controller(drawArea, scroll);

	GtkGesture* drag = gtk_gesture_drag_new();
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), 0);
	g_signal_connect(drag, "drag-begin", G_CALLBACK(mapDragBegin), state);
	g_signal_connect(drag, "drag-update", G_CALLBACK(mapDragUpdate), state);
	g_signal_connect(drag, "drag-end", G_CALLBACK(mapDragEnd), state);
	gtk_widget_add_controller(drawArea, GTK_EVENT_CONTROLLER(drag));

	gtk_box_append(GTK_BOX(mainBox), drawArea);

	/* ─── Sidebar ─── */
	GtkWidget* sb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_size_request(sb, 195, -1);
	gtk_widget_add_css_class(sb, "sidebar");
	gtk_box_append(GTK_BOX(mainBox), sb);

	/* Откуда */
	GtkWidget* fromLbl = gtk_label_new("Откуда");
	gtk_label_set_xalign(GTK_LABEL(fromLbl), 0.0);
	gtk_widget_add_css_class(fromLbl, "field-label");
	gtk_box_append(GTK_BOX(sb), fromLbl);

	state->startEntry = gtk_entry_new();
	g_signal_connect(state->startEntry, "changed",
		G_CALLBACK(onStartEntryChanged), state);
	gtk_box_append(GTK_BOX(sb), state->startEntry);

	state->startResults = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
	gtk_box_append(GTK_BOX(sb), state->startResults);

	/* Куда */
	GtkWidget* toLbl = gtk_label_new("Куда");
	gtk_label_set_xalign(GTK_LABEL(toLbl), 0.0);
	gtk_widget_add_css_class(toLbl, "field-label");
	gtk_box_append(GTK_BOX(sb), toLbl);

	state->endEntry = gtk_entry_new();
	g_signal_connect(state->endEntry, "changed",
		G_CALLBACK(onEndEntryChanged), state);
	gtk_box_append(GTK_BOX(sb), state->endEntry);

	state->endResults = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
	gtk_box_append(GTK_BOX(sb), state->endResults);

	state->statusLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(state->statusLabel), 0.0);
	gtk_widget_add_css_class(state->statusLabel, "status-label");
	gtk_box_append(GTK_BOX(sb), state->statusLabel);

	GtkWidget* sep1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_append(GTK_BOX(sb), sep1);

	/* Маршрут */
	GtkWidget* routeTitle = gtk_label_new("Маршрут");
	gtk_label_set_xalign(GTK_LABEL(routeTitle), 0.0);
	gtk_widget_add_css_class(routeTitle, "title-label");
	gtk_box_append(GTK_BOX(sb), routeTitle);

	state->timeLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(state->timeLabel), 0.0);
	gtk_widget_add_css_class(state->timeLabel, "route-info");
	gtk_box_append(GTK_BOX(sb), state->timeLabel);

	state->transfersLabel = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(state->transfersLabel), 0.0);
	gtk_widget_add_css_class(state->transfersLabel, "route-info");
	gtk_box_append(GTK_BOX(sb), state->transfersLabel);

	GtkWidget* scrollWin = gtk_scrolled_window_new();
	gtk_widget_set_vexpand(scrollWin, TRUE);
	gtk_box_append(GTK_BOX(sb), scrollWin);

	state->routeBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrollWin),
		state->routeBox);

	/* Кнопки внизу */
	GtkWidget* sep2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_append(GTK_BOX(sb), sep2);

	state->findButton = gtk_button_new_with_label("Найти маршрут");
	gtk_widget_add_css_class(state->findButton, "suggested-action");
	g_signal_connect(state->findButton, "clicked",
		G_CALLBACK(onFindClicked), state);
	gtk_box_append(GTK_BOX(sb), state->findButton);

	GtkWidget* resetBtn = gtk_button_new_with_label("Сбросить");
	g_signal_connect(resetBtn, "clicked",
		G_CALLBACK(onResetClicked), state);
	gtk_box_append(GTK_BOX(sb), resetBtn);

	/* Remove old search fields from state (not used) */
	state->searchEntry = NULL;
	state->searchResults = NULL;

	return win;
}
