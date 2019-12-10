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

#include "cngdb-paralle.h"
#include "cngdb-util.h"
#include "cngdb-state.h"
#include "linux-nat.h"
#include "cngdb-api.h"
#include "cngdb-linux-nat.h"
#include "inferior.h"
#include "cngdb-thread.h"


char* group_task_cmd = NULL;

/* cngdb focusing core */
cngdb_coords fcore = NULL;

/* all cngdb coords */
extern cngdb_coords coords;

/* tasks to be done */
cngdb_tasks tasks;

/* cngdb core groups */
cngdb_core_groups core_groups;

int auto_switch = true;

/* all threads need to get into device execution*/
extern struct cngdb_thread_chain *device_threads;

/* initialize focus core to logical core 0 */
void
cngdb_focus_initialize (void)
{
  fcore = coords;
}

cngdb_coords
cngdb_current_focus (void)
{
  return fcore;
}

void
cngdb_switch_focus (cngdb_coords coord)
{
  CNGDB_INFOD (CNDBG_DOMAIN_PARALLE, "cngdb switch focus");
  if (coord == fcore)
    {
      printf_unfiltered (_("[Switch focus core failed:"
                           " same with original.]\n"));
      gdb_flush (gdb_stdout);
      return;
    }
  printf_unfiltered (_("[Switch from logical device %u cluster %u core %u"
                       " to logical device %u cluster %u core %u.]\n"),
                     fcore->dev_id,
                     fcore->cluster_id,
                     fcore->core_id,
                     coord->dev_id,
                     coord->cluster_id,
                     coord->core_id);
  fcore = coord;
  gdb_flush (gdb_stdout);
  /* when switch focus core, the registers_changed() can flush the gdb reg cache */
  registers_changed ();
  /* once switch focus success, return set thread info step on bp on */
  device_threads->tinfo->stepping_over_breakpoint = 1;
  /* update stop pc after stopped */
  cngdb_api_read_pc (fcore->dev_id, fcore->cluster_id, fcore->core_id, &stop_pc);
  /* when switch core, frame cache need to be re-genetate */
  reinit_frame_cache ();
  return;
}

void
cngdb_set_auto_switch (void)
{
  CNGDB_INFOD (CNDBG_DOMAIN_PARALLE, "cngdb set auto switch");
  auto_switch = true;
  return;
}

void
cngdb_unset_auto_switch (void)
{
  CNGDB_INFOD (CNDBG_DOMAIN_PARALLE, "cngdb unset auto switch");
  auto_switch = false;
  return;
}

int
cngdb_auto_switch (void)
{
  return auto_switch;
}

void
cngdb_create_core_group (char *name, VEC (cngdb_coords) *group_coords)
{
  return;
}

void
cngdb_info_group (char *name)
{
  return;
}

void
cngdb_command_group (char *name, char *cmd)
{
  return;
}

int
cngdb_auto_switch_focus (void)
{
  uint32_t dev, cluster, core;
  CNDBGResult res = cngdb_api_get_valid_core (&dev, &cluster, &core);
  if (res == CNDBG_ERROR_FINISHED)
    {
      CNGDB_ERROR ("auto switch focus failed, already finished");
      return false;
    }
  else if (res == CNDBG_ERROR_NO_VALID)
    {
      CNGDB_ERROR("auto switch failed, no valid core");
      return false;
    }
  cngdb_coords coord = cngdb_find_coord (dev, cluster, core);
  cngdb_switch_focus (coord);

  return true;
}

int
cngdb_in_group_mode (void)
{
  return false;
}

void
cngdb_update_task_state (void)
{
  return;
}

void
cngdb_do_task (void)
{
  return;
}

void
cngdb_async_initialize (void)
{
  return;
}

void
cngdb_async_mark (void)
{
  async_file_mark ();
  return;
}

void
cngdb_async_clear (void)
{
  return;
}
