#ifdef _WIN32
	#define _CRT_SECURE_NO_WARNINGS
	#include <locale.h>
	#include <windows.h>
	void set_locale() {
		setlocale(LC_ALL, ".UTF-8");
		SetConsoleCP(65001);
		SetConsoleOutputCP(65001);
	}
#else
	void set_locale() {
		return;
	}
#endif

#include "pathfinder.h"

#define STATIONS_DAT "data/stations.dat"
#define STATIONS_DAT_SIG "data/stations.dat.sig"
#define EDGES_DAT "data/edges.dat"
#define EDGES_DAT_SIG "data/edges.dat.sig"
#define LINES_DAT "data/lines.dat"
#define LINES_DAT_SIG "data/lines.dat.sig"

int main() {
	set_locale();

	if (get_stations(STATIONS_DAT, STATIONS_DAT_SIG) != 0) {
		printf("Error occured while stations info loading.\n");
		return 1;
	}

	if (get_edges(EDGES_DAT, EDGES_DAT_SIG) != 0) {
		printf("Error occured while edges info loading.\n");
		return 1;
	}

	if (get_lines(LINES_DAT, LINES_DAT_SIG) != 0) {
		printf("Error occured while lines info loading.\n");
		return 1;
	}
	
	char startname[64] = { 0 }, endname[64] = {0};
	int start, end;
	
	while (1) {
		printf("Введите название начальной станции: ");

		fgets(startname, (int)sizeof(startname), stdin);
		char* _endl_pos = strchr(startname, '\n');
		if (_endl_pos)
			*_endl_pos = '\0';
		else {
			printf("Введено некорректное название станции.\n");
			char c;
			while ((c = getchar()) != '\n' && c != EOF); // дочитываем мусор из входного потока
			continue;
		}

		printf("Введение название конечной станциии: ");

		fgets(endname, (int)sizeof(endname), stdin);
		_endl_pos = strchr(endname, '\n');
		if (_endl_pos)
			*_endl_pos = '\0';
		else {
			printf("Введено некорректное название станции.\n");
			char c;
			while ((c = getchar()) != '\n' && c != EOF);
			continue;
		}

		start = find_station(startname);
		end = find_station(endname);

		if (start == -1 || end == -1) {
			printf("Одна из найденных станций не найдена.\n");
			continue;
		}

		Path* path = fshortpath(start, end);
		if (path) {
			printPath(path);
			freePath(path);
		}
		else
			printf("Маршрут не найден.\n");
	}

	free_all();
	printf("Программа завершена корректно.\n");

	return 0;
}