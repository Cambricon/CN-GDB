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

#include <ctype.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/syscall.h>

#include "defs.h"
#include "cngdb-tdep.h"
#include "cngdb-util.h"
#include "gdb_assert.h"
#include "gdbthread.h"
#include "inferior.h"
#include "stdint.h"
#include "stdbool.h"
#include "cngdb-state.h"

#include "cngdb-notification.h"
#include "cngdb-util.h"
#include "cngdb-api.h"
#include "linux-nat.h"

pthread_t ThreadHandle;

/* cngdb_notification_info shows info sent state */
static struct {
  bool initialized;       /* True if info initialized */
  uint32_t pid;           /* The thread id which signal was sent to. */
  bool guarding;          /* True if we are guarding deive state */
  bool running;           /* The guard thread is running */
  pthread_mutex_t mutex;  /* Mutex for the cngdb_notification_* functions */
} cngdb_notification_info;

void
cngdb_notification_initialize (void)
{
  memset (&cngdb_notification_info, 0, sizeof cngdb_notification_info);
  pthread_mutex_init (&cngdb_notification_info.mutex, NULL);
  cngdb_notification_info.initialized = true;
}

/* lock notification thrad mutex, for thread safety*/
static void
cngdb_notification_lock (void)
{
  gdb_assert (cngdb_notification_info.initialized);
  pthread_mutex_lock (&cngdb_notification_info.mutex);
}

/* unlock notification thrad mutex, for thread safety*/
static void
cngdb_notification_unlock (void)
{
  gdb_assert (cngdb_notification_info.initialized);
  pthread_mutex_unlock (&cngdb_notification_info.mutex);
}

/* notify main thread by mark linux async file mark */
static void
cngdb_notification_notify (void)
{
  async_file_mark ();
  return;
}

/* notification callback function, check device state and notify gdb */
static void *
cngdb_notification_callback (void *arg)
{
  gdb_assert (cngdb_notification_info.initialized);
  while (cngdb_notification_info.running)
    {
      cngdb_notification_lock ();

      if (cngdb_notification_info.guarding)
        {
          /* check device state */
          uint32_t res = 0;
          cngdb_api_query_stop (&res);
          if (res)
            {
              /* if all core stopped, notify main thread */
              cngdb_notification_notify ();
              cngdb_notification_info.guarding = false;
            }
        }
      cngdb_notification_unlock ();
    }
  return NULL;
}

void
cngdb_notification_thread_create (void)
{
  cngdb_notification_info.running = true;
  cngdb_notification_info.guarding = false;
  cngdb_notification_info.pid = getpid ();
  pthread_create (&ThreadHandle, NULL, cngdb_notification_callback, NULL);
}

void
cngdb_notification_thread_destroy (void)
{
  cngdb_notification_info.running = false;
  cngdb_notification_info.pid = false;
  pthread_join (ThreadHandle, NULL);
}

void
cngdb_notification_guard_start (void)
{
  cngdb_notification_lock ();
  gdb_assert (cngdb_notification_info.initialized);
  cngdb_notification_info.guarding = true;
  cngdb_notification_unlock ();
}

void
cngdb_notification_guard_stop (void)
{
  cngdb_notification_lock ();
  gdb_assert (cngdb_notification_info.initialized);
  cngdb_notification_info.guarding = false;
  cngdb_notification_unlock ();
}

