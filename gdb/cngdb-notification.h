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

#ifndef CNGDB_NOTIFICATIONS_H
#define CNGDB_NOTIFICATIONS_H

#include "cndebugger.h"

void cngdb_notification_initialize (void);

/* create/destroy cngdb notification thread */
void cngdb_notification_thread_create (void);
void cngdb_notification_thread_destroy (void);

/* start/stop guarding cngdb running state */
void cngdb_notification_guard_start (void);
void cngdb_notification_guard_stop (void);

#endif
