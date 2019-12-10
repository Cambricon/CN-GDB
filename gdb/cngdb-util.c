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


#include "cngdb-util.h"
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>

/* debug info switch */
static cngdb_debug_level_t dlevel = CNDBG_LEVEL_FATAL;
static int domain_enable = 0x0;

static int domain_size = 9;
static char* level_name[5] = {"FATAL", "ERROR", "WARNING", "INFO", "DEBUG"};
static char* cngdb_domain_name[10] = {"GENERAL", "API", "BREAKPOINT",
                                     "FAKEDEVICE", "NOTIFICATION", "LINUXNAT",
                                     "PARSER", "THREAD", "PARALLE", "UNKNOWN"};

void
cngdb_set_debug_level (cngdb_debug_level_t level)
{
  dlevel = level;
}

void
cngdb_enable_debug_domain (cngdb_trace_domain_t domain)
{
  domain_enable |= domain;
}

void
cngdb_disable_debug_domain (cngdb_trace_domain_t domain)
{
  domain_enable &= ~domain;
}

void
cngdb_enable_all_domain (void)
{
  domain_enable = ~0;
}

void
cngdb_disable_all_domain (void)
{
  domain_enable = 0;
}

void
cngdb_log (cngdb_trace_domain_t domain, cngdb_debug_level_t level,
           const char *filename, int line, const char *str)
{
  if (level == CNDBG_LEVEL_FATAL)
    {
      printf ("[CNGDB %s] : %s\n", level_name[level], str);
      throw_error (GENERIC_ERROR, " cngdb internal");
    }
  /* check level && domain mask */
  if (level && (!(domain & domain_enable) || dlevel < level))
    return;

/* In release mode, cngdb don't need to print info except fatal */
}

void
cngdb_log_initialize (void)
{
  /* cngdb debug level initialize : by default, print warning and above */
  cngdb_enable_all_domain ();
  // cngdb_disable_debug_domain (CNDBG_DOMAIN_FAKEDEVICE);
  cngdb_disable_debug_domain (CNDBG_DOMAIN_API);
  cngdb_disable_debug_domain (CNDBG_DOMAIN_PARSER);
  cngdb_set_debug_level (config_log_level);
}

/* create dir in .cngdb, warning : only linux */
int
cngdb_create_dir (char* muldir)
{
  mode_t old_mask = umask(00);
  int i, len;
  char str[512];
  strncpy (str, muldir, 512);
  len = strlen (muldir);
  for (i = 0; i < len; i++)
    {
      if (str[i] == '/')
        {
          str[i] = '\0';
          if (access (str, 0) != 0)
            {
              mkdir (str, 0777);
            }
          str[i] = '/';
        }
    }
  if (len > 0 && access (str, 0) != 0)
    {
      mkdir (str, 0777);
    }
  umask(old_mask);
  return 0;
}

/* rm dir in .cngdb, warning : only linux */
int
cngdb_remove_dir (char* dir)
{
  char * cur_dir = ".";
  char * up_dir = "..";
  char dir_name[1000];

  struct dirent *dp;
  struct stat dir_stat;
  DIR * dircontext;

  /* fail if directory not exist */
  if (0 != access (dir, F_OK))
    return 0;

  /* get dir state */
  if (0 > stat (dir, &dir_stat))
    {
      CNGDB_ERROR ("remove dir fail, dir state get error");
      return -1;
    }

  /* remove dir's file and dir recursive */
  if (S_ISREG (dir_stat.st_mode))
    {
      remove (dir);
      return 0;
    }
  else if (S_ISDIR (dir_stat.st_mode))
    {
      dircontext = opendir (dir);
      while ((dp = readdir (dircontext)) != NULL)
        {
          if ((0 == strcmp (cur_dir, dp->d_name)) ||
              (0 == strcmp (up_dir,  dp->d_name)))
            continue;
          sprintf (dir_name, "%s/%s", dir, dp->d_name);
          if (0 != cngdb_remove_dir (dir_name))
            {
              closedir (dircontext);
              return -1;
            }
        }
      closedir (dircontext);

      /* context in dir removed, rm directory */
      rmdir (dir);
      return 0;
    }
  else
    {
      CNGDB_ERROR ("unexcepted obj");
      return -1;
    }
}

DOUBLEST
cngdb_read_half (const gdb_byte *valaddr)
{
  int re = (uint16_t)(valaddr[1]) << 8 | valaddr[0];
  float f = 0.;
  int sign = (re >> 15) ? (-1) : 1;
  int exp = (re >> 10) & 0x1f;
  int eff = re & 0x3ff;
  float half_max = 65504.;
  float half_min = -65504.;
  if (exp == 0x1f && sign == 1)
    {
      return half_max;
    }
  else if (exp == 0x1f && sign == -1)
    {
      return half_min;
    }

  if (exp > 0)
    {
      exp -= 15;
      eff = eff | 0x400;
    }
  else
    {
      exp = -14;
    }

  int sft;
  sft = exp - 10;
  if (sft < 0)
    {
      f = (float) sign * eff / (1 << (-sft));
    }
  else
    {
      f = ((float) sign) * (1 << sft) * eff;
    }
  return (DOUBLEST) f;
}

void
cngdb_write_half  (gdb_byte * ptr, DOUBLEST num)
{
    float x = (float)num;
    const int fs_shift = 31;
    const int fe_shift = 23;
    const int fe_mark = 0xff;
    const int hs_shift = 15;
    const int he_shift = 10;
    int *in1 = (int *) &x;
    int in = *in1;
    int sign = in >> fs_shift;
    int exp = ((in >> fe_shift) & fe_mark) - 127;
    int denorm = 0;
    int eff;
    if (exp >= 16)
      {
        exp = 0xf;
        eff = 0x3ff;
      }
    else if (exp >= -14)
      {
        eff = (in >> 13) & 0x3ff;
      }
    else if (exp >= -24)
      {
        eff = (((in & 0x7fffff) | 0x800000) >> (-exp - 1)) & 0x3ff;
        denorm = 1;
        exp = 0;
      }
    else
      {
        exp = 0;
        denorm = 1;
        eff = (in & 0x7fffffff) ? 1 : 0;
      }
    exp = (denorm == 1) ? exp : (exp + 15);
    uint16_t result = (sign << hs_shift) + (exp << he_shift) + eff;
    ptr[0] = (uint8_t)(result << 8 >> 8);
    ptr[1] = (uint8_t)(result >> 8);
    return;
}

inline long
cngdb_get_thread_id (ptid_t ptid)
{
  if (ptid_get_tid (ptid))
    {
      /* this is a user thread */
      return ptid_get_tid (ptid);
    }
  else if (ptid_get_lwp (ptid))
    {
      /* this is a light weight process */
      return ptid_get_lwp (ptid);
    }
  else
    {
      /* this is a non-thread process */
      return ptid_get_pid (ptid);
    }
}

/* get thread communication directory, return if this floder exist */
int
cngdb_get_current_thread_dir_name (char *dir, struct thread_info *tp)
{
  int gdb_pid = getpid ();
  sprintf (dir, "/tmp/.cngdb/%d/%ld", gdb_pid, cngdb_get_thread_id (tp->ptid));
  if (access (dir, 0))
    {
      return 0;
    }
  else
    {
      return 1;
    }
}
