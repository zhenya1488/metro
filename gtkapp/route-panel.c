#include <string.h>
#include "route-panel.h"
#include "../metroapp/pathfinder.h"

static const char* lineColorHex(const char* name) {
	if (strcmp(name, "red") == 0)    return "#e83838";
	if (strcmp(name, "blue") == 0)   return "#3373f2";
	if (strcmp(name, "green") == 0)  return "#2ec659";
	if (strcmp(name, "orange") == 0) return "#fa941e";
	if (strcmp(name, "purple") == 0) return "#a648d1";
	if (strcmp(name, "brown") == 0)  return "#996138";
	return "#808080";
}

static const char* findLineName(unsigned char lineId) {
	const Line* ln = getLines();
	for (int i = 0; i < LINESCOUNT; i++)
		if (ln[i].id == lineId) return ln[i].name;
	return "";
}

static const char* findLineColorStr(unsigned char lineId) {
	const Line* ln = getLines();
	for (int i = 0; i < LINESCOUNT; i++)
		if (ln[i].id == lineId) return ln[i].color;
	return "";
}

void updateRoutePanel(AppState* state) {
	const Path* path = state->currentPath;
	if (!path) return;

	const Station* st = getStations();
	clearRoutePanel(state);

	char buf[64];
	snprintf(buf, sizeof(buf), "Время в пути: %hu мин", path->total_time);
	gtk_label_set_text(GTK_LABEL(state->timeLabel), buf);

	snprintf(buf, sizeof(buf), "Пересадок: %hhu", path->transfers);
	gtk_label_set_text(GTK_LABEL(state->transfersLabel), buf);

	unsigned char curLine = st[(int)path->stations[0]].line_id;

	for (int i = 0; i < path->length; i++) {
		const int idx = (int)path->stations[i];
		const unsigned char stLine = st[idx].line_id;
		const char* hex = lineColorHex(findLineColorStr(stLine));

		if (i > 0 && stLine != curLine) {
			GtkWidget* xferLabel = gtk_label_new(NULL);
			char markup[512];
			snprintf(markup, sizeof(markup),
				"<span color='%s' font_weight='bold'>⇄ Пересадка: %s</span>",
				lineColorHex(findLineColorStr(stLine)),
				findLineName(stLine));
			gtk_label_set_markup(GTK_LABEL(xferLabel), markup);
			gtk_label_set_xalign(GTK_LABEL(xferLabel), 0.0);
			gtk_widget_add_css_class(xferLabel, "transfer-label");
			gtk_box_append(GTK_BOX(state->routeBox), xferLabel);
			curLine = stLine;
		}

		GtkWidget* label = gtk_label_new(NULL);
		char markup[512];

		const int isFirst = (i == 0);
		const int isLast = (i == path->length - 1);

		if (isFirst || isLast) {
			snprintf(markup, sizeof(markup),
				"<span color='%s' font_size='large'>●</span>"
				" <b>%s</b>",
				hex, st[idx].name);
		} else {
			snprintf(markup, sizeof(markup),
				"<span color='%s'>●</span> %s",
				hex, st[idx].name);
		}

		gtk_label_set_markup(GTK_LABEL(label), markup);
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);

		if (isFirst)
			gtk_widget_add_css_class(label, "route-station-first");
		else if (isLast)
			gtk_widget_add_css_class(label, "route-station-last");
		else
			gtk_widget_add_css_class(label, "route-station");

		gtk_box_append(GTK_BOX(state->routeBox), label);
	}
}

void clearRoutePanel(AppState* state) {
	GtkWidget* child = gtk_widget_get_first_child(state->routeBox);
	while (child) {
		GtkWidget* next = gtk_widget_get_next_sibling(child);
		gtk_box_remove(GTK_BOX(state->routeBox), child);
		child = next;
	}
	gtk_label_set_text(GTK_LABEL(state->timeLabel), "");
	gtk_label_set_text(GTK_LABEL(state->transfersLabel), "");
}
