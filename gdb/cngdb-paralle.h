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

#ifndef CNGDB_PARALLE_H
#define CNGDB_PARALLE_H

#include "cngdb-state.h"
#include "command.h"

typedef struct {
  char *group_name;
  VEC (cngdb_coords) *group_coords;
} cngdb_core_group_t;

/* Task type */
typedef enum {
  TASK_CORE, /* continue in focus mode */
  TASK_GROUP, /* group task */
} cngdb_task_type_t;

struct cngdb_task_t;

struct cngdb_task_t {
  char *task_command;
  cmd_cfunc_ftype *task_func;
  cngdb_coords task_coord;
  int task_from_tty;
  struct cngdb_task_t *next;
  cngdb_task_type_t task_type;
};

typedef struct cngdb_task_t *cngdb_tasks;
typedef cngdb_core_group_t *cngdb_core_groups;

/* initialize cngdb focus to core 0 */
void cngdb_focus_initialize (void);

/* Return NULL if not focus on device or not focus on single core */
cngdb_coords cngdb_current_focus (void);

/* Switch cngdb focus core to coord */
void cngdb_switch_focus (cngdb_coords coord);

/* Create core group with name and group coords */
void cngdb_create_core_group (char *name, VEC (cngdb_coords) *group_coords);

/* Query group info with name */
void cngdb_info_group (char *name);

/* Invoke 'func' in every core in group  */
void cngdb_command_group (char *name, char *cmd);

/* Switch focus, return switch focus succeed */
int cngdb_auto_switch_focus (void);

/* Group mode flag */
int cngdb_in_group_mode (void);

/* Update cngdb task, for upper state */
void cngdb_update_task_state (void);

/* Cngdb do one task */
void cngdb_do_task (void);

/* Cngdb async event */
void cngdb_async_initialize (void);
void cngdb_async_mark (void);
void cngdb_async_clear (void);

/* set/unset auto switch */
int cngdb_auto_switch (void);
void cngdb_set_auto_switch (void);
void cngdb_unset_auto_switch (void);

#endif
