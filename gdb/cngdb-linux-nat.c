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

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <signal.h>
#include <defs.h>
#include <time.h>
#include <objfiles.h>
#include "block.h"
#include "bfd.h"                /* Binary File Description */
#include "gdbthread.h"
#include "language.h"
#include "demangle.h"
#include "regcache.h"
#include "arch-utils.h"
#include "buildsym.h"
#include "valprint.h"
#include "command.h"
#include "gdbcmd.h"
#include "observer.h"
#include "linux-nat.h"
#include "inf-child.h"
#include "cngdb-linux-nat.h"
#include "cngdb-util.h"
#include "infrun.h"
#include "cngdb-tdep.h"
#include "cngdb-notification.h"
#include "cngdb-command.h"
#include "gdbthread.h"
#include "cngdb-thread.h"
#include "cngdb-paralle.h"
#include "symfile.h"
#include "cngdb-asm.h"

void *cncode_content;
CNDBGBranch* branch_pointer;
CNDBGBarrier* barrier_pointer;

/* global variables */
static bool cngdb_debug_enable = 1;

static struct target_ops host_target_ops;

pthread_t multi_thread_handle;
pthread_t task_build_handle;

extern CNDBGTaskType ttype;

int cngdb_auto_load = false;

int cngdb_error_occured = false;

#define MAX_BRANCH 100000000
#define MAX_BARRIER 100000000

unsigned long long cncode_size;

int branch_count = 0;
int barrier_count = 0;
/* weather we are counting branch/barrier */
unsigned long long cncode_size;

/* weather we are counting branch/barrier */
int counting_bb = 0;

/* cngdb_nat_kill(): */
static void
cngdb_nat_kill (struct target_ops *ops)
{
  CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT, "cngdb nat kill");
  cngdb_cleanup ();
  pthread_cancel (multi_thread_handle);
  host_target_ops.to_kill (ops);
}

/* cngdb_nat_mourn_inferior(): destroy inferior */
static void
cngdb_nat_mourn_inferior (struct target_ops *ops)
{
  CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT, "cngdb mourn inferior");
  mark_breakpoints_out ();

  cngdb_api_finalize ();

  cngdb_cleanup ();
  pthread_cancel (multi_thread_handle);
  host_target_ops.to_mourn_inferior (ops);
}

void
cngdb_nat_resume_thread (ptid_t ptid)
{
  target_resume (ptid, false, GDB_SIGNAL_0);
}

/* cngdb_nat_resume(): continue to run */
static void
cngdb_nat_resume (struct target_ops *ops, ptid_t ptid, int sstep,
                  enum gdb_signal ts)
{
  if (cngdb_focus_on_device ())
    {
      if (cngdb_error_occured)
        {
          CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
            "skip resume when error occured");
          return;
        }
      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
        "--------------- gdb resume device, sstep %d, ts %d -----------------", sstep, ts);
      /*  check if cngdb state is uninitialized */
      if (cngdb_api_get_state () != CNDBG_API_STATE_INITIALIZED)
        {
          CNGDB_FATAL ("api uninitialzed, resume fail");
          return;
        }

      /* check if device initialized */
      int core_count = cngdb_get_total_core_count ();
      gdb_assert (core_count);

      /* get current focus core */
      uint32_t dev, cluster, core;
      cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);

      /* resume core */
      cngdb_api_resume (sstep, dev, cluster, core);

      /* cngdb always aync */
      linux_nat_async (ops, 1);
      async_file_flush ();

      /* start waiting thread */
      cngdb_notification_guard_start ();
      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
         "========cngdb notification running!========");
    }
  else
    {
      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
        "--------------- gdb resume host %ld, sstep %d, ts %d -----------------",
        cngdb_get_thread_id (ptid), sstep, ts);
      host_target_ops.to_resume (ops, ptid, sstep, ts);
    }
}

static ptid_t
cngdb_nat_wait (struct target_ops *ops, ptid_t ptid,
                struct target_waitstatus *ws,
                int target_options)
{
  if (cngdb_focus_on_device ())
    {
      /* at first flush async event */
      async_file_flush ();

      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
                   "--------------- gdb wait device -----------------");
      /* check api status */
      if (cngdb_api_get_state () != CNDBG_API_STATE_INITIALIZED)
        {
          CNGDB_FATAL ("api uninitialzed, wait fail");
          return inferior_ptid;
        }

      /* check device initialized */
      int core_count = cngdb_get_total_core_count ();
      gdb_assert (core_count);

      /* start notification thread to watch device state */
      cngdb_notification_guard_stop ();
      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
                 "========cngdb notification stop!========");
      /* alloc event and inspect state */
      CNDBGEvent* events = (CNDBGEvent*) xmalloc
                                 (sizeof (CNDBGEvent) * core_count);
      cngdb_api_get_sync_event (events);
      cngdb_update_state (events);

      if (!cngdb_search_state (CNDBG_SELECT_ERROR | CNDBG_SELECT_UNKNOWN))
        {
          cngdb_error_occured = false;
        }
      else if (cngdb_error_occured)
        {
          ws->value.sig = GDB_SIGNAL_SEGV;
          ws->kind = TARGET_WAITKIND_SIGNALLED;
          cngdb_nat_kill (ops);
          return inferior_ptid;
        }

      if (!cngdb_focus_on_device ())
        {
          /* kernel excution finish */
          ws->value.sig = GDB_SIGNAL_0;
          /* if we are in sim, use ignore, else use stopped */
          ws->kind = TARGET_WAITKIND_STOPPED;
          cngdb_api_task_destroy ();

          /* by default, cngdb debug only one device thread */
          cngdb_thread_mark (
              cngdb_find_thread_by_status (CNDBG_THREAD_DEBUGGING),
              CNDBG_THREAD_FINISHED);
        }
      else
        {
          if (cngdb_search_state (CNDBG_SELECT_RUNNING))
            {
              /* kernel not stopped */
              cngdb_notification_guard_start ();
              CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
                 "========cngdb notification running!========");
              ws->value.sig = GDB_SIGNAL_0;
              ws->kind = TARGET_WAITKIND_IGNORE;
              ptid_t bp_back = {-1, 0, 0};
              xfree (events);
              return bp_back;
            }
          else if (cngdb_search_state (CNDBG_SELECT_ERROR |
                                       CNDBG_SELECT_UNKNOWN))
            {
              /* kernel stopped, but has some error */
              ws->value.sig = GDB_SIGNAL_SEGV;
              ws->kind = TARGET_WAITKIND_STOPPED;
              cngdb_error_occured = true;
            }
          else
            {
              /* kernel stopped by breakpoint */
              ws->value.sig = GDB_SIGNAL_TRAP;
              ws->kind = TARGET_WAITKIND_STOPPED;
              target_async (0);
              CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT, "waiting finished");
            }
        }
      xfree (events);
      /* cngdb should return inferior's ptid */
      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT, "cngdb wait device finished ...");
      return inferior_ptid;
    }
  else
    {
      /* close device error */
      cngdb_error_occured = false;
      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
                   "--------------- gdb wait host -----------------");
      ptid_t back_ptid = host_target_ops.to_wait (ops, ptid,
                                                  ws, target_options);
      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT, "host wait status : %d, %d",
                   ws->kind, ws->value.sig);

      if (ws->kind == TARGET_WAITKIND_STOPPED &&
          ws->value.sig == GDB_SIGNAL_TRAP)
        {
          struct cngdb_thread_chain * thread =
             cngdb_find_thread_by_ptid (back_ptid);
          if (thread != NULL)
            {
              CNGDB_INFO ("thread chain found");
              if (thread->thread_status == CNDBG_THREAD_WAITING)
                {
                  CNGDB_INFO ("thread chain found waiting");
                  if (!cngdb_find_thread_by_status (CNDBG_THREAD_DEBUGGING |
                                                    CNDBG_THREAD_PENDING))
                    {
                      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT, "mark %ld as pending",
                                   cngdb_get_thread_id (back_ptid));
                      cngdb_thread_mark (thread, CNDBG_THREAD_PENDING);
                      /* open thread to build cngdb backend */
                      pthread_create (&task_build_handle, NULL,
                                      cngdb_api_task_build,
                                      (void*)(&(thread->tinfo->ptid.pid)));
                      usleep (500000);
                    }
                  else
                    {
                      ws->kind = TARGET_WAITKIND_IGNORE;
                      ws->value.sig = GDB_SIGNAL_0;
                    }
                }
              else if (thread->thread_status == CNDBG_THREAD_PENDING)
                {
                  CNGDB_INFO ("thread chain found pending");
                  int res = pthread_join(task_build_handle, NULL);
                  if (ttype != CNDBG_TASK_UNINITIALIZE)
                    {
                      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT, "device initialize");
                      /* start device debug */
                      cngdb_coords_initialize ();
                      /* initialize focus core */
                      cngdb_focus_initialize ();
                      cngdb_notification_thread_create ();

                      ws->kind = TARGET_WAITKIND_IGNORE;
                      ws->value.sig = GDB_SIGNAL_0;
                      cngdb_thread_mark (thread, CNDBG_THREAD_DEBUGGING);
                      return back_ptid;
                    }
                }
            }
        }
      return back_ptid;
    }
}

/* cngdb_nat_fetch_registers(): get register */
static void
cngdb_nat_fetch_registers (struct target_ops *ops,
                           struct regcache *regcache,
                           int regno)
{
  if (cngdb_focus_on_device () && regno != -1)
    {
      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
                   "--------------- gdb fetch register -----------------");
      if (cngdb_api_get_state () != CNDBG_API_STATE_INITIALIZED)
        {
          CNGDB_FATAL ("api uninitialzed, fetch register fail");
          return;
        }
      struct gdbarch *gdbarch = get_regcache_arch (regcache);
      /* magic number here, max reg size */
      gdb_byte *buf = xmalloc (8);
      enum register_status status =
              cngdb_pseudo_register_read (gdbarch, regcache, regno, buf);
      gdb_assert (status == REG_VALID);
      regcache_raw_supply (regcache, regno, buf);
      xfree (buf);
    }
  else
    {
      return host_target_ops.to_fetch_registers (ops, regcache, regno);
    }
}

/* cngdb_nat_store_registers(): store register */
static void
cngdb_nat_store_registers (struct target_ops *ops,
                           struct regcache *regcache,
                           int regno)
{
  if (cngdb_focus_on_device () && regno != -1)
    {
      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
                   "--------------- gdb store register -----------------");
      if (cngdb_api_get_state () != CNDBG_API_STATE_INITIALIZED)
        {
          CNGDB_FATAL ("api uninitialzed, store register fail");
          return;
        }

      gdb_byte *buf = (gdb_byte *)xmalloc (8);
      struct gdbarch *gdbarch = get_regcache_arch (regcache);
      regcache_raw_collect (regcache, regno, buf);
      cngdb_pseudo_register_write (gdbarch, regcache, regno, buf);
      xfree (buf);
    }
  else
    {
      return host_target_ops.to_store_registers (ops, regcache, regno);
    }
}

/* cngdb_nat_insert_breakpoint(): insert breakpoint  */
static int
cngdb_nat_insert_breakpoint (struct target_ops *ops,
                             struct gdbarch *gdbarch,
                             struct bp_target_info *bp_tgt)
{
  CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
               "cngdb set breakpoint at 0x%lX", bp_tgt->placed_address);
  if (bp_tgt->cngdb_breakpoint_location)
    {
      if (!cngdb_focus_on_device ())
        return 1;
      if (cngdb_api_get_state () != CNDBG_API_STATE_INITIALIZED)
        {
          CNGDB_ERROR ("api uninitialzed, insert breakpoint fail");
          return 1;
        }
      return cngdb_api_insert_breakpoint (bp_tgt->placed_address) !=
               CNDBG_SUCCESS;
    }
  else
    {
      return host_target_ops.to_insert_breakpoint (ops, gdbarch, bp_tgt);
    }
}

/* cngdb_nat_remove_breakpoint(): remove breakpoint */
static int
cngdb_nat_remove_breakpoint (struct target_ops *ops,
                             struct gdbarch *gdbarch,
                             struct bp_target_info *bp_tgt)
{
  CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
               "cngdb remove breakpoint at %lX", bp_tgt->placed_address);
  if (bp_tgt->cngdb_breakpoint_location)
    {
      if (!cngdb_focus_on_device ())
        return 1;
      if (cngdb_api_get_state () != CNDBG_API_STATE_INITIALIZED)
        {
          CNGDB_ERROR ("api uninitialzed, remove breakpoint fail");
          return 1;
        }
      return cngdb_api_remove_breakpoint (bp_tgt->placed_address);
    }
  else
    {
      return host_target_ops.to_remove_breakpoint (ops, gdbarch, bp_tgt);
    }
}

static enum target_xfer_status
cngdb_nat_xfer_partial (struct target_ops *ops,
                        enum target_object object, const char *annex,
                        gdb_byte *readbuf, const gdb_byte *writebuf,
                        ULONGEST offset, ULONGEST len, ULONGEST *xfered_len)
{
  CNGDB_DEBUGD (CNDBG_DOMAIN_LINUXNAT,
               "xfer partial, obj : %d, offset : %ld, len : %ld",
               object, offset, len);
  if (!cngdb_focus_on_device ())
    return host_target_ops.to_xfer_partial (ops, object, annex, readbuf,
                                            writebuf, offset, len,
                                            xfered_len);

  uint32_t dev, cluster, core;
  cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);
  switch (object)
    {
    case TARGET_OBJECT_MEMORY:
      {
        struct gdbarch *gdbarch = target_gdbarch ();
        enum dwarf_location_atom op;
        uint64_t addr = cngdb_recover_offseted_space (gdbarch, &op, offset);
        switch (op)
          {
          case DW_OP_CAMBRICON_nram:
            if (readbuf != NULL)
              {
                CNDBGResult res = cngdb_api_read_nram (addr, len,
                                                       dev, cluster, core,
                                                       (void *)readbuf);
                if (res == CNDBG_SUCCESS)
                  {
                    *xfered_len = len;
                    return TARGET_XFER_OK;
                  }
                else
                  {
                    return TARGET_XFER_E_IO;
                  }
              }
            else
              {
                CNDBGResult res = cngdb_api_write_nram (addr, len,
                                                        dev, cluster, core,
                                                        (void *)writebuf);
                if (res == CNDBG_SUCCESS)
                  {
                    *xfered_len = len;
                    return TARGET_XFER_OK;
                  }
                else
                  {
                    return TARGET_XFER_E_IO;
                  }
              }
          case DW_OP_CAMBRICON_wram:
            if (readbuf != NULL)
              {
                CNDBGResult res = cngdb_api_read_wram (addr, len,
                                                       dev, cluster, core,
                                                       (void *)readbuf);
                if (res == CNDBG_SUCCESS)
                  {
                    *xfered_len = len;
                    return TARGET_XFER_OK;
                  }
                else
                  {
                    return TARGET_XFER_E_IO;
                  }
              }
            else
              {
                CNDBGResult res = cngdb_api_write_wram (addr, len,
                                                        dev, cluster, core,
                                                        (void *)writebuf);
                if (res == CNDBG_SUCCESS)
                  {
                    *xfered_len = len;
                    return TARGET_XFER_OK;
                  }
                else
                  {
                    return TARGET_XFER_E_IO;
                  }
              }
          case DW_OP_CAMBRICON_sram:
            if (readbuf != NULL)
              {
                CNDBGResult res = cngdb_api_read_sram (addr, len,
                                                       dev, cluster, core,
                                                       (void *)readbuf);
                if (res == CNDBG_SUCCESS)
                  {
                    *xfered_len = len;
                    return TARGET_XFER_OK;
                  }
                else
                  {
                    return TARGET_XFER_E_IO;
                  }
              }
            else
              {
                CNDBGResult res = cngdb_api_write_sram (addr, len,
                                                        dev, cluster, core,
                                                        (void *)writebuf);
                if (res == CNDBG_SUCCESS)
                  {
                    *xfered_len = len;
                    return TARGET_XFER_OK;
                  }
                else
                  {
                    return TARGET_XFER_E_IO;
                  }
              }
          case DW_OP_CAMBRICON_dram:
            if (readbuf != NULL)
              {
                CNDBGResult res = cngdb_api_read_dram (addr, len,
                                                       dev, cluster, core,
                                                       (void *)readbuf);
                if (res == CNDBG_SUCCESS)
                  {
                    *xfered_len = len;
                    return TARGET_XFER_OK;
                  }
                else
                  {
                    return TARGET_XFER_E_IO;
                  }
              }
            else
              {
                CNDBGResult res = cngdb_api_write_dram (addr, len,
                                                        dev, cluster, core,
                                                        (void *)writebuf);
                if (res == CNDBG_SUCCESS)
                  {
                    *xfered_len = len;
                    return TARGET_XFER_OK;
                  }
                else
                  {
                    return TARGET_XFER_E_IO;
                  }
              }
          }
      }
    default:
      CNGDB_ERROR ("bad addr space query when execute on device");
      return TARGET_XFER_UNAVAILABLE;
    }
}

/* cngdb_nat_thread_architecture(): get cngdb arch */
static struct gdbarch *
cngdb_nat_thread_architecture (struct target_ops *ops, ptid_t ptid)
{
  if (cngdb_focus_on_device ())
    return cngdb_get_gdbarch ();
  else
    return target_gdbarch ();
}

/* cngdb_always_non_stop_p(): to_always_non_stop_p implementation.  */
static int
cngdb_always_non_stop_p (struct target_ops *self)
{
  if (cngdb_focus_on_device ())
    return false;
  else
    return host_target_ops.to_always_non_stop_p (self);
}

/* target_is_async_p implementation.  */
static int
cngdb_is_async_p (struct target_ops *ops)
{
  return host_target_ops.to_is_async_p (ops);
}

static int
cngdb_stopped_by_sw_breakpoint (struct target_ops *ops)
{
  if (cngdb_focus_on_device ())
    {
      if (!cngdb_search_state (~CNDBG_SELECT_INITIALIZED))
        {
          return false;
        }
      return !cngdb_search_state (CNDBG_SELECT_ERROR |
                                  CNDBG_SELECT_UNKNOWN);
    }
  else
    return host_target_ops.to_stopped_by_sw_breakpoint (ops);
}

static int
cngdb_stopped_by_hw_breakpoint (struct target_ops *ops)
{
  if (cngdb_focus_on_device ())
    return false;
  else
    return host_target_ops.to_stopped_by_hw_breakpoint (ops);
}

static void
cngdb_create_inferior (struct target_ops *ops,
                       char *exec_file, char *allargs, char **env,
                       int from_tty)
{
  /* thread operator initialize */
  pthread_create (&multi_thread_handle, NULL, cngdb_thread_monitoring, NULL);
  host_target_ops.to_create_inferior (ops, exec_file, allargs, env, from_tty);
}

void
cngdb_nat_add_target (struct target_ops *t)
{
  static char shortname[128];
  static char longname[128];
  static char doc[256];

  if (!cngdb_debug_enable) return;

  /* Save the original set of target operations */
  host_target_ops = *t;

  /* Build the meta-data strings without using malloc */
  strncpy (shortname, t->to_shortname, sizeof (shortname) - 1);
  strncat (shortname, " + mlu", sizeof (shortname) - 1 - strlen (shortname));
  strncpy (longname, t->to_longname, sizeof (longname) - 1);
  strncat (longname, " + mlu support",
           sizeof (longname) - 1 - strlen (longname));
  strncpy (doc, t->to_doc, sizeof (doc) - 1);
  strncat (doc, " with mlu support", sizeof (doc) - 1 - strlen (doc));

  /* Override what we need to */
  t->to_shortname                = shortname;
  t->to_longname                 = longname;
  t->to_doc                      = doc;
  t->to_kill                     = cngdb_nat_kill;
  t->to_mourn_inferior           = cngdb_nat_mourn_inferior;
  t->to_resume                   = cngdb_nat_resume;
  t->to_wait                     = cngdb_nat_wait;
  t->to_fetch_registers          = cngdb_nat_fetch_registers;
  t->to_store_registers          = cngdb_nat_store_registers;
  t->to_insert_breakpoint        = cngdb_nat_insert_breakpoint;
  t->to_remove_breakpoint        = cngdb_nat_remove_breakpoint;
  t->to_xfer_partial             = cngdb_nat_xfer_partial;
  t->to_is_async_p               = cngdb_is_async_p;
  t->to_stopped_by_sw_breakpoint = cngdb_stopped_by_sw_breakpoint;
  t->to_stopped_by_hw_breakpoint = cngdb_stopped_by_hw_breakpoint;
  /* So far, we don't need thread archi && attach support */
  t->to_thread_architecture      = cngdb_nat_thread_architecture;
  t->to_always_non_stop_p        = cngdb_always_non_stop_p;
  // t->to_detach                = cngdb_nat_detach;
  t->to_create_inferior          = cngdb_create_inferior;
}

extern initialize_file_ftype _initialize_cngdb_nat;

void* cngdb_thread_monitoring (void* args);

/* read cngdb specific info */
void
cngdb_read_sections (struct bfd *abfd, struct bfd_section *asect,
                     void *objfile)
{
}

/* get instruction */
void
cngdb_get_inst (void)
{
  /* get all instruction from ddr */
  CNDBGResult res =
    cngdb_api_get_inst (&cncode_content, (unsigned long*) &cncode_size);

  if (res != CNDBG_SUCCESS)
    {
      CNGDB_ERROR ("get instructions from ddr fail");
      return ;
    }
  /* feed cngdb backend with barrier & jump table */
  counting_bb = 1;
  branch_count = 0;
  barrier_count = 0;

  if (branch_pointer != NULL)
    {
      xfree (branch_pointer);
    }
  if (barrier_pointer != NULL)
    {
      xfree (barrier_pointer);
    }
  branch_pointer = (CNDBGBranch *) malloc (
					   sizeof (CNDBGBranch) * (MAX_BRANCH));
  barrier_pointer = (CNDBGBarrier *) malloc (
					     sizeof (CNDBGBarrier) * (MAX_BARRIER));
  /* tmp container for asm output */
  char tmp[1000];
  for (int i = 0; i < cncode_size; i++)
    {
      (branch_pointer + branch_count)->CBpc = i;
      (barrier_pointer + barrier_count)->BApc = i;
      cngdb_asm_inst (i, tmp);
    }
  fill_table (branch_count, branch_pointer, barrier_count, barrier_pointer);
  counting_bb = 0;
}

/* initialized cngdb target*/
void
_initialize_cngdb_nat (void)
{
  /* initialize log system */
  cngdb_log_initialize ();

  char gdb_pid_dir_path[100];
  sprintf (gdb_pid_dir_path, "%s", "/tmp/.cngdb/");
  CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
               "gdb ipc path is : %s\n", gdb_pid_dir_path);
  if (0 == cngdb_create_dir (gdb_pid_dir_path))
    {
      CNGDB_INFOD (CNDBG_DOMAIN_LINUXNAT,
                   "cngdb create ipc path successfully!");
    }
  else
    {
      CNGDB_FATAL ("cngdb create ipc path fail");
    }

  /* cngdb api version initialize */
  cngdb_api_version_t version;
  if (cngdb_hw == CNDBG_HW_1M_SIM || cngdb_hw == CNDBG_HW_1M_SIMM)
    {
      version = CNDBG_SIMULATOR_1M;
    }
  else if (cngdb_hw == CNDBG_HW_C20)
    {
      version = CNDBG_C20;
    }
  else
    {
      CNGDB_FATAL ("bad hardware input!");
    }

  cngdb_api_initialize (version);
  cngdb_notification_initialize ();
  cngdb_command_initialize ();

  cngdb_debug_enable = 1;
}
