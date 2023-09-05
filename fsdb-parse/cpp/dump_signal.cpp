/* *****************************************************************************
// [read_verilog.cpp]
//
//  Copyright 1999-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// Program Name	: print_header.cpp
//
// Purpose	: Demonstrate how to call fsdb reader APIs to access 
//		  the value changes of verilog type fsdb.
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
#include <string.h>
#include <cstdlib>

#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE	1
#endif



// static bool_T fwrite_str(FILE *file, char* str) {
//     fwrite(str, sizeof(str), 1, fptr);
//     return TRUE;
// }


//
// The tree callback function, it's used to traverse the design 
// hierarchies. 
//
static bool_T __MyTreeCB(fsdbTreeCBType cb_type, 
			 void *client_data, void *tree_cb_data);


static void 
__PrintTimeValChng(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr, FILE *fptr);

//
// dump scope definition
//
static bool_T 
__DumpScope(fsdbTreeCBDataScope *scope);


//
// dump var definition 
// 
static void 
__DumpVar(fsdbTreeCBDataVar *var);


int 
main(int argc, char *argv[])
{

    if (3 != argc) {
	fprintf(stderr, "usage: ./ds verilog_type_fsdb signal_idcode\n");
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

    ffrObject *fsdb_obj =
	ffrObject::ffrOpen3(argv[1]);
    if (NULL == fsdb_obj) {
	fprintf(stderr, "ffrObject::ffrOpen() failed.\n");
	exit(FSDB_RC_OBJECT_CREATION_FAILED);
    }
    // fsdb_obj->ffrSetTreeCBFunc(__MyTreeCB, NULL);

    // core.io_imem_resp_bits_data[31:0] 213650
    // ex_reg_inst[31:0]  307318
    // mem_reg_inst[31:0]  307337
    // wb_reg_inst[31:0]  307354
    // wb_reg_raw_inst
    int signal_idcode = atoi(argv[2]);

    // char *output_file = strcat("bin/",strcat(strtok(basename(argv[1]), "."),".bin"));
    char *output_file = strcat(strtok(basename(argv[1]), "."),".txt");
    char output_path[50]; strcpy(output_path,"./out/inst_stream/");
    strcat(output_path, output_file);
    FILE *fptr;
    fptr = fopen(output_path, "w");
    if (fptr == NULL) {
        fprintf(stderr, "ERROR: File does not exist, %s", output_path);
        return 0;
    }


    if (FSDB_FT_VERILOG != fsdb_obj->ffrGetFileType()) {
	fprintf(stderr, 
		"%s is not verilog type fsdb, just return.\n", argv[1]);
	fsdb_obj->ffrClose();
	return FSDB_RC_SUCCESS;
    }

    fsdb_obj->ffrReadScopeVarTree();

    
    fsdb_obj->ffrAddToSignalList(signal_idcode);

    fsdb_obj->ffrLoadSignals();

    ffrVCTrvsHdl vc_trvs_hdl;

    fsdbTag64 time;
    int	      glitch_num;
    byte_T    *vc_ptr;
    vc_trvs_hdl = fsdb_obj->ffrCreateVCTraverseHandle(signal_idcode);
        if (NULL == vc_trvs_hdl) {
        fprintf(stderr, "Failed to create a traverse handle(%u)\n", 
            signal_idcode);
        exit(FSDB_RC_OBJECT_CREATION_FAILED);
        }
    vc_trvs_hdl->ffrGetMinXTag((void*)&time);
    vc_trvs_hdl->ffrGotoXTag((void*)&time);

    do {
        vc_trvs_hdl->ffrGetXTag(&time);
        vc_trvs_hdl->ffrGetVC(&vc_ptr);
        __PrintTimeValChng(vc_trvs_hdl, &time, vc_ptr, fptr);
    } while(FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoNextVC());
    vc_trvs_hdl->ffrFree();
    fclose(fptr);

    fsdb_obj->ffrResetSignalList();

    fsdb_obj->ffrUnloadSignals();


    fsdb_obj->ffrClose();
    return 0;
}

static bool_T __MyTreeCB(fsdbTreeCBType cb_type, 
			 void *client_data, void *tree_cb_data)
{

    switch (cb_type) {
    case FSDB_TREE_CBT_BEGIN_TREE:
	fprintf(stdout, "<BeginTree>\n");
	break;

    case FSDB_TREE_CBT_SCOPE:
    __DumpScope((fsdbTreeCBDataScope*)tree_cb_data);
	break;

    case FSDB_TREE_CBT_VAR:
	__DumpVar((fsdbTreeCBDataVar*)tree_cb_data);
	break;

    case FSDB_TREE_CBT_UPSCOPE:
	fprintf(stdout, "<Upscope>\n");
	break;

    case FSDB_TREE_CBT_END_TREE:
	fprintf(stdout, "<EndTree>");
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

    case FSDB_TREE_CBT_ARRAY_BEGIN:
        fprintf(stdout, "<BeginArray>\n");
        break;
        
    case FSDB_TREE_CBT_ARRAY_END:
        fprintf(stdout, "<EndArray>\n");
        break;

    case FSDB_TREE_CBT_RECORD_BEGIN:
        fprintf(stdout, "<BeginRecord>\n");
        break;
        
    case FSDB_TREE_CBT_RECORD_END:
        fprintf(stdout, "<EndRecord>\n");
        break;
             
    default:
	return FALSE;
    }

    return TRUE;
}

static bool_T 
__DumpScope(fsdbTreeCBDataScope* scope)
{
    str_T type;

    switch (scope->type) {
    case FSDB_ST_VCD_MODULE:
	type = (str_T) "module"; 
	break;

    case FSDB_ST_VCD_TASK:
	type = (str_T) "task"; 
	break;

    case FSDB_ST_VCD_FUNCTION:
	type = (str_T) "function"; 
	break;

    case FSDB_ST_VCD_BEGIN:
	type = (str_T) "begin"; 
	break;

    case FSDB_ST_VCD_FORK:
	type = (str_T) "fork"; 
	break;

    default:
	type = (str_T) "unknown_scope_type";
	break;
    }

    fprintf(stdout, "<Scope> name:%s  type:%s\n", 
	    scope->name, type);
    
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
	bpb = (str_T) "XB";
	break;
    }

    switch (var->type) {
    case FSDB_VT_VCD_EVENT:
	type = (str_T) "event"; 
  	break;

    case FSDB_VT_VCD_INTEGER:
	type = (str_T) "integer"; 
	break;

    case FSDB_VT_VCD_PARAMETER:
	type = (str_T) "parameter"; 
	break;

    case FSDB_VT_VCD_REAL:
	type = (str_T) "real"; 
	break;

    case FSDB_VT_VCD_REG:
	type = (str_T) "reg"; 
	break;

    case FSDB_VT_VCD_SUPPLY0:
	type = (str_T) "supply0"; 
	break;

    case FSDB_VT_VCD_SUPPLY1:
	type = (str_T) "supply1"; 
	break;

    case FSDB_VT_VCD_TIME:
	type = (str_T) "time";
	break;

    case FSDB_VT_VCD_TRI:
	type = (str_T) "tri";
	break;

    case FSDB_VT_VCD_TRIAND:
	type = (str_T) "triand";
	break;

    case FSDB_VT_VCD_TRIOR:
	type = (str_T) "trior";
	break;

    case FSDB_VT_VCD_TRIREG:
	type = (str_T) "trireg";
	break;

    case FSDB_VT_VCD_TRI0:
	type = (str_T) "tri0";
	break;

    case FSDB_VT_VCD_TRI1:
	type = (str_T) "tri1";
	break;

    case FSDB_VT_VCD_WAND:
	type = (str_T) "wand";
	break;

    case FSDB_VT_VCD_WIRE:
	type = (str_T) "wire";
	break;

    case FSDB_VT_VCD_WOR:
	type = (str_T) "wor";
	break;

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

    case FSDB_VT_VCD_MEMORY:
        type = (str_T) "vcd_memory";
        break;

    case FSDB_VT_VHDL_MEMORY:
        type = (str_T) "vhdl_memory";
        break;
        
    case FSDB_VT_VCD_MEMORY_DEPTH:
        type = (str_T) "vcd_memory_depth_or_range";
        break;
        
    case FSDB_VT_VHDL_MEMORY_DEPTH:         
        type = (str_T) "vhdl_memory_depth";
        break;

    default:
	type = (str_T) "unknown_var_type";
	break;
    }

    fprintf(stdout,
	"<Var>  %s  %u\n",
	var->name, var->u.idcode);
}


static void 
__PrintTimeValChng(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr, FILE *fptr)
{ 
    static byte_T buffer[FSDB_MAX_BIT_SIZE+1];
    byte_T        *ret_vc;
    uint_T        i;
    fsdbVarType   var_type; 
    
    switch (vc_trvs_hdl->ffrGetBytesPerBit()) {
    case FSDB_BYTES_PER_BIT_1B:

	//
 	// Convert each verilog bit type to corresponding
 	// character.
 	//
        for (i = 0; i < vc_trvs_hdl->ffrGetBitSize(); i++) {
	    switch(vc_ptr[i]) {
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
		//
		// unknown verilog bit type found.
 		//
	        buffer[i] = 'u';
	    }
        }
        buffer[i] = '\0';
	fprintf(fptr, "%u %s\n",
		time->L, buffer);	
	break;

    case FSDB_BYTES_PER_BIT_4B:
	//
	// Not 0, 1, x, z since their bytes per bit is
	// FSDB_BYTES_PER_BIT_1B. 
 	//
	// For verilog type fsdb, there is no array of 
  	// real/float/double so far, so we don't have to
	// care about that kind of case.
	//

        //
        // The var type of memory range variable is
        // FSDB_VT_VCD_MEMORY_DEPTH. This kind of var
        // has two value changes at certain time step.
        // The first value change is the index of the 
        // beginning memory variable which has a value change
        // and the second is the index of the end memory variable
        // which has a value change at this time step. The index
        // is stored as an unsigned integer and its bpb is 4B.
        //
        
        var_type = vc_trvs_hdl->ffrGetVarType();
        switch(var_type){
        case FSDB_VT_VCD_MEMORY_DEPTH:
        case FSDB_VT_VHDL_MEMORY_DEPTH:
	    fprintf(fptr, "time: (%u %u)", time->H, time->L);
            fprintf(fptr, "  begin: %d", *((int*)vc_ptr));
            vc_ptr = vc_ptr + sizeof(uint_T);
            fprintf(fptr, "  end: %d\n", *((int*)vc_ptr));  
            break;
               
        default:    
            vc_trvs_hdl->ffrGetVC(&vc_ptr);
	    fprintf(fptr, "%u %f\n",
                    time->L, *((float*)vc_ptr));
	    break;
        }
        break;

    case FSDB_BYTES_PER_BIT_8B:
	//
	// Not 0, 1, x, z since their bytes per bit is
	// FSDB_BYTES_PER_BIT_1B. 
 	//
	// For verilog type fsdb, there is no array of 
  	// real/float/double so far, so we don't have to
	// care about that kind of case.
	//
	fprintf(fptr, "%u %e\n",
		time->L, *((double*)vc_ptr));
	break;

    default:
	fprintf(fptr, "Control flow should not reach here.\n");
	break;
    }
}