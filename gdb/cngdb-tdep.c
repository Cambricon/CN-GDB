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

#include <time.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <signal.h>
#ifndef __ANDROID__
#include <execinfo.h>
#endif

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

#include "gdb_assert.h"
#include "cngdb-tdep.h"
#include "cngdb-state.h"
#include "cngdb-api.h"
#include "cngdb-asm.h"
#include "cngdb-paralle.h"
#include "cngdb-thread.h"
#include "cngdb-frame.h"

#define MAX_NAME_LENGTH 1024
#define MAX_INST_ASM_LENGTH 10240
#define DRAM_MASK_CODE 0x000000FFFFFFFFFF

/*-------------------------------- Globals ---------------------------------*/
struct gdbarch *cngdb_gdbarch = NULL;

/* For Mac OS X */
bool
cngdb_platform_supports_tid (void)
{
#if defined(__ANDROID__) || (defined(linux) && defined(SYS_gettid))
    return true;
#else
    return false;
#endif
}

int
cngdb_gdb_get_tid (ptid_t ptid)
{
  if (cngdb_platform_supports_tid ())
    return ptid_get_lwp (ptid);
  else
    return ptid_get_tid (ptid);
}

struct gdbarch_tdep
{
  /* Registers size */
  int num_greg;
  int num_sreg;
  int num_pred;
  int num_pseudo_regs;

  /* Registers offset */
  int greg_offset;
  int sreg_offset;
  int pred_offset;
  int pc_offset;
  int link_reg_offset;
  int max_regnum;

  /* ABI */
  int fp_offset;    /* frame base pointer reg */
  int sp_offset;    /* stack base pointer reg */
  int ra_offset;    /* return address reg */
  int rv_offset;    /* reutn value reg */
  int inst_offset;  /* inst addr reg */
  int ldram_offset; /* ldram addr reg */

  /* Ram size */
  long long nram_size;
  long long wram_size;
  long long sram_size;
  long long dram_size;

  /* Pointer size */
  long long nram_ptr_bit;
  long long wram_ptr_bit;
  long long sram_ptr_bit;
  long long dram_ptr_bit;

  /* Ram offset */
  long long nram_offset_dwarf;
  long long wram_offset_dwarf;
  long long sram_offset_dwarf;
  long long dram_offset_dwarf;

  long long nram_offset_core;
  long long wram_offset_core;
  long long sram_offset_core;
  long long dram_offset_core;

  /* dwarf abi for cngdb */
  char ra_dwarf_column;

  int invalid_lo_regnum;
  int invalid_hi_regnum;
};

int
cngdb_get_fp_regnum (struct gdbarch* gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return tdep->fp_offset;
}

int
cngdb_get_sp_regnum (struct gdbarch* gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return tdep->sp_offset;
}

int
cngdb_get_ra_regnum (struct gdbarch* gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return tdep->ra_offset;
}

int
cngdb_get_rv_regnum (struct gdbarch* gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return tdep->rv_offset;
}

int
cngdb_get_inst_base_regnum (struct gdbarch* gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return tdep->inst_offset;
}

int
cngdb_get_ldram_base_regnum (struct gdbarch* gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return tdep->ldram_offset;
}

int
cngdb_get_pc_regnum (struct gdbarch* gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return tdep->pc_offset;
}

int
cngdb_get_ra_dwarf_column (struct gdbarch* gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return tdep->ra_dwarf_column;
}

/* clean up cngdb workaround */
void
cngdb_cleanup (void)
{
  cngdb_api_finalize ();
  cngdb_clean_thread ();
}

static bool
cngdb_invalid_reg (struct gdbarch* gdbarch, int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  if (regnum > tdep->max_regnum)
    {
      CNGDB_FATAL ("reg number dealing fail");
    }
  return regnum == tdep->invalid_lo_regnum ||
         regnum == tdep->invalid_hi_regnum;
}

static bool
cngdb_genaral_reg (struct gdbarch* gdbarch, int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return regnum >= tdep->greg_offset &&
         regnum <  tdep->greg_offset + tdep->num_greg;
}

static bool
cngdb_special_reg (struct gdbarch* gdbarch, int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return regnum >= tdep->sreg_offset &&
         regnum <  tdep->sreg_offset + tdep->num_sreg;
}

static bool
cngdb_pred_reg (struct gdbarch* gdbarch, int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return regnum >= tdep->pred_offset &&
         regnum <  tdep->pred_offset + tdep->num_pred;
}

static bool
cngdb_pc_reg (struct gdbarch* gdbarch, int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return regnum == tdep->pc_offset;
}

static bool
cngdb_link_reg (struct gdbarch* gdbarch, int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return regnum == tdep->link_reg_offset;
}

bool
cngdb_offseted_nram (struct gdbarch* gdbarch, uint64_t addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return (addr >= tdep->nram_offset_dwarf &&
          addr <  tdep->nram_offset_dwarf + tdep->nram_size) ||
         (addr >= tdep->nram_offset_core &&
          addr <  tdep->nram_offset_core + tdep->nram_size);
}

static bool
cngdb_offseted_wram (struct gdbarch* gdbarch, uint64_t addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return (addr >= tdep->wram_offset_dwarf &&
          addr <  tdep->wram_offset_dwarf + tdep->wram_size) ||
         (addr >= tdep->wram_offset_core &&
          addr <  tdep->wram_offset_core + tdep->wram_size);
}

bool
cngdb_offseted_sram (struct gdbarch* gdbarch, uint64_t addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  return (addr >= tdep->sram_offset_dwarf &&
          addr <  tdep->sram_offset_dwarf + tdep->sram_size) ||
         (addr >= tdep->sram_offset_core &&
          addr <  tdep->sram_offset_core + tdep->sram_size);
}

static bool
cngdb_offseted_dram (struct gdbarch* gdbarch, uint64_t addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  uint64_t addr_virtual = addr & DRAM_MASK_CODE;
  return (addr >= tdep->dram_offset_dwarf &&
          addr <  tdep->dram_offset_dwarf + tdep->dram_size) ||
         (addr >= tdep->dram_offset_core &&
          addr <  tdep->dram_size) ||
         (addr_virtual >= tdep->dram_offset_dwarf &&
          addr_virtual <  tdep->dram_offset_dwarf + tdep->dram_size) ||
         (addr_virtual >= tdep->dram_offset_core &&
          addr_virtual <  tdep->dram_size);
}

uint64_t
cngdb_recover_offseted_space (struct gdbarch* gdbarch,
                              enum dwarf_location_atom *op,
                              uint64_t offseted_addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  if (cngdb_offseted_nram (gdbarch, offseted_addr))
    {
      *op = DW_OP_CAMBRICON_nram;
      if (offseted_addr >= tdep->nram_offset_dwarf)
        {
          return offseted_addr - tdep->nram_offset_dwarf;
        }
      else
        {
          return offseted_addr - tdep->nram_offset_core;
        }
    }
  if (cngdb_offseted_wram (gdbarch, offseted_addr))
    {
      *op = DW_OP_CAMBRICON_wram;
      if (offseted_addr >= tdep->wram_offset_dwarf)
        {
          return offseted_addr - tdep->wram_offset_dwarf;
        }
      else
        {
          return offseted_addr - tdep->wram_offset_core;
        }
    }
  if (cngdb_offseted_sram (gdbarch, offseted_addr))
    {
      *op = DW_OP_CAMBRICON_sram;
      if (offseted_addr >= tdep->sram_offset_dwarf)
        {
          return offseted_addr - tdep->sram_offset_dwarf;
        }
      else
        {
          return offseted_addr - tdep->sram_offset_core;
        }
    }
  if (cngdb_offseted_dram (gdbarch, offseted_addr))
    {
      *op = DW_OP_CAMBRICON_dram;
      if (offseted_addr >= tdep->dram_offset_dwarf)
        {
          if (!cngdb_focus_on_device ())
            {
              CNGDB_FATAL ("read dram while not on device");
            }
          uint32_t dev, cluster, core;
          uint64_t ldram_start;
          cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);
          cngdb_api_read_greg (cngdb_get_ldram_base_regnum (gdbarch),
                               dev, cluster, core, &ldram_start);
          return offseted_addr - tdep->dram_offset_dwarf + ldram_start;
        }
      else
        {
          return offseted_addr;
        }
    }
  return 0;
}

/* cngdb dwarf reg to gdbarch internal reg number */
static int
cngdb_reg_to_regnum (struct gdbarch* gdbarch, int dwarf_regnum)
{
  int offset = 0;
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  switch (0xff & dwarf_regnum)
    {
    case 2: offset = tdep->pred_offset; break;
    case 3: offset = tdep->sreg_offset; break;
    case 4: offset = tdep->greg_offset; break;
    default:
    return dwarf_regnum;
    }
  return offset + (dwarf_regnum >> 8);
}

static int
cngdb_adjust_regnum (struct gdbarch* gdbarch, int dwarf_regnum, int frame_p)
{
  return cngdb_reg_to_regnum (gdbarch, dwarf_regnum);
}


enum register_status
cngdb_pseudo_register_read (struct gdbarch *gdbarch,
                            struct regcache *regcache,
                            int cookednum,
                            gdb_byte *buf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  uint32_t dev, cluster, core;
  cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);

  if (cngdb_invalid_reg (gdbarch, cookednum))
    {
      CNGDB_ERROR ("invalid register");
      return REG_UNAVAILABLE;
    }

  if (cngdb_genaral_reg (gdbarch, cookednum))
    {
      cngdb_api_read_greg (cookednum - tdep->greg_offset,
                           dev, cluster, core, (void *)buf);
      return REG_VALID;
    }

  if (cngdb_special_reg (gdbarch, cookednum))
    {
      cngdb_api_read_sreg (cookednum - tdep->sreg_offset,
                           dev, cluster, core, (void *)buf);
      return REG_VALID;
    }

  if (cngdb_pred_reg (gdbarch, cookednum))
    {
      cngdb_api_read_preg (cookednum - tdep->pred_offset,
                           dev, cluster, core, (void *)buf);
      return REG_VALID;
    }

  if (cngdb_pc_reg (gdbarch, cookednum))
    {
      cngdb_api_read_pc (dev, cluster, core, (uint64_t *)buf);
      return REG_VALID;
    }

  if (cngdb_link_reg (gdbarch, cookednum))
    {
      /* link reg is in cooked register, so read/write link reg is possiable */
      CNGDB_WARNING ("illegal modify link reg");
      return REG_UNAVAILABLE;
    }
    return REG_UNAVAILABLE;
}

void
cngdb_pseudo_register_write (struct gdbarch *gdbarch,
                             struct regcache *regcache,
                             int cookednum,
                             const gdb_byte *buf)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  uint32_t dev, cluster, core;
  cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);

  if (cngdb_invalid_reg (gdbarch, cookednum))
    {
      /* link reg is in cooked register, so read/write link reg is possiable */
      CNGDB_WARNING ("invalid register");
      return;
    }

  if (cngdb_genaral_reg (gdbarch, cookednum))
    {
      cngdb_api_write_greg (cookednum - tdep->greg_offset,
                            dev, cluster, core, (void *)buf);
      return;
    }

  if (cngdb_special_reg (gdbarch, cookednum))
    {
      cngdb_api_write_sreg (cookednum - tdep->sreg_offset,
                            dev, cluster, core, (void *)buf);
      return;
    }

  if (cngdb_pred_reg (gdbarch, cookednum))
    {
      cngdb_api_write_preg (cookednum - tdep->pred_offset,
                            dev, cluster, core, (void *)buf);
      return;
    }

  if (cngdb_pc_reg (gdbarch, cookednum))
    {
      cngdb_api_write_pc (dev, cluster, core, (*(uint64_t *)buf));
      return;
    }

  if (cngdb_link_reg (gdbarch, cookednum))
    {
      CNGDB_ERROR ("illegal modify link reg");
      return;
    }
}

static const char*
cngdb_register_name (struct gdbarch* gdbarch, int regnr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  static char buf[MAX_NAME_LENGTH];

  if (cngdb_invalid_reg (gdbarch, regnr))
    {
      snprintf (buf, sizeof (buf), "INVALID");
      return buf;
    }

  if (cngdb_genaral_reg (gdbarch, regnr))
    {
      snprintf (buf, sizeof (buf), "R%d", regnr - tdep->greg_offset);
      return buf;
    }

  if (cngdb_special_reg (gdbarch, regnr))
    {
      snprintf (buf, sizeof (buf), "SR%d", regnr - tdep->sreg_offset);
      return buf;
    }

  if (cngdb_pred_reg (gdbarch, regnr))
    {
      snprintf (buf, sizeof (buf), "PR%d", regnr - tdep->pred_offset);
      return buf;
    }

  if (cngdb_pc_reg (gdbarch, regnr))
    {
      snprintf (buf, sizeof (buf), "pc");
      return buf;
    }

  if (cngdb_link_reg (gdbarch, regnr))
    {
      snprintf (buf, sizeof (buf), "LR");
      return buf;
    }

  return NULL;
}

static struct type *
cngdb_register_type (struct gdbarch *gdbarch, int regnum)
{
  if (cngdb_pc_reg (gdbarch, regnum))
    return builtin_type (gdbarch)->builtin_func_ptr;

  return builtin_type (gdbarch)->builtin_int64;
}

static int
cngdb_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
                           struct reggroup *group)
{
  if (group == save_reggroup || group == restore_reggroup)
    return regnum < gdbarch_num_regs (gdbarch) +
                    gdbarch_num_pseudo_regs (gdbarch);
  return default_register_reggroup_p (gdbarch, regnum, group);
}

static int
cngdb_convert_register_p (struct gdbarch *gdbarch, int regnum, struct type *type)
{
  return false;
}

static int
cngdb_register_to_value (struct frame_info *frame, int regnum,
                         struct type *type, gdb_byte *to,
                         int *optimizep, int *unavailablep)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  uint32_t dev, cluster, core;
  cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);

  if (cngdb_invalid_reg (gdbarch, regnum))
    {
      CNGDB_ERROR ("invalid register");
      *unavailablep = 1;
      return false;
    }

  *optimizep = *unavailablep = 0;

  if (cngdb_genaral_reg (gdbarch, regnum))
    {
      cngdb_api_read_greg (regnum, dev, cluster, core, (long *)to);
      return true;
    }

  if (cngdb_special_reg (gdbarch, regnum))
    {
      cngdb_api_read_sreg (regnum, dev, cluster, core, (long *)to);
      return true;
    }

  if (cngdb_pred_reg (gdbarch, regnum))
    {
      cngdb_api_read_preg (regnum, dev, cluster, core, (int *)to);
      return true;
    }

  if (cngdb_pc_reg (gdbarch, regnum))
    {
      cngdb_api_read_pc (dev, cluster, core, (uint64_t *)to);
      return true;
    }

  if (cngdb_link_reg (gdbarch, regnum))
    {
      CNGDB_ERROR ("illegal read link reg");
      return true;
    }

    *unavailablep = 1;
    return false;
}

static void
cngdb_value_to_register (struct frame_info *frame, int regnum,
                        struct type *type, const gdb_byte *from)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  uint32_t dev, cluster, core;
  cngdb_unpack_coord (cngdb_current_focus (), &dev, &cluster, &core);

  if (cngdb_invalid_reg (gdbarch, regnum))
    {
      CNGDB_ERROR ("invalid register");
    }

  if (cngdb_genaral_reg (gdbarch, regnum))
    {
      cngdb_api_write_greg (regnum, dev, cluster, core, (void *)from);
    }

  if (cngdb_special_reg (gdbarch, regnum))
    {
      cngdb_api_write_sreg (regnum, dev, cluster, core, (void *)from);
    }

  if (cngdb_pred_reg (gdbarch, regnum))
    {
      cngdb_api_write_preg (regnum, dev, cluster, core, (void *)from);
    }

  if (cngdb_pc_reg (gdbarch, regnum))
    {
      cngdb_api_write_pc (dev, cluster, core, *((uint64_t *)from));
    }

  if (cngdb_link_reg (gdbarch, regnum))
    {
      CNGDB_ERROR ("illegal write link reg");
    }
}

static const gdb_byte *
cngdb_breakpoint_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pc, int *len)
{
  return NULL;
}

static CORE_ADDR
cngdb_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  CORE_ADDR start_addr, end_addr, post_prologue_pc;

  if (find_pc_partial_function (pc, NULL, &start_addr, &end_addr))
    {
      post_prologue_pc = skip_prologue_using_sal (gdbarch, start_addr);

      if (post_prologue_pc > end_addr)
        post_prologue_pc = pc;

      /* If the post_prologue_pc does not make sense, return the given PC. */
      if (post_prologue_pc < pc)
        post_prologue_pc = pc;

      return post_prologue_pc;
    }

  /* If we can do no better than the original pc, then just return it. */
  return pc;
}

static int
cngdb_print_insn (bfd_vma vma, struct disassemble_info *info)
{
  info->fprintf_func (info->stream, "asm not supported");
  return 1;
}

static CORE_ADDR
cngdb_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  char ra_column = cngdb_get_ra_dwarf_column (gdbarch);
  CORE_ADDR pc = frame_unwind_register_unsigned (next_frame, ra_column);
  return pc;
}


void
cngdb_reset_pc (struct gdbarch *gdbarch, struct regcache* regcache)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  int pc_regno = tdep->pc_offset;
  regcache_cooked_write_unsigned (regcache, pc_regno, 0);
}

static enum return_value_convention
cngdb_return_value (struct gdbarch *gdbarch, struct value *function,
                    struct type *valtype, struct regcache *regcache,
                    gdb_byte *readbuf, const gdb_byte *writebuf)
{
  /* which can be recognized by abi */
  uint64_t regval;
  if (TYPE_CODE (valtype) == TYPE_CODE_INT ||
      TYPE_CODE (valtype) == TYPE_CODE_FLT ||
      TYPE_CODE (valtype) == TYPE_CODE_BOOL)
    {
      if (readbuf)
        {
          regcache_cooked_read_unsigned (regcache,
                                         cngdb_get_rv_regnum (gdbarch),
                                         &regval);
          memcpy (readbuf, &regval, 8);
        }
      else if (writebuf)
        {
          memcpy (&regval, writebuf, 8);
          regcache_cooked_write_unsigned (regcache,
                                          cngdb_get_rv_regnum (gdbarch),
                                          regval);
        }
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
  else
    {
      CNGDB_ERROR ("return type not supported, return show on gdb may wrong");
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
}

static struct gdbarch *
cngdb_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch      *gdbarch;
  struct gdbarch_tdep *tdep;

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* Allocate space for the new architecture.  */
  tdep = (struct gdbarch_tdep *)xcalloc (1, sizeof(struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  /* Set extra CAMBRICON architecture specific information */
  tdep->num_greg           = 32; /* general reg */
  tdep->num_sreg           = 1000; /* special reg */
  tdep->num_pred           = 32; /* predicate reg */
  tdep->greg_offset        = 0;
  tdep->sreg_offset        = tdep->greg_offset + tdep->num_greg;
  tdep->pred_offset        = tdep->sreg_offset + tdep->num_sreg;
  tdep->pc_offset          = tdep->pred_offset + tdep->num_pred;
  tdep->link_reg_offset    = tdep->pc_offset + 1;
  tdep->invalid_lo_regnum  = tdep->link_reg_offset + 1;
  tdep->invalid_hi_regnum  = tdep->invalid_lo_regnum + 1;
  tdep->max_regnum         = tdep->invalid_hi_regnum;
  tdep->nram_ptr_bit       = 16;
  tdep->wram_ptr_bit       = 16;
  tdep->sram_ptr_bit       = 64;
  tdep->nram_size          = 512 * 1024;
  tdep->wram_size          = 1024 * 1024;
  tdep->sram_size          = 2 * 1024 * 1024;
  tdep->dram_size          = ((long long) 1 << 40);

  tdep->nram_offset_dwarf  = (long long)6 << 56;
  tdep->wram_offset_dwarf  = (long long)7 << 56;
  tdep->sram_offset_dwarf  = (long long)8 << 56;
  tdep->dram_offset_dwarf  = (long long)9 << 56;
  /* hardware ram space coding, by default use MLU200 size */
  tdep->nram_offset_core   = (long long)2 << 20;
  tdep->wram_offset_core   = (long long)3 << 20;
  tdep->sram_offset_core   = (long long)4 << 20;
  tdep->dram_offset_core   = (long long)8 * 1024 * 1024;
  /* 2 for pc + link reg */
  tdep->num_pseudo_regs    = tdep->max_regnum - tdep->num_greg;

  /* ABI info*/
  tdep->ldram_offset       = 1;
  tdep->inst_offset        = 2;
  tdep->sp_offset          = 9;
  tdep->fp_offset          = 10;
  tdep->ra_offset          = 11;
  tdep->rv_offset          = 12;

  /* Data types.  */
  set_gdbarch_char_signed (gdbarch, 0);
  set_gdbarch_ptr_bit (gdbarch, 64);
  set_gdbarch_addr_bit (gdbarch, 64);
  set_gdbarch_short_bit (gdbarch, 16); /* use short as half */
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_long_bit (gdbarch, 64);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_float16_bit (gdbarch, 16);
  set_gdbarch_double_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 128);
  set_gdbarch_float_format (gdbarch, floatformats_ieee_single);
  set_gdbarch_double_format (gdbarch, floatformats_ieee_double);
  set_gdbarch_long_double_format (gdbarch, floatformats_ieee_double);

  /* Registers and Memory */
  set_gdbarch_num_regs        (gdbarch, tdep->num_greg);
  set_gdbarch_num_pseudo_regs (gdbarch, tdep->num_pseudo_regs);

  set_gdbarch_pc_regnum  (gdbarch, tdep->pc_offset);
  set_gdbarch_ps_regnum  (gdbarch, -1);
  set_gdbarch_sp_regnum  (gdbarch, tdep->sp_offset);
  set_gdbarch_fp0_regnum (gdbarch, tdep->fp_offset);

  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, cngdb_reg_to_regnum);

  set_gdbarch_pseudo_register_write (gdbarch, cngdb_pseudo_register_write);
  set_gdbarch_pseudo_register_read  (gdbarch, cngdb_pseudo_register_read);

  set_gdbarch_read_pc  (gdbarch, NULL);
  set_gdbarch_write_pc (gdbarch, NULL);

  set_gdbarch_register_name (gdbarch, cngdb_register_name);
  set_gdbarch_register_type (gdbarch, cngdb_register_type);
  set_gdbarch_register_reggroup_p (gdbarch, cngdb_register_reggroup_p);

  set_gdbarch_print_float_info     (gdbarch, NULL);
  set_gdbarch_print_vector_info    (gdbarch, NULL);

  set_gdbarch_convert_register_p  (gdbarch, cngdb_convert_register_p);
  set_gdbarch_register_to_value   (gdbarch, cngdb_register_to_value);
  set_gdbarch_value_to_register   (gdbarch, cngdb_value_to_register);

  /* Pointers and Addresses */
  set_gdbarch_fetch_pointer_argument (gdbarch, NULL);

  /* Address Classes */
  set_gdbarch_address_class_name_to_type_flags(gdbarch, NULL);
  set_gdbarch_address_class_type_flags_to_name(gdbarch, NULL);
  set_gdbarch_address_class_type_flags (gdbarch, NULL);

  set_gdbarch_elf_make_msymbol_special (gdbarch, NULL);

  /* Frame Interpretation */
  set_gdbarch_skip_prologue (gdbarch, cngdb_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_unwind_pc (gdbarch, cngdb_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, NULL);
  set_gdbarch_frame_align (gdbarch, NULL);
  set_gdbarch_frame_red_zone_size (gdbarch, 0);
  set_gdbarch_frame_args_skip (gdbarch, 0);
  set_gdbarch_frame_num_args (gdbarch, NULL);
  set_gdbarch_return_value (gdbarch, cngdb_return_value);
  frame_unwind_append_unwinder (gdbarch, &cngdb_frame_unwind);
  frame_base_append_sniffer (gdbarch, cngdb_frame_base_sniffer);
  frame_base_set_default (gdbarch, &cngdb_frame_base);
  dwarf2_append_unwinders (gdbarch);
  dwarf2_frame_set_adjust_regnum (gdbarch, cngdb_adjust_regnum);

  set_gdbarch_skip_permanent_breakpoint (gdbarch, NULL);
  set_gdbarch_fast_tracepoint_valid_at (gdbarch, NULL);
  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  set_gdbarch_max_insn_length (gdbarch, 8);

  /* Instructions */
  set_gdbarch_print_insn (gdbarch, cngdb_print_insn);
  set_gdbarch_relocate_instruction (gdbarch, NULL);
  set_gdbarch_breakpoint_from_pc   (gdbarch, cngdb_breakpoint_from_pc);
  set_gdbarch_adjust_breakpoint_address (gdbarch, NULL);

  /* dwarf abi for cngdb */
  tdep->ra_dwarf_column = 33;

  /* cngdb - no address space management */
  set_gdbarch_has_global_breakpoints (gdbarch, 1);

  set_gdbarch_get_siginfo_type (gdbarch, linux_get_siginfo_type);


  return gdbarch;
}

struct gdbarch *
cngdb_get_gdbarch (void)
{
  struct gdbarch_info info;
  if (!cngdb_gdbarch)
    {
      gdbarch_info_init (&info);
      info.bfd_arch_info = bfd_lookup_arch (bfd_arch_m68k, 0);
      info.byte_order = BFD_ENDIAN_LITTLE;
      info.byte_order_for_code = BFD_ENDIAN_LITTLE;
      cngdb_gdbarch = gdbarch_find_by_info (info);
    }
  return cngdb_gdbarch;
}

void
_initialize_cngdb_tdep (void)
{
  register_gdbarch_init (bfd_arch_m68k, cngdb_gdbarch_init);
  cngdb_get_gdbarch ();
}
