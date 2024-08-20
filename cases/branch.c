#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../perf_stat.h"
#include "../perf_case.h"

struct branch_data {
	int num;
	int iterations;
};

static int opt_num = 1;
static int opt_iterations = 1000000;

static struct perf_option branch_opts[] = {
	{{"num", optional_argument, NULL, 'n' }, "n:", "Operations per loop. (n: 1-6)"},
	{{"iterations", optional_argument, NULL, 'i' }, "i:", "Iteration loops. (default: 1000K)"},
};

static int branch_getopt(struct perf_case* p_case, int opt)
{
	switch (opt) {
	case 'n':
		opt_num = atoi(optarg);
		break;
	case 'i':
		opt_iterations = atoi(optarg);
		break;
	default:
		return ERROR;
	}
	return SUCCESS;
}

static int branch_init(struct perf_case *p_case, struct perf_stat *p_stat, int argc, char *argv[])
{
	struct branch_data *p_data;
	
	p_case->data = malloc(sizeof(struct branch_data));
	if (!p_case->data)
		return ERROR;

	p_data = (struct branch_data*)p_case->data;

	p_data->num = opt_num;
	p_data->iterations = opt_iterations;

	printf("iterations: %d\n", p_data->iterations);

	return SUCCESS;
}

static int branch_exit(struct perf_case *p_case, struct perf_stat *p_stat)
{
	free(p_case->data);
	return SUCCESS;
}

#define JUMP1(pri, maj, min, sub)				\
	"b NEXT_BRANCH_" #pri "_" #maj "_" #min "_" #sub "\n"	\
	"NEXT_BRANCH_" #pri "_"  #maj "_" #min "_" #sub ":\n"

#define JUMP16(pri, maj, min)					\
	JUMP1(pri, maj, min, 0x0) JUMP1(pri, maj, min, 0x1)	\
	JUMP1(pri, maj, min, 0x2) JUMP1(pri, maj, min, 0x3)	\
	JUMP1(pri, maj, min, 0x4) JUMP1(pri, maj, min, 0x5)	\
	JUMP1(pri, maj, min, 0x6) JUMP1(pri, maj, min, 0x7)	\
	JUMP1(pri, maj, min, 0x8) JUMP1(pri, maj, min, 0x9)	\
	JUMP1(pri, maj, min, 0xA) JUMP1(pri, maj, min, 0xB)	\
	JUMP1(pri, maj, min, 0xC) JUMP1(pri, maj, min, 0xD)	\
	JUMP1(pri, maj, min, 0xE) JUMP1(pri, maj, min, 0xF)

#define JUMP256(pri, maj)					\
	JUMP16(pri, maj, 0x0) JUMP16(pri, maj, 0x1)		\
	JUMP16(pri, maj, 0x2) JUMP16(pri, maj, 0x3)		\
	JUMP16(pri, maj, 0x4) JUMP16(pri, maj, 0x5)		\
	JUMP16(pri, maj, 0x6) JUMP16(pri, maj, 0x7)		\
	JUMP16(pri, maj, 0x8) JUMP16(pri, maj, 0x9)		\
	JUMP16(pri, maj, 0xA) JUMP16(pri, maj, 0xB)		\
	JUMP16(pri, maj, 0xC) JUMP16(pri, maj, 0xD)		\
	JUMP16(pri, maj, 0xE) JUMP16(pri, maj, 0xF)

#define JUMP4096(pri)						\
	JUMP256(pri, 0x0) JUMP256(pri, 0x1)			\
	JUMP256(pri, 0x2) JUMP256(pri, 0x3)			\
	JUMP256(pri, 0x4) JUMP256(pri, 0x5)			\
	JUMP256(pri, 0x6) JUMP256(pri, 0x7)			\
	JUMP256(pri, 0x8) JUMP256(pri, 0x9)			\
	JUMP256(pri, 0xA) JUMP256(pri, 0xB)			\
	JUMP256(pri, 0xC) JUMP256(pri, 0xD)			\
	JUMP256(pri, 0xE) JUMP256(pri, 0xF)

#pragma GCC push_options
#pragma GCC optimize ("O0")
static void branch_next_func(struct perf_case *p_case, struct perf_stat *p_stat)
{
	struct branch_data *p_data = (struct branch_data*)p_case->data;
	int num = p_data->num, err = 0;
	register int i = 0, loops = p_data->iterations;
	perf_stat_begin(p_stat);
	if (num == 1) {
		while (i < loops) {
			__asm__ volatile (
				JUMP1(0x0, 0x0, 0x0, 0x0)
			);
			i++;
		}
	} else if (num == 16) {
		while (i < loops) {
			__asm__ volatile (
				JUMP16(0x1, 0x0, 0x0)
			);
			i++;
		}
	} else if (num == 256) {
		while (i < loops) {
			__asm__ volatile (
				JUMP256(0x2, 0x0)
			);
			i++;
		}
	} else if (num == 1024) {
		while (i < loops) {
			__asm__ volatile (
				JUMP256(0x3, 0x0)
				JUMP256(0x3, 0x1)
				JUMP256(0x3, 0x2)
				JUMP256(0x3, 0x3)
			);
			i++;
		}
	} else if (num == 4096) {
		while (i < loops) {
			__asm__ volatile (
				
				JUMP4096(0x4)
			);
			i++;
		}
	} else if (num == 8192) {
		while (i < loops) {
			__asm__ volatile (
				JUMP4096(0x5)
				JUMP4096(0x6)
			);
			i++;
		}
	} else {
		err = 1;
	}
	perf_stat_end(p_stat);
	if (err) {
		printf("ERROR: Only support n = [1, 16, 256, 1024, 4096, 8192]\n");
		exit(0);
	}
	/* Consume the data to avoid compiler optimizing. */
	printf("%d", i);
}
#pragma GCC pop_options

PERF_CASE_DEFINE(branch_next) = {
	.name = "branch_next",
	.desc = "jump to next instruction for n times.",
	.init = branch_init,
	.exit = branch_exit,
	.func = branch_next_func,
	.getopt = branch_getopt,
	.opts = branch_opts,
	.opts_num = sizeof(branch_opts) / sizeof(struct perf_option),
	.inner_stat = true
};