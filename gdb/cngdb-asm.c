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

#include "cngdb-asm-c20.h"
#include "cngdb-asm.h"
#include "cngdb-util.h"
#include "cndebugger.h"

/* .cncode section from elf */
extern void *cncode_content;

/* cncode size */
extern unsigned long long cncode_size;

void
cngdb_asm_inst (unsigned long long pc, char *output)
{
  if (pc >= cncode_size)
    {
    }
  else
    {
      CNDBGResult res = cngdb_asm_c20 (
              (void *)((unsigned long long) cncode_content + 64 * pc), output);
      if (res != CNDBG_SUCCESS)
        {
          CNGDB_ERROR ("asm failed");
        }
    }
}
