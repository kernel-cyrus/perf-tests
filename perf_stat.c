#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

#include "perf_stat.h"

int __perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

int perf_event_open(uint32_t type, uint64_t event_id)
{
	struct perf_event_attr attr;

	memset(&attr, 0, sizeof(struct perf_event_attr));

	attr.size = sizeof(struct perf_event_attr);
	attr.type = type;
	attr.config = event_id;
	attr.disabled = 1;

	//printf("type=%d, event=0x%llx, size=%d\n", attr.type, attr.config, attr.size);

	return __perf_event_open(&attr, 0, -1, -1, 0);
}

int perf_event_start(int fd)
{
	return ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}

int perf_event_stop(int fd)
{
	return ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
}

uint64_t perf_event_read(int fd)
{
	int ret;
	uint64_t count;

	ret = read(fd, &count, sizeof(count));
	if (ret < 0)
		return 0;

	return count;
}

int perf_event_close(int fd)
{
	return close(fd);
}

void perf_simple_stat()
{
	int fd;
	uint64_t count;

	fd = perf_event_open(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK);
	if (fd < 0) {
		printf("ERROR: Event not supported.");
		exit(-1);
	}

	perf_event_start(fd);
	sleep(1);
	perf_event_stop(fd);

	count = perf_event_read(fd);
	printf("count: %ld\n", count);

	perf_event_close(fd);
}

int __read_event_file(const char* path, char* buf, int buf_size)
{
	FILE *file;
	int n_bytes;

	file = fopen(path, "r");
	if (file == NULL)
		return ERROR;

	n_bytes = fread(buf, 1, buf_size - 1, file);

	fclose(file);
	return n_bytes;
}

int perf_stat_init_raw_event(struct perf_event *event)
{
	char *device_name, *file_name;
	char file_buf[16];
	static char file_path[128];
	static char event_path[128];
	int n_bytes, offset, end;

	memset(event_path, 0, sizeof(event_path));
	strncpy(event_path, event->event_path, sizeof(event_path) - 1);

	device_name = strtok(event_path, "/");
	if (!device_name)
		return ERROR;

	file_name = strtok(NULL, "/");
	if (!file_name)
		return ERROR;

	if (!event->event_name)
		event->event_name = event->event_path + (file_name - event_path);

	// Get type
	sprintf(file_path, "/sys/bus/event_source/devices/%s/type", device_name);

	n_bytes = __read_event_file(file_path, file_buf, sizeof(file_buf));
	if (n_bytes <= 0)
		return ERROR;

	file_buf[n_bytes] = '\0';
	event->type = atoi(file_buf);

	// Get format
	sprintf(file_path, "/sys/bus/event_source/devices/%s/format/event", device_name);

	n_bytes = __read_event_file(file_path, file_buf, sizeof(file_buf));
	if (n_bytes <= 0)
		return ERROR;

	file_buf[n_bytes] = '\0';
	sscanf(file_buf, "config:%d-%d", &offset, &end);

	// Get event
	sprintf(file_path, "/sys/bus/event_source/devices/%s/events/%s", device_name, file_name);

	n_bytes = __read_event_file(file_path, file_buf, sizeof(file_buf));
	if (n_bytes <=0 )
		return ERROR;

	file_buf[n_bytes] = '\0';
	sscanf(file_buf, "event=%lX", &event->event_id);
	event->event_id = event->event_id << offset;

	return SUCCESS;
}

void perf_stat_init_events(struct perf_event *events, int event_num)
{
	int err;
	for (int i = 0; i < event_num; i++) {
		if (events[i].event_path && !events[i].event_id) {
			err = perf_stat_init_raw_event(&events[i]);
			if (err) {
				printf("WARNING: Can not open event: %s\n", events[i].event_path);
				continue;
			}
		}
	}
}

int perf_stat_init(struct perf_stat *stat, const char* name, struct perf_event *events, int event_num)
{
	if (!stat || !name || !events)
		return ERROR;

	if (event_num > MAX_PERF_EVENTS)
		return ERROR;

	perf_stat_init_events(events, event_num);

	memset(stat, 0, sizeof(struct perf_stat));
	stat->events = events;
	stat->event_num = event_num;
	strncpy(stat->name, name, sizeof(stat->name) - 1);

	return SUCCESS;
}

void perf_stat_begin(struct perf_stat *stat)
{
	for (int i = 0; i < stat->event_num; i++)
		stat->event_fds[i] = perf_event_open(stat->events[i].type, stat->events[i].event_id);

	clock_gettime(CLOCK_MONOTONIC, &stat->start);

	for (int i = 0; i < stat->event_num; i++)
		if (stat->event_fds[i] > 0)
			perf_event_start(stat->event_fds[i]);
}

void perf_stat_end(struct perf_stat *stat)
{
	long secs, nano;

	for (int i = 0; i < stat->event_num; i++)
		if (stat->event_fds[i] > 0)
			perf_event_stop(stat->event_fds[i]);

	clock_gettime(CLOCK_MONOTONIC, &stat->end);
	secs = stat->end.tv_sec - stat->start.tv_sec;
	nano = stat->end.tv_nsec - stat->start.tv_nsec;
	stat->duration = secs * 1000000000L + nano;

	for (int i = 0; i < stat->event_num; i++) {
		if (stat->event_fds[i] > 0) {
			stat->event_counts[i] = perf_event_read(stat->event_fds[i]);
			perf_event_close(stat->event_fds[i]);
		}
	}
}

void perf_stat_report(struct perf_stat *stat)
{
	printf("TEST: %s\n", stat->name);
	printf("-----------------------\n");
	for (int i = 0; i < stat->event_num; i++)
		printf("%*s: %*ld\n", 16, stat->events[i].event_name, 16, stat->event_counts[i]);
	printf("-----------------------\n");
	printf("Time spent: %f ms\n\n", (double)stat->duration / 1000000);
}
