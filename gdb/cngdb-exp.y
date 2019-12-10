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

%{

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "cngdb-parser.h"

int  cngdb_lex (void);
void cngdb_error (char* msg);

extern parser_state_t cngdb_pstate;


%}


%token<value> DEVICE CLUSTER CORE
%token<value> LOGICAL
%token<value> CMP
%token<value> VALUE X
%token<value> OPENBR CLOSEBR

%union
  {
    int64_t value;
  }

%%

command : expr
{

};

expr : expr LOGICAL expr
{
  concat_expr ();
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get expr lo expr");
};

expr : OPENBR expr CLOSEBR
{
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get (expr)");
};

expr : device_msg cluster_msg core_msg
{
  push_expr ();
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "expr generated");
};

device_msg : { CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "empty device msg"); } ;
device_msg : DEVICE me
{
  cngdb_pstate.device_assign = 1;
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get device me");
};

cluster_msg : { CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "empty cluster msg"); } ;
cluster_msg : CLUSTER me
{
  cngdb_pstate.cluster_assign = 1;
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get cluster me");
};

core_msg : { CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "empty core msg"); } ;
core_msg : CORE me
{
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get core me");
};

me : func
{
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get me");
};


func : OPENBR sfunc LOGICAL sfunc CLOSEBR
{
  concat_func ();
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get func -> sfunc logical func");
};

sfunc : OPENBR sfunc CLOSEBR
{
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get (sfunc)");
};

sfunc : sfunc LOGICAL sfunc
{
  concat_func ();
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get sfunc logical func");
}

sfunc : funcall;
func :  funcall;

funcall : X CMP VALUE
{
  eval_func ();
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get x op number");
};

funcall : VALUE
{
  eval_func ();
  CNGDB_INFOD (CNDBG_DOMAIN_PARSER, "get number");
};

%%

void cngdb_error (char *msg)
{
  CNGDB_ERROR ("%s", msg);
  cngdb_pstate.error = 1;
}
