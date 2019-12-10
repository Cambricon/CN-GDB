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

#ifndef CNGDB_LINUX_NAT_H
#define CNGDB_LINUX_NAT_H

#include "defs.h"
#include "bfd.h"
#include "elf-bfd.h"
#include "dwarf2.h"
#include "cndebugger.h"
#include "cngdb-util.h"
#include "cngdb-api.h"
#include "cngdb-tdep.h"
#include "cngdb-state.h"

void cngdb_read_sections (struct bfd *abfd, struct bfd_section *asect,
                          void *objfilep);

/* get instructions from backend */
void cngdb_get_inst (void);

void cngdb_nat_add_target (struct target_ops *t);

void cngdb_nat_resume_thread (ptid_t ptid);

#endif  // CNGDB_LINUX_NAT_H
