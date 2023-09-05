/* *****************************************************************************
// [test_file_info.cpp]
//
//  Copyright 2009-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// Program Name	: test_file_info.cpp
//
// Purpose	: test all APIs about file information
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
DumpVar(ffrVarInfo *var_info);

static void
DumpVarRec(fsdbVarRec *var);

static void
__PrintSupportInfo(fsdbSupInfo supinfo);

int 
main(int argc, char *argv[])
{
    if (2 != argc) { 
        fprintf(stderr, "usage: test file information\n");
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
    fsdbRC rc;
 
    ffrObject::ffrGetFSDBInfo(argv[1], fsdb_info);
    if (FSDB_FT_VERILOG != fsdb_info.file_type) {
        fprintf(stderr, "file type is not verilog.\n");
        return FSDB_RC_FAILURE;
    }
    
    rc = ffrObject::ffrCheckFile(argv[1]);
    switch (rc) {
    case FSDB_RC_FILE_DOES_NOT_EXIST:
        fprintf(stderr, "This file(%s) does not exist.\n", argv[1]);
        return FSDB_RC_FAILURE;
        
    case FSDB_RC_FILE_IS_NOT_READABLE:
        fprintf(stderr, "This file(%s) is not readable.\n", argv[1]);
        return FSDB_RC_FAILURE;
    
    case FSDB_RC_FILE_IS_A_DIRECTORY:
        fprintf(stderr, "This file(%s) is a directory.\n", argv[1]);
        return FSDB_RC_FAILURE;
        
    default:
        break;
    }

    rc = ffrObject::ffrCheckFSDB(argv[1]);
    switch (rc) {
    case FSDB_RC_FILE_IS_EMPTY:
        fprintf(stderr, "This file(%s) is empty.\n", argv[1]);
        return FSDB_RC_FAILURE;

    case FSDB_RC_FAILURE:
        fprintf(stderr, "This file(%s) is too large to open.\n", argv[1]);
        return FSDB_RC_FAILURE;

    default:
        break;
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
    ffrObject *fsdb_obj = ffrObject::ffrOpen3(argv[1]);
    if (NULL == fsdb_obj) {
	printf("ffrObject::ffrOpen() failed.\n");
	exit(FSDB_RC_FAILURE);
    }
    fsdb_obj->ffrSetTreeCBFunc(MyTreeCB, NULL);
    
    //
    // to know the file has sequence number var
    //
    if(TRUE == fsdb_obj->ffrIsThereSeqNum())
        fprintf(stderr, "File has Sequnece Number.\n");
    else
        fprintf(stderr, "File has no Sequnece Number.\n");

    //
    // test view window
    //
    
    fsdbXTag start_xtag, close_xtag;
    start_xtag.hltag.H = 0;
    start_xtag.hltag.L = 2050;
    close_xtag.hltag.H = 0;
    close_xtag.hltag.L = 4050;

    fsdb_obj->ffrSetViewWindow(&start_xtag, &close_xtag);
    if(FALSE == fsdb_obj->ffrIsViewWindowSet()) {
        fprintf(stderr, "Set View Window Error!\n");
        return FSDB_RC_FAILURE;
    }
    fprintf(stderr, " Current view window (0 %u) - (0 %u).\n",
         start_xtag.hltag.L, close_xtag.hltag.L);

   fsdb_obj->ffrSetViewWindow(&(fsdb_info.min_xtag), &(fsdb_info.max_xtag));
    if(FALSE == fsdb_obj->ffrIsViewWindowSet()) {
        fprintf(stderr, "Set View Window Error!\n");
        return FSDB_RC_FAILURE;
    }
   fprintf(stderr, " after reset, current view window (0 %u) - (0 %u).\n",
         start_xtag.hltag.L, close_xtag.hltag.L);

   str_T file_info;
   //
   // get file information
   //
   fprintf(stderr, "File Detail Information:\n");
   file_info = fsdb_obj->ffrGetScaleUnit();
   fprintf(stderr, "\tScale Unit is:\t%s\n", file_info);

   file_info = fsdb_obj->ffrGetScopeSeparator();
   fprintf(stderr, "\tScopeSeparator: \t%s\n", file_info);

   file_info = fsdb_obj->ffrGetSimVersion();
   fprintf(stderr, "\tSimulation Version: \t%s\n", file_info);

   file_info = fsdb_obj->ffrGetSimDate();
   fprintf(stderr, "\tSimulation Date: \t%s\n", file_info);

   file_info = fsdb_obj->ffrGetFileSimType();
   fprintf(stderr, "\tSimulation Type: \t%s\n", file_info);
   
   file_info = fsdb_obj->ffrGetFileDumperType();
   fprintf(stderr, "\tDumper Type: \t\t%s\n", file_info);
   
   uint_T length;
   file_info = fsdb_obj->ffrGetAnnotation(length);
   fprintf(stderr, "\tAnnotation length: \t%d\n", length);
   fprintf(stderr, "\tAnnotation: \t%s\n", file_info);


   //
   // session information
   //
   ffrSessionInfo *session_info;
   fprintf(stderr, "File Session Information:\n");
   do {
        session_info = fsdb_obj->ffrGetSessionListInfo();
        fprintf(stderr, "\tSession range (%u %u) - (%u %u).\n",
                session_info->start_xtag.hltag.H, session_info->start_xtag.hltag.L, 
                session_info->close_xtag.hltag.H, session_info->close_xtag.hltag.L);
        session_info = session_info->next;
   }while(session_info != NULL);

    //
    // support information
    //
    if(FALSE == fsdb_obj->ffrHasSupportInfo()) {
        fprintf(stderr, "this file has no support information\n");
    }
    else {
        fsdbSupInfo sup_info;
        rc = fsdb_obj->ffrGetSupportInfo(sup_info);
        __PrintSupportInfo(sup_info);
    } 
   
    //
    // test file has loop marker varialbe or not
    //
   uint_T loop_marker_count;
   fsdbVarIdcode *idcode_array;
    if (FALSE == fsdb_obj->ffrHasLoopMarkerVar()) {
        fprintf(stderr, "the file has no loop marker variable\n");
    }
    else {
        fsdb_obj->ffrGetLoopMarkerArray(loop_marker_count, idcode_array);
        fprintf(stderr, "the file has %d loop marker variables\n", loop_marker_count);
    }

    //
    // test fle has trigger point varialbe or not
    //
    fsdbVarIdcode idcode;
    if (fsdb_obj->ffrHasTriggerPointVar()) {
        if (FSDB_RC_SUCCESS == fsdb_obj->ffrGetTriggerPointVarIdcode(idcode) &&
            FSDB_INVALID_VAR_IDCODE != idcode) {
            fprintf(stderr,"Trigger point var idcode = %u.\n", idcode);
        }       
        else {  
            fprintf(stderr, "Fail to locate trigger point var idcode!\n");
            return FSDB_RC_FAILURE;
        }       
    }
    else {  
        fprintf(stderr, "This FSDB file has no trigger point variables.\n");
        return FSDB_RC_FAILURE;
    }

    //
    // test file has dumpoff range or not
    //
    uint_T count;
    fsdbDumpOffRange *range;
    uint_T idx;
    fsdb_obj->ffrGetDumpOffRange(count, range);
    if(count == 0) {
        fprintf(stderr, "the file has no DumpOff\n");
    }
    else {
       for (idx = 0; idx < count; idx++) {
           fprintf(stderr, "dump off range %u: begin (%u %u), end (%u %u)\n",
                    idx + 1,
                    (range + idx)->begin.hltag.H, (range + idx)->begin.hltag.L,
                    (range + idx)->end.hltag.H, (range + idx)->end.hltag.L);
       }
    }

    // check how many unique signals are there in this fsdb file.
    //
    fsdbVarIdcode max_var_infocode = fsdb_obj->ffrGetMaxVarIdcode();
      
    fsdb_obj->ffrLoadSignals();

    fsdbTag64  	 time;
    ffrVCTrvsHdl vc_trvs_hdl;
    ffrVarInfo *var_info;
    int *glitch_num = NULL;
    
    for (idcode = FSDB_MIN_VAR_IDCODE; idcode < max_var_infocode; idcode++) {
        //
        // var basic information: check varinfo without vc_trvs_hdl
        //
        rc = fsdb_obj->ffrGetVarInfoByVarIdcode(idcode, &var_info);
        if(FSDB_RC_FAILURE == rc) {
            fprintf(stderr, "Failed to read var(%d) dtail information\n", idcode);
            return rc;
        }
        DumpVar(var_info);

        //
        // get glitch number
        //
        if(TRUE == fsdb_obj->ffrHasGlitch(idcode)) {
            vc_trvs_hdl = fsdb_obj->ffrCreateVCTrvsHdl(idcode);
            vc_trvs_hdl->ffrGetGlitchNum(glitch_num);
            fprintf(stderr, "Var(%d) has glitch, glitch number is %d\n", idcode, *glitch_num);

        }
        else
            fprintf(stderr, "Var(%d) has no glitch\n", idcode);
        

        ffrVCTrvsHdl vc_trvs_hdl = fsdb_obj->ffrCreateVCTrvsHdl(idcode); 
        if (NULL == vc_trvs_hdl) {
            fprintf(stderr, "Failed to create a traverse handle(%u)\n", 
                    max_var_infocode);
            exit(FSDB_RC_OBJECT_CREATION_FAILED);
        }
        
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
	    DumpVarRec(var);
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
DumpVar(ffrVarInfo *var_info)
{
    str_T type;
    str_T bytes_per_bit;

    switch(var_info->bpb) {
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
	fprintf(stdout, "DumpVar(): invalid var_info->bytes_per_bit(%d).\n", var_info->bpb);
	return;
    }

    switch (var_info->type) {
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
        printf("VarType  VarBytesPerBit: [%d:%d] %s %s\n",
	    	var_info->lbitnum, var_info->rbitnum, type, bytes_per_bit);
    else
        printf("VarType VarBytesPerBit: [%d:%d] %d %s\n",
	    	var_info->lbitnum, var_info->rbitnum, var_info->type, bytes_per_bit);
}
static void 
DumpVarRec(fsdbVarRec *var_id)
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
	fprintf(stdout, "DumpVarRec(): invalid var_id->bytes_per_bit(%d).\n", var_id->bytes_per_bit);
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

static void
__PrintSupportInfo(fsdbSupInfo supinfo)
{
    //
    // Dump collected suppport info to stdout
    //

    fprintf(stdout, "producer       = %s\n", supinfo.producer);
    fprintf(stdout, "user_defined   = %s\n", supinfo.user_defined);
    fprintf(stderr, "Environment variable = %s.\n ", supinfo.env_var);
}
