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

#include "cngdb-state.h"

VEC (cngdb_coords) * cngdb_parse_coord (char *arg);

/* comparision types */
typedef enum {
  CMP_NONE, /* ignored */
  CMP_EQ,   /* == */
  CMP_NE,   /* != */
  CMP_LT,   /* < */
  CMP_LE,   /* <= */
  CMP_GT,   /* > */
  CMP_GE,   /* >= */
} compare_t;

/* table operators */
typedef enum {
  TABLE_NONE, /* invalid op */
  TABLE_AND,  /* && */
  TABLE_OR,   /* || */
} table_op_t;
typedef table_op_t* table_ops;

typedef int* valid_table;
DEF_VEC_P (valid_table);

typedef enum {
  PRO_NONE, /* none, default pro */
  PRO_DEVICE, /* parsing device */
  PRO_CLUSTER, /* parsing device */
  PRO_CORE, /* parsing device */
} parser_progress_t;

/* parser state, see 'cngdb-parser.c' to tell what is expr/ME */
typedef struct {
  /* what we are processing */
  parser_progress_t pro;

  /* weather device/cluster assign will affect behavior */
  int device_assign;
  int cluster_assign;

  /* saved expr to be combination */
  VEC (valid_table) * saved_expr;
  VEC (int) * saved_expr_op;

  /* saved ME to be combination */
  VEC (valid_table) * saved_func;
  VEC (int) * saved_func_op;

  compare_t cmp; /* cmp operand we are parsing */
  int num; /* number parsing */
  int in_func; /* weather we are in processing func */
  int error;
} parser_state_t;

/* get judge range from global parser_state_t */
int get_judge_range (void);

/* get cover size from global parser_state_t */
int get_cover_size (void);

/* destroy all tables */
void table_destroy (void);

/* eval function(F) and push input func expr */
void eval_func (void);

/* concat function(F), aka : F logical F */
void concat_func (void);

/* clear function(F) and push into expr vector */
void push_expr (void);

/* concat expr, aka : expr logical expr */
void concat_expr (void);

/* warpper for operator new */
valid_table table_alloc (void);

/* vt1 = (vt1 op vt2) */
valid_table table_op (valid_table vt1, valid_table vt2, table_op_t op);

/* vt = S { x | x op num == true } */
valid_table table_gen (int num, compare_t op, int judge_range, int cover);
