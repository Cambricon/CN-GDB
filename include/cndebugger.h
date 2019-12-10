/*
   CAMBRICON CNGDB Copyright(C) 2018 cambricon Corporation
   This file is part of cngdb.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef CNDEBUGGER_H
#define CNDEBUGGER_H
#include <stdlib.h>
#include "stdint.h"

#define p_red(str)   "\033[1;31m" str "\033[0m"
#define p_green(str) "\033[1;32m" str "\033[0m"
typedef enum {
  CNDBG_HW_UNKNOWN,
  CNDBG_HW_1M_SIM,
  CNDBG_HW_1M_SIMM,
  CNDBG_HW_C20,
} CNGDB_hw_type;

// use defination to switch hardware mode
// by default, use c20 hardware
#ifdef SIM_1M
  static CNGDB_hw_type cngdb_hw = CNDBG_HW_1M_SIM;
#elif  SIMM_1M
  static CNGDB_hw_type cngdb_hw = CNDBG_HW_1M_SIMM;
#elif  DRV_1M
  static CNGDB_hw_type cngdb_hw = CNDBG_HW_C20;
#else
  static CNGDB_hw_type cngdb_hw = CNDBG_HW_UNKNOWN;
#endif


/* OS-agnostic _CNDBG_INLINE */
#if defined(_WIN32)
#define _CNDBG_INLINE __inline
#else
#define _CNDBG_INLINE inline
#endif

/* ------------- API VERSION ------------- */

#define CNDBG_API_VERSION_MAJOR 0 /* Major release version number */
#define CNDBG_API_VERSION_MINOR 0 /* Minor release version number */
#define CNDBG_API_VERSION_REVISION 0 /* Revision (build) number */

/* -------------- CONSTANT --------------- */

#define CNDBG_MAX_DEVICES    1   /* Maximum number of supported device */
#define CNDBG_MAX_CLUSTERS   4   /* Maximum number of cluster per device */
#define CNDBG_MAX_CORES      4   /* Maximum number of core per cluster */

/* -------------- TYPE DEF ---------------- */
//typedef               char    int8_t;
//typedef unsigned      char   uint8_t;
//
//typedef               short  int16_t;
//typedef unsigned      short uint16_t;
//
//typedef               long   int32_t;
//typedef unsigned      long  uint32_t;
//
//typedef          long long   int64_t;
//typedef unsigned long long  uint64_t;

/*----------------------------- API Return Types -----------------------------*/

typedef enum {
    CNDBG_SUCCESS                           = 0x0000,  /* Successful execution */
    CNDBG_ERROR_UNKNOWN                     = 0x0001,  /* Error type not listed below */
    CNDBG_ERROR_UNINITIALIZED               = 0x0002,  /* Debugger API has not yet been properly initialized */
    CNDBG_ERROR_INVALID_COORDINATES         = 0x0003,  /* Invalid coordinates were provided */
    CNDBG_ERROR_INTERNAL                    = 0x0004,  /* A debugger internal error occurred */
    CNDBG_ERROR_INVALID_DEVICE              = 0x0005,  /* Specified device cannot be found */
    CNDBG_ERROR_INVALID_CLUSTER             = 0x0006,  /* Specified cluster cannot be found */
    CNDBG_ERROR_INVALID_CORE                = 0x0007,  /* Specified core cannot be found */
    CNDBG_ERROR_SUSPENDED_DEVICE            = 0x0008,  /* device is suspended */
    CNDBG_ERROR_RUNNING_DEVICE              = 0x0009,  /* device is running and not suspended */
    CNDBG_ERROR_INVALID_ADDRESS             = 0x000a,  /* address is out-of-range */
    CNDBG_ERROR_INCOMPATIBLE_API            = 0x000b,  /* API version does not match */
    CNDBG_ERROR_INITIALIZATION_FAILURE      = 0x000c,  /* The CNGDB Driver failed to initialize */
    CNDBG_ERROR_NO_EVENT_AVAILABLE          = 0x000e,  /* No event left to be processed */
    CNDBG_ERROR_COMMUNICATION_FAILURE       = 0x000f,  /* Communication error between the debugger and the application. */
    CNDBG_ERROR_FORK_FAILED                 = 0x0010,  /* Error while forking the debugger process */
    CNDBG_ERROR_NO_DEVICE_AVAILABLE         = 0x0011,  /* No capable mlu device was found */
    CNDBG_ERROR_BAD_CALL                    = 0x0012,  /* Bad time to call function */
    CNDBG_ERROR_FINISHED                    = 0x0013,  /* Device already finish, call failed */
    CNDBG_ERROR_NO_VALID                    = 0x0014,  /* Query valid core while no valid */
    CNDBG_ERROR_BAD_BREAKPOINT              = 0x0015,  /* Bad breakpoint */
} CNDBGResult;

static const char *CNDBGResultNames[45] = {
    "CNDBG_SUCCESS",
    "CNDBG_ERROR_UNKNOWN",
    "CNDBG_ERROR_UNINITIALIZED",
    "CNDBG_ERROR_INVALID_COORDINATES",
    "CNDBG_ERROR_INTERNAL",
    "CNDBG_ERROR_INVALID_DEVICE",
    "CNDBG_ERROR_INVALID_CLUSTER",
    "CNDBG_ERROR_INVALID_CORE",
    "CNDBG_ERROR_SUSPENDED_DEVICE",
    "CNDBG_ERROR_RUNNING_DEVICE",
    "CNDBG_ERROR_INVALID_ADDRESS",
    "CNDBG_ERROR_INCOMPATIBLE_API",
    "CNDBG_ERROR_INITIALIZATION_FAILURE",
    "CNDBG_ERROR_NO_EVENT_AVAILABLE",
    "CNDBG_ERROR_COMMUNICATION_FAILURE",
    "CNDBG_ERROR_FORK_FAILED",
    "CNDBG_ERROR_NO_DEVICE_AVAILABLE",
    "CNDBG_ERROR_BAD_CALL",
    "CNDBG_ERROR_FINISHED",
    "CNDBG_ERROR_NO_VALID",
    "CNDBG_ERROR_BAD_BREAKPOINT",
};

typedef enum {
  KERNEL_BREAKPOINT = 0x1,
  KERNEL_FINISH = 0x2,
  KERNEL_ERROR = 0x4,
  KERNEL_RUNNING = 0x8,
  KERNEL_SYNC = 0x10,
  KERNEL_UNKNOWN = 0x20,
} CNDBGEvent;

static const char *CNDBGEventNames[6] = {
  "KERNEL_BREAKPOINT",
  "KERNEL_FINISH",
  "KERNEL_ERROR",
  "KERNEL_RUNNING",
  "KERNEL_SYNC",
  "KERNEL_UNKNOWN",
};

static _CNDBG_INLINE const char *cndbgGetEventString (CNDBGEvent event)
{
  for (int i = 0; i < 6; i++)
    {
      if ((1 << i) & event)
        {
          return CNDBGEventNames[i];
        }
    }
  return "*UNDEFINED EVENT*";
}

/* cngdb task type, including:
     block task (1, 2, 3 or 4 cal core)
     union1 task (4 cal core + 1 mem core)
     union2 task (8 cal core + 2 mem core)
     union4 task (16 cal core + 4 mem core) */
typedef enum {
  CNDBG_TASK_ERROR,
  CNDBG_TASK_BLOCK1,
  CNDBG_TASK_BLOCK2,
  CNDBG_TASK_BLOCK3,
  CNDBG_TASK_BLOCK4,
  CNDBG_TASK_UNION1,
  CNDBG_TASK_UNION2,
  CNDBG_TASK_UNION4,
  CNDBG_TASK_UNINITIALIZE,
} CNDBGTaskType;

static _CNDBG_INLINE const char *cndbgGetErrorString (CNDBGResult error)
{
  if (((unsigned)error) * sizeof(char*) >= sizeof (CNDBGResultNames))
    return "*UNDEFINED*";
  return CNDBGResultNames[(unsigned)error];
}

/* cndbgapi : all cngdb device target has to implement following funcs */
struct CNDBGAPI_st {
  /* init & destroy */
  CNDBGResult (*initialize) ();
  CNDBGResult (*finalize) ();

  /* task build/destroy */
  CNDBGTaskType (*taskBuild) (int tid);
  CNDBGResult (*taskDestroy) ();

  /* breakpoints */
  CNDBGResult (*insertBreakpoint) (uint64_t pc);
  CNDBGResult (*removeBreakpoint) (uint64_t pc);

  /* execetion control */
  CNDBGResult (*resume) (int step, uint32_t dev, uint32_t cluster, uint32_t core);

  /* device state read/write */
  CNDBGResult (*readGReg)   (uint64_t reg_id, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*readSReg)   (uint64_t reg_id, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*readPReg)   (uint64_t reg_id, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*readPc)     (uint32_t dev, uint32_t cluster, uint32_t core, uint64_t *pc);
  CNDBGResult (*readNram)   (uint64_t addr, uint64_t size, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*readWram)   (uint64_t addr, uint64_t size, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*readSram)   (uint64_t addr, uint64_t size, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*readDram)   (uint64_t addr, uint64_t size, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*writeGReg)  (uint64_t reg_id, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*writeSReg)  (uint64_t reg_id, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*writePReg)  (uint64_t reg_id, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*writePc)    (uint32_t dev, uint32_t cluster, uint32_t core, uint64_t pc);
  CNDBGResult (*writeNram)  (uint64_t addr, uint64_t size, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*writeWram)  (uint64_t addr, uint64_t size, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*writeSram)  (uint64_t addr, uint64_t size, uint32_t dev, uint32_t cluster, uint32_t core, void *val);
  CNDBGResult (*writeDram)  (uint64_t addr, uint64_t size, uint32_t dev, uint32_t cluster, uint32_t core, void *val);

  /* events */
  CNDBGResult (*queryStop) (uint32_t *res);
  CNDBGResult (*queryDevice) (CNDBGEvent *enent);

  /* error handle */
  CNDBGResult (*analysisError) ();

  /* binary info */
  CNDBGResult (*transferInfo) (void *info);

  /* valid core query */
  CNDBGResult (*getValidCore) (uint32_t *dev, uint32_t *cluster, uint32_t *core);

  /* get instructions */
  CNDBGResult (*getInst) (void **cninst_content, unsigned long *cninst_size);

  /* get arch info */
  int (*getArchInfo) ();
};

typedef struct {
  uint32_t tid;
  uint32_t timeout;
} CNDBGEventCallbackData;


typedef const struct CNDBGAPI_st *CNDBGAPI;

typedef enum {
  CB_EQ = 2,
  CB_NE = 3,
  CB_GE = 4,
  CB_LT = 5,
} CB_type;

typedef enum {
  CB_uint16 = 1,
  CB_uint32 = 2,
  CB_int16  = 3,
  CB_int32  = 4,
  CB_uint48 = 13,
  CB_int48  = 14,
} OP_type;

typedef struct {
  uint64_t CBpc;
  CB_type CBcType;
  OP_type CBoType;
  int CBARegId;
  int CBBRegEn;
  int CBBRegId;
  uint64_t CBBImm;
  int CBreverse;
  int CBpredRegId;
  int CBaddrRegEn;
  uint32_t CBaddrImm;
  int CBaddrRegId;
} CNDBGBranch;

typedef struct {
  uint64_t BApc;
  int BAbarrierIdRegEn;
  int BAbarrierIdRegId;
  uint64_t BAbarrierIdImm;
  int BAbarrierCountRegEn;
  int BAbarrierCountRegId;
  uint64_t BAbarrierCountImm;
  int BAbarrierSync;
  int BAbarrierGlobal;
  int BApredRegId;
} CNDBGBarrier;

#endif
