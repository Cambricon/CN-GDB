/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *************************************************************************/
#ifndef CNGDB_BACKEND_1M_DRV_CNGDB_BACKEND_H_
#define CNGDB_BACKEND_1M_DRV_CNGDB_BACKEND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "../../include/cndebugger.h"

/* init & destory */
CNDBGResult cngdb_backend_initialize();
CNDBGResult cngdb_backend_finalize(void);

/* build/destroy */
CNDBGTaskType cngdb_backend_task_build(int pid);
CNDBGResult cngdb_backend_task_destroy(void);

/* breakpoints */
CNDBGResult cngdb_backend_insert_breakpoint(uint64_t pc);
CNDBGResult cngdb_backend_remove_breakpoint(uint64_t pc);

/* execetion control */
CNDBGResult cngdb_backend_resume(int step, uint32_t dev, uint32_t cluster,
                                 uint32_t core);
CNDBGResult cngdb_backend_resume_all(void);


/* device state read/write */
CNDBGResult cngdb_backend_read_greg(uint64_t reg_id, uint32_t dev,
                                    uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_backend_read_sreg(uint64_t reg_id, uint32_t dev,
                                    uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_backend_read_preg(uint64_t reg_id, uint32_t dev,
                                    uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_backend_read_pc(uint32_t dev, uint32_t cluster,
                                    uint32_t core, uint64_t *pc);
CNDBGResult cngdb_backend_read_nram(uint64_t addr, uint64_t size, uint32_t dev,
                                    uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_backend_read_wram(uint64_t addr, uint64_t size, uint32_t dev,
                                    uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_backend_read_sram(uint64_t addr, uint64_t size, uint32_t dev,
                                    uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_backend_read_dram(uint64_t addr, uint64_t size, uint32_t dev,
                                    uint32_t cluster, uint32_t core, void *val);
CNDBGResult cngdb_backend_write_greg(uint64_t reg_id, uint32_t dev,
                                     uint32_t cluster, uint32_t core,
                                     void *val);
CNDBGResult cngdb_backend_write_sreg(uint64_t reg_id, uint32_t dev,
                                     uint32_t cluster, uint32_t core,
                                     void *val);
CNDBGResult cngdb_backend_write_preg(uint64_t reg_id, uint32_t dev,
                                     uint32_t cluster, uint32_t core,
                                     void *val);
CNDBGResult cngdb_backend_write_pc(uint32_t dev, uint32_t cluster,
                                   uint32_t core, uint64_t pc);
CNDBGResult cngdb_backend_write_nram(uint64_t addr, uint64_t size,
                                     uint32_t dev, uint32_t cluster,
                                     uint32_t core, void *val);
CNDBGResult cngdb_backend_write_wram(uint64_t addr, uint64_t size,
                                     uint32_t dev, uint32_t cluster,
                                     uint32_t core, void *val);
CNDBGResult cngdb_backend_write_sram(uint64_t addr, uint64_t size,
                                     uint32_t dev, uint32_t cluster,
                                     uint32_t core, void *val);
CNDBGResult cngdb_backend_write_dram(uint64_t addr, uint64_t size,
                                     uint32_t dev, uint32_t cluster,
                                     uint32_t core, void *val);

/* query if stop */
CNDBGResult cngdb_backend_query_stop(uint32_t *res);

/* sync events */
CNDBGResult cngdb_backend_query_sync_event(CNDBGEvent *event);

/* error handle */
CNDBGResult cngdb_backend_analysis_error(void);

/* binary */
CNDBGResult cngdb_backend_transfer_info(void *info);

/* valid core */
CNDBGResult cngdb_backend_get_valid_core(uint32_t *dev, uint32_t *cluster,
                                         uint32_t *core);

/* get instrutction */
CNDBGResult cngdb_backend_get_inst(void **cninst_content,
				   unsigned long *cninst_size);

/* get arch info */
int cngdb_backend_get_arch_info();

void fill_table(int cb_count, CNDBGBranch* cbs,
                int barrier_count, CNDBGBarrier* barriers);

#ifdef __cplusplus
}
#endif

#endif  // CNGDB_BACKEND_1M_DRV_CNGDB_BACKEND_H_
