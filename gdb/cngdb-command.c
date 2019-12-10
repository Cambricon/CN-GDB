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

#include "cngdb-command.h"
#include "cngdb-parser.h"
#include "command.h"
#include "gdb_assert.h"
#include "cngdb-state.h"
#include "completer.h"
#include "vec.h"
#include "cngdb-paralle.h"
#include "cngdb-api.h"
#include "cngdb-asm.h"

/* return if breakpoint always on device */
extern int always_device_breakpoint;

extern cngdb_coords coords;

#define COORD_INITIALIZED (coords != NULL)

extern void *cncode_content;
extern unsigned long long cncode_size;

#define MAX_BRANCH 100000000
#define MAX_BARRIER 100000000
extern CNDBGBranch* branch_pointer;
extern CNDBGBarrier* barrier_pointer;
extern int branch_count;
extern int barrier_count;

/* weather we are counting branch/barrier */
extern int counting_bb;

struct cmd_list_element *cngdblist;

static void
info_cngdb_info_command (char *arg)
{
  printf_unfiltered (_("Info command use to watch hardware info.\n"));
}

static void
info_cngdb_focus_command (char *arg)
{
  printf_unfiltered (_( "Focus command use to watch/switch "
                        " focus on specific core.\n"));
}


static void
info_cngdb_auto_switch_command (char *arg)
{
  printf_unfiltered (_("Set/Unset auto switch.\n"));
}

static void
info_cngdb_breakpoint_command (char *arg)
{
  printf_unfiltered (_("Set/Unset breakpoint always on device.\n"));
}

static struct {
  char *name;
  void (*func) (char *);
  char *help;
} cngdb_info_subcommands[] =
{
  { "info",               info_cngdb_info_command,
             "information about hardware" },
  { "focus",              info_cngdb_focus_command,
             "watch/modify focus hardware" },
  { "auto-switch",        info_cngdb_auto_switch_command,
             "set/unset auto switch" },
  { "breakpoint",         info_cngdb_breakpoint_command,
             "set/unset bp always on device" },
  { NULL, NULL, NULL},
};

static void
cngdb_info_command (char *arg, int from_tty)
{
  if (!cngdb_focus_on_device ())
    {
      printf_unfiltered (_("Command failed, not focusing on device.\n"));
      return;
    }
  struct ui_out *uiout = current_uiout;
  struct cleanup *table_chain, *row_chain;
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "cngdb info command : %s", arg);
  /* if null, show all core info */
  VEC (cngdb_coords) * command_coord = NULL;
  if (!arg)
    {
      for (unsigned int i = 0; i < cngdb_get_total_core_count (); i++)
        {
          VEC_safe_push(cngdb_coords, command_coord, coords + i);
        }
    }
  else
    {
      command_coord = cngdb_parse_coord (arg);
      if (command_coord == NULL)
        {
          CNGDB_ERROR ("cngdb command failed.")
          return;
        }

      if (VEC_length (cngdb_coords, command_coord) < 1)
        {
          CNGDB_ERROR ("focus command failed, get %d cores",
                       VEC_length (cngdb_coords, command_coord));
          return;
        }
    }

  int len = VEC_length (cngdb_coords, command_coord);

  struct { uint32_t dev, cluster, core, pc, status, focus; } width;

  /* column headers */
  const char *header_dev                  = "device";
  const char *header_cluster              = "cluster";
  const char *header_core                 = "core";
  const char *header_pc                   = "pc";
  const char *header_status               = "core state";
  const char *header_focus                = "focus";

  width.dev     = strlen (header_dev);
  width.cluster = strlen (header_cluster);
  width.core    = strlen (header_core);
  /* max pc : 10^5 */
  width.pc      = 5;
  /* max status string */
  width.status  = 20;
  width.focus   = strlen (header_focus);

  /* start print cngdb info */
  table_chain = make_cleanup_ui_out_table_begin_end
                   (uiout, 6, len, "InfoCngdbDevicesTable");
  ui_out_table_header (uiout, width.dev,     ui_center, "dev", header_dev);
  ui_out_table_header (uiout, width.cluster, ui_center, "cluster",
                       header_cluster);
  ui_out_table_header (uiout, width.core,    ui_center, "core", header_core);
  ui_out_table_header (uiout, width.pc,      ui_center, "pc", header_pc);
  ui_out_table_header (uiout, width.status,  ui_center, "status",
                       header_status);
  ui_out_table_header (uiout, width.focus,   ui_center, "focus", header_focus);
  ui_out_table_body (uiout);

  cngdb_coords elt = NULL;
  for (int ix = 0; VEC_iterate (cngdb_coords, command_coord, ix, elt); ix++)
    {
      if (elt == NULL)
        {
          CNGDB_ERROR ("cngdb command failed, null coord.");
          continue;
        }

      /* get pc/status/is_focus*/
      uint64_t pc;
      cngdb_api_read_pc (elt->dev_id, elt->cluster_id, elt->core_id, &pc);

      row_chain = make_cleanup_ui_out_tuple_begin_end (uiout,
                                                      "InfoCngdbDevicesRow");
      ui_out_field_int       (uiout, "dev"    , elt->dev_id);
      ui_out_field_int       (uiout, "cluster", elt->cluster_id);
      ui_out_field_int       (uiout, "core"   , elt->core_id);
      ui_out_field_int       (uiout, "pc"     , (int)pc);
      ui_out_field_string    (uiout, "status" ,
                              cndbgGetEventString (elt->state));
      ui_out_field_string    (uiout, "focus"  , elt == cngdb_current_focus() ?
                              "*" : "");
      ui_out_text            (uiout, "\n");
      do_cleanups (row_chain);
    }

  do_cleanups (table_chain);

  gdb_flush (gdb_stdout);

  VEC_free (cngdb_coords, command_coord);
}

static void
cngdb_focus_command (char *arg, int from_tty)
{
  if (!cngdb_focus_on_device ())
    {
      printf_unfiltered (_("Command failed, not focusing on device.\n"));
      return;
    }
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "cngdb focus command : %s", arg);
  if (!arg)
    {
      printf_unfiltered (_("[Focusing on logical "
                           "device %u cluster %u core %u]\n"),
                         cngdb_current_focus ()->dev_id,
                         cngdb_current_focus ()->cluster_id,
                         cngdb_current_focus ()->core_id);
      return;
    }

  VEC (cngdb_coords) * command_coord = cngdb_parse_coord (arg);
  if (command_coord == NULL)
    {
      CNGDB_ERROR ("cngdb command failed.")
      return;
    }

  int len = VEC_length (cngdb_coords, command_coord);
  if (len != 1)
    {
      CNGDB_ERROR ("focus command failed, get %d cores", len);
    }
  else
    {
      cngdb_coords focus_core = VEC_index (cngdb_coords, command_coord, 0);
      if (focus_core == NULL)
        {
          CNGDB_ERROR ("cngdb command failed, null coord.")
        }
      cngdb_switch_focus (focus_core);
    }
  VEC_free (cngdb_coords, command_coord);
}


static void
cngdb_modify_auto_switch (char *arg, int from_tty)
{
  if (!arg)
    error ("use \"cngdb auto-switch ON/OFF\" to control switch.");

  if (!strcmp (arg, "on") || !strcmp (arg, "ON"))
    {
      cngdb_set_auto_switch ();
    }
  else if (!strcmp (arg, "off") || !strcmp (arg, "OFF"))
    {
      cngdb_unset_auto_switch ();
    }
  else
    {
      error ("use \"cngdb auto-switch ON/OFF\" to control switch.");
    }
}

static void
cngdb_modify_breakpoint (char *arg, int from_tty)
{
  if (!arg)
    error ("use \"cngdb breakpoint ON/OFF\" to control if breakpoint\
            always on device.");

  if (!strcmp (arg, "on") || !strcmp (arg, "ON"))
    {
      always_device_breakpoint = 1;
    }
  else if (!strcmp (arg, "off") || !strcmp (arg, "OFF"))
    {
      always_device_breakpoint = 0;
    }
  else
    {
      error ("use \"cngdb breakpoint ON/OFF\" to control if breakpoint\
              always on device.");
    }
}

static void
cngdb_command (char* arg, int from_tty)
{
  if (!arg)
    error ("command missing arguements.");
}

static void
info_cngdb_command (char *arg, int from_tty)
{
  int cnt;
  char *argument;
  void (*command)(char *) = NULL;

  if (!arg)
    error (_("Missing option."));

  for (cnt = 0; cngdb_info_subcommands[cnt].name; cnt++)
    if (strstr(arg, cngdb_info_subcommands[cnt].name) == arg)
      {
        command = cngdb_info_subcommands[cnt].func;
        argument = arg + strlen(cngdb_info_subcommands[cnt].name);
        break;
      }

  if (!command)
    error (_("Unrecognized cngdb info option: '%s'."), arg);

  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "run command : %s", argument);
}

static char cngdb_info_cmd_help_str[1024];

static int
cngdb_info_subcommands_max_name_length (void)
{
  int cnt, rc;
  for (cnt = 0, rc = 0; cngdb_info_subcommands[cnt].name; cnt++)
    rc = max (rc, strlen (cngdb_info_subcommands[cnt].name));
  return rc;
}

/* Prepare help for info cngdb command */
static void
cngdb_build_info_cngdb_help_message (void)
{
  char *ptr = cngdb_info_cmd_help_str;
  int size = sizeof(cngdb_info_cmd_help_str);
  int rc, cnt;

  rc = snprintf (ptr, size,
    _("Print informations about the current cngdb activities.\
       Available options:\n"));
  ptr += rc; size -= rc;
  for (cnt = 0; cngdb_info_subcommands[cnt].name; cnt++)
    {
       rc = snprintf (ptr, size, " %*s : %s\n",
          cngdb_info_subcommands_max_name_length(),
          cngdb_info_subcommands[cnt].name,
          _(cngdb_info_subcommands[cnt].help) );
        if (rc <= 0) break;
        ptr += rc;
        size -= rc;
    }
}

static VEC (char_ptr) *
cngdb_info_command_completer (struct cmd_list_element *self,
                              const char *text, const char *word)
{
  VEC (char_ptr) *return_val = NULL;
  char *name;
  int cnt;
  long offset;
  char *p;

  offset = (long)word - (long)text;

  for (cnt = 0; cngdb_info_subcommands[cnt].name; cnt++)
    {
       name = cngdb_info_subcommands[cnt].name;
       if (offset >= strlen(name)) continue;
       if (strstr(name, text) != name) continue;
       p = xstrdup (name + offset);
       VEC_safe_push (char_ptr, return_val, p);
    }
  return return_val;
}

void
cngdb_command_initialize (void)
{
  struct cmd_list_element *cmd;

  add_prefix_cmd ("cngdb", class_cngdb, cngdb_command,
                  _("Cngdb specific commands"),
                  &cngdblist, "cngdb ", 0, &cmdlist);

  add_cmd ("info", no_class, cngdb_info_command,
           _("Show cngdb hardware info."), &cngdblist);

  add_cmd ("focus", no_class, cngdb_focus_command,
           _("Modify/show cngdb focus state."), &cngdblist);


  cmd = add_cmd ("auto-switch", no_class, cngdb_modify_auto_switch,
                _("Open/Close auto switch func."), &cngdblist);

  cmd = add_cmd ("breakpoint", no_class, cngdb_modify_breakpoint,
                _("Set/Unset breakpoint always on device"), &cngdblist);

  cngdb_build_info_cngdb_help_message ();
  cmd = add_info ("cngdb", info_cngdb_command, cngdb_info_cmd_help_str);
  set_cmd_completer (cmd, cngdb_info_command_completer);
}
