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

#include <signal.h>
#include "cngdb-api.h"

#include "defs.h"
#include "inferior.h"
#include "gdb_assert.h"
#include "gdbcore.h"
#include "cngdb-state.h"
#include "linux-nat.h"
#include "cngdb-notification.h"
#include "cngdb-util.h"
#include "cngdb-paralle.h"
#include "breakpoint.h"
#include "cngdb-thread.h"
#include <sys/ptrace.h>
#include "cngdb-linux-nat.h"
#include "../lib/cngdb_backend.h"

#define START_CHECK                                       \
  {                                                       \
    CORE_ADDR pc;                                         \
    cngdb_api_read_pc (dev, cluster, core, &pc);          \
    if (pc == 0)                                          \
      {                                                   \
         return CNDBG_ERROR_INTERNAL;                     \
      }                                                   \
  }

extern void *cncode_content;
extern CNDBGBranch* branch_pointer;
extern CNDBGBarrier* barrier_pointer;

extern unsigned long long cncode_size;

extern struct breakpoint *breakpoint_chain;

static bool api_initialized = false;
/* the pointer of cngdb-api functions */
static CNDBGAPI api = NULL;
/* all threads need to get into device execution*/
extern struct cngdb_thread_chain *device_threads;
/* all threads that has just leave device, wait to continue cpu execution */
extern struct cngdb_thread_chain *offdevice_threads;

int first_run_flag = 0;

extern CNDBGTaskType ttype;

static const struct CNDBGAPI_st Drv1MApi = {
  /* init & destory */
  cngdb_backend_initialize,
  cngdb_backend_finalize,

  /* task build/destroy */
  cngdb_backend_task_build,
  cngdb_backend_task_destroy,

  /* breakpoints */
  cngdb_backend_insert_breakpoint,
  cngdb_backend_remove_breakpoint,

  /* execetion control */
  cngdb_backend_resume,

  /* device state read/write */
  cngdb_backend_read_greg,
  cngdb_backend_read_sreg,
  cngdb_backend_read_preg,
  cngdb_backend_read_pc,
  cngdb_backend_read_nram,
  cngdb_backend_read_wram,
  cngdb_backend_read_sram,
  cngdb_backend_read_dram,
  cngdb_backend_write_greg,
  cngdb_backend_write_sreg,
  cngdb_backend_write_preg,
  cngdb_backend_write_pc,
  cngdb_backend_write_nram,
  cngdb_backend_write_wram,
  cngdb_backend_write_sram,
  cngdb_backend_write_dram,

  /* sync events */
  cngdb_backend_query_stop,
  cngdb_backend_query_sync_event,

  /* error handle */
  cngdb_backend_analysis_error,

  /* binary */
  cngdb_backend_transfer_info,

  /* valid core */
  cngdb_backend_get_valid_core,

  /* get inst */
  cngdb_backend_get_inst,

  /* get arch info */
  cngdb_backend_get_arch_info,
};

/* cngdb_api_check_error : check if error and report */
static CNDBGResult
cngdb_api_check_error (unsigned int res, const char *fmt, ...)
{
  if (res == CNDBG_SUCCESS) return CNDBG_SUCCESS;
  CNGDB_ERROR ("Error in cngdb api %s : %s", cndbgGetErrorString(res), fmt);
  return res;
}

void
cngdb_api_initialize (cngdb_api_version_t version)
{
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api initialie");
  if (version == CNDBG_C20)
    {
      CNGDB_WARNING ("API VERSION : 1M_DRV");
      api = &Drv1MApi;
    }
  else
    {
      CNGDB_FATAL ("API VERSION : not implemented");
      return;
    }
  res = api->initialize ();
  if (res != CNDBG_SUCCESS)
    {
      cngdb_api_check_error (res, "api initialze");
      CNGDB_FATAL ("initialize failed");
    }
  api_initialized = true;
}

void
cngdb_api_finalize (void)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api finalize");
  if (!api_initialized)
    {
      CNGDB_WARNINGD (CNDBG_DOMAIN_API, "cngdb finalize while not initialzed");
      return;
    }
  if (cngdb_focus_on_device ())
    {
      cngdb_api_task_destroy ();
      res = api->finalize ();
      if (res != CNDBG_SUCCESS)
        {
          cngdb_api_check_error (res, "api finalize");
          CNGDB_FATAL ("finalize failed");
        }
    }
}

/* get state of api */
cngdb_api_state_t
cngdb_api_get_state (void)
{
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api get state");
  return api_initialized ? CNDBG_API_STATE_INITIALIZED :
                           CNDBG_API_STATE_UNINITIALIZED;
}

CNDBGResult
cngdb_api_insert_breakpoint (uint64_t pc)
{
  if (pc > cncode_size)
    {
      CNGDB_WARNING ("bad pc size");
      return CNDBG_SUCCESS;
    }
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api set breakpoint");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb breakpoint while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }

  res = api->insertBreakpoint (pc);
  if (res != CNDBG_SUCCESS && res != CNDBG_ERROR_BAD_BREAKPOINT)
    {
      cngdb_api_check_error (res, "set breakpoint fail");
      return CNDBG_ERROR_INTERNAL;
    }
  if (res == CNDBG_ERROR_BAD_BREAKPOINT)
    {
      struct breakpoint *b = NULL;
      for (b = breakpoint_chain; b; b = b->next)
        {
          if (b->loc->address == pc)
            {
              disable_breakpoint (b);
            }
        }
      error (_("Breakpoint at %lu insert failed,"
            " disabled.\n"), pc);
    }
  return CNDBG_SUCCESS;
}

/* remove breakpoint on hardware, param "pc" is logical offset */
CNDBGResult
cngdb_api_remove_breakpoint (uint64_t pc)
{
  if (pc > cncode_size)
    {
      CNGDB_WARNING ("bad pc size");
      return CNDBG_SUCCESS;
    }
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api remove breakpoint");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API,
                    "cngdb remove breakpoint while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  res = api->removeBreakpoint (pc);
  if (res != CNDBG_SUCCESS)
    {
      CNGDB_ERROR ("remove breakpoint failed");
      cngdb_api_check_error (res, "remove breakpoint");
      return CNDBG_ERROR_INTERNAL;
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_resume (int step, uint32_t dev, uint32_t cluster, uint32_t core)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api resume device");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb resume while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  res = api->resume (step, dev, cluster, core);
  if (res != CNDBG_SUCCESS)
    {
      cngdb_api_check_error (res, "resume device");
      CNGDB_FATAL ("resume failed");
    }
  return CNDBG_SUCCESS;
}

/* device state read */
CNDBGResult
cngdb_api_read_greg (uint64_t reg_id, uint32_t dev, uint32_t cluster,
                     uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api read greg %lu", reg_id);
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb read greg while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->readGReg (reg_id, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      cngdb_api_check_error (res, "read greg");
      CNGDB_FATAL ("read greg failed");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_read_sreg (uint64_t reg_id, uint32_t dev, uint32_t cluster,
                     uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api read sreg");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb read sreg while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->readSReg (reg_id, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      cngdb_api_check_error (res, "read sreg");
      CNGDB_FATAL ("read sreg failed");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_read_preg (uint64_t reg_id, uint32_t dev, uint32_t cluster,
                     uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api read preg");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb read preg while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->readPReg (reg_id, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      cngdb_api_check_error (res, "read preg");
      CNGDB_FATAL ("read preg failed");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_read_pc (uint32_t dev, uint32_t cluster, uint32_t core, uint64_t *pc)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api read pc");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb read pc while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  /* check device status first */
  cngdb_coords coord = cngdb_find_coord (dev,  cluster, core);
  if (coord->state == CNDBG_COORD_FINISHED)
    {
      *pc = 0xffffffff;
    }
  else
    {
      res = api->readPc (dev, cluster, core, pc);
      if (res != CNDBG_SUCCESS)
        {
          cngdb_api_check_error (res, "read pc");
          CNGDB_FATAL ("read pc failed");
        }
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_read_nram (uint64_t addr, uint64_t size, uint32_t dev,
                     uint32_t cluster, uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api read nram");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb read nram while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->readNram (addr, size, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      CNGDB_FATAL ("read nram failed");
      cngdb_api_check_error (res, "read nram");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_read_wram (uint64_t addr, uint64_t size, uint32_t dev,
                     uint32_t cluster, uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api read wram");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb read wram while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->readWram (addr, size, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      CNGDB_FATAL ("read wram failed");
      cngdb_api_check_error (res, "read wram");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_read_sram (uint64_t addr, uint64_t size, uint32_t dev,
                     uint32_t cluster, uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api read sram");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb read sram while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->readSram (addr, size, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      CNGDB_FATAL ("read sram failed");
      cngdb_api_check_error (res, "read sram");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_read_dram (uint64_t addr, uint64_t size, uint32_t dev,
                     uint32_t cluster, uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api read dram");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb read dram while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->readDram (addr, size, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      CNGDB_FATAL ("read dram failed");
      cngdb_api_check_error (res, "read dram");
    }
  return CNDBG_SUCCESS;
}

/* device state write */
CNDBGResult
cngdb_api_write_greg (uint64_t reg_id, uint32_t dev, uint32_t cluster,
                     uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api write greg");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb write greg while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->writeGReg (reg_id, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      cngdb_api_check_error (res, "write greg");
      CNGDB_FATAL ("write greg failed");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_write_sreg (uint64_t reg_id, uint32_t dev, uint32_t cluster,
                     uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api write sreg");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb write sreg while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->writeSReg (reg_id, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      cngdb_api_check_error (res, "write sreg");
      CNGDB_FATAL ("write sreg failed");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_write_preg (uint64_t reg_id, uint32_t dev, uint32_t cluster,
                     uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api write preg");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb write preg while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->writePReg (reg_id, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      cngdb_api_check_error (res, "write preg");
      CNGDB_FATAL ("write preg failed");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_write_pc (uint32_t dev, uint32_t cluster, uint32_t core, uint64_t pc)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api write pc");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb write pc while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  res = api->writePc (dev, cluster, core, pc);
  if (res != CNDBG_SUCCESS)
    {
      cngdb_api_check_error (res, "write pc");
      CNGDB_FATAL ("write pc failed");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_write_nram (uint64_t addr, uint64_t size, uint32_t dev,
                      uint32_t cluster, uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api write nram");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb write nram while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->writeNram (addr, size, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      CNGDB_FATAL ("write nram failed");
      cngdb_api_check_error (res, "write nram");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_write_wram (uint64_t addr, uint64_t size, uint32_t dev,
                      uint32_t cluster, uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api write wram");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb write wram while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->writeWram (addr, size, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      CNGDB_FATAL ("write wram failed");
      cngdb_api_check_error (res, "write wram");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_write_sram (uint64_t addr, uint64_t size, uint32_t dev,
                      uint32_t cluster, uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api write sram");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb write sram while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->writeSram (addr, size, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      CNGDB_FATAL ("write sram failed");
      cngdb_api_check_error (res, "write sram");
    }
  return CNDBG_SUCCESS;
}

CNDBGResult
cngdb_api_write_dram (uint64_t addr, uint64_t size, uint32_t dev,
                      uint32_t cluster, uint32_t core, void *val)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api write dram");
  if (!api_initialized)
    {
      CNGDB_ERRORD (CNDBG_DOMAIN_API, "cngdb write dram while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  START_CHECK
  res = api->writeDram (addr, size, dev, cluster, core, val);
  if (res != CNDBG_SUCCESS)
    {
      CNGDB_FATAL ("write dram failed");
      cngdb_api_check_error (res, "write dram");
    }
  return CNDBG_SUCCESS;
}

/* query device sync event */
CNDBGResult
cngdb_api_get_sync_event (CNDBGEvent *event)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult res;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api get sync event");
  if (!api_initialized)
    {
      CNGDB_ERROR ("cngdb get event while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  res = api->queryDevice (event);
  if (res != CNDBG_SUCCESS)
    {
      CNGDB_FATAL ("get event failed");
      cngdb_api_check_error (res, "get sync event");
    }
  return CNDBG_SUCCESS;
}

/* query device stop */
CNDBGResult
cngdb_api_query_stop (uint32_t *res)
{
  if (api == NULL)
    {
      CNGDB_ERROR (" cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  CNDBGResult result;
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api get stop");
  if (!api_initialized)
    {
      CNGDB_ERROR ("cngdb get event while not initialzed");
      return CNDBG_ERROR_UNINITIALIZED;
    }
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  result = api->queryStop (res);
  if (result != CNDBG_SUCCESS)
    {
      CNGDB_FATAL ("get event failed");
      cngdb_api_check_error (result, "get sync event");
    }
  return CNDBG_SUCCESS;
}

/* error checking function */
CNDBGResult
cngdb_api_analysis_error (void)
{
  if (!cngdb_focus_on_device ())
    {
      CNGDB_ERROR ("cngdb not focus on device while executing api");
      return CNDBG_ERROR_BAD_CALL;
    }
  api->analysisError ();
  return CNDBG_SUCCESS;
}

/* transfer debug info that backend need */
CNDBGResult
cngdb_api_trandfer_info (void* info)
{
  api->transferInfo (info);
  return CNDBG_SUCCESS;
}

/* build a device task, also means invoke kernel on device */
void *
cngdb_api_task_build (void * args)
{
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api task build");
  first_run_flag = 1;

  cngdb_mark_all_breakpoint_not_inserted ();

  if (args)
    ttype = api->taskBuild (*((int*)args));
  else
    ttype = api->taskBuild (0);

  if (ttype == CNDBG_TASK_ERROR)
    {
      CNGDB_FATAL ("task start failed");
    }
  async_file_mark ();

  /* get inst from ddr */
  cngdb_get_inst ();

  return NULL;
}

/* destroy task while device kernel finish */
void
cngdb_api_task_destroy (void)
{
  CNGDB_INFOD (CNDBG_DOMAIN_API, "cngdb api task destroy");
  cngdb_notification_thread_destroy ();
  cngdb_coords_destroy ();

  api->taskDestroy ();

  /* do something more */
  async_file_mark ();

  /* free space that malloc in cngdb_get_inst */
  if (cncode_content != NULL)
    {
      cncode_content = NULL;
    }
  if (branch_pointer != NULL)
    {
      xfree (branch_pointer);
      branch_pointer = NULL;
    }
  if (barrier_pointer != NULL)
    {
      xfree (barrier_pointer);
      barrier_pointer = NULL;
    }
}


/* fetch valid core from backend */
CNDBGResult
cngdb_api_get_valid_core (uint32_t *dev, uint32_t *cluster, uint32_t *core)
{
  CNDBGResult res = api->getValidCore (dev, cluster, core);
  if (res != CNDBG_SUCCESS &&
      res != CNDBG_ERROR_FINISHED &&
      res != CNDBG_ERROR_NO_VALID)
    {
      CNGDB_ERROR ("get valid core failed");
      cngdb_api_check_error (res, "get valid core");
    }
  return res;
}

CNDBGResult
cngdb_api_get_inst (void **cninst_content, unsigned long *cninst_size)
{
  if (api == NULL)
    {
      CNGDB_ERROR ("cngdb api uninitialized!");
      return CNDBG_ERROR_UNINITIALIZED;
    }

  CNDBGResult res = api->getInst (cninst_content, cninst_size);

  if (res == CNDBG_SUCCESS &&
      (*cninst_content == NULL || *cninst_size <= 0))
    {
      CNGDB_FATAL ("cngdb error occur, init failed");
      return CNDBG_ERROR_COMMUNICATION_FAILURE;
    }
  else if (res == CNDBG_ERROR_COMMUNICATION_FAILURE)
    {
      CNGDB_FATAL ("error occurs in mlu device, concat fail");
      return CNDBG_ERROR_COMMUNICATION_FAILURE;
    }
  return CNDBG_SUCCESS;
}

int cngdb_api_get_arch_info() {
  return api->getArchInfo();
}
