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

#ifndef CNGDB_ASM_C20_H
#define CNGDB_ASM_C20_H

/* c20 field type exceped const */
enum c20_field_type {
  ConstData,
  Finish,
  LayerType,
  OperatorType,
  RESERVED,
  Predicate,
  ChangePosEn,
  Src1RegId,
  Src2RegId,
  Src2RegEn,
  Src2Imme,
  BarId,
  BarCount,
  Sync,
  Global,
  BarCountRegEn,
  BarCountRegId,
  BarIdRegEn,
  BarIdRegId,
  DMA0NotSyncEn,
  DMA1NotSyncEn,
  ScalarNotSyncEn,
  StreamNotSyncEn,
  RobNotSyncEn,
  AddrRegEn,
  AddrRegId,
  AddrImme,
  AddrImmeH,
  AddrImmeL,
  SyncDMA0En,
  SyncDMA1En,
  SyncScalarEn,
  SyncStreamEn,
  SyncRobEn,
  LinkRegEn,
  PrefetchLegthRegId,
  PrefetchLegthRegEn,
  PrefetchLegthRegImm,
};

/* a field in c20 instruction */
struct c20_inst_field {
  enum c20_field_type field_type;
  int end;
  int start;
  unsigned int const_data;
};

typedef void asm_instr_func (char *asm_output, struct c20_inst_field *fs,
                             void *inst);

struct c20_instruction {
  asm_instr_func* asm_func;
  struct c20_inst_field fs[512];
};

int cngdb_asm_c20 (void* inst, char *output);

void cngdb_asm_c20_cb (char *asm_output, struct c20_inst_field *fs,
                       void *inst);
void cngdb_asm_c20_jump_prefetch (char *asm_output, struct c20_inst_field *fs,
                                  void *inst);

void cngdb_asm_c20_barrier (char *asm_output, struct c20_inst_field *fs,
                            void *inst);
#endif
