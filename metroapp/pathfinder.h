#ifndef PATHFINDER_H
#define PATHFINDER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct {
	char* stations; // массив id станций
	unsigned short length; // количество станций в пути
	unsigned short total_time; // время в пути
	unsigned char transfers; // количество пересадок
} Path;

int valid_file_check(char *filename, char* signame);
int get_stations(char* filename, char* signame);
int get_edges(char* filename, char* signame);
int get_lines(char* filename, char* signame);
void free_all();

Path* fshortpath(int start, int end);
void printPath(Path* path);
void freePath(Path* path);

int find_station(char* name); // вернет id станции, чье название совпадает с переданным

#endif
