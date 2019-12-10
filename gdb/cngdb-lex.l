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

WHITE   [ \t\n]
DIG     [0-9]
NUM     ({DIG}({DIG})*)

%{

#include <stdlib.h>
#include "defs.h"
#include "value.h"
#include "cngdb-parser.h"
#include "cngdb-exp.h"

extern parser_state_t cngdb_pstate;
extern int cngdb_parser_lex (void);

%}

/*100% input available when parsing -> faster */
%option  never-interactive

/*generates error if unidentified token -> safer */
%option  default

/*only one input, no need to define yywrap -> cleaner */
%option  noyywrap

/*don't generate input or yyunput, which are never used */
%option noinput
%option nounput
%option yylineno

%%

device          {
                  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get indicator device");
                  if (cngdb_pstate.pro != PRO_NONE)
                    {
                      CNGDB_ERROR ("bad 'device' time");
                      cngdb_pstate.error = 1;
                    }
                  eval_func ();
                  cngdb_pstate.pro = PRO_DEVICE;
                  return DEVICE;
                }
cluster         {
                  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get indicator cluster");
                  if (cngdb_pstate.pro > PRO_DEVICE)
                    {
                      CNGDB_ERROR ("bad 'cluster' time");
                      cngdb_pstate.error = 1;
                    }
                  /* eval device function before change pro */
                  eval_func ();
                  cngdb_pstate.pro = PRO_CLUSTER;
                  return CLUSTER;
                }
core            {
                  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get indicator core");
                  if (cngdb_pstate.pro == PRO_CORE)
                    {
                      CNGDB_ERROR ("bad 'core' time");
                      cngdb_pstate.error = 1;
                    }
                  /* eval device/cluster function before change pro */
                  eval_func ();
                  cngdb_pstate.pro = PRO_CORE;
                  return CORE;
                }

"x"             { return X; }
{NUM}           {
                  cngdb_pstate.num = strtol (yytext, NULL, 10);
                  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get num : %d",
                               cngdb_pstate.num);
                  return VALUE;
                }

"&&"            {
                  if (cngdb_pstate.in_func)
                    {
                      CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get && in func : %d",
                                   cngdb_pstate.in_func);
                      VEC_safe_push (int, cngdb_pstate.saved_func_op,
                                     TABLE_AND);
                    }
                  else
                    {
                      CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get && out func");
                      VEC_safe_push (int, cngdb_pstate.saved_expr_op,
                                     TABLE_AND);
                    }
                  return LOGICAL;
                }
"||"            {
                  if (cngdb_pstate.in_func)
                    {
                      CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get || in func : %d",
                                   cngdb_pstate.in_func);
                      VEC_safe_push (int, cngdb_pstate.saved_func_op, TABLE_OR);
                    }
                  else
                    {
                      CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get || out func");
                      VEC_safe_push (int, cngdb_pstate.saved_expr_op, TABLE_OR);
                    }
                  return LOGICAL;
                }

"=="            { CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get cmp");
                  cngdb_pstate.cmp = CMP_EQ; return CMP; }
"!="            { CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get cmp");
                  cngdb_pstate.cmp = CMP_NE; return CMP; }
"<"             { CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get cmp");
                  cngdb_pstate.cmp = CMP_LT; return CMP; }
">"             { CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get cmp");
                  cngdb_pstate.cmp = CMP_GT; return CMP; }
"<="            { CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get cmp");
                  cngdb_pstate.cmp = CMP_LE; return CMP; }
">="            { CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get cmp");
                  cngdb_pstate.cmp = CMP_GE; return CMP; }

"("             {
                  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get BR '('");
                  if (cngdb_pstate.pro != PRO_NONE)
                    {
                      cngdb_pstate.in_func++;
                    }
                  return OPENBR;
                }

")"             {
                  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get BR ')'");
                  if (cngdb_pstate.pro != PRO_NONE && cngdb_pstate.in_func)
                    {
                      cngdb_pstate.in_func--;
                    }
                  return CLOSEBR;
                }

{WHITE}         { /* ignore */ }

%%
