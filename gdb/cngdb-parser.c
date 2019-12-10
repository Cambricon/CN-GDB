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

#include "cngdb-parser.h"

int device_each;
int cluster_each;
int core_each;
int core_count;
extern cngdb_coords coords;

parser_state_t cngdb_pstate;

#include "cngdb-exp.h"

/* flag, what we are parsing */
VEC (valid_table) * table_allocator;


/* root of parser, input args, output vec<cngdb_coords> */
VEC (cngdb_coords) *
cngdb_parse_coord (char* arg)
{
  if (!arg) return NULL;
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "parser : %s", arg);

  /* initialize task size info */
  device_each  = cngdb_get_device_count ();
  cluster_each = cngdb_get_cluster_count ();
  core_each    = cngdb_get_core_count ();
  core_count   = cngdb_get_total_core_count ();

  /* initialize env */
  cngdb_pstate.saved_expr = NULL;
  cngdb_pstate.saved_func = NULL;
  cngdb_pstate.saved_expr_op = NULL;
  cngdb_pstate.saved_func_op = NULL;
  cngdb_pstate.num = -1;
  cngdb_pstate.cmp = CMP_NONE;
  cngdb_pstate.pro = PRO_NONE;
  VEC (cngdb_coords) * result_coords = NULL;
  cngdb_pstate.device_assign = 0;
  cngdb_pstate.cluster_assign = 0;
  cngdb_pstate.error = 0;

  /* feed input */
  cngdb__switch_to_buffer (cngdb__scan_string (arg));
  cngdb_parse ();

  /* check if cngdb parser failed */
  if (cngdb_pstate.device_assign || cngdb_pstate.cluster_assign ||
      cngdb_pstate.num != -1 || cngdb_pstate.cmp != CMP_NONE ||
      VEC_length (valid_table, cngdb_pstate.saved_expr) != 1 ||
      (cngdb_pstate.saved_func &&
       VEC_length (valid_table, cngdb_pstate.saved_func) != 0) ||
      (cngdb_pstate.saved_expr_op &&
       VEC_length (int, cngdb_pstate.saved_expr_op) != 0) ||
      (cngdb_pstate.saved_func_op &&
       VEC_length (int, cngdb_pstate.saved_func_op) != 0) ||
      cngdb_pstate.error)
    {
      CNGDB_ERROR ("logical error!");
      table_destroy ();
      return result_coords;
    }

  /* set all coord in valid table */
  valid_table vt = VEC_pop (valid_table, cngdb_pstate.saved_expr);
  for (unsigned int i = 0; i < core_count; i++)
    {
      if (!vt[i]) continue;
      VEC_safe_push(cngdb_coords, result_coords, coords + i);
    }
  table_destroy ();
  return result_coords;
}

void
table_destroy (void)
{
  if (cngdb_pstate.error) return;
  valid_table elt = NULL;
  for (int idx = 0; VEC_iterate(valid_table, table_allocator, idx, elt); idx++)
    {
      if (elt != NULL)
        {
          free (elt);
        }
    }
  VEC_free (valid_table, table_allocator);
  VEC_free (valid_table, cngdb_pstate.saved_expr);
  VEC_free (valid_table, cngdb_pstate.saved_func);
  VEC_free (int, cngdb_pstate.saved_expr_op);
  VEC_free (int, cngdb_pstate.saved_func_op);
  cngdb_pstate.saved_expr = NULL;
  cngdb_pstate.saved_func = NULL;
  cngdb_pstate.saved_expr_op = NULL;
  cngdb_pstate.saved_func_op = NULL;
}

/* reset table to all 0 */
static void
table_reset (valid_table vt)
{
  if (cngdb_pstate.error) return;
  for (int i = 0; i < core_count; i++)
    {
      vt[i] = 0;
    }
}

/* alloc table */
valid_table
table_alloc (void)
{
  if (cngdb_pstate.error) return NULL;
  valid_table vt = (valid_table) malloc (sizeof (int) * core_count);
  table_reset (vt);
  VEC_safe_push (valid_table, table_allocator, vt);
  return vt;
}

/* get max range we are parsering */
int
get_judge_range (void)
{
  if (cngdb_pstate.error) return 0;
  if (cngdb_pstate.pro == PRO_DEVICE)
    {
      return device_each;
    }
  if (cngdb_pstate.pro == PRO_CLUSTER)
    {
      return cngdb_pstate.device_assign ? cluster_each :
                                          cluster_each * device_each;
    }
  if (cngdb_pstate.pro == PRO_CORE)
    {
      if (cngdb_pstate.device_assign)
        {
          return cngdb_pstate.cluster_assign ? core_each :
                                               cluster_each * core_each;
        }
      else
        {
          return cngdb_pstate.cluster_assign ? core_each : core_count;
        }
    }
  CNGDB_ERROR ("undefined progress %d", cngdb_pstate.pro)
  cngdb_pstate.error = 1;
  return 0;
}

/* get the size when we step 1 current */
int
get_cover_size (void)
{
  if (cngdb_pstate.error) return 0;
  switch (cngdb_pstate.pro)
    {
    case PRO_DEVICE:
      return cluster_each * core_each;
    case PRO_CLUSTER:
      return core_each;
    case PRO_CORE:
      return 1;
    }
  CNGDB_ERROR ("undefined progress %d", cngdb_pstate.pro)
  cngdb_pstate.error = 1;
  return 0;
}

/* vt = (vt1 op vt2) */
valid_table
table_op (valid_table vt1, valid_table vt2, table_op_t op)
{
  if (cngdb_pstate.error) return vt1;
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "table op : %d", op);
  switch (op)
    {
    case TABLE_AND:
      {
        for (unsigned int i = 0; i < core_count; i++)
          {
            vt1[i] = vt1[i] && vt2[i];
          }
      }
      break;
    case TABLE_OR:
      {
        for (unsigned int i = 0; i < core_count; i++)
          {
            vt1[i] = vt1[i] || vt2[i];
          }
      }
      break;
    case TABLE_NONE:
    default:
      {
        CNGDB_ERROR (" bad compare op ");
        cngdb_pstate.error = 1;
        return NULL;
      }
      break;
    }
  return vt1;
}

valid_table
table_gen (int num, compare_t op, int judge_range, int cover)
{
  if (cngdb_pstate.error) return NULL;
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "table gen : num(%d), op(%d), judge_range(%d), cover(%d)", num, op, judge_range, cover);
  if (num > judge_range || judge_range > core_count
        || core_count % (cover * judge_range))
    {
      CNGDB_ERROR (" bad compare number ");
      cngdb_pstate.error = 1;
      return NULL;
    }
  valid_table vt = table_alloc ();
  int loop_count = core_count / (judge_range * cover);
  switch (op)
    {
    case CMP_EQ:
      {
        for (unsigned int loop = 0; loop < loop_count; loop++)
          {
            for (unsigned int i = 0; i < judge_range; i++)
              {
                if (i == num) {
                  for (int j = 0; j < cover; j++)
                    {
                      vt[loop * judge_range * cover + i * cover + j] = 1;
                    }
                  break;
                }
              }
          }
      }
      break;
    case CMP_NE:
      {
        for (unsigned int loop = 0; loop < loop_count; loop++)
          {
            for (unsigned int i = 0; i < judge_range; i++)
              {
                if (i != num) {
                  for (int j = 0; j < cover; j++)
                    {
                      vt[loop * judge_range * cover + i * cover + j] = 1;
                    }
                }
              }
          }
      }
      break;
    case CMP_LT:
      {
        for (unsigned int loop = 0; loop < loop_count; loop++)
          {
            for (unsigned int i = 0; i < judge_range; i++)
              {
                if (i < num) {
                  for (int j = 0; j < cover; j++)
                    {
                      vt[loop * judge_range * cover + i * cover + j] = 1;
                    }
                }
              }
          }
      }
      break;
    case CMP_LE:
      {
        for (unsigned int loop = 0; loop < loop_count; loop++)
          {
            for (unsigned int i = 0; i < judge_range; i++)
              {
                if (i <= num) {
                  for (int j = 0; j < cover; j++)
                    {
                      vt[loop * judge_range * cover + i * cover + j] = 1;
                    }
                }
              }
          }
      }
      break;
    case CMP_GT:
      {
        for (unsigned int loop = 0; loop < loop_count; loop++)
          {
            for (unsigned int i = 0; i < judge_range; i++)
              {
                if (i > num) {
                  for (int j = 0; j < cover; j++)
                    {
                      vt[loop * judge_range * cover + i * cover + j] = 1;
                    }
                }
              }
          }
      }
      break;
    case CMP_GE:
      {
        for (unsigned int loop = 0; loop < loop_count; loop++)
          {
            for (unsigned int i = 0; i < judge_range; i++)
              {
                if (i >= num) {
                  for (int j = 0; j < cover; j++)
                    {
                      vt[loop * judge_range * cover + i * cover + j] = 1;
                    }
                }
              }
          }
      }
      break;
    case CMP_NONE:
      {
        for (unsigned int loop = 0; loop < loop_count; loop++)
          {
            for (unsigned int i = 0; i < judge_range; i++)
              {
                if (i == num) {
                  for (int j = 0; j < cover; j++)
                    {
                      vt[loop * judge_range * cover + i * cover + j] = 1;
                    }
                }
              }
          }
      }
      break;
    default:
      CNGDB_ERROR ("bad compare op");
      cngdb_pstate.error = 1;
    }
  return vt;
}

/* eval function(F) and push input func expr */
void
eval_func (void)
{
  if (cngdb_pstate.error) return;
  if (cngdb_pstate.pro == PRO_NONE || cngdb_pstate.num < 0)
    {
      return;
    }

  valid_table vt = table_gen (cngdb_pstate.num, cngdb_pstate.cmp,
                              get_judge_range (), get_cover_size ());
  VEC_safe_push (valid_table, cngdb_pstate.saved_func, vt);

  /* reset status */
  cngdb_pstate.cmp = CMP_NONE;
  cngdb_pstate.num = -1;
}

/* concat function(F), aka : F logical F */
void
concat_func (void)
{
  if (cngdb_pstate.error) return;
  /* check state */
  int func_remain = VEC_length (valid_table, cngdb_pstate.saved_func);
  int op_remain = VEC_length (int, cngdb_pstate.saved_func_op);
  if (func_remain < 2 || op_remain < 1)
    {
      CNGDB_ERROR ("bad time to call 'concat_func()'");
      cngdb_pstate.error = 1;
      return;
    }

  /* concat func */
  table_op_t op = VEC_pop (int, cngdb_pstate.saved_func_op);
  valid_table vt1 = VEC_pop (valid_table, cngdb_pstate.saved_func);
  valid_table vt2 = VEC_pop (valid_table, cngdb_pstate.saved_func);
  VEC_quick_push (valid_table, cngdb_pstate.saved_func, table_op (vt1, vt2, op));
}

/* clear function(F) and push into expr vector */
void
push_expr (void)
{
  if (cngdb_pstate.error) return;
  int func_remain = VEC_length (valid_table, cngdb_pstate.saved_func);
  if (!func_remain)
    {
      CNGDB_ERROR ("bad func size when push expr : %d", func_remain);
      cngdb_pstate.error = 1;
      return;
    }
  /*  check if status correct first */
  if (VEC_length (int, cngdb_pstate.saved_func_op) != 0 ||
      func_remain != cngdb_pstate.device_assign +
                     cngdb_pstate.cluster_assign +
                     (cngdb_pstate.pro == PRO_CORE))
    {
      CNGDB_ERROR ("bad time to call 'push_expr', op size : %d, func size : %d",
                    VEC_length (int, cngdb_pstate.saved_func_op),
                    VEC_length (valid_table, cngdb_pstate.saved_func));
      cngdb_pstate.error = 1;
      return;
    }

  /* merge device/cluster/core info */
  valid_table vt = VEC_pop (valid_table, cngdb_pstate.saved_func);
  for (int i = 0; i < func_remain - 1; i++)
    {
      table_op (vt, VEC_pop (valid_table, cngdb_pstate.saved_func), TABLE_AND);
    }

  /* push info to expr */
  VEC_safe_push (valid_table, cngdb_pstate.saved_expr, vt);

  /* clean info */
  cngdb_pstate.device_assign = 0;
  cngdb_pstate.cluster_assign = 0;
  cngdb_pstate.pro = PRO_NONE;
}

/* concat expr, aka : expr logical expr */
void
concat_expr (void)
{
  if (cngdb_pstate.error) return;
  /* check state */
  int func_remain = VEC_length (valid_table, cngdb_pstate.saved_expr);
  int op_remain = VEC_length (int, cngdb_pstate.saved_expr_op);
  if (func_remain < 2 || op_remain < 1)
    {
      CNGDB_ERROR ("bad time to call 'concat_expr()'");
      cngdb_pstate.error = 1;
      return;
    }

  table_op_t op = VEC_pop (int, cngdb_pstate.saved_expr_op);
  valid_table vt1 = VEC_pop (valid_table, cngdb_pstate.saved_expr);
  valid_table vt2 = VEC_pop (valid_table, cngdb_pstate.saved_expr);
  VEC_quick_push (valid_table, cngdb_pstate.saved_expr,
                  table_op (vt1, vt2, op));
}
