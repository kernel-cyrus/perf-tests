#ifndef __PERF_STAT_H
#define __PERF_STAT_H

#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <linux/perf_event.h>

#define EVENT_NAME_LEN		32
#define STAT_NAME_LEN		32
#define MAX_PERF_EVENTS		6
#define SUCCESS			0
#define ERROR			-1

#define PERF_EVENT(_type, _id, _name) \
	{.event_name = _name, .type = _type, .event_id = _id, .event_path = NULL}

#define PERF_RAW_EVENT(_path, _name) \
	{.event_name = _name, .type = PERF_TYPE_RAW, .event_id = 0, .event_path = _path}

struct perf_event {
	char *event_name;
	char *event_path;
	uint32_t type;
	uint64_t event_id;
};

struct perf_stat {
	char name[STAT_NAME_LEN];
	struct perf_event *events;
	uint64_t event_counts[MAX_PERF_EVENTS];
	int event_fds[MAX_PERF_EVENTS];
	int event_num;
	int cpu;
	struct timespec start;
	struct timespec end;
	long duration;
};

/* easy to use macros */
#define PERF_STAT_BEGIN(name, events, event_num) 				\
	do {									\
		int cpu, err;							\
		struct perf_stat *__stat = malloc(sizeof(struct perf_stat));	\
		cpu = sched_getcpu()						\
		err = perf_stat_init(__stat, name, events, event_num, cpu);	\
		if (err) {							\
			free(__stat);						\
			break;							\
		}								\
		perf_stat_begin(__stat);

#define PERF_STAT_END()								\
		perf_stat_end(__stat);						\
		perf_stat_report(__stat);					\
		free(__stat);							\
	} while (0)

/* perf event interfaces */
int perf_event_open(uint32_t type, uint64_t event_id, int cpu);
int perf_event_start(int fd);
int perf_event_stop(int fd);
uint64_t perf_event_read(int fd);
int perf_event_close(int fd);

/* perf stat interfaces */
int perf_stat_init(struct perf_stat *stat, const char* name, struct perf_event *events, int event_num, int cpu);
void perf_stat_begin(struct perf_stat *stat);
void perf_stat_end(struct perf_stat *stat);
void perf_stat_report(struct perf_stat *stat);

#endif
