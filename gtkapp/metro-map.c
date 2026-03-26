#include <math.h>
#include <string.h>
#include "metro-map.h"
#include "station-coords.h"
#include "../metroapp/pathfinder.h"

#define STATION_RADIUS   10.0
#define TRANSFER_RADIUS  14.0
#define HIT_RADIUS       22.0
#define LINE_WIDTH        8.0
#define FONT_SIZE        10.0
#define ANIM_INTERVAL_MS 200

#define LOGICAL_SIZE   1000.0
#define LOGICAL_H       950.0
#define MAP_PADDING      50.0
#define ZOOM_MIN          0.5
#define ZOOM_MAX          4.0
#define ZOOM_STEP         1.15

typedef struct { double r, g, b; } Color;

/* ─── Цвета линий ─── */

static Color parseLineColor(const char* name) {
	if (strcmp(name, "red") == 0)    return (Color){1.00, 0.25, 0.25};
	if (strcmp(name, "blue") == 0)   return (Color){0.30, 0.55, 1.00};
	if (strcmp(name, "green") == 0)  return (Color){0.25, 0.88, 0.45};
	if (strcmp(name, "orange") == 0) return (Color){1.00, 0.68, 0.15};
	if (strcmp(name, "purple") == 0) return (Color){0.78, 0.35, 1.00};
	if (strcmp(name, "brown") == 0)  return (Color){0.80, 0.55, 0.30};
	return (Color){0.5, 0.5, 0.5};
}

static Color colorForStation(int idx) {
	const Station* st = getStations();
	const Line* ln = getLines();
	for (int i = 0; i < LINESCOUNT; i++)
		if (ln[i].id == st[idx].line_id)
			return parseLineColor(ln[i].color);
	return (Color){0.5, 0.5, 0.5};
}

/* ─── Viewport transform ─── */

static void calcTransform(int w, int h, double zoom,
                          double panX, double panY,
                          double* outScale, double* outTx, double* outTy)
{
	const double usableW = w - MAP_PADDING * 2;
	const double usableH = h - MAP_PADDING * 2;
	const double scaleX = usableW / LOGICAL_SIZE;
	const double scaleY = usableH / LOGICAL_H;
	const double base = fmin(scaleX, scaleY);
	*outScale = base * zoom;
	*outTx = (w - LOGICAL_SIZE * (*outScale)) / 2.0 + panX;
	*outTy = (h - LOGICAL_H * (*outScale)) / 2.0 + panY;
}

void screenToLogical(const AppState* state, int w, int h,
                     double sx, double sy, double* lx, double* ly)
{
	double scale, tx, ty;
	calcTransform(w, h, state->zoomLevel, state->panX, state->panY,
	              &scale, &tx, &ty);
	*lx = (sx - tx) / scale;
	*ly = (sy - ty) / scale;
}

/* ─── Path queries ─── */

static int stationInPath(int idx, const Path* p, int upTo) {
	if (!p) return 0;
	const int lim = (upTo >= 0 && upTo < p->length) ? upTo + 1 : p->length;
	for (int i = 0; i < lim; i++)
		if ((int)p->stations[i] == idx) return 1;
	return 0;
}

static int edgeInPath(int a, int b, const Path* p, int upTo) {
	if (!p) return 0;
	const int lim = (upTo >= 0 && upTo < p->length) ? upTo + 1 : p->length;
	for (int i = 0; i < lim - 1; i++) {
		const int pa = (int)p->stations[i];
		const int pb = (int)p->stations[i + 1];
		if ((pa == a && pb == b) || (pa == b && pb == a)) return 1;
	}
	return 0;
}

static int isTransfer(int idx) {
	const Station* st = getStations();
	for (int j = 0; j < st[idx].neighbors_count; j++)
		if (st[idx].neighbors_edges[j]->transfer) return 1;
	return 0;
}

static Color colorForLineId(unsigned char lineId) {
	const Line* ln = getLines();
	for (int i = 0; i < LINESCOUNT; i++)
		if (ln[i].id == lineId)
			return parseLineColor(ln[i].color);
	return (Color){0.5, 0.5, 0.5};
}

static int collectTransferLineIds(int idx, unsigned char* out, int maxOut) {
	const Station* st = getStations();
	int count = 0;
	out[count++] = st[idx].line_id;

	for (int j = 0; j < st[idx].neighbors_count; j++) {
		if (!st[idx].neighbors_edges[j]->transfer) continue;
		const int nb = st[idx].neighbors_edges[j]->to;
		const unsigned char nLine = st[nb].line_id;
		int dup = 0;
		for (int k = 0; k < count; k++)
			if (out[k] == nLine) { dup = 1; break; }
		if (!dup && count < maxOut)
			out[count++] = nLine;
	}
	return count;
}

static void drawPizzaStation(cairo_t* cr, double x, double y,
                             double radius, int stationIdx, double alpha)
{
	unsigned char lineIds[6];
	const int count = collectTransferLineIds(stationIdx, lineIds, 6);
	const double slice = 2.0 * M_PI / count;

	for (int i = 0; i < count; i++) {
		const Color c = colorForLineId(lineIds[i]);
		cairo_set_source_rgba(cr, c.r, c.g, c.b, alpha);
		cairo_move_to(cr, x, y);
		cairo_arc(cr, x, y, radius,
			-M_PI / 2.0 + i * slice,
			-M_PI / 2.0 + (i + 1) * slice);
		cairo_close_path(cr);
		cairo_fill(cr);
	}

	/* Белый ободок */
	cairo_set_source_rgba(cr, 1, 1, 1, alpha);
	cairo_set_line_width(cr, 2.5);
	cairo_arc(cr, x, y, radius, 0, 2 * M_PI);
	cairo_stroke(cr);

	/* Разделители секторов */
	if (count > 1) {
		cairo_set_source_rgba(cr, 1, 1, 1, alpha * 0.9);
		cairo_set_line_width(cr, 1.5);
		for (int i = 0; i < count; i++) {
			const double angle = -M_PI / 2.0 + i * slice;
			cairo_move_to(cr, x, y);
			cairo_line_to(cr, x + radius * cos(angle),
				y + radius * sin(angle));
			cairo_stroke(cr);
		}
	}
}

static int stationMatchesSearch(int idx, const char* query) {
	if (!query || query[0] == '\0') return 0;
	const Station* st = getStations();
	const char* name = st[idx].name;
	const char* found = strcasestr(name, query);
	return found != NULL;
}

/* ─── Drawing: Neva river (water background) ─── */

static void drawNeva(cairo_t* cr) {
	cairo_set_source_rgba(cr, 0.35, 0.55, 0.78, 0.15);

	/* Главное русло Невы: от восточного края через центр на запад */
	cairo_move_to(cr, 950, 340);
	cairo_curve_to(cr, 800, 330, 700, 350, 600, 370);
	cairo_curve_to(cr, 520, 385, 450, 380, 350, 360);
	cairo_curve_to(cr, 250, 340, 150, 330, 50, 310);
	cairo_line_to(cr, 50, 340);
	cairo_curve_to(cr, 150, 360, 250, 370, 350, 390);
	cairo_curve_to(cr, 450, 410, 520, 415, 600, 400);
	cairo_curve_to(cr, 700, 380, 800, 360, 950, 370);
	cairo_close_path(cr);
	cairo_fill(cr);

	/* Малая Нева (ответвление на запад) */
	cairo_move_to(cr, 380, 330);
	cairo_curve_to(cr, 300, 320, 200, 310, 80, 290);
	cairo_line_to(cr, 80, 310);
	cairo_curve_to(cr, 200, 330, 300, 340, 380, 350);
	cairo_close_path(cr);
	cairo_fill(cr);

	/* Финский залив (левый край) */
	cairo_move_to(cr, 50, 250);
	cairo_curve_to(cr, 30, 300, 30, 350, 50, 400);
	cairo_line_to(cr, 80, 400);
	cairo_curve_to(cr, 60, 350, 60, 300, 80, 250);
	cairo_close_path(cr);
	cairo_fill(cr);
}

/* ─── Drawing: edges ─── */

static void drawEdges(cairo_t* cr, const AppState* state) {
	const Station* st = getStations();
	const StationCoord* co = getStationCoords();

	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

	for (int i = 0; i < STATIONSCOUNT; i++) {
		for (int j = 0; j < st[i].neighbors_count; j++) {
			const Edge* e = st[i].neighbors_edges[j];
			if (e->to <= i || e->transfer) continue;

			const int to = e->to;
			const Color c = colorForStation(i);
			const int inP = edgeInPath(i, to, state->currentPath, state->animStep);
			const int hasRoute = (state->currentPath != NULL);

			if (hasRoute && inP) {
				cairo_set_source_rgba(cr, 1, 1, 1, 0.5);
				cairo_set_line_width(cr, LINE_WIDTH + 6);
				cairo_move_to(cr, co[i].x, co[i].y);
				cairo_line_to(cr, co[to].x, co[to].y);
				cairo_stroke(cr);
			}

			if (hasRoute && !inP)
				cairo_set_source_rgba(cr, c.r, c.g, c.b, 0.15);
			else
				cairo_set_source_rgb(cr, c.r, c.g, c.b);

			cairo_set_line_width(cr, LINE_WIDTH);
			cairo_move_to(cr, co[i].x, co[i].y);
			cairo_line_to(cr, co[to].x, co[to].y);
			cairo_stroke(cr);
		}
	}
}

/* ─── Drawing: transfer connectors ─── */

static void drawTransfers(cairo_t* cr, const AppState* state) {
	const Station* st = getStations();
	const StationCoord* co = getStationCoords();

	for (int i = 0; i < STATIONSCOUNT; i++) {
		for (int j = 0; j < st[i].neighbors_count; j++) {
			const Edge* e = st[i].neighbors_edges[j];
			if (!e->transfer || e->to <= i) continue;

			const int to = e->to;
			const int inP = edgeInPath(i, to, state->currentPath, state->animStep);

			if (inP) {
				cairo_set_source_rgb(cr, 1, 1, 1);
				cairo_set_line_width(cr, 3.5);
			} else {
				cairo_set_source_rgba(cr, 0.85, 0.85, 0.85, 0.5);
				cairo_set_line_width(cr, 2.5);
			}

			cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
			const double dash[] = {6.0, 4.0};
			cairo_set_dash(cr, dash, 2, 0);
			cairo_move_to(cr, co[i].x, co[i].y);
			cairo_line_to(cr, co[to].x, co[to].y);
			cairo_stroke(cr);
			cairo_set_dash(cr, NULL, 0, 0);
		}
	}
}

/* ─── Drawing: stations ─── */

static void drawStations(cairo_t* cr, const AppState* state) {
	const StationCoord* co = getStationCoords();
	const int hasRoute = (state->currentPath != NULL);
	const char* query = state->searchQuery;

	for (int i = 0; i < STATIONSCOUNT; i++) {
		const Color c = colorForStation(i);
		const int inP = stationInPath(i, state->currentPath, state->animStep);
		const int sel = (i == state->selectedStart || i == state->selectedEnd);
		const int hov = (i == state->hoveredStation);
		const int xfer = isTransfer(i);
		const int searched = stationMatchesSearch(i, query);

		double r = xfer ? TRANSFER_RADIUS : STATION_RADIUS;
		if (sel) r += 4;
		else if (hov) r += 3;
		else if (inP) r += 2;

		const double dimAlpha = (hasRoute && !inP && !sel && !hov) ? 0.15 : 1.0;

		/* Glow для выбранных/hovered */
		if (sel || hov) {
			cairo_set_source_rgba(cr, 1, 1, 0.3, 0.35);
			cairo_arc(cr, co[i].x, co[i].y, r + 8, 0, 2 * M_PI);
			cairo_fill(cr);
		}

		/* Glow для найденных поиском */
		if (searched && !sel && !hov) {
			cairo_set_source_rgba(cr, 0.3, 1, 0.3, 0.3);
			cairo_arc(cr, co[i].x, co[i].y, r + 6, 0, 2 * M_PI);
			cairo_fill(cr);
		}

		if (xfer) {
			/* Pizza-slice: секторы в цветах всех пересадочных веток */
			drawPizzaStation(cr, co[i].x, co[i].y, r, i, dimAlpha);
		} else {
			/* Обычная станция: белый ободок + цветной круг */
			cairo_set_source_rgba(cr, 1, 1, 1, dimAlpha);
			cairo_arc(cr, co[i].x, co[i].y, r + 2.5, 0, 2 * M_PI);
			cairo_fill(cr);

			cairo_set_source_rgba(cr, c.r, c.g, c.b, dimAlpha);
			cairo_arc(cr, co[i].x, co[i].y, r, 0, 2 * M_PI);
			cairo_fill(cr);
		}

		/* Обводка выбранных */
		if (sel) {
			cairo_set_source_rgba(cr, 1, 1, 0.2, 0.9);
			cairo_set_line_width(cr, 3.0);
			cairo_arc(cr, co[i].x, co[i].y, r + 5, 0, 2 * M_PI);
			cairo_stroke(cr);
		}
	}
}

/* ─── Drawing: labels ─── */

static void drawLabels(cairo_t* cr, const AppState* state) {
	const Station* st = getStations();
	const StationCoord* co = getStationCoords();
	const int hasRoute = (state->currentPath != NULL);
	const char* query = state->searchQuery;

	cairo_select_font_face(cr, "sans-serif",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, FONT_SIZE);

	for (int i = 0; i < STATIONSCOUNT; i++) {
		const int inP = stationInPath(i, state->currentPath, state->animStep);
		const int sel = (i == state->selectedStart || i == state->selectedEnd);
		const int hov = (i == state->hoveredStation);
		const int searched = stationMatchesSearch(i, query);

		double alpha;
		if (sel || hov)
			alpha = 1.0;
		else if (searched)
			alpha = 0.95;
		else if (hasRoute && !inP)
			alpha = 0.12;
		else
			alpha = 0.85;

		cairo_text_extents_t ext;
		cairo_text_extents(cr, st[i].name, &ext);

		double tx, ty;
		const double off = STATION_RADIUS + 8;

		switch (co[i].labelSide) {
		case LABEL_RIGHT:
			tx = co[i].x + off;
			ty = co[i].y + ext.height / 2.0;
			break;
		case LABEL_LEFT:
			tx = co[i].x - off - ext.width;
			ty = co[i].y + ext.height / 2.0;
			break;
		case LABEL_TOP:
			tx = co[i].x - ext.width / 2.0;
			ty = co[i].y - off;
			break;
		case LABEL_BOTTOM:
			tx = co[i].x - ext.width / 2.0;
			ty = co[i].y + off + ext.height;
			break;
		}

		/* Фон под текстом */
		const double pad = 3.0;
		cairo_set_source_rgba(cr, 0.06, 0.06, 0.10, alpha * 0.7);
		cairo_rectangle(cr,
			tx - pad, ty - ext.height - pad,
			ext.width + pad * 2, ext.height + pad * 2);
		cairo_fill(cr);

		/* Текст */
		cairo_set_source_rgba(cr, 0.93, 0.93, 0.93, alpha);
		cairo_move_to(cr, tx, ty);
		cairo_show_text(cr, st[i].name);
	}
}

/* ─── Drawing: legend ─── */

static void drawLegend(cairo_t* cr, double scale) {
	const Line* ln = getLines();
	const double x0 = 820;
	const double y0 = 30;
	const double lineLen = 30;
	const double rowH = 22;

	/* Фон легенды */
	cairo_set_source_rgba(cr, 0.08, 0.08, 0.14, 0.85);
	cairo_rectangle(cr, x0 - 10, y0 - 8,
		180, LINESCOUNT * rowH + 16);
	cairo_fill(cr);

	cairo_select_font_face(cr, "sans-serif",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 9.0);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

	for (int i = 0; i < LINESCOUNT; i++) {
		const double y = y0 + i * rowH + rowH / 2.0;
		const Color c = parseLineColor(ln[i].color);

		cairo_set_source_rgb(cr, c.r, c.g, c.b);
		cairo_set_line_width(cr, 4.0);
		cairo_move_to(cr, x0, y);
		cairo_line_to(cr, x0 + lineLen, y);
		cairo_stroke(cr);

		cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.85);
		cairo_move_to(cr, x0 + lineLen + 8, y + 4);
		cairo_show_text(cr, ln[i].name);
	}

	(void)scale;
}

/* ─── Main draw function ─── */

void mapDrawFunc(GtkDrawingArea* area, cairo_t* cr,
                 int width, int height, gpointer userData)
{
	(void)area;
	const AppState* state = (const AppState*)userData;

	/* Фон */
	cairo_set_source_rgb(cr, 0.08, 0.08, 0.14);
	cairo_paint(cr);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

	/* Viewport transform */
	double scale, tx, ty;
	calcTransform(width, height, state->zoomLevel,
	              state->panX, state->panY, &scale, &tx, &ty);

	cairo_save(cr);
	cairo_translate(cr, tx, ty);
	cairo_scale(cr, scale, scale);

	drawNeva(cr);
	drawEdges(cr, state);
	drawTransfers(cr, state);
	drawStations(cr, state);
	drawLabels(cr, state);
	drawLegend(cr, scale);

	cairo_restore(cr);

	/* Tooltip для hovered station */
	if (state->hoveredStation >= 0) {
		const Station* st = getStations();
		const StationCoord* co = getStationCoords();
		const int idx = state->hoveredStation;

		const double sx = co[idx].x * scale + tx;
		const double sy = co[idx].y * scale + ty - 25 * state->zoomLevel;

		cairo_select_font_face(cr, "sans-serif",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 13);

		cairo_text_extents_t ext;
		cairo_text_extents(cr, st[idx].name, &ext);

		const double px = 8, py = 5;
		const double bx = sx - ext.width / 2.0 - px;
		const double by = sy - ext.height - py * 2;

		cairo_set_source_rgba(cr, 0.1, 0.1, 0.2, 0.9);
		cairo_rectangle(cr, bx, by,
			ext.width + px * 2, ext.height + py * 2);
		cairo_fill(cr);

		cairo_set_source_rgba(cr, 0.3, 0.5, 1.0, 0.8);
		cairo_set_line_width(cr, 1.5);
		cairo_rectangle(cr, bx, by,
			ext.width + px * 2, ext.height + py * 2);
		cairo_stroke(cr);

		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_move_to(cr, bx + px, by + py + ext.height);
		cairo_show_text(cr, st[idx].name);
	}
}

/* ─── Hit test ─── */

static int findStationAt(const AppState* state, int w, int h,
                         double sx, double sy)
{
	const StationCoord* co = getStationCoords();
	double lx, ly;
	screenToLogical(state, w, h, sx, sy, &lx, &ly);

	int closest = -1;
	double best = HIT_RADIUS * HIT_RADIUS;

	for (int i = 0; i < STATIONSCOUNT; i++) {
		const double dx = co[i].x - lx;
		const double dy = co[i].y - ly;
		const double d2 = dx * dx + dy * dy;
		if (d2 < best) { best = d2; closest = i; }
	}
	return closest;
}

/* ─── Event handlers ─── */

void mapClickHandler(GtkGestureClick* gesture, int nPress,
                     double x, double y, gpointer userData)
{
	(void)gesture; (void)nPress;
	AppState* state = (AppState*)userData;
	const Station* st = getStations();

	const int w = gtk_widget_get_width(state->drawArea);
	const int h = gtk_widget_get_height(state->drawArea);
	const int hit = findStationAt(state, w, h, x, y);
	if (hit < 0) return;

	/* Блокируем "changed" чтобы не сбросить selection при set_text */
	g_signal_handlers_block_matched(state->startEntry,
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, state);
	g_signal_handlers_block_matched(state->endEntry,
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, state);

	if (state->selectedStart < 0) {
		state->selectedStart = hit;
		gtk_editable_set_text(GTK_EDITABLE(state->startEntry), st[hit].name);
	} else if (state->selectedEnd < 0) {
		state->selectedEnd = hit;
		gtk_editable_set_text(GTK_EDITABLE(state->endEntry), st[hit].name);
	} else {
		stopRouteAnimation(state);
		if (state->currentPath) {
			freePath(state->currentPath);
			state->currentPath = NULL;
		}
		state->selectedStart = hit;
		state->selectedEnd = -1;
		gtk_editable_set_text(GTK_EDITABLE(state->startEntry), st[hit].name);
		gtk_editable_set_text(GTK_EDITABLE(state->endEntry), "");
		gtk_label_set_text(GTK_LABEL(state->statusLabel), "");
		gtk_label_set_text(GTK_LABEL(state->timeLabel), "");
		gtk_label_set_text(GTK_LABEL(state->transfersLabel), "");
	}

	g_signal_handlers_unblock_matched(state->startEntry,
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, state);
	g_signal_handlers_unblock_matched(state->endEntry,
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, state);

	gtk_widget_queue_draw(state->drawArea);
}

void mapMotionHandler(GtkEventControllerMotion* ctrl,
                      double x, double y, gpointer userData)
{
	(void)ctrl;
	AppState* state = (AppState*)userData;
	const int w = gtk_widget_get_width(state->drawArea);
	const int h = gtk_widget_get_height(state->drawArea);

	const int prev = state->hoveredStation;
	state->hoveredStation = findStationAt(state, w, h, x, y);

	if (state->hoveredStation != prev)
		gtk_widget_queue_draw(state->drawArea);
}

void mapLeaveHandler(GtkEventControllerMotion* ctrl,
                     gpointer userData)
{
	(void)ctrl;
	AppState* state = (AppState*)userData;
	if (state->hoveredStation >= 0) {
		state->hoveredStation = -1;
		gtk_widget_queue_draw(state->drawArea);
	}
}

gboolean mapScrollHandler(GtkEventControllerScroll* ctrl,
                          double dx, double dy, gpointer userData)
{
	(void)ctrl; (void)dx;
	AppState* state = (AppState*)userData;

	const double oldZoom = state->zoomLevel;

	/* dy > 0 = scroll down = zoom out, dy < 0 = scroll up = zoom in */
	/* На macOS с natural scrolling: dy > 0 = пальцы вверх = zoom in */
	if (dy > 0)
		state->zoomLevel *= ZOOM_STEP;
	else if (dy < 0)
		state->zoomLevel /= ZOOM_STEP;

	if (state->zoomLevel < ZOOM_MIN) state->zoomLevel = ZOOM_MIN;
	if (state->zoomLevel > ZOOM_MAX) state->zoomLevel = ZOOM_MAX;

	if (state->zoomLevel != oldZoom)
		gtk_widget_queue_draw(state->drawArea);

	return TRUE;
}

void mapDragBegin(GtkGestureDrag* gesture,
                  double x, double y, gpointer userData)
{
	(void)gesture;
	AppState* state = (AppState*)userData;

	const int w = gtk_widget_get_width(state->drawArea);
	const int h = gtk_widget_get_height(state->drawArea);
	const int hit = findStationAt(state, w, h, x, y);

	if (hit >= 0) {
		state->dragging = 0;
		return;
	}

	state->dragging = 1;
	state->dragStartX = x;
	state->dragStartY = y;
	state->dragPanStartX = state->panX;
	state->dragPanStartY = state->panY;
}

void mapDragUpdate(GtkGestureDrag* gesture,
                   double dx, double dy, gpointer userData)
{
	(void)gesture;
	AppState* state = (AppState*)userData;
	if (!state->dragging) return;

	state->panX = state->dragPanStartX + dx;
	state->panY = state->dragPanStartY + dy;
	gtk_widget_queue_draw(state->drawArea);
}

void mapDragEnd(GtkGestureDrag* gesture,
                double dx, double dy, gpointer userData)
{
	(void)gesture; (void)dx; (void)dy;
	AppState* state = (AppState*)userData;
	state->dragging = 0;
}

/* ─── Animation ─── */

gboolean animateStep(gpointer userData) {
	AppState* state = (AppState*)userData;
	if (!state->currentPath || state->animStep < 0) {
		state->animTimerId = 0;
		return G_SOURCE_REMOVE;
	}

	state->animStep++;
	if (state->animStep >= state->currentPath->length) {
		state->animStep = state->currentPath->length - 1;
		state->animTimerId = 0;
		gtk_widget_queue_draw(state->drawArea);
		return G_SOURCE_REMOVE;
	}

	gtk_widget_queue_draw(state->drawArea);
	return G_SOURCE_CONTINUE;
}

void startRouteAnimation(AppState* state) {
	stopRouteAnimation(state);
	state->animStep = 0;
	state->animTimerId = g_timeout_add(ANIM_INTERVAL_MS, animateStep, state);
	gtk_widget_queue_draw(state->drawArea);
}

void stopRouteAnimation(AppState* state) {
	if (state->animTimerId > 0) {
		g_source_remove(state->animTimerId);
		state->animTimerId = 0;
	}
	state->animStep = -1;
}
