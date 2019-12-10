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

#ifndef CNGDB_UTIL_H
#define CNGDB_UTIL_H

#include <time.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <signal.h>

#include "defs.h"
#include "arch-utils.h"
#include "command.h"
#include "dummy-frame.h"
#include "dwarf2-frame.h"
#include "doublest.h"
#include "floatformat.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "symfile.h"
#include "symtab.h"
#include "target.h"
#include "dis-asm.h"
#include "source.h"
#include "block.h"
#include "gdb/signals.h"
#include "gdbthread.h"
#include "language.h"
#include "demangle.h"
#include "main.h"
#include "target.h"
#include "valprint.h"
#include "user-regs.h"
#include "linux-tdep.h"
#include "exec.h"
#include "value.h"
#include "exceptions.h"
#include "breakpoint.h"
#include "reggroups.h"
#include "stdbool.h"
#include "utils.h"

#define CNGDB_FATALD(domain, line, ...)                                        \
   {                                                                           \
     char print_ori[1000];                                                     \
     snprintf (print_ori, sizeof(print_ori), line, ##__VA_ARGS__);             \
     cngdb_log (domain, CNDBG_LEVEL_FATAL, __FILE__, __LINE__, print_ori);     \
   }

#define CNGDB_ERRORD(domain, line, ...)                                        \
   {                                                                           \
     char print_ori[1000];                                                     \
     snprintf (print_ori, sizeof(print_ori), line, ##__VA_ARGS__);             \
     cngdb_log (domain, CNDBG_LEVEL_ERROR, __FILE__, __LINE__, print_ori);     \
   }

#define CNGDB_WARNINGD(domain, line, ...)                                      \
   {                                                                           \
     char print_ori[1000];                                                     \
     snprintf (print_ori, sizeof(print_ori), line, ##__VA_ARGS__);             \
     cngdb_log (domain, CNDBG_LEVEL_WARNING, __FILE__, __LINE__, print_ori);   \
   }

#define CNGDB_INFOD(domain, line, ...)                                         \
   {                                                                           \
     char print_ori[1000];                                                     \
     snprintf (print_ori, sizeof(print_ori), line, ##__VA_ARGS__);             \
     cngdb_log (domain, CNDBG_LEVEL_INFO, __FILE__, __LINE__, print_ori);      \
   }

#define CNGDB_DEBUGD(domain, line, ...)                                        \
   {                                                                           \
     char print_ori[1000];                                                     \
     snprintf (print_ori, sizeof(print_ori), line, ##__VA_ARGS__);             \
     cngdb_log (domain, CNDBG_LEVEL_DEBUG, __FILE__, __LINE__, print_ori);     \
   }

#define CNGDB_FATAL(line, ...) CNGDB_FATALD(CNDBG_DOMAIN_UNKNOWN, line, ##__VA_ARGS__)
#define CNGDB_ERROR(line, ...) CNGDB_ERRORD(CNDBG_DOMAIN_UNKNOWN, line, ##__VA_ARGS__)
#define CNGDB_WARNING(line, ...) CNGDB_WARNINGD(CNDBG_DOMAIN_UNKNOWN, line, ##__VA_ARGS__)
#define CNGDB_INFO(line, ...) CNGDB_INFOD(CNDBG_DOMAIN_UNKNOWN, line, ##__VA_ARGS__)
#define CNGDB_DEBUG(line, ...) CNGDB_DEBUGD(CNDBG_DOMAIN_UNKNOWN, line, ##__VA_ARGS__)

typedef enum {
  CNDBG_LEVEL_FATAL   = 0x0, /* output just ori user info + fatal error */
  CNDBG_LEVEL_ERROR   = 0x1, /* output user info + fatal error + ordinary error */
  CNDBG_LEVEL_WARNING = 0x2, /* output anything important than warning */
  CNDBG_LEVEL_INFO    = 0x3, /* any info we need to see */
  CNDBG_LEVEL_DEBUG   = 0x4, /* detail debug info */
} cngdb_debug_level_t;

#ifdef LOG_ERROR
  static cngdb_debug_level_t config_log_level = CNDBG_LEVEL_ERROR;
#elif LOG_WARNING
  static cngdb_debug_level_t config_log_level = CNDBG_LEVEL_WARNING;
#elif LOG_INFO
  static cngdb_debug_level_t config_log_level = CNDBG_LEVEL_INFO;
#elif LOG_DEBUG
  static cngdb_debug_level_t config_log_level = CNDBG_LEVEL_DEBUG;
#else
  static cngdb_debug_level_t config_log_level = CNDBG_LEVEL_FATAL;
#endif

typedef enum {
  CNDBG_DOMAIN_GENERAL      = 0x1,     /* general feild */
  CNDBG_DOMAIN_API          = 0x2,     /* cngdb api feild */
  CNDBG_DOMAIN_BREAKPOINT   = 0x4,     /* breakpoint feild */
  CNDBG_DOMAIN_FAKEDEVICE   = 0x8,     /* fake device feild */
  CNDBG_DOMAIN_NOTIFICATION = 0x10,    /* notification feild */
  CNDBG_DOMAIN_LINUXNAT     = 0x20,    /* linux nat feild */
  CNDBG_DOMAIN_PARSER       = 0x40,    /* parser feild */
  CNDBG_DOMAIN_THREAD       = 0x80,    /* thread feild */
  CNDBG_DOMAIN_PARALLE      = 0x100,   /* paralle feild */
  CNDBG_DOMAIN_UNKNOWN      = 0x100,   /* unknown feild */
} cngdb_trace_domain_t;

void cngdb_set_debug_level (cngdb_debug_level_t level);

void cngdb_enable_debug_domain (cngdb_trace_domain_t domain);

void cngdb_disable_debug_domain (cngdb_trace_domain_t domain);

void cngdb_enable_all_domain (void);

void cngdb_disable_all_domain (void);

void cngdb_log (cngdb_trace_domain_t domain, cngdb_debug_level_t level,
           const char *filename, int line, const char *str);

void cngdb_log_initialize (void);

int cngdb_create_dir (char* dir);

int cngdb_remove_dir (char* dir);

DOUBLEST cngdb_read_half (const gdb_byte *valaddr);

void cngdb_write_half (gdb_byte * ptr, DOUBLEST num);

extern long cngdb_get_thread_id (ptid_t ptid);

int cngdb_get_current_thread_dir_name (char *dir, struct thread_info *tp);

#endif
