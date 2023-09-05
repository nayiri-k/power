/* *****************************************************************************
// [test_sig_arr.cpp]
//
//  Copyright 2001-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// Program Name	: test_sig_arr.cpp
//
// Purpose	: test ffrLoadSignals API 
//

//
// NOVAS_FSDB is internally used in NOVAS
//

#ifdef NOVAS_FSDB
#undef NOVAS_FSDB
#endif

#include "ffrAPI.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE	1
#endif

#define NUM_JUMP_TEST		38	

//
// The tree call back function(for traversing all the signals' definitions)
//
static bool_T
MyTreeCB(fsdbTreeCBType type, void *client_data, void *cb_data);

//
// For dumping scope's definition
//
static void 
DumpScope(fsdbScopeRec* scope);

//
// For dumping var's definition
// 
static void 
DumpVar(fsdbVarRec *var_id);

int 
main(int argc, char *argv[])
{
    if (2 != argc) { 
        fprintf(stderr, "usage: test_sig_arr verilog_type_fsdb\n");
        return FSDB_RC_FAILURE;
    }
 
    //      
    // check the file to see if it's a fsdb file or not. 
    //
    if (FALSE == ffrObject::ffrIsFSDB(argv[1])) {
        fprintf(stderr, "%s is not an fsdb file.\n", argv[1]);
        return FSDB_RC_FAILURE;
    }
 
    ffrFSDBInfo fsdb_info;
 
    ffrObject::ffrGetFSDBInfo(argv[1], fsdb_info);
    if (FSDB_FT_VERILOG != fsdb_info.file_type) {
        fprintf(stderr, "file type is not verilog.\n");
        return FSDB_RC_FAILURE;
    }


    uint_T digit;
    char   *unit;
   
    ffrObject::ffrExtractScaleUnit("10ps", digit, unit);
    fprintf(stderr, "10ps ---> digit = %u, unit = %s\n", digit, unit);
    
    ffrObject::ffrExtractScaleUnit("1024", digit, unit);
    fprintf(stderr, "1024 ---> digit = %u, unit = %s\n", digit, unit);
    

    ffrObject::ffrExtractScaleUnit("psns", digit, unit);
    fprintf(stderr, "psns ---> digit = %u, unit = %s\n", digit, unit);
    

    ffrObject::ffrExtractScaleUnit("1fs", digit, unit);
    fprintf(stderr, "1fs ---> digit = %u, unit = %s\n", digit, unit);
   
    if (NULL == argv[1]) {
	fprintf(stderr, "usage: test_sig_arr fsdb_file\n");
  	exit(FSDB_RC_FAILURE);
    } 
 
    // 
    // check the file to see if it's a fsdb file or not.
    //
    if (FALSE == ffrObject::ffrIsFSDB(argv[1])) {
	printf("%s is not a fsdb file.\n", argv[1]);
	exit(FSDB_RC_FAILURE);
    }

    //  
    // Open the file. Note that we can register a tree callback function with ffrOpen(),
    // if this file can be opened successfully, then the tree callback function will be called
    // in ffrOpen(). 
    //
    ffrObject *fsdbObj = ffrObject::ffrOpen3(argv[1]);
    if (NULL == fsdbObj) {
	printf("ffrObject::ffrOpen() failed.\n");
	exit(FSDB_RC_FAILURE);
    }
    fsdbObj->ffrSetTreeCBFunc(MyTreeCB, NULL);

    //
    // check how many unique signals are there in this fsdb file.
    //
    fsdbVarIdcode max_var_idcode = fsdbObj->ffrGetMaxVarIdcode();

    fsdbVarIdcode 	idcode;
    uint_T 		idx;

      
    fsdbObj->ffrLoadSignals();

    fsdbTag64  	 time;
    ffrVCTrvsHdl vc_trvs_hdl;


    for (idcode = FSDB_MIN_VAR_IDCODE; idcode < max_var_idcode; idcode++) {

        ffrVCTrvsHdl vc_trvs_hdl = fsdbObj->ffrCreateVCTrvsHdl(idcode); 
        if (NULL == vc_trvs_hdl) {
            fprintf(stderr, "Failed to create a traverse handle(%u)\n", 
                    max_var_idcode);
            exit(FSDB_RC_OBJECT_CREATION_FAILED);
        }
        
        // var basic information: byte_count and view range check
        uint_T byte_count;
        byte_count = vc_trvs_hdl->ffrGetByteCount();
        fprintf(stderr, "signal (%d) byte count is : %d, ", idcode, byte_count);
        if(TRUE == vc_trvs_hdl->ffrGetVwIsSet())
            fprintf(stderr, "and has set view window.\n");
        else
            fprintf(stderr, "and has no set view window.\n");

	vc_trvs_hdl->ffrGetMinXTag((void*)&time);
	fprintf(stderr, "signal with idcode %d: The first vc happens at time: %u %u\n",
		idcode, time.H, time.L);

 	if (FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoXTag((void*)&time)) {

	    for (idx = 1; ; idx++) {

		if (FSDB_RC_SUCCESS != vc_trvs_hdl->ffrGotoNextVC())
		    break;
	    }

	    fprintf(stderr, "signal with idcode %d has %u value changes.\n",
		    idcode, idx);
	}

	vc_trvs_hdl->ffrFree();
    }

    return 0;
}

static bool_T
MyTreeCB(fsdbTreeCBType type, void *client_data, void *cb_data)
{
    return TRUE;

    switch (type) {
    case FSDB_TREE_CBT_BEGIN_TREE:
	fprintf(stdout, "BeginTree\n");
	break;
    case FSDB_TREE_CBT_SCOPE:
	DumpScope((fsdbScopeRec*) cb_data);
	break;
    case FSDB_TREE_CBT_VAR: {
	    fsdbVarRec *var = (fsdbVarRec*) cb_data;
	    DumpVar(var);
	}
	break;
    case FSDB_TREE_CBT_UPSCOPE:
	fprintf(stdout, "Upscope.\n\n");
	break;
    case FSDB_TREE_CBT_END_TREE:
	fprintf(stdout, "EndTree\n\n");
	break;
    case FSDB_TREE_CBT_FILE_TYPE:
	break;
    case FSDB_TREE_CBT_SIMULATOR_VERSION:
	break;
    case FSDB_TREE_CBT_SIMULATION_DATE:
	break;
    case FSDB_TREE_CBT_X_AXIS_SCALE:
	break;
    case FSDB_TREE_CBT_END_ALL_TREE:
	break;

    case FSDB_TREE_CBT_DT_ENUM: {
	fsdbDTEnumRec *pEnum = (fsdbDTEnumRec*) cb_data;
	fprintf(stdout, "\t Enum data type: idcode     = %d\n", pEnum->idcode);
	fprintf(stdout, "\t Enum data type: numLiteral = %d\n", pEnum->numLiteral);
	for(int i = 0; i < pEnum->numLiteral; i++)  
	    fprintf(stdout, "\t arrLiteral[%d] = %s\n", i, pEnum->arrLiteral[i]);

	fprintf(stdout, "\t End of Enum data type.\n");
	}
  	break; 

    case FSDB_TREE_CBT_DT_INT: {
	fsdbDTIntRec *pInt = (fsdbDTIntRec*) cb_data;
	fprintf(stdout, "\t Int data type: idcode     = %d\n", pInt->idcode);
	fprintf(stdout, "\t Int data type: leftBound  = %d\n", pInt->leftBound);
	fprintf(stdout, "\t Int data type: rightBound = %d\n", pInt->rightBound);
	fprintf(stdout, "\t End of Int data type.\n");
   	}
  	break; 

    case FSDB_TREE_CBT_DT_FLOAT: {
	fsdbDTFloatRec *pFloat = (fsdbDTFloatRec*) cb_data;
	fprintf(stdout, "\t Float data type: idcode     = %d\n", pFloat->idcode);
	fprintf(stdout, "\t Float data type: leftBound  = %e\n", pFloat->leftBound);
	fprintf(stdout, "\t Float data type: rightBound = %e\n", pFloat->rightBound);
	fprintf(stdout, "\t End of Float data type.\n");
	}
  	break; 

    case FSDB_TREE_CBT_DT_PHYSICAL_BEGIN: {
	fsdbDTPhysicalRec *pPhysical = (fsdbDTPhysicalRec*) cb_data;
	fprintf(stdout, "\t Physical data type: idcode     = %d\n", pPhysical->idcode);
	fprintf(stdout, "\t Physical data type: leftBound  = %e\n", pPhysical->leftBound);
	fprintf(stdout, "\t Physical data type: rightBound = %e\n", pPhysical->rightBound);
	fprintf(stdout, "\t Physical data type: baseUnit   = %s\n\n", pPhysical->baseUnit);
	}
  	break; 

    case FSDB_TREE_CBT_DT_PHYSICAL_SECONDARY_UNIT: {
	fsdbDTPhysicalSecondaryUnitRec *pPhysicalSecondaryUnit = 
	    (fsdbDTPhysicalSecondaryUnitRec*) cb_data;
	fprintf(stdout, "\t PhysicalSecondaryUnit data type: multiple   = %e\n", 
	    pPhysicalSecondaryUnit->multiple);
	fprintf(stdout, "\t PhysicalSecondaryUnit data type: identifier = %s\n\n", 
	    pPhysicalSecondaryUnit->identifier);
	}
  	break; 

    case FSDB_TREE_CBT_DT_PHYSICAL_END:
	fprintf(stdout, "\t End of Physical data type.\n");
  	break; 

    case FSDB_TREE_CBT_ARRAY_BEGIN:
	fprintf(stdout, "\t Array Begin\n");
  	break; 

    case FSDB_TREE_CBT_ARRAY_END:
	fprintf(stdout, "\t Array End\n\n");
  	break; 

    case FSDB_TREE_CBT_RECORD_BEGIN:
	fprintf(stdout, "\t Record Begin(Record Name: %s)\n", (str_T)cb_data);
  	break; 

    case FSDB_TREE_CBT_RECORD_END:
	fprintf(stdout, "\t Record End.\n\n");
   
 	break;

    default:
	fprintf(stdout, "MyTreeCB(): unkonw object type.\n");
	break;
    }

    return TRUE;
}

static void 
DumpScope(fsdbScopeRec* scope)
{
    str_T type;
    switch (scope->type) {
    case FSDB_ST_VCD_MODULE:
	type = "module"; 
	break;
    case FSDB_ST_VCD_TASK:
	type = "task"; 
	break;
    case FSDB_ST_VCD_FUNCTION:
	type = "function"; 
	break;
    case FSDB_ST_VCD_BEGIN:
	type = "begin"; 
	break;
    case FSDB_ST_VCD_FORK:
	type = "fork"; 
	break;
    case FSDB_ST_VHDL_ARCHITECTURE:
	type = "architecture"; 
	break;
    case FSDB_ST_VHDL_PROCEDURE:
	type = "procedure"; 
	break;
    case FSDB_ST_VHDL_FUNCTION:
	type = "function"; 
	break;
    default:
	// unknown scope type or not vcd scope type
	type = NULL;
	break;
    }
  
    if(NULL != type) 
        fprintf(stdout, "ScopeName(Type): %s (%s)\n", scope->name, type);
    else
	fprintf(stdout, "ScopeName(Type): %s (%d)\n", scope->name, scope->type);

}

static void 
DumpVar(fsdbVarRec *var_id)
{
    str_T type;
    str_T bytes_per_bit;

    switch(var_id->bytes_per_bit) {
    case FSDB_BYTES_PER_BIT_1B:
	bytes_per_bit = "FSDB_BYTES_PER_BIT_1B";
	break;
    case FSDB_BYTES_PER_BIT_2B:
	bytes_per_bit = "FSDB_BYTES_PER_BIT_2B";
	break;
    case FSDB_BYTES_PER_BIT_4B:
	bytes_per_bit = "FSDB_BYTES_PER_BIT_4B";
	break;
    case FSDB_BYTES_PER_BIT_8B:
	bytes_per_bit = "FSDB_BYTES_PER_BIT_8B";
	break;
    default:
	fprintf(stdout, "DumpVar(): invalid var_id->bytes_per_bit(%d).\n", var_id->bytes_per_bit);
	return;
    }

    switch (var_id->type) {
    case FSDB_VT_VCD_EVENT:
	type = "event"; 
  	break;
    case FSDB_VT_VCD_INTEGER:
	type = "integer"; 
	break;
    case FSDB_VT_VCD_PARAMETER:
	type = "parameter"; 
	break;
    case FSDB_VT_VCD_REAL:
	type = "real"; 
	break;
    case FSDB_VT_VCD_REG:
	type = "reg"; 
	break;
    case FSDB_VT_VCD_SUPPLY0:
	type = "supply0"; 
	break;
    case FSDB_VT_VCD_SUPPLY1:
	type = "supply1"; 
	break;
    case FSDB_VT_VCD_TIME:
	type = "time";
	break;
    case FSDB_VT_VCD_TRI:
	type = "tri";
	break;
    case FSDB_VT_VCD_TRIAND:
	type = "triand";
	break;
    case FSDB_VT_VCD_TRIOR:
	type = "trior";
	break;
    case FSDB_VT_VCD_TRIREG:
	type = "trireg";
	break;
    case FSDB_VT_VCD_TRI0:
	type = "tri0";
	break;
    case FSDB_VT_VCD_TRI1:
	type = "tri1";
	break;
    case FSDB_VT_VCD_WAND:
	type = "wand";
	break;
    case FSDB_VT_VCD_WIRE:
	type = "wire";
	break;
    case FSDB_VT_VCD_WOR:
	type = "wor";
	break;
    case FSDB_VT_VHDL_SIGNAL:
	type = "signal";
	break;
    case FSDB_VT_VHDL_VARIABLE:
	type = "variable";
	break;
    case FSDB_VT_VHDL_CONSTANT:
	type = "constant";
	break;
    case FSDB_VT_VHDL_FILE:
	type = "file";
	break;
    default:
	// unknown var type or not vcd var type
	type = NULL;
	break;
    }

    if(NULL != type)
        printf("VarIdcode VarName VarType VarDTIdcode VarBytesPerBit: %d %s[%d:%d] %s %d %s\n",
	    	var_id->u.idcode, var_id->name, var_id->lbitnum, var_id->rbitnum, type, 
	    	var_id->dtidcode, bytes_per_bit);
    else
        printf("VarIdcode VarName VarType VarDTIdcode VarBytesPerBit: %d %s[%d:%d] %d %d %s\n",
	    	var_id->u.idcode, var_id->name, var_id->lbitnum, var_id->rbitnum, var_id->type, 
		var_id->dtidcode, bytes_per_bit);
	
}
