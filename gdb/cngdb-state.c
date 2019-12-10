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

#include "defs.h"
#include "breakpoint.h"
#include "inferior.h"
#include "target.h"

#include "cngdb-state.h"
#include "cngdb-tdep.h"
#include "cngdb-util.h"
#include "stdbool.h"
#include "infrun.h"
#include "linux-nat.h"
#include "cngdb-notification.h"
#include "cngdb-paralle.h"
#include "cngdb-api.h"

#define COORD_INITIALIZED (coords != NULL)

#define CORE_IN_CLUSTER 5

CNDBGTaskType ttype = CNDBG_TASK_UNINITIALIZE;
cngdb_coords coords = NULL;

void
cngdb_coords_destroy (void)
{
  if (COORD_INITIALIZED)
    {
      xfree (coords);
    }
  coords = NULL;
}

/* get current coords info */
int
cngdb_get_device_count (void)
{
  return 1;
}

int
cngdb_get_cluster_count (void)
{
  gdb_assert (COORD_INITIALIZED);
  switch (ttype)
    {
    case CNDBG_TASK_BLOCK1:
    case CNDBG_TASK_BLOCK2:
    case CNDBG_TASK_BLOCK3:
    case CNDBG_TASK_BLOCK4:
    case CNDBG_TASK_UNION1:
      return 1;
    case CNDBG_TASK_UNION2:
      return 2;
    case CNDBG_TASK_UNION4:
      return 4;
    default :
      CNGDB_FATAL ("unknown ttype");
      return 0;
    }
}

int
cngdb_get_core_count (void)
{
  gdb_assert (COORD_INITIALIZED);
  switch (ttype)
    {
    case CNDBG_TASK_BLOCK1:
      return 1;
    case CNDBG_TASK_BLOCK2:
      return 2;
    case CNDBG_TASK_BLOCK3:
      return 3;
    case CNDBG_TASK_BLOCK4:
      return 4;
    default :
      return CORE_IN_CLUSTER;
    }
}

int
cngdb_get_total_core_count (void)
{
  gdb_assert (COORD_INITIALIZED);
  return cngdb_get_core_count () * cngdb_get_cluster_count () *
         cngdb_get_device_count ();
}

void
cngdb_coords_initialize (void)
{
  if (coords != NULL)
    {
      CNGDB_ERROR ("initialize coords failed");
      cngdb_coords_destroy ();
    }
  switch (ttype)
    {
    case CNDBG_TASK_BLOCK1:
      coords = (cngdb_coords) xmalloc (sizeof (cngdb_coords_t));
      break;
    case CNDBG_TASK_BLOCK2:
      coords = (cngdb_coords) xmalloc (sizeof (cngdb_coords_t) * 2);
      break;
    case CNDBG_TASK_BLOCK3:
      coords = (cngdb_coords) xmalloc (sizeof (cngdb_coords_t) * 3);
      break;
    case CNDBG_TASK_BLOCK4:
      coords = (cngdb_coords) xmalloc (sizeof (cngdb_coords_t) * 4);
      break;
    case CNDBG_TASK_UNION1:
      coords = (cngdb_coords) xmalloc (sizeof (cngdb_coords_t) *
                                       CORE_IN_CLUSTER);
      break;
    case CNDBG_TASK_UNION2:
      coords = (cngdb_coords) xmalloc (sizeof (cngdb_coords_t) *
                                       CORE_IN_CLUSTER * 2);
      break;
    case CNDBG_TASK_UNION4:
      coords = (cngdb_coords) xmalloc (sizeof (cngdb_coords_t) *
                                       CORE_IN_CLUSTER * 4);
      break;
    default :
      CNGDB_FATAL ("unknown ttype");
    }
  for (int i = 0; i < cngdb_get_total_core_count (); i++)
    {
      (coords + i)->state = CNDBG_COORD_INITIALIZED;
      (coords + i)->dev_id = 0;
      (coords + i)->cluster_id = i / cngdb_get_core_count ();
      (coords + i)->core_id = i % cngdb_get_core_count ();
    }
}


bool
cngdb_focus_on_device (void)
{
  if (!COORD_INITIALIZED)
    return false;
  cngdb_iterator iter;
  cngdb_iterator_create (&iter, CNDBG_SELECT_ERROR |
                                CNDBG_SELECT_BREAKPOINT |
                                CNDBG_SELECT_INITIALIZED |
                                CNDBG_SELECT_RUNNING |
                                CNDBG_SELECT_SYNC);
  cngdb_iterator_start (iter);
  bool on_device = !cngdb_iterator_end (iter);
  cngdb_iterator_destroy (iter);
  return on_device;
}

bool
cngdb_search_state (int mask)
{
  if (!COORD_INITIALIZED)
    return false;
  cngdb_iterator iter;
  cngdb_iterator_create (&iter, mask);
  cngdb_iterator_start (iter);
  bool state_found = !cngdb_iterator_end (iter);
  cngdb_iterator_destroy (iter);
  return state_found;
}

void
cngdb_update_state (CNDBGEvent* events)
{
  gdb_assert (COORD_INITIALIZED);
  cngdb_iterator iter;
  cngdb_iterator_create (&iter, CNDBG_SELECT_ALL);
  for (cngdb_iterator_start (iter);
       !cngdb_iterator_end (iter);
       cngdb_iterator_next (iter))
    {
      iter->current_coord->state = *(events++);
    }
  cngdb_iterator_destroy (iter);
}

void
cngdb_iterator_create (cngdb_iterator *iter, int mask)
{
  gdb_assert (COORD_INITIALIZED);
  *iter = (cngdb_iterator) xmalloc (sizeof (cngdb_iterator_t));
  (*iter)->mask = mask;
  (*iter)->current_coord = NULL;
  (*iter)->index = -1;
}

void
cngdb_iterator_destroy (cngdb_iterator iter)
{
  gdb_assert (COORD_INITIALIZED);
  xfree(iter);
}

void
cngdb_iterator_start (cngdb_iterator iter)
{
  gdb_assert (COORD_INITIALIZED);
  if (iter == NULL)
    CNGDB_ERROR ("bad iterator to call start");
  iter->index = cngdb_get_total_core_count ();
  iter->current_coord = NULL;
  for (int i = 0; i < cngdb_get_total_core_count (); i++)
    {
      if (iter->mask & coords[i].state)
        {
          iter->index = i;
          iter->current_coord = coords + i;
          return;
        }
    }
}

bool
cngdb_iterator_end (cngdb_iterator iter)
{
  gdb_assert (COORD_INITIALIZED);
  if (iter == NULL)
    CNGDB_ERROR ("bad iterator to call end");
  return iter->index >= cngdb_get_total_core_count ();
}

void
cngdb_iterator_next (cngdb_iterator iter)
{
  gdb_assert (COORD_INITIALIZED);
  if (iter == NULL)
    CNGDB_ERROR ("bad iterator to call next");
  if (cngdb_iterator_end (iter))
    {
      CNGDB_WARNING ("already hit end of iterator");
      return;
    }
  for (int idx = iter->index + 1; idx < cngdb_get_total_core_count (); idx++)
    {
      if (coords[idx].state & iter->mask)
        {
          iter->index = idx;
          iter->current_coord = coords + idx;
          return;
        }
    }
  iter->index = cngdb_get_total_core_count ();
  iter->current_coord = NULL;
}

void
cngdb_iterator_last (cngdb_iterator iter)
{
  gdb_assert (COORD_INITIALIZED);
  if (iter == NULL)
    CNGDB_ERROR ("bad iterator to call last");
  if (iter->index < 0)
    {
      CNGDB_WARNING ("already be first iterator!");
    }
  for (int idx = iter->index - 1; idx >= 0; idx--)
    {
      if (coords[idx].state & iter->mask)
        {
          iter->index = idx;
          iter->current_coord = coords + idx;
          return;
        }
    }
  iter->index = -1;
  iter->current_coord = NULL;
}

cngdb_coords
cngdb_find_coord (uint32_t dev, uint32_t cluster, uint32_t core)
{
  for (int i = 0; i < cngdb_get_total_core_count (); i++)
    {
      if (coords[i].dev_id == dev && coords[i].cluster_id == cluster &&
          coords[i].core_id == core)
        {
          return coords + i;
        }
    }
  return NULL;
}

void
cngdb_unpack_coord (cngdb_coords coord, uint32_t *dev,
                    uint32_t *cluster, uint32_t *core)
{
  *dev = coord->dev_id;
  *cluster = coord->cluster_id;
  *core = coord->core_id;
}

int
cngdb_on_start (void)
{
  uint32_t dev, cluster, core;
  CORE_ADDR pc;
  cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);
  cngdb_api_read_pc (dev, cluster, core, &pc);
  return (pc == 0);
}
