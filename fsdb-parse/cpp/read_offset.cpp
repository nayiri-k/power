/* *****************************************************************************
// [read_offset.cpp]
//
//  Copyright 2009-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// NAME : ffr_offset_r.cpp
//
// PURPOSE : A regression test for multiple offset checking and read var
//           on demand.
//
// DESCRIPTION :
//
//              Read all scope logical offsets 
//              and use those scope offset to read the variable with ffrReadVarByLogUOff
//

#ifdef NOVAS_FSDB
    #undef NOVAS_FSDB
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ffrAPI.h"

//
// Constants
//

#ifndef TRUE
    const int TRUE = 1;
#endif

#ifndef FALSE
    const int FALSE = 0;
#endif

const int NUM_SCOPE_OFFSET = 512; // Number of logical offsets in scope 'Top'

static void
__DumpVar(fsdbTreeCBDataVar *var);

// Logical offsets in scope 'Top'

fsdbLUOff scope_offset[NUM_SCOPE_OFFSET];
static int scope_count;

//
// Function Declaration
//

static bool_T MyTreeCB(fsdbTreeCBType cb_type, void *client_data,
    void *tree_cb_data);

//
// Main Program
//

int
main(int argc, char *argv[])
{
    ffrObject *fsdb_obj;
    uint_T i;
    fsdbRC rc;

    // Check and open the fsdb file 

    if (FALSE == ffrObject::ffrIsFSDB(argv[1])) {
        fprintf(stderr, "%s is not an fsdb file.\n", argv[1]);
        exit(FSDB_RC_FILE_IS_NOT_A_FSDB_FILE);
    }

    fsdb_obj =
        ffrObject::ffrOpen2(argv[1], MyTreeCB, NULL);

    if (NULL == fsdb_obj) {
        fprintf(stderr, "ffrObject::ffrOpen2 failed.\n");
        exit(FSDB_RC_OBJECT_CREATION_FAILED);
    }

    if (FSDB_FT_VHDL != fsdb_obj->ffrGetFileType()) {
        fprintf(stderr, "%s is not VHDL type fsdb\n", argv[1]);
        exit(FSDB_RC_UNKOWN_FILE_TYPE);
    }

    // Read the design hierarchy

    fsdb_obj->ffrReadScopeTree();

    // If the offsets are correct, check var using these offsets


    for(i = 0; i < scope_count; i++) {
        if(TRUE == fsdb_obj->ffrReadVarByLogUOff(&scope_offset[i]))
            return FALSE;
   }
    return TRUE;
}

//
// NAME : MyTreeCB
//
// DESCRIPTION: A callback function used by fsdb reader
//
// PARAMETERS : See fsdb reader document.
//
// RETURN : See fsdb reader document.
//

static bool_T
MyTreeCB(fsdbTreeCBType cb_type, void *client_data, void *tree_cb_data)
{
    fsdbTreeCBDataScope* pScope;
    fsdbTreeCBDataUpscope* pUpscope;
    fsdbTreeCBDataVar* pVar;

    switch (cb_type) {
    case FSDB_TREE_CBT_SCOPE :

        // The first time 'Top' is entered, record its logical offset.
		// Other logical offsets of 'Top' will be known when an
		// UPSCOPE sets the current traverse point to 'Top'.

        pScope = (fsdbTreeCBDataScope*) tree_cb_data;
        scope_offset[scope_count++] = pScope->var_start_log_uoff;

        break;

    case FSDB_TREE_CBT_UPSCOPE :
        pUpscope = (fsdbTreeCBDataUpscope*) tree_cb_data;
        scope_offset[scope_count++] = pUpscope->var_start_log_uoff;

        break;

    case FSDB_TREE_CBT_VAR:
        pVar = (fsdbTreeCBDataVar*) tree_cb_data;
        __DumpVar(pVar);
        break;

    default:
        return TRUE;
    }

    return TRUE;
}


static void 
__DumpVar(fsdbTreeCBDataVar *var)
{
    str_T type;
    str_T bpb;

    switch(var->bytes_per_bit) {
    case FSDB_BYTES_PER_BIT_1B:
	bpb = (str_T) "1B";
	break;

    case FSDB_BYTES_PER_BIT_2B:
	bpb = (str_T) "2B";
	break;

    case FSDB_BYTES_PER_BIT_4B:
	bpb = (str_T) "4B";
	break;

    case FSDB_BYTES_PER_BIT_8B:
	bpb = (str_T) "8B";
	break;

    default:
	bpb = (str_T) "?B";
	break;
    }

    switch (var->type) {
    case FSDB_VT_VHDL_SIGNAL:
	type = (str_T) "signal"; 
  	break;

    case FSDB_VT_VHDL_VARIABLE:
	type = (str_T) "variable"; 
	break;

    case FSDB_VT_VHDL_CONSTANT:
	type = (str_T) "constant"; 
	break;

    case FSDB_VT_VHDL_FILE:
	type = (str_T) "file"; 
	break;

    case FSDB_VT_VHDL_MEMORY:
	type = (str_T) "memory"; 
	break;

    case FSDB_VT_VHDL_MEMORY_DEPTH:
	type = (str_T) "memory depth"; 
	break;

    default:
	type = (str_T) "unknown_var_type";
	break;
    }

    fprintf(stderr,
	"<Var>  name:%s  l:%u  r:%u  type:%s  ",
	var->name, var->lbitnum, var->rbitnum, type);
    fprintf(stderr,
	"idcode:%u  dtidcode:%u  bpb:%s\n",
	var->u.idcode, var->dtidcode, bpb);
}
