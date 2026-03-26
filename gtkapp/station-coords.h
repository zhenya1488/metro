#ifndef STATION_COORDS_H
#define STATION_COORDS_H

typedef enum {
	LABEL_RIGHT,
	LABEL_LEFT,
	LABEL_TOP,
	LABEL_BOTTOM
} LabelSide;

typedef struct {
	double x;
	double y;
	LabelSide labelSide;
} StationCoord;

const StationCoord* getStationCoords();

#endif
