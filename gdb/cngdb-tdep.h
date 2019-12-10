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


#ifndef CNGDB_TDEP
#define CNGDB_TDEP

#include "stdbool.h"
#include "dwarf2.h"

/* clean up cngdb workaround */
void cngdb_cleanup (void);

bool cngdb_platform_supports_tid (void);

int cngdb_gdb_get_tid (ptid_t ptid);

void _initialize_cngdb_tdep (void);

uint64_t cngdb_recover_offseted_space (struct gdbarch* gdbarch,
                                       enum dwarf_location_atom *op,
                                       uint64_t offseted_addr);

enum register_status cngdb_pseudo_register_read (struct gdbarch *gdbarch,
                                                 struct regcache *regcache,
                                                 int cookednum,
                                                 gdb_byte *buf);

void cngdb_pseudo_register_write (struct gdbarch *gdbarch,
                                  struct regcache *regcache,
                                  int cookednum,
                                  const gdb_byte *buf);

void cngdb_reset_pc (struct gdbarch *gdbarch,
                     struct regcache* regcache);

/*------------------------Global Variables --------------------------*/
struct gdbarch * cngdb_get_gdbarch (void);

int cngdb_get_fp_regnum (struct gdbarch* gdbarch);
int cngdb_get_sp_regnum (struct gdbarch* gdbarch);
int cngdb_get_ra_regnum (struct gdbarch* gdbarch);
int cngdb_get_rv_regnum (struct gdbarch* gdbarch);
int cngdb_get_pc_regnum (struct gdbarch* gdbarch);
int cngdb_get_inst_base_regnum (struct gdbarch* gdbarch);
int cngdb_get_ldram_base_regnum (struct gdbarch* gdbarch);
int cngdb_get_ra_dwarf_column (struct gdbarch* gdbarch);
bool cngdb_offseted_nram (struct gdbarch* gdbarch, uint64_t addr);
bool cngdb_offseted_sram (struct gdbarch* gdbarch, uint64_t addr);

#endif
