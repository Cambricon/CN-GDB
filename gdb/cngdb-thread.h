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

#ifndef CNGDB_THREAD_H
#define CNGDB_THREAD_H

enum sdata_type {
  STYPE_ERROR = 0,
  STYPE_KERNEL_ID,
  STYPE_DEBUG_FLAG,
  STYPE_LAUNCH_INFO,
  STYPE_TID,
};

enum cngdb_thread_status {
  CNDBG_THREAD_WAITING = 0x1,
  CNDBG_THREAD_PENDING = 0x2,
  CNDBG_THREAD_DEBUGGING = 0x4,
  CNDBG_THREAD_FINISHED = 0x8,
};

struct data_transfer {
  enum sdata_type type;
  unsigned long long data[5];
};

struct cngdb_thread_chain;

struct cngdb_thread_chain {
  struct thread_info *tinfo;
  enum cngdb_thread_status thread_status;
  struct cngdb_thread_chain *next;
};

void cngdb_thread_lock (void);
void cngdb_thread_unlock (void);

struct cngdb_thread_chain * cngdb_find_thread_by_ptid (ptid_t ptid);

struct cngdb_thread_chain * cngdb_find_thread_by_status (int status);

void cngdb_remove_offdevice_thread (ptid_t ptid);

void cngdb_thread_mark (struct cngdb_thread_chain * thread,
                        enum cngdb_thread_status status);

void * cngdb_thread_monitoring (void* args);

void cngdb_clean_thread (void);

#endif
