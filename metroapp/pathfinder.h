#ifndef PATHFINDER_H
#define PATHFINDER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define STATIONSCOUNT 75
#define LINESCOUNT 6

typedef struct {
	unsigned char to;
	unsigned char time;
	unsigned char transfer;
} Edge;

typedef struct {
	unsigned char id;
	char name[64];
	unsigned char line_id;
	unsigned char neighbors_count;
	Edge** neighbors_edges;
} Station;

typedef struct {
	unsigned char id;
	char name[64];
	char color[8];
} Line;

typedef struct {
	char* stations;
	unsigned short length;
	unsigned short total_time;
	unsigned char transfers;
} Path;

int valid_file_check(char *filename, char* signame);
int get_stations(char* filename, char* signame);
int get_edges(char* filename, char* signame);
int get_lines(char* filename, char* signame);
void free_all();

Path* fshortpath(int start, int end);
void printPath(Path* path);
void freePath(Path* path);

int find_station(char* name);

const Station* getStations();
const Line* getLines();

#endif
