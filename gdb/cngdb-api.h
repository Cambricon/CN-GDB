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

#ifndef CNGDB_API_H
#define CNGDB_API_H

#include "cndebugger.h"

/* cngdb-api state */
typedef enum {
  CNDBG_API_STATE_UNINITIALIZED,
  CNDBG_API_STATE_INITIALIZED,
} cngdb_api_state_t;

/* cngdb-api version */
typedef enum  {
  CNDBG_SIMULATOR_1M,
  CNDBG_C20,
} cngdb_api_version_t;

/* initializetion/finalization */
void cngdb_api_initialize (cngdb_api_version_t version);
void cngdb_api_finalize (void);

/* task start/finish, also enter/exit device */
void * cngdb_api_task_build (void * args);
void cngdb_api_task_destroy (void);

/* state query */
cngdb_api_state_t cngdb_api_get_state (void);

/* breakpoints */
CNDBGResult cngdb_api_insert_breakpoint (uint64_t pc);
CNDBGResult cngdb_api_remove_breakpoint (uint64_t pc);

/* execetion control */
CNDBGResult cngdb_api_resume (int step, uint32_t dev,
                              uint32_t cluster, uint32_t core);
CNDBGResult cngdb_api_resume_all (void);

/* device state read/write */
CNDBGResult cngdb_api_read_greg  (uint64_t reg_id, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_read_sreg  (uint64_t reg_id, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_read_preg  (uint64_t reg_id, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_read_pc    (uint32_t dev, uint32_t cluster,
                                  uint32_t core, uint64_t *pc);
CNDBGResult cngdb_api_read_nram  (uint64_t addr, uint64_t size, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_read_wram  (uint64_t addr, uint64_t size, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_read_sram  (uint64_t addr, uint64_t size, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_read_dram  (uint64_t addr, uint64_t size, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_write_greg (uint64_t reg_id, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_write_sreg (uint64_t reg_id, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_write_preg (uint64_t reg_id, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_write_pc   (uint32_t dev, uint32_t cluster,
                                  uint32_t core, uint64_t pc);
CNDBGResult cngdb_api_write_nram (uint64_t addr, uint64_t size, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_write_wram (uint64_t addr, uint64_t size, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_write_sram (uint64_t addr, uint64_t size, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_api_write_dram (uint64_t addr, uint64_t size, uint32_t dev,
                                  uint32_t cluster, uint32_t core, void *val);

/* events */
CNDBGResult cngdb_api_query_stop (uint32_t *res);
CNDBGResult cngdb_api_get_sync_event (CNDBGEvent *event);

/* error handle */
CNDBGResult cngdb_api_analysis_error (void);

/* binary info transfer */
CNDBGResult cngdb_api_trandfer_info (void *info);

/* get valid core */
CNDBGResult cngdb_api_get_valid_core (uint32_t *dev, uint32_t *cluster,
                                      uint32_t *core);

/* get instructions from backend */
CNDBGResult cngdb_api_get_inst (void **cninst_content,
				unsigned long *cninst_size);

/* get arch info */
int cngdb_api_get_arch_info();

#endif
