#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include "pathfinder.h"

#define STATIONSCOUNT 75
#define LINESCOUNT 6

typedef struct {
	unsigned char to;
	unsigned char time; // в минутах
	unsigned char transfer; // 1 - пересадка, 0 - нет
} Edge;

typedef struct {
	unsigned char id;
	char name[64];
	unsigned char line_id; // id линии метро (из lines.dat)
	unsigned char neighbors_count; // сколько соседей имеем
	Edge** neighbors_edges; // массив указателей на соседей
} Station;

typedef struct {
	unsigned char id;
	char name[64];
	char color[8];
} Line;

typedef struct {
	int* stations;
	int* dist;
	int size;
	int capacity;
} PriorityQueue;

Station* stations = NULL;
Line* lines = NULL;

int p1[] = { (int)'s' ^ 0xA0, (int)'i' ^ 0xA1, (int)'g' ^ 0xA2 };
int p2[] = { (int)'!' ^ 0xA2, (int)'!' ^ 0xA1, (int)'!' ^ 0xA0 };
int p3[] = { (int)'k' ^ 0xA0, (int)'e' ^ 0xA1, (int)'y' ^ 0xA2 };

int valid_file_check(char* filename, char* signame) {
	FILE* file = fopen(filename, "rb");
	if (!file)
		return 1;
	FILE* sign = fopen(signame, "rb");
	if (!sign)
		return 1;
	
	fseek(file, 0, SEEK_END); // 0 - offset, SEEK_END - origin (от конца файла)
	long size = ftell(file);
	rewind(file);

	unsigned char *data = (unsigned char*)malloc(size * sizeof(unsigned char));
	fread(data, sizeof(char), (size_t)size, file);

	char key[10] = { 0 };
	for (int i = 0; i < 3; i++) {
		key[i] = (char)(p1[i] ^ (0xA0 + i));
		key[i + 3] = (char)(p2[i] ^ (0xA2 - i));
		key[i + 6] = (char)(p3[i] ^ (0xA0 + i));
	}

	unsigned char hmac[32]; unsigned int hmac_len = 0;
	HMAC(EVP_sha256(), key, strlen(key), data, size, hmac, &hmac_len);
	memset(key, 0, sizeof(key));

	free(data);
	
	char buffer[32];
	fread(buffer, sizeof(char), sizeof(buffer), sign);

		if (CRYPTO_memcmp(buffer, hmac, sizeof(buffer)) != 0) {
		printf("Нарушена целостность файла %s.\n", filename);
		return 1;
	}

	fclose(file);
	fclose(sign);
	return 0;
}

int get_stations(char* filename, char* signame) {
	if (valid_file_check(filename, signame) != 0)
		return 1;

	FILE* file = fopen(filename, "r");
	if (!file) {
		printf("Unable to open stations file.\n");
		return 1;
	}

	stations = (Station*)calloc(STATIONSCOUNT, sizeof(Station));
	if (!stations) {
		fclose(file);
		return 1;
	}

	char line[128] = { 0 };
	
	int i = 0;
	while (fgets(line, (int)sizeof(line), file)) {
		if (line[0] == '#') continue;

		unsigned char id, line_id;
		char name[64] = { 0 };

		sscanf(line, "%hhu;%[^;];%hhu", &id, name, &line_id);

		stations[i].id = id;
		stations[i].line_id = line_id;
		strncpy(stations[i].name, name, sizeof(name));
		stations[i].neighbors_count = 0;
		stations[i].neighbors_edges = NULL;

		i++;
	}

	fclose(file);
	return 0;
}

int get_edges(char* filename, char* signame) {
	if (valid_file_check(filename, signame) != 0)
		return 1;

	FILE* file = fopen(filename, "r");
	if (!file) {
		printf("Unable to open edges file.\n");
		return 1;
	}

	char line[128] = { 0 };

	while (fgets(line, (int)sizeof(line), file)) {
		if (line[0] == '#') continue;

		unsigned char from, to, time, transfer;
		sscanf(line, "%hhu;%hhu;%hhu;%hhu", &from, &to, &time, &transfer);

		stations[from - 1].neighbors_count++;
		stations[to - 1].neighbors_count++;
	}

	for (int i = 0; i < STATIONSCOUNT; i++) {
		if (stations[i].neighbors_count > 0) {
			stations[i].neighbors_edges = (Edge**)calloc(stations[i].neighbors_count, sizeof(Edge*));
			if (!stations[i].neighbors_edges)
				return 1;
		}
	}

	rewind(file);

	int* current_edge = (int*)calloc(STATIONSCOUNT, sizeof(int));
	if (!current_edge)
		return 1;

	while (fgets(line, (int)sizeof(line), file)) {
		if (line[0] == '#') continue;

		unsigned char from, to, time, transfer;
		sscanf(line, "%hhu;%hhu;%hhu;%hhu", &from, &to, &time, &transfer);

		Edge* edge1 = (Edge*)malloc(sizeof(Edge));
		if (!edge1)
			return 1;
		edge1->to = to - 1;
		edge1->time = time;
		edge1->transfer = transfer;

		stations[from - 1].neighbors_edges[current_edge[from - 1]] = edge1;
		current_edge[from - 1]++;

		Edge* edge2 = (Edge*)malloc(sizeof(Edge));
		if (!edge2)
			return 1;
		edge2->to = from - 1;
		edge2->time = time;
		edge2->transfer = transfer;

		stations[to - 1].neighbors_edges[current_edge[to - 1]] = edge2;
		current_edge[to - 1]++;
	}

	free(current_edge);
	fclose(file);

	return 0;
}

int get_lines(char* filename, char* signame) {
	if (valid_file_check(filename, signame) != 0)
		return 1;
		
	FILE* file = fopen(filename, "r");
	if (!file) {
		printf("Unable to open lines file.\n");
		return 1;
	}

	lines = (Line*)calloc(LINESCOUNT, sizeof(Line));
	if (!lines) {
		fclose(file);
		return 1;
	}

	char line[128] = { 0 };
	
	int i = 0;
	while (fgets(line, (int)sizeof(line), file)) {
		if (line[0] == '#') continue;

		unsigned char id;
		char name[64] = { 0 };
		char color[8] = { 0 };

		sscanf(line, "%hhu;%[^;];%[^\n]", &id, name, color);

		lines[i].id = id;
		strncpy(lines[i].name, name, sizeof(name));
		strncpy(lines[i].color, color, sizeof(color));

		i++;
	}

	fclose(file);

	return 0;
}

void free_all() {
	if (!stations) return;

	if (stations) {
		for (int i = 0; i < STATIONSCOUNT; i++) {
			if (stations[i].neighbors_edges) {
				for (int j = 0; j < stations[i].neighbors_count; j++)
					free(stations[i].neighbors_edges[j]);
				free(stations[i].neighbors_edges);
			}
		}
		free(stations);
		stations = NULL;
	}
	else if (lines) {
		free(lines);
		lines = NULL;
	}
}

PriorityQueue* make_queue(int capacity) {
	PriorityQueue* pq = (PriorityQueue*)malloc(sizeof(PriorityQueue));
	if (!pq) return NULL;

	pq->stations = (int*)malloc(capacity * sizeof(int));
	if (!pq->stations) return NULL;

	pq->dist = (int*)malloc(capacity * sizeof(int));
	if (!pq->dist) return NULL;

	pq->size = 0;
	pq->capacity = capacity;
	return pq;
}

void push_queue(PriorityQueue* pq, int station, int dist) {
	pq->stations[pq->size] = station;
	pq->dist[pq->size] = dist;
	pq->size++;

	for (int i = pq->size - 1; i > 0; i--) {
		if (pq->dist[i] < pq->dist[i - 1]) {
			int temp_station = pq->stations[i];
			int temp_dist = pq->dist[i];
			
			pq->stations[i] = pq->stations[i - 1];
			pq->dist[i] = pq->dist[i - 1];
			pq->stations[i - 1] = temp_station;
			pq->dist[i - 1] = temp_dist;
		} 
		else
			break;
	}
}

int pop_queue(PriorityQueue* pq) {
	if (pq->size == 0) return -1;

	int result = pq->stations[0];

	for (int i = 0; i < pq->size - 1; i++) {
		pq->stations[i] = pq->stations[i + 1];
		pq->dist[i] = pq->dist[i + 1];
	}
	pq->size--;

	return result;
}

void free_queue(PriorityQueue* pq) {
	free(pq->stations);
	free(pq->dist);
	free(pq);
}

Path* fshortpath(int start, int end) {
	if (start < 0 || start >= STATIONSCOUNT || end < 0 || end >= STATIONSCOUNT)
		return NULL;

	unsigned short* dist = (unsigned short*)calloc(STATIONSCOUNT, sizeof(unsigned short));
	if (!dist)
		return NULL;
	short* previous = (short*)calloc(STATIONSCOUNT, sizeof(short));
	if (!previous)
		return NULL;
	unsigned char* visited = (unsigned char*)calloc(STATIONSCOUNT, sizeof(unsigned char));
	if (!visited)
		return NULL;
	unsigned char* previous_transfer = (unsigned char*)calloc(STATIONSCOUNT, sizeof(unsigned char));
	if (!previous_transfer)
		return NULL;

	for (int i = 0; i < STATIONSCOUNT; i++) {
		dist[i] = USHRT_MAX;
		previous[i] = -1;
		previous_transfer[i] = 0;
	}

	dist[start] = 1; // закладываем предварительное ожидание поезда

	PriorityQueue* pq = make_queue(STATIONSCOUNT);
	if (!pq)
		return NULL;
	push_queue(pq, start, 0);

	while (pq->size != 0) {
		int current = pop_queue(pq);
		if (current < 0 || current >= STATIONSCOUNT)
			break;

		if (visited[current]) continue;
		visited[current] = 1;

		if (current == end) break;

		for (unsigned char i = 0; i < stations[current].neighbors_count; i++) {
			Edge* edge = stations[current].neighbors_edges[i];
			unsigned char neighbor = edge->to;

			if (visited[neighbor]) continue;

			unsigned short new_dist = dist[current] + edge->time;
			if (stations[current].line_id != stations[neighbor].line_id)
				new_dist += 3; // если пересадка, то докидываем время ожидания поезда

			if (new_dist < dist[neighbor]) {
				dist[neighbor] = new_dist;
				previous[neighbor] = current;
				previous_transfer[neighbor] = edge->transfer;
				push_queue(pq, neighbor, new_dist);
			}
		}
	}

	if (dist[end] == USHRT_MAX) {
		free(dist); free(previous); free(visited); free(previous_transfer); free_queue(pq);
		return NULL;
	}

	unsigned short pathlen = 0;
	int current = end;
	while (current != -1) {
		pathlen++;
		current = previous[current];
	}

	Path* path = (Path*)malloc(sizeof(Path));
	if (!path)
		return NULL;
	path->stations = (char*)malloc(pathlen * sizeof(char));
	if (!path->stations)
		return NULL;
	path->length = pathlen;
	path->total_time = dist[end];
	path->transfers = 0;

	current = end;
	for (short i = pathlen - 1; i >= 0; i--) {
		path->stations[i] = current;
		current = previous[current];
	}

	char current_line = stations[(int)path->stations[0]].line_id;
	for (unsigned short i = 1; i < pathlen; i++) {
		unsigned char station_line = stations[(int)path->stations[i]].line_id;
		if (station_line != current_line) {
			path->transfers++;
			current_line = station_line;
		}
	}

	free(dist); free(previous); free(visited); free(previous_transfer); free_queue(pq);
	return path;
}

int find_station(char* name) {
	for (int i = 0; i < STATIONSCOUNT; i++) {
		if (strcmp(stations[i].name, name) == 0)
			return i;
	}
	return -1;
}

void printPath(Path* path) {
	if (!path || path->length == 0) {
		printf("Path has not been found.\n");
		return;
	}
	
	unsigned char current_line = stations[(int)path->stations[0]].line_id;
	printf("%s", stations[(int)path->stations[0]].name);

	for (int i = 1; i < path->length; i++) {
		unsigned char next_line = stations[(int)path->stations[i]].line_id;

		if (next_line != current_line) {
			for (int line_count = 0; line_count < LINESCOUNT; line_count++) {
				if (next_line == lines[line_count].id) {
					printf("\n -- пересадка на линию %hhu (%s, %s)\n", lines[line_count].id, lines[line_count].name,lines[line_count].color);
					break;
				}
			}
			printf("%s", stations[(int)path->stations[i]].name);
			current_line = next_line;
		}
		else
			printf(" to %s", stations[(int)path->stations[i]].name);
	}

	printf("\nОбщее время: %hu минут\n", path->total_time);
	printf("Пересадок: %hhu\n", path->transfers);
}

void freePath(Path* path) {
	if (path) {
		free(path->stations);
		free(path);
	}
}