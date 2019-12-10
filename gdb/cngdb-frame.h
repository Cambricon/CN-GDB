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

#ifndef CNGDB_FRAME_H
#define CNGDB_FRAME_H

#include "defs.h"
#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"

extern const struct frame_unwind cngdb_frame_unwind;
extern const struct frame_base cngdb_frame_base;

enum unwind_stop_reason
  cngdb_frame_unwind_stop_reason (struct frame_info *this_frame,
                                  void **this_prologue_cache);

/* build cngdb frame_id */
void cngdb_frame_this_id (struct frame_info *this_frame,
                          void **this_prologue_cache,
                          struct frame_id *this_id);

/* fetch register in last frame */
struct value * cngdb_frame_prev_register (struct frame_info *this_frame,
                                          void **this_prologue_cache,
                                          int regnum);

/* is sniffer for cngdb frame exist */
int cngdb_frame_sniffer (const struct frame_unwind *self,
                         struct frame_info *this_frame,
                         void **this_prologue_cache);

/* get frame base address of current frame */
CORE_ADDR cngdb_frame_base_address (struct frame_info *this_frame,
                                    void **this_base_cache);

/* is this frame's base address sniffer exist */
const struct frame_base *
  cngdb_frame_base_sniffer (struct frame_info *this_frame);

/* is this frame outmost frame */
int cngdb_frame_outmost (struct frame_info *this_frame);

int cngdb_frame_can_print (void);

#endif
