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

#ifndef CNGDB_STATE_H
#define CNGDB_STATE_H

#include "cndebugger.h"
#include "stdbool.h"

#include "cngdb-util.h"
//#include "cngdb-fake-driver.h"

typedef enum {
  CNDBG_SELECT_ALL = ~0,
  CNDBG_SELECT_BREAKPOINT = 0x1,
  CNDBG_SELECT_FINISHED = 0x2,
  CNDBG_SELECT_ERROR = 0x4,
  CNDBG_SELECT_RUNNING = 0x8,
  CNDBG_SELECT_SYNC = 0x10,
  CNDBG_SELECT_UNKNOWN = 0x20,
  CNDBG_SELECT_INITIALIZED = 0x40,
} cngdb_select_mask_t;

typedef enum {
  CNDBG_COORD_BREAKPOINT = 0x1,
  CNDBG_COORD_FINISHED = 0x2,
  CNDBG_COORD_ERROR = 0x4,
  CNDBG_COORD_RUNNING = 0x8,
  CNDBG_COORD_SYNC = 0x10,
  CNDBG_COORD_UNKNOWN = 0x20,
  CNDBG_COORD_INITIALIZED = 0x40,
} cngdb_coord_state_t;

typedef struct {
  cngdb_coord_state_t state; /* core state */
  uint32_t dev_id; /* device id */
  uint32_t cluster_id; /* cluster id */
  uint32_t core_id; /* core id */
} cngdb_coords_t;

extern int first_run_flag;

typedef cngdb_coords_t *cngdb_coords;
DEF_VEC_P(cngdb_coords);

typedef struct {
  int mask;
  int index;
  cngdb_coords current_coord;
} cngdb_iterator_t;

typedef cngdb_iterator_t *cngdb_iterator;
extern int cngdb_signal_mask;

void cngdb_coords_initialize (void);
void cngdb_coords_destroy (void);

/* signal mask process */
void cngdb_signal_mask_set (void);
void cngdb_signal_mask_unset (void);

/* check state */
bool cngdb_focus_on_device (void);
bool cngdb_search_state (int mask);
int cngdb_get_total_core_count (void);
int cngdb_get_device_count (void);
int cngdb_get_cluster_count (void);
int cngdb_get_core_count (void);

/* modify state */
void cngdb_update_state (CNDBGEvent* events);

/* core iterator */
void cngdb_iterator_create (cngdb_iterator *iter, int mask);
void cngdb_iterator_destroy (cngdb_iterator iter);
void cngdb_iterator_start (cngdb_iterator iter);
bool cngdb_iterator_end (cngdb_iterator iter);
void cngdb_iterator_next (cngdb_iterator iter);
void cngdb_iterator_last (cngdb_iterator iter);

cngdb_coords cngdb_find_coord (uint32_t dev, uint32_t cluster, uint32_t core);

void cngdb_unpack_coord (cngdb_coords coord, uint32_t *dev,
                         uint32_t *cluster, uint32_t *core);

int cngdb_on_start (void);

#endif
