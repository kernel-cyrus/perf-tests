#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sched.h>
#include <sys/mman.h>
#include "perf_case.h"
#include "perf_stat.h"
#include "arch/arm_pmuv3.h"

static struct perf_option default_options[] = {
	{{"help",   optional_argument, NULL, 'h' }, "h",  "Help."},
	{{"cpu",    optional_argument, NULL, 'c' }, "c:", "Choose a CPU to run."},
	{{"events", optional_argument, NULL, 'e' }, "e:", "Run case with events. (case|default|armv8|orin)."},
};

static struct perf_eventset *g_eventset = NULL;

static struct perf_event default_events[] = {
	PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "cpu-cycles"),
	PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "instructions"),
	PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES, "cache-refs"),
	PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES, "cache-misses"),
	PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND, "stall-frontend"),
	PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND, "stall-backend"),
	PERF_EVENT(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS, "page-faults")
};

static struct perf_event armv8_events[] = {

	/* basic events */
	PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, 		"cpu-cycles"),
	PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, 		"instructions"),
	PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES, 		"cache-refs"),
	PERF_EVENT(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES, 		"cache-misses"),
	PERF_EVENT(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS, 		"page-faults"),
	PERF_EVENT(PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES, 		"context-switch"),

	/* memory */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_MEM_ACCESS, 		"mem_access"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_MEM_ACCESS_RD, 		"mem_access_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_MEM_ACCESS_WR, 		"mem_access_wr"),

	/* branch */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_BR_PRED, 			"br_pred"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_BR_MIS_PRED, 		"br_miss_pred"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_BR_RETIRED, 		"br_retired"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_BR_MIS_PRED_RETIRED, 	"br_miss_pred_retired"),

	/* stall */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_STALL, 			"stall"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_STALL_FRONTEND, 		"stall_frontend"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_STALL_BACKEND, 		"stall_backend"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_STALL_SLOT, 		"stall_slot"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_STALL_SLOT_BACKEND, 	"stall_slot_backend"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_STALL_SLOT_FRONTEND, 	"stall_slot_frontend"),

	/* tlb */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1I_TLB, 			"l1i_tlb"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1I_TLB_REFILL, 		"l1i_tlb_refill"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2I_TLB, 			"l2i_tlb"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2I_TLB_REFILL, 		"l2i_tlb_refill"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1D_TLB, 			"l1d_tlb"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_TLB_RD, 		"l1d_tlb_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_TLB_WR, 		"l1d_tlb_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1D_TLB_REFILL, 		"l1d_tlb_refill"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_TLB_REFILL_RD, 	"l1d_tlb_refill_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_TLB_REFILL_WR, 	"l1d_tlb_refill_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2D_TLB, 			"l2d_tlb"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_TLB_RD, 		"l2d_tlb_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_TLB_WR, 		"l2d_tlb_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2D_TLB_REFILL, 		"l2d_tlb_refill"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_TLB_REFILL_RD, 	"l2d_tlb_refill_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_TLB_REFILL_WR, 	"l2d_tlb_refill_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_ITLB_WALK, 		"itlb_walk"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_DTLB_WALK, 		"dtlb_walk"),

	/* l1i */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1I_CACHE, 		"l1i_cache"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1I_CACHE_REFILL, 	"l1i_cache_refill"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1I_CACHE_LMISS, 		"l1i_cache_lmiss"),

	/* l2i */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2I_CACHE, 		"l2i_cache"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2I_CACHE_REFILL, 	"l2i_cache_refill"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2I_CACHE_LMISS, 		"l2i_cache_lmiss"),

	/* l1d */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1D_CACHE, 		"l1d_cache"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_CACHE_RD, 		"l1d_cache_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_CACHE_WR, 		"l1d_cache_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1D_CACHE_REFILL, 	"l1d_cache_refill"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_RD, 	"l1d_cache_refill_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_WR, 	"l1d_cache_refill_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_INNER, 	"l1d_cache_refill_inner"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_CACHE_REFILL_OUTER, 	"l1d_cache_refill_outer"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1D_CACHE_ALLOCATE, 	"l1d_cache_allocate"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_CACHE_INVAL, 	"l1d_cache_inval"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1D_CACHE_WB, 		"l1d_cache_wb"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_CACHE_WB_CLEAN, 	"l1d_cache_wb_clean"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L1D_CACHE_WB_VICTIM, 	"l1d_cache_wb_victim"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L1D_CACHE_LMISS_RD, 	"l1d_cache_lmiss_rd"),

	/* l2d */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2D_CACHE, 		"l2d_cache"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_CACHE_RD, 		"l2d_cache_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_CACHE_WR, 		"l2d_cache_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2D_CACHE_REFILL, 	"l2d_cache_refill"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_CACHE_REFILL_RD, 	"l2d_cache_refill_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_CACHE_REFILL_WR, 	"l2d_cache_refill_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2D_CACHE_ALLOCATE, 	"l2d_cache_allocate"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_CACHE_INVAL, 	"l2d_cache_inval"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2D_CACHE_WB, 		"l2d_cache_wb"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_CACHE_WB_CLEAN, 	"l2d_cache_wb_clean"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L2D_CACHE_WB_VICTIM, 	"l2d_cache_wb_victim"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L2D_CACHE_LMISS_RD, 	"l2d_cache_lmiss_rd"),

	/* l3d */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L3D_CACHE, 		"l3d_cache"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L3D_CACHE_RD, 		"l3d_cache_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L3D_CACHE_WR, 		"l3d_cache_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L3D_CACHE_REFILL, 	"l3d_cache_refill"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L3D_CACHE_REFILL_RD, 	"l3d_cache_refill_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L3D_CACHE_REFILL_WR, 	"l3d_cache_refill_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L3D_CACHE_ALLOCATE, 	"l3d_cache_allocate"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L3D_CACHE_INVAL, 	"l3d_cache_inval"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L3D_CACHE_WB, 		"l3d_cache_wb"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L3D_CACHE_WB_CLEAN, 	"l3d_cache_wb_clean"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_L3D_CACHE_WB_VICTIM, 	"l3d_cache_wb_victim"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_L3D_CACHE_LMISS_RD, 	"l3d_cache_lmiss_rd"),

	/* llc (impl) */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_LL_CACHE, 		"llc_cache"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_LL_CACHE_RD, 		"llc_cache_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_LL_CACHE_MISS, 		"llc_cache_miss"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_LL_CACHE_MISS_RD, 	"llc_cache_miss_rd"),

	/* bus (dsu) */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_BUS_CYCLES, 		"bus_cycles"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_BUS_ACCESS, 		"bus_access"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_RD, 		"bus_access_rd"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_WR, 		"bus_access_wr"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_NORMAL, 	"bus_access_normal"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_PERIPH, 	"bus_access_periph"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_SHARED, 	"bus_access_shared"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_BUS_ACCESS_NOT_SHARED, 	"bus_access_not_shared"),

	/* inst */
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_INST_SPEC, 		"inst_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_INST_RETIRED, 		"inst_retired"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_OP_SPEC, 			"op_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_OP_RETIRED, 		"op_retired"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_LD_SPEC, 		"ld_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_ST_SPEC, 		"st_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_LDREX_SPEC, 		"ldrex_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_STREX_SPEC, 		"strex_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_DP_SPEC, 		"dp_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_VFP_SPEC, 		"vfp_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_ASE_SPEC, 		"ase_spec"),
	/*
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_BR_IMMED_SPEC, 		"br_immed_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_BR_RETURN_SPEC, 		"br_return_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_BR_INDIRECT_SPEC, 	"br_indirect_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_ISB_SPEC, 		"isb_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_DSB_SPEC, 		"dsb_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_DMB_SPEC, 		"dmb_spec"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_STREX_PASS_SPEC, 	""),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_STREX_FAIL_SPEC, 	""),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_UNALIGNED_LDST_SPEC, 	""),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_UNALIGNED_LD_SPEC, 	""),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_UNALIGNED_ST_SPEC, 	""),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_LDST_SPEC, 		""),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_PC_WRITE_SPEC, 		""),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_CRYPTO_SPEC, 		""),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_LD_RETIRED, 		"ld_retired"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_ST_RETIRED, 		"st_retired"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_BR_IMMED_RETIRED, 	"br_immed_retired"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_BR_RETURN_RETIRED, 	"br_return_retired"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_UNALIGNED_LDST_RETIRED, 	""),
	*/

	/* exceptions
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_EXC_TAKEN, 		"excp_taken"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_PMUV3_PERFCTR_EXC_RETURN, 		"excp_return"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_UNDEF, 		"excp_undef"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_SVC, 		"excp_svc"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_PABORT, 		"excp_pabort"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_DABORT, 		"excp_dabort"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_IRQ, 		"excp_irq"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_FIQ, 		"excp_fiq"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_SMC, 		"excp_smc"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_HVC, 		"excp_hvc"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_TRAP_PABORT, 	"trap_pabort"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_TRAP_DABORT, 	"trap_dabort"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_TRAP_IRQ, 		"trap_irq"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_TRAP_FIQ, 		"trap_fiq"),
	PERF_EVENT(PERF_TYPE_RAW, ARMV8_IMPDEF_PERFCTR_EXC_TRAP_OTHER, 		"trap_other"),
	*/
};

// Orin events, only support run on CPU0
static struct perf_event orin_events[] = {
	PERF_RAW_EVENT("scf_pmu/bus_cycles", 			"scf_bus_cycles"),
	PERF_RAW_EVENT("scf_pmu/bus_access", 			"scf_bus_access"),
	PERF_RAW_EVENT("scf_pmu/bus_access_rd", 		"scf_bus_access_rd"),
	PERF_RAW_EVENT("scf_pmu/bus_access_wr", 		"scf_bus_access_wr"),
	PERF_RAW_EVENT("scf_pmu/bus_access_normal", 		"scf_bus_access_normal"),
	PERF_RAW_EVENT("scf_pmu/bus_access_periph", 		"scf_bus_access_periph"),
	PERF_RAW_EVENT("scf_pmu/bus_access_shared", 		"scf_bus_access_shared"),
	PERF_RAW_EVENT("scf_pmu/bus_access_not_shared", 	"scf_bus_access_unshared"),
	PERF_RAW_EVENT("scf_pmu/scf_cache", 			NULL),
	PERF_RAW_EVENT("scf_pmu/scf_cache_allocate", 		NULL),
	PERF_RAW_EVENT("scf_pmu/scf_cache_refill", 		NULL),
	PERF_RAW_EVENT("scf_pmu/scf_cache_wb", 			NULL),
};

static struct perf_eventset perf_event_sets[] = {
	PERF_EVENT_SET("default", default_events),
	PERF_EVENT_SET("armv8", armv8_events),
	PERF_EVENT_SET("orin", orin_events),
};

static struct perf_case *perf_cases[] = {
	PERF_CASE(memset_malloc),
	PERF_CASE(memset_malloc_x2),
	PERF_CASE(memset_mmap),
	PERF_CASE(memset_static_bss),
	PERF_CASE(memset_static_data),
	PERF_CASE(membw_rd_1),
	PERF_CASE(membw_rd_4),
	PERF_CASE(membw_rd_8),
	PERF_CASE(membw_rd_1_4x),
	PERF_CASE(membw_rd_4_4x),
	PERF_CASE(membw_rd_8_4x),
	PERF_CASE(membw_wr_1),
	PERF_CASE(membw_wr_4),
	PERF_CASE(membw_wr_8),
	PERF_CASE(membw_wr_1_4x),
	PERF_CASE(membw_wr_4_4x),
	PERF_CASE(membw_wr_8_4x),
	PERF_CASE(membw_cp_1),
	PERF_CASE(membw_cp_4),
	PERF_CASE(membw_cp_8),
	PERF_CASE(membw_cp_1_4x),
	PERF_CASE(membw_cp_4_4x),
	PERF_CASE(membw_cp_8_4x),
	PERF_CASE(memlat_random),
	PERF_CASE(cpuint_add),
	PERF_CASE(cpuint_mul),
	PERF_CASE(cpufp_add),
	PERF_CASE(cpufp_mul),
	PERF_CASE(cpusimd_add),
	PERF_CASE(cpusimd_mul),
	PERF_CASE(branch_pred),
	PERF_CASE(branch_next),
	PERF_CASE(ustress_branch_direct),
	PERF_CASE(ustress_branch_indirect),
	PERF_CASE(ustress_call_return),
	PERF_CASE(ustress_div32),
	PERF_CASE(ustress_div64),
	PERF_CASE(ustress_double2int),
	PERF_CASE(ustress_fpdiv),
	PERF_CASE(ustress_fpmac),
	PERF_CASE(ustress_fpmul),
	PERF_CASE(ustress_fpsqrt),
	PERF_CASE(ustress_int2double),
	PERF_CASE(ustress_isb),
	PERF_CASE(ustress_l1d_cache),
	PERF_CASE(ustress_l1d_tlb),
	PERF_CASE(ustress_l1i_cache),
	PERF_CASE(ustress_l2d_cache),
	PERF_CASE(ustress_load_after_store),
	PERF_CASE(ustress_mac32),
	PERF_CASE(ustress_mac64),
	PERF_CASE(ustress_memcpy),
	PERF_CASE(ustress_mul32),
	PERF_CASE(ustress_mul64),
	PERF_CASE(ustress_store_buffer_full),
};

static int g_cpu_id = -1;

static void init_cpu(int cpu)
{
	int err;
	pid_t pid = getpid();
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);

	err = sched_setaffinity(pid, sizeof(mask), &mask);
	if (err) {
		printf("ERROR: Set cpu affinity failed.\n");
		exit(0);
	}
	g_cpu_id = cpu;
	printf("Run on CPU: %d\n", cpu);
}

static int get_cpu()
{
	return g_cpu_id;
}

struct perf_case* perf_case_find(char* name)
{
	int case_num = sizeof(perf_cases) / sizeof(struct perf_case*);

	for (int i = 0; i < case_num; i++)
		if (!strcmp(name, perf_cases[i]->name))
			return perf_cases[i];

	return NULL;
}

struct perf_eventset* perf_eventset_find(char* name)
{
	int num = sizeof(perf_event_sets) / sizeof(struct perf_eventset);

	for (int i = 0; i < num; i++)
		if (!strcmp(name, perf_event_sets[i].name))
			return &perf_event_sets[i];

	return NULL;
}

struct perf_run* perf_case_create_run(struct perf_case *p_case)
{
	struct perf_run *p_run;
	struct perf_event *events;
	int event_num, stat_num, err;

	if (!p_case)
		return NULL;

	p_run = malloc(sizeof(struct perf_run));
	memset(p_run, 0, sizeof(struct perf_run));

	if (g_eventset) {
		events = g_eventset->events;
		event_num = g_eventset->event_num;
	} else if (p_case->events) {
		events = p_case->events;
		event_num = p_case->event_num;
	} else {
		events = default_events;
		event_num = sizeof(default_events) / sizeof(struct perf_event);
	}

	stat_num = ceill((float)event_num / (float)MAX_PERF_EVENTS);
	p_run->stats = malloc(sizeof(struct perf_stat) * stat_num);
	p_run->stat_num = stat_num;
	p_run->p_case = p_case;

	for (int i = 0; i < stat_num; i++) {
		err = perf_stat_init(									\
			&p_run->stats[i], p_case->name,							\
			events + MAX_PERF_EVENTS * i, 							\
			(i == stat_num - 1) ? (event_num - MAX_PERF_EVENTS * i) : MAX_PERF_EVENTS,	\
			get_cpu()									\
		);
		if (err)
			goto ERR_EXIT;
	}

	return p_run;

ERR_EXIT:
	free(p_run->stats);
	free(p_run);
	return NULL;
}

void perf_case_destroy_run(struct perf_run *p_run)
{
	free(p_run->stats);
	free(p_run);
}

int perf_case_run(struct perf_run* p_run, int argc, char **argv)
{
	struct perf_case *p_case;
	struct perf_stat *p_stat;
	long total_dur = 0;
	int err;

	p_case = p_run->p_case;

	for (int i = 0; i < p_run->stat_num; i++) {

		p_stat = &p_run->stats[i];

		if (p_case->init) {
			err = p_case->init(p_case, p_stat, argc, argv);
			if (err)
				return ERROR;
		}

		if (!p_case->inner_stat)
			perf_stat_begin(&p_run->stats[i]);

		p_case->func(p_case, p_stat);

		if (!p_case->inner_stat)
			perf_stat_end(&p_run->stats[i]);

		if (p_case->exit) {
			err = p_case->exit(p_case, p_stat);
			if (err)
				return ERROR;
		}

		if (!p_run->min_dur || p_stat->duration < p_run->min_dur)
			p_run->min_dur = p_stat->duration;
		if (!p_run->max_dur || p_stat->duration > p_run->max_dur)
			p_run->max_dur = p_stat->duration;
		total_dur += p_stat->duration;
	}

	p_run->avg_dur = total_dur / p_run->stat_num;

	return SUCCESS;
}

void perf_case_report_run(struct perf_run *p_run)
{
	printf("-----------------------\n");
	for (int i = 0; i < p_run->stat_num; i++)
		for (int j = 0; j < p_run->stats[i].event_num; j++)
			printf("    %-24s: %16ld\n",			\
				p_run->stats[i].events[j].event_name,	\
				p_run->stats[i].event_counts[j]		\
			);
	printf("-----------------------\n");
	if (p_run->stat_num > 1) {
		printf("finished with %d runs:\n", p_run->stat_num);
		printf("    min time: %f ms\n", (double)p_run->min_dur / 1000000);
		printf("    max time: %f ms\n", (double)p_run->max_dur / 1000000);
		printf("    avg time: %f ms\n", (double)p_run->avg_dur / 1000000);
	} else {
		printf("time: %f ms\n", (double)p_run->avg_dur / 1000000);
	}
}

void run_case(struct perf_case *p_case, int argc, char **argv)
{
	int err;
	struct perf_run *p_run;

	if (!p_case)
		return;

	p_run = perf_case_create_run(p_case);
	if (!p_run) {
		printf("ERROR: Failed to create a run.\n");
		exit(0);
	}

	printf("%s\n", p_case->name);
	printf("-----------------------\n");

	err = perf_case_run(p_run, argc, argv);
	if (err) {
		printf("ERROR: Case run failed.\n");
		exit(0);
	}

	perf_case_report_run(p_run);

	perf_case_destroy_run(p_run);
}

static void print_default_opts()
{
	int def_num = sizeof(default_options) / sizeof(struct perf_option);

	for (int i = 0; i < def_num; i++) {
		printf("    -%c, --%-14s %s\n",			\
			(char)default_options[i].opt.val,	\
			default_options[i].opt.name,		\
			default_options[i].desc			\
		);
	}
}

static void print_help()
{
	struct perf_case *p_case;
	int case_num = sizeof(perf_cases) / sizeof(struct perf_case*);

	printf("\n");
	printf("Perf-Case\n");
	printf("==========================\n");
	printf("A simple perf event test framework and some interesting testcases.\n\n");
	printf("Usage:\n");
	printf("    ./perf_case [case] [options]      // run a case\n");
	printf("    ./perf_case -h                    // help\n");
	printf("    ./perf_case -h [case]             // help for each case\n\n");
	printf("Options:\n");
	print_default_opts();
	printf("\n");
	printf("Cases Available:\n");
	for (int i = 0; i < case_num; i++) {
		p_case = perf_cases[i];
		printf("    %-26s - %s\n", p_case->name, p_case->desc);
	}
	printf("\n");
}

static void print_case_opts(struct perf_case *p_case)
{
	for (int i = 0; i < p_case->opts_num; i++) {
		printf("    -%c, --%-14s %s\n",			\
			(char)p_case->opts[i].opt.val,		\
			p_case->opts[i].opt.name,		\
			p_case->opts[i].desc			\
		);
	}
}

static void print_case_help(struct perf_case *p_case)
{
	printf("%s\n", p_case->name);
	printf("==========================\n");
	printf("%s\n\n", p_case->desc);
	printf("Usage:\n");
	printf("    ./perf_case %s [options]\n", p_case->name);
	printf("Options:\n");

	if (p_case->opts) {
		print_case_opts(p_case);
	} else {
		printf("    [no options]\n");
	}

	if (p_case->help) {
		p_case->help(p_case);
	}

	printf("\n");
}

static void init_opts(struct perf_case *p_case, int argc, char **argv)
{
	struct option *opts;
	char ostr[64];
	int opt, opt_idx;
	int opt_num, def_num;
	int i, j;
	int opt_cpu = 0;

	def_num = sizeof(default_options) / sizeof(struct perf_option);
	opt_num = def_num + p_case->opts_num;

	opts = malloc(sizeof(struct option) * (opt_num + 1));

	/* Add default options */
	for (i = 0; i < def_num; i++) {
		memcpy(&opts[i], &default_options[i].opt, sizeof(struct option));
		strcat(ostr, default_options[i].ostr);
	}

	/* Add case options */
	for (j = 0; j < p_case->opts_num; j++, i++) {
		memcpy(&opts[i], &p_case->opts[j].opt, sizeof(struct option));
		strcat(ostr, p_case->opts[j].ostr);
	}

	memset(&opts[i], 0, sizeof(struct option));

	while ((opt = getopt_long(argc, argv, ostr, opts, &opt_idx)) != -1) {
		switch (opt) {
		case 'h':
			print_case_help(p_case);
			exit(0);
		case 'c':
			opt_cpu = atoi(optarg);
			break;
		case 'e':
			if (!strcmp(optarg, "case"))
				break;
			g_eventset = perf_eventset_find(optarg);
			if (!g_eventset) {
				printf("ERROR: No event set named \"%s\"\n", optarg);
				exit(0);
			}
			printf("Enable events: %s\n", g_eventset->name);
			break;
		default:
			if (!p_case->getopt(p_case, opt))
				break;
			printf("ERROR: Invalid parameter, please run \"./perf_case -h\" for help.\n");
			exit(0);
		}
	}

	init_cpu(opt_cpu);

	free(opts);
}

int main(int argc, char **argv)
{
	char *case_name;
	struct perf_case *p_case;

	if (argc < 2) {
		print_help();
		return 0;
	}

	if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		if (argc == 2 || argv[2][0] == '-') {
			print_help();
			return 0;
		}
		case_name = argv[2];
		p_case = perf_case_find(case_name);
		if (!p_case) {
			printf("ERROR: No case named \"%s\"\n", case_name);
			return 0;
		}
		print_case_help(p_case);
		return 0;
	}

	case_name = argv[1];

	p_case = perf_case_find(case_name);
	if (!p_case) {
		printf("ERROR: No case named \"%s\"\n", case_name);
		return 0;
	}

	init_opts(p_case, argc, argv);

	run_case(p_case, argc, argv);

	return 0;
}
