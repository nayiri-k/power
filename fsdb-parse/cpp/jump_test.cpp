/* *****************************************************************************
// [jump_test.cpp]
//
//  Copyright 1999-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// Program Name	: jump_test.cpp
//
// Purpose	: testing the jumping algorithm of pattern fsdb 
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

//
// convert the value change to the bit type of vcd.
//
static char*
ToVcdBitType(ushort_T bit_sz, fsdbBytesPerBit bytes_per_bit, byte_T *vc_ptr);

int 
main(int argc, char *argv[])
{
    if (2 >= argc) {
        printf("usage: jump_test verilog_type_fsdb idcode1 idcode2 ......\n");
	return FSDB_RC_FAILURE;
    }

    // 
    // get the fsdb reader APIs ready
    //
 
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

    //  
    // Open the file. Note that we can register a tree callback function with ffrOpen(),
    // if this file can be opened successfully, then the tree callback function will be called
    // in ffrOpen(). 
    //
    ffrObject *fsdbObj = ffrObject::ffrOpen3(argv[1]);
    if(NULL == fsdbObj) {
	printf("ffrObject::ffrOpen() failed.\n");
	exit(-1);
    }
    fsdbObj->ffrSetTreeCBFunc(MyTreeCB, NULL);

    //
    // check how many unique signals are there in this fsdb file.
    //
    fsdbVarIdcode max_var_idcode = fsdbObj->ffrGetMaxVarIdcode();
    int i, j;
    fsdbVarIdcode idcode;

    //
    // In order to read the value changes of signals, we have to add the idcode
    // of interested signal into signal list.
    // 
    // Note that the var idcode begins from FSDB_MIN_VAR_IDCODE whose value is
    // defined in fsdbShr.h
    //
    ffrVCTraverseHandle vc_traverse_handle;
    fsdbTag64  	time64;
    int		glitch_num;
    byte_T    	*vc_ptr;


    fsdbTag64 jump[NUM_JUMP_TEST];

    jump[0].H = 0;
    jump[0].L = 0;
   
    jump[1].H = 0;
    jump[1].L = UINT_MAX;

    jump[2].H = 0;
    jump[2].L = UCHAR_MAX;

    jump[3].H = UCHAR_MAX;
    jump[3].L = UINT_MAX;

    jump[4].H = 0;
    jump[4].L = SHRT_MAX;

    jump[5].H = 0;
    jump[5].L = USHRT_MAX;

    fsdbObj->ffrGetMaxFsdbTag64(&time64);
    time64.L >>= 5; 	// time64.L = time64.L / 32
     
    for (i = 6; i < NUM_JUMP_TEST; i++) {
	jump[i].H = 0;
	jump[i].L = time64.L * (i - 5); 
    }

    for (i = 2; i < argc; i++) {
	idcode = atoi(argv[i]);
 	fsdbObj->ffrAddToSignalList(idcode);	
        fsdbObj->ffrLoadSignals();

        vc_traverse_handle = fsdbObj->ffrCreateVCTraverseHandle(idcode); 
        if(NULL == vc_traverse_handle) {
	    printf("Failed to create a traverse handle for the signal whose idcode is %u", 
		   idcode);
	    exit(-1);
        }
  	printf("\n------ value change of the var whose idcode is (%u) ------\n", idcode); 

        time64.H = 0;
        time64.L = 0;

        if(FSDB_RC_SUCCESS != vc_traverse_handle->ffrGotoXTag((void*)&time64))  
		printf("This traverse handle(idcode = %u) has no value change at time64 (%u %u).\n", 
	       		idcode, time64.H, time64.L);

        vc_traverse_handle->ffrGetMinXTag((void*)&time64);
    	printf("Minimum xtag of this traverse handle is (%u %u).\n", time64.H, time64.L);

        vc_traverse_handle->ffrGetMaxXTag((void*)&time64);
        printf("Maximum xtag of this traverse handle is (%u %u).\n", time64.H, time64.L);

        //
        // get the x tag at current traverse point 
        //
        vc_traverse_handle->ffrGetXTag((void*)&time64);

        //
        // get the glitch number at current traverse point 
        //
        vc_traverse_handle->ffrGetGlitchNum(&glitch_num);
        printf("current pointer: time64 = (%u %u)  glitch_num = %u.\n", 
		time64.H, time64.L, glitch_num);

        //
        // get the value change at current traverse point
        //
        if(FSDB_RC_SUCCESS == vc_traverse_handle->ffrGetVC(&vc_ptr)) {
	    vc_traverse_handle->ffrGetXTag(&time64);
	    printf("time64 = (%u %u)  VC = %s.\n", time64.H, time64.L, 
	       	    ToVcdBitType(vc_traverse_handle->ffrGetBitSize(), 
			         vc_traverse_handle->ffrGetBytesPerBit(), vc_ptr));
        }

        //
        // move traverse point to the next value change and then print out the value change
        // repeatly till there is no more value change.
        // 
        while(FSDB_RC_SUCCESS == vc_traverse_handle->ffrGotoNextVC()) {
	    vc_traverse_handle->ffrGetXTag(&time64);
 	    vc_traverse_handle->ffrGetVC(&vc_ptr);
	    printf("time64 = (%u %u)  VC = %s.\n", time64.H, time64.L, 
	  	    ToVcdBitType(vc_traverse_handle->ffrGetBitSize(), 
			         vc_traverse_handle->ffrGetBytesPerBit(), vc_ptr));	
        }
	
        //
        // move traverse point to the previous value change and then print out the value change
        // repeatly till there is no more value change.
        // 
        while(FSDB_RC_SUCCESS == vc_traverse_handle->ffrGotoPrevVC()) {
	    vc_traverse_handle->ffrGetXTag(&time64);
 	    vc_traverse_handle->ffrGetVC(&vc_ptr);
	    printf("time64 = (%u %u)  VC = %s.\n", time64.H, time64.L, 
		    ToVcdBitType(vc_traverse_handle->ffrGetBitSize(), 
			     vc_traverse_handle->ffrGetBytesPerBit(), vc_ptr));
        }

	for (j = 0; j < NUM_JUMP_TEST; j++) {
            if (FSDB_RC_SUCCESS != vc_traverse_handle->ffrGotoXTag((void*)&jump[j]))  
		printf("This traverse handle(idcode = %u) has no value change at time64 (%u %u).\n", 
	       	       idcode, jump[j].H, jump[j].L);
	    else {
	        vc_traverse_handle->ffrGetXTag(&time64);
 	        vc_traverse_handle->ffrGetVC(&vc_ptr);
	 	printf("jump time = (%u %u) ", jump[j].H, jump[j].L);
	        printf("located time = (%u %u)  vc = %s.\n", time64.H, time64.L, 
		        ToVcdBitType(vc_traverse_handle->ffrGetBitSize(), 
			             vc_traverse_handle->ffrGetBytesPerBit(), vc_ptr));
	    }
  	}
        
        // 
        // get the sequens numbur.
        //
        fsdbSeqNum ret_seq_num;
        vc_traverse_handle->ffrGetSeqNum(&ret_seq_num);

        // 
        // free the data structures allocated for the traverse handle
        //
        vc_traverse_handle->ffrFree();

	fsdbObj->ffrResetSignalList();
    }

    return 0;
}

static char*
ToVcdBitType(ushort_T bit_sz, fsdbBytesPerBit bytes_per_bit, byte_T *vc_ptr)
{ 
    uint_T byteCount;

    switch(bytes_per_bit) {
    case FSDB_BYTES_PER_BIT_1B:
	byteCount = 1*bit_sz;
	break;
    case FSDB_BYTES_PER_BIT_2B:
	byteCount = 2*bit_sz;
	break;
    case FSDB_BYTES_PER_BIT_4B:
	byteCount = 4*bit_sz;
	break;
    case FSDB_BYTES_PER_BIT_8B:
	byteCount = 8*bit_sz;
	break;
    default:
	printf("unknown bytes_per_bit.\n");
	exit(-1);
    }


    static char buffer[FSDB_MAX_BIT_SIZE+1];
    int i;

    for(i = 0; i < byteCount; i++, vc_ptr++) {
	switch((int)*vc_ptr) {
 	case FSDB_BT_VCD_0:
	    buffer[i] = '0';
	    break;

	case FSDB_BT_VCD_1:
	    buffer[i] = '1';
	    break;

	case FSDB_BT_VCD_X:
	    buffer[i] = 'x';
	    break;

	case FSDB_BT_VCD_Z:
	    buffer[i] = 'z';
	    break;

	default:
	    printf("unknown bit type.\n");
	    exit(-1);
	}
    }
    buffer[byteCount] = '\0';

    return buffer;
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
