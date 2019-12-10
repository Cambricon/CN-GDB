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

#include "cngdb-frame.h"
#include "cngdb-tdep.h"
#include "arch-utils.h"
#include "dummy-frame.h"
#include "frame.h"
#include "dwarf2-frame.h"
#include "frame-base.h"
#include "regcache.h"
#include "user-regs.h"
#include "value.h"
#include "block.h"
#include "cngdb-api.h"
#include "cngdb-util.h"
#include "gdbtypes.h"
#include "cngdb-state.h"
#include "cngdb-paralle.h"
#include "stack.h"


// forward declare
static CORE_ADDR
cngdb_abi_frame_prev_pc (struct frame_info *this_frame);

const struct frame_unwind cngdb_frame_unwind =
{
  NORMAL_FRAME,
  cngdb_frame_unwind_stop_reason,
  cngdb_frame_this_id,
  cngdb_frame_prev_register,
  NULL,
  cngdb_frame_sniffer,
  NULL,
  NULL,
};

const struct frame_base cngdb_frame_base =
{
  &cngdb_frame_unwind,
  cngdb_frame_base_address,
  cngdb_frame_base_address,
  cngdb_frame_base_address
};

enum unwind_stop_reason
cngdb_frame_unwind_stop_reason
  (struct frame_info *this_frame, void **this_prologue_cache)
{
  if (cngdb_frame_outmost (this_frame))
    {
      return UNWIND_OUTERMOST;
    }
  else
    {
      return 0;
    }
}

static int
cngdb_abi_prologue_analysis (void)
{
  /* first, get current pc */
  CORE_ADDR pc;
  uint32_t dev, cluster, core;
  cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);
  cngdb_api_read_pc (dev, cluster, core, &pc);

  /* second, find pc's function */
  CORE_ADDR pc_start = get_pc_function_start (pc);

  /* return if this is prologue start */
  if (pc >= pc_start && pc - pc_start < 2)
    return pc - pc_start + 1;
  else
    return 0;
}

/* get frame base via abi info */
static CORE_ADDR
cngdb_abi_get_frame_base (struct frame_info *this_frame)
{
  struct gdbarch *gdbarch = cngdb_get_gdbarch ();
  int fp_regnum = cngdb_get_fp_regnum (gdbarch);
  int sp_regnum = cngdb_get_sp_regnum (gdbarch);
  uint32_t dev, cluster, core;
  cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);
  gdb_byte buf[8];

  memset (buf, 0, sizeof (buf));
  int prologue = cngdb_abi_prologue_analysis ();
  if (frame_relative_level (this_frame) <= 0)
    {
      CORE_ADDR frame_base;
      if (prologue != 0)
        {
          cngdb_api_read_greg (sp_regnum, dev, cluster, core, &frame_base);
        }
      else
        {
          cngdb_api_read_greg (fp_regnum, dev, cluster, core, &frame_base);
        }
      return frame_base;
    }
  else
    {
      if (frame_relative_level (this_frame) == 1 && prologue == 1)
        {
          CORE_ADDR frame_base;
          cngdb_api_read_greg (fp_regnum, dev, cluster, core, &frame_base);
          return frame_base;
        }
      else
        {
          frame_unwind_register (get_next_frame(this_frame), fp_regnum, buf);
          return extract_unsigned_integer (buf, sizeof buf, BFD_ENDIAN_LITTLE);
        }
    }
}

static CORE_ADDR
cngdb_get_frame_base (struct frame_info *this_frame)
{
  return cngdb_abi_get_frame_base (this_frame);
}

/* build frame id */
void
cngdb_frame_this_id (struct frame_info *this_frame,
                     void **this_prologue_cache,
                     struct frame_id *this_id)
{
  int this_level = frame_relative_level (this_frame);

  *this_prologue_cache = 0;

  CORE_ADDR base = cngdb_get_frame_base (this_frame);

  CORE_ADDR pc = get_frame_func (this_frame);

  *this_id = frame_id_build (base, pc);

  if (frame_debug)
    {
      fprintf_unfiltered (gdb_stdlog, "{ cngdb_frame_this_id "
                          "(frame=%d)", this_level);
      fprintf_unfiltered (gdb_stdlog, " -> this_id=");
      fprint_frame_id (gdb_stdlog, *this_id);
      fprintf_unfiltered (gdb_stdlog, " }\n");
    }
}

/* fetch prev frame's pc from this_frame */
static CORE_ADDR
cngdb_abi_frame_prev_pc (struct frame_info *this_frame)
{
  struct gdbarch *gdbarch = cngdb_get_gdbarch ();
  int pc_regnum = cngdb_get_pc_regnum (gdbarch);
  int inst_base_regnum = cngdb_get_inst_base_regnum (gdbarch);

  /* inst base shouldn't change, just read */
  uint64_t addr_base;
  uint32_t dev, cluster, core;
  cngdb_unpack_coord (cngdb_current_focus(), &dev, &cluster, &core);
  cngdb_api_read_greg (inst_base_regnum, dev, cluster, core, &addr_base);

  CORE_ADDR fp = cngdb_get_frame_base (this_frame);
  struct value * val = frame_unwind_got_memory (this_frame, pc_regnum, fp);
  CORE_ADDR prev_addr =
    extract_unsigned_integer (value_contents (val),
                              type_length_units (value_type (val)),
                              BFD_ENDIAN_LITTLE);
  CORE_ADDR pc = prev_addr - addr_base;
  CNGDB_INFO ("cngdb get prev addr : %lu", prev_addr);
  CNGDB_INFO ("cngdb get addr base : %lu", addr_base);
  CNGDB_INFO ("cngdb get pc : %lu", pc);
  return pc;
}

static CORE_ADDR
cngdb_frame_prev_pc (struct frame_info *this_frame)
{
  return cngdb_abi_frame_prev_pc (this_frame);
}


/* get previous register from this frame */
static struct value *
cngdb_dwarf_frame_prev_register (struct frame_info *this_frame,
                               void **this_prologue_cache,
                               int regnum)
{
  struct gdbarch *gdbarch = get_frame_arch (this_frame);
  uint32_t pc_regnum = gdbarch_pc_regnum (gdbarch);
  CORE_ADDR pc = 0;
  struct value *value = NULL;
  struct frame_info *frame = NULL;
  void* dwarfcache = NULL;
  enum frame_type frame_type = SENTINEL_FRAME;
  bool read_register_from_device = true;

  /* use dwrf2 frame unwind to unwind variable */
  int dwarf2 = dwarf2_frame_unwind.sniffer (&dwarf2_frame_unwind,
      this_frame,
      this_prologue_cache);

  if (dwarf2)
    {
      value = dwarf2_frame_unwind.prev_register (this_frame,
          this_prologue_cache,
          regnum);

      CORE_ADDR temp = extract_unsigned_integer(value_contents (value),
          8, gdbarch_byte_order (gdbarch));

      char ra_column = cngdb_get_ra_dwarf_column (gdbarch);
      if (regnum == ra_column)
        {
          uint64_t addr_base;
          uint32_t dev, cluster, core;
          int inst_base_regnum = cngdb_get_inst_base_regnum (gdbarch);
          cngdb_unpack_coord (cngdb_current_focus(), &dev, &cluster, &core);
          cngdb_api_read_greg (inst_base_regnum, dev, cluster, core, &addr_base);

          CORE_ADDR pc = extract_unsigned_integer(value_contents (value),
              8, gdbarch_byte_order (gdbarch));

          pc = pc - addr_base;

          value = value_zero (register_type (gdbarch, regnum), not_lval);
          pack_long (value_contents_writeable (value),
              register_type (gdbarch, regnum), pc);
        }
    }

  if (!value)
    {
      value = frame_unwind_got_register (this_frame, regnum, regnum);
    }

  return value;
}


struct value *
cngdb_frame_prev_register (struct frame_info *this_frame,
                           void **this_prologue_cache,
                           int regnum)
{
  return cngdb_dwarf_frame_prev_register (this_frame, this_prologue_cache,
                                        regnum);
}

int
cngdb_frame_sniffer (const struct frame_unwind *self,
                     struct frame_info *this_frame,
                     void **this_prologue_cache)
{
  return true;
}

CORE_ADDR
cngdb_frame_base_address (struct frame_info *this_frame,
                          void **this_base_cache)
{
  return cngdb_get_frame_base (this_frame);
}

const struct frame_base *
cngdb_frame_base_sniffer (struct frame_info *this_frame)
{
  return NULL;
}

/* weather is frame outmost frame */
static int
cngdb_abi_frame_outmost (struct frame_info *this_frame)
{
  struct gdbarch *gdbarch = get_frame_arch (this_frame);

  /** Provide an invalid cache */
  void* temp_cache = NULL;

  char ra_column = cngdb_get_ra_dwarf_column (gdbarch);
  struct value * val = cngdb_frame_prev_register (this_frame,
      &temp_cache, ra_column);

  CNGDB_INFO ("-----------prev pc : %lu---------",
              ((uint64_t *)value_contents(val))[0]);
  CORE_ADDR prev_addr =
    extract_unsigned_integer (value_contents (val),
                              type_length_units (value_type (val)),
                              BFD_ENDIAN_LITTLE);

  uint64_t pc = prev_addr;

  #define MAX_INST 1000000000
  return pc >= MAX_INST;
  #undef MAX_INST
}

int
cngdb_frame_outmost (struct frame_info *this_frame)
{
  return cngdb_abi_frame_outmost (this_frame);
}

int
cngdb_frame_can_print (void)
{
  struct gdbarch *gdbarch = cngdb_get_gdbarch ();
  int fp_regnum = cngdb_get_fp_regnum (gdbarch);
  int sp_regnum = cngdb_get_sp_regnum (gdbarch);
  uint32_t dev, cluster, core;
  CORE_ADDR frame_base, stack_pointer;
  cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);
  cngdb_api_read_greg (fp_regnum, dev, cluster, core, &frame_base);
  cngdb_api_read_greg (sp_regnum, dev, cluster, core, &stack_pointer);
  return (cngdb_offseted_nram (gdbarch, frame_base) &&
          cngdb_offseted_nram (gdbarch, stack_pointer)) ||
         (cngdb_offseted_sram (gdbarch, stack_pointer));
}
