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
#include "cngdb-asm-c20.h"
#include "cndebugger.h"

#define get_field(f) cngdb_get_field_val (fs, f, inst)

#define write_out(...) {;}

/* barrier/branch info */
extern CNDBGBranch* branch_pointer;
extern CNDBGBarrier* barrier_pointer;
extern int branch_count;
extern int barrier_count;

/* weather we are counting branch/barrier */
extern int counting_bb;

#include "cngdb-asm-c20-def.h"


static void
write_string (char **output, char *input)
{
}

/* decode data from start to end bit, return decoded data */
static unsigned long long
cngdb_get_range_val (int start, int end, void *data)
{
  unsigned long long output = 0;
  uint8_t *pos = (uint8_t *)((unsigned long long)data + start / 8);
  int offset = start % 8;
  int remain_size = end - start + 1;
  int shift = 0;

  if (remain_size <= 0 || remain_size > 64)
    {
      CNGDB_FATAL ("asm count remain internal error");
    }
  while (remain_size)
    {
      if (remain_size < 0)
        {
          CNGDB_FATAL ("asm find val internal error");
        }
      if (offset + remain_size >= 8)
        {
          /* use all */
          output |= ((unsigned long long) pos[0]) >> offset << shift;
          shift += 8 - offset;
          remain_size = offset + remain_size - 8;
          offset = 0;
          pos += 1;
        }
      else
        {
          /* use part */
          output |= ((unsigned long long)(
            ((pos[0] << (8 - offset - remain_size)) % 256) >>
            (8 - remain_size))) << shift;
          remain_size = 0;
        }
    }
  return output;
}

/* decode data in feild */
static unsigned long long
cngdb_get_field_val (struct c20_inst_field *fld, enum c20_field_type ft,
                     void *data)
{
  if (ft == Predicate)
    {
      return cngdb_get_range_val (0, 4, data);
    }
  while (fld && fld->field_type != Predicate)
    {
      if (fld->field_type == ft)
        break;
      fld++;
    }
  if (!fld || (fld->field_type == Predicate && !counting_bb))
    {
      CNGDB_FATAL ("find field %d fail", ft);
    }
  return cngdb_get_range_val (fld->start, fld->end, data);
}

/* return if instruction in data is fit with 'ins' */
static int
cngdb_instr_fit (struct c20_instruction *ins, void *data)
{
  /* skip finish feild */
  struct c20_inst_field *fld = ins->fs + 1;

  /* check const field */
  while (fld && fld->field_type == ConstData)
    {
      if (cngdb_get_range_val (fld->start, fld->end, data) != fld->const_data)
        {
          return false;
        }
      fld++;
    }
  if (fld)
    {
      return true;
    }
  else
    {
      CNGDB_FATAL ("field error !");
      return false;
    }
}

void
cngdb_asm_c20_cb (char *asm_output, struct c20_inst_field *fs, void *inst)
{
  if (counting_bb)
    {
      /* generating jump table */
      CNDBGBranch *barray = (CNDBGBranch *)(branch_pointer + branch_count);
      barray->CBcType = (int) get_field (LayerType);
      barray->CBoType = (int) get_field (OperatorType);
      barray->CBARegId = (int) get_field (Src1RegId);
      barray->CBBRegEn = (int) get_field (Src2RegEn);
      barray->CBBRegId = (int) get_field (Src2RegId);
      barray->CBBImm = (int) get_field (Src2Imme);
      barray->CBreverse = (int) get_field (ChangePosEn);
      barray->CBpredRegId = (int) get_field (Predicate);
      barray->CBaddrRegEn = (int) get_field (AddrRegEn);
      barray->CBaddrRegId = (int) get_field (AddrRegId);
      barray->CBaddrImm = (uint64_t) get_field (AddrImmeH) << 16 |
                          get_field (AddrImme);
      branch_count ++;
    }

}

void
cngdb_asm_c20_jump_prefetch (char *asm_output, struct c20_inst_field *fs,
                             void *inst)
{
  if (counting_bb)
    {
      CNDBGBranch *barray = (CNDBGBranch *)(branch_pointer + branch_count);
      barray->CBcType = CB_EQ;
      barray->CBoType = CB_uint16;
      barray->CBARegId = 0;
      barray->CBBRegEn = 0;
      barray->CBBRegId = 0;
      barray->CBBImm = 0;
      barray->CBreverse = 0;
      barray->CBpredRegId = 0;
      barray->CBaddrRegEn = (int) get_field (AddrRegEn);
      barray->CBaddrRegId = (int) get_field (AddrRegId);
      barray->CBaddrImm = (uint64_t) get_field (AddrImmeH) << 16 |
                          get_field (AddrImmeL);
      branch_count ++;
    }
}

void
cngdb_asm_c20_barrier (char *asm_output, struct c20_inst_field *fs, void *inst)
{
  if (counting_bb)
    {
      CNDBGBarrier *barray = (CNDBGBarrier *)(barrier_pointer + barrier_count);
      barray->BAbarrierIdRegEn = (int) get_field (BarIdRegEn);
      barray->BAbarrierIdRegId = (int) get_field (BarIdRegId);
      barray->BAbarrierIdImm = (int) get_field (BarId);
      barray->BAbarrierCountRegEn = (int) get_field (BarCountRegEn);
      barray->BAbarrierCountRegId = (int) get_field (BarCountRegId);
      barray->BAbarrierCountImm = (int) get_field (BarCount);
      barray->BAbarrierSync = (int) get_field (Sync);
      barray->BAbarrierGlobal = (int) get_field (Global);
      barray->BApredRegId = (int) get_field (Predicate);
      barrier_count ++;
    }

}

int cngdb_asm_c20 (void *inst, char *output) {
  struct c20_instruction *iter = c20_instr;
  while (iter->asm_func != NULL)
    {
      if (cngdb_instr_fit (iter, inst))
        break;
      iter ++;
    }

  if (iter->asm_func != NULL)
    iter->asm_func (output, iter->fs, inst);
  char unsupport_inst[] = "unsupport asm";
  memcpy (output, unsupport_inst, 14);

  return CNDBG_SUCCESS;
}
