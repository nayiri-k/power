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
// Program Name	: read_verilog.cpp
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
#include <libgen.h>

#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE	1
#endif

// #define PRINT_CHANGE
#define PRINT_ARRAY

uint_T CLOCK_PERIOD = 1000;
uint_T N_CYCLES = 10;
uint_T td = CLOCK_PERIOD*N_CYCLES;
uint_T max_time_window = 0;
uint_T max_idx = 0;

uint_T BUFFER_SIZE = 100000;
float* toggle_buff = (float*) malloc(BUFFER_SIZE*sizeof(float));

//
// The tree callback function, it's used to traverse the design 
// hierarchies. 
//
static bool_T __MyTreeCB(fsdbTreeCBType cb_type, 
			 void *client_data, void *tree_cb_data);


//
// dump scope definition
//
static void 
__DumpScope(fsdbTreeCBDataScope *scope);


//
// dump var definition 
// 
static void 
__DumpVar(fsdbTreeCBDataVar *var);


static void 
__DumpVar_less(fsdbTreeCBDataVar *var);

static bool_T 
__ValChng(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr, 
           byte_T *prev_vc_ptr);

static void 
__PrintTimeValChng(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr);

int 
main(int argc, char *argv[])
{
    if (2 != argc) {
	fprintf(stderr, "usage: read_verilog verilog_type_fsdb\n");
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

    //
    // Open the fsdb file.
    //
    // From fsdb v2.0(Debussy 5.0), there are two APIs to open a 
    // fsdb file: ffrOpen() and ffrOpen2(). Both APIs take three 
    // parameters, the first one is the fsdb file name, the second 
    // one is a tree callback function written by application, the 
    // last one is the client data that application would like 
    // fsdb reader to pass it back in tree callback function.
    //
    // Open a fsdb file with ffrOpen(), the tree callback function
    // will be activated many times during open session; open a fsdb
    // file with ffrOpen2(), the tree callback function will not be
    // activated during open session, applicaiton has to call an API
    // called "ffrReadScopeVarTree()" to activate the tree callback
    // function. 
    // 
    // In tree callback function, application can tell what the
    // callback data is, based on the callback type. For example, if 
    // the callback type is scope(FFR_TREE_CBT_SCOPE), then 
    // applicaiton knows that it has to perform (fsdbTreeCBDataScope*) 
    // type case on the callback data so that it can read the scope 
    // defition.
    //
    ffrObject *fsdb_obj =
	ffrObject::ffrOpen3(argv[1]);
    if (NULL == fsdb_obj) {
	fprintf(stderr, "ffrObject::ffrOpen() failed.\n");
	exit(FSDB_RC_OBJECT_CREATION_FAILED);
    }
    fsdb_obj->ffrSetTreeCBFunc(__MyTreeCB, NULL);

    // NOTE (nk): this only works if dumpoff is called in sim
    // uint_T count;
    // fsdbDumpOffRange *range;
    // uint_T idx;
    // fsdb_obj->ffrGetDumpOffRange(count, range);
    // for (idx = 0; idx < count; idx++) {
    //     fprintf(stderr, "dump off range %u: begin (%u %u), end (%u %u)\n",
    //                 idx + 1,
    //                 (range + idx)->begin.hltag.H, (range + idx)->begin.hltag.L,
    //                 (range + idx)->end.hltag.H, (range + idx)->end.hltag.L);
    // }

    char *output_file = strcat(strtok(basename(argv[1]), "."),".delta");

    FILE *fptr;
    fptr = fopen(output_file, "w");

    if (FSDB_FT_VERILOG != fsdb_obj->ffrGetFileType()) {
	fprintf(stderr, 
		"%s is not verilog type fsdb, just return.\n", argv[1]);
	fsdb_obj->ffrClose();
	return FSDB_RC_SUCCESS;
    }

    //
    // Activate the tree callback funciton, read the design 
    // hierarchies. Application has to perform proper type case 
    // on tree callback data based on the callback type, then uses 
    // the type case structure view to access the wanted data.
    //
    fsdb_obj->ffrReadScopeVarTree();

    //
    // Each unique var is represented by a unique idcode in fsdb 
    // file, these idcodes are positive integer and continuous from 
    // the smallest to the biggest one. So the maximum idcode also 
    // means that how many unique vars are there in this fsdb file. 
    //
    // Application can know the maximum var idcode by the following
    // API:
    //
    //		ffrGetMaxVarIdcode()
    //
    fsdbVarIdcode max_var_idcode = fsdb_obj->ffrGetMaxVarIdcode();


    //
    // In order to load value changes of vars onto memory, application
    // has to tell fsdb reader about what vars it's interested in. 
    // Application selects the interested vars by giving their idcodes
    // to fsdb reader, this is done by the following API:
    //
    //		ffrAddToSignalList()
    // 
    int i;
    for (i = FSDB_MIN_VAR_IDCODE; i <= max_var_idcode; i++)
    	fsdb_obj->ffrAddToSignalList(i);


    //
    // Load the value changes of the selected vars onto memory. Note 
    // that value changes of unselected vars are not loaded.
    //
    fsdb_obj->ffrLoadSignals();
    


    //
    // In order to traverse the value changes of a specific var,
    // application must create a value change traverse handle for 
    // that sepcific var. Once the value change traverse handle is 
    // created successfully, there are lots of traverse functions 
    // available to traverse the value changes backward and forward, 
    // or jump to a sepcific time, etc.
    //
    ffrVCTrvsHdl vc_trvs_hdl;
    vc_trvs_hdl = 
	fsdb_obj->ffrCreateVCTraverseHandle(max_var_idcode); 
    if (NULL == vc_trvs_hdl) {
	fprintf(stderr, "Failed to create a traverse handle(%u)\n", 
		max_var_idcode);
	exit(FSDB_RC_OBJECT_CREATION_FAILED);
    }


    fsdbTag64 time;
    int	      glitch_num;
    byte_T    *vc_ptr;

    //
    // Check to see if this var has value changes or not.
    //
    if (FALSE == vc_trvs_hdl->ffrHasIncoreVC()) {
        fprintf(stderr, 
	        "This var(%u) has no value change at all.\n", 
		max_var_idcode);
    }
    else {
        // //
        // // Get the maximum time(xtag) where has value change. 
        // //
        // if (FSDB_RC_SUCCESS != 
	    // vc_trvs_hdl->ffrGetMaxXTag((void*)&time)) {
	    // fprintf(stderr, "should not happen.\n");
	    // exit(FSDB_RC_FAILURE);
 	    // }
       	// fprintf(stderr, "trvs hdl(%u): maximum time is (%u %u).\n", 
        //     	max_var_idcode, time.H, time.L);
            
        // //
        // // Get the minimum time(xtag) where has value change. 
        // // 
        // if (FSDB_RC_SUCCESS != 
	    // vc_trvs_hdl->ffrGetMinXTag((void*)&time)) {
	    // fprintf(stderr, "should not happen.\n");
	    // exit(FSDB_RC_FAILURE);
 	    // }
       	// fprintf(stderr, "trvs hdl(%u): minimum time is (%u %u).\n", 
        //     	max_var_idcode, time.H, time.L);
    
        // //
        // // Jump to the specific time specified by the parameter of 
        // // ffrGotoXTag(). The specified time may have or have not 
        // // value change; if it has value change, then the return time 
        // // is exactly the same as the specified time; if it has not 
        // // value change, then the return time will be aligned forward
        // // (toward smaller time direction). 
        //     //
        //     // There is an exception for the jump alignment: If the 
        // // specified time is smaller than the minimum time where has 
        // // value changes, then the return time will be aligned to the 
        // // minimum time.
        // //
        // if (FSDB_RC_SUCCESS != vc_trvs_hdl->ffrGotoXTag((void*)&time)) {
	    // fprintf(stderr, "should not happen.\n");
	    // exit(FSDB_RC_FAILURE);
        // }	
    
        // //
        // // Get the value change. 
        // //
        // if (FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGetVC(&vc_ptr))
        //     __PrintTimeValChng(vc_trvs_hdl, &time, vc_ptr);
         
    
        //
        // Value change traverse handle keeps an internal index
        // which points to the current time and value change; each
        // traverse API may move that internal index backward or
        // forward.
        // 
        // ffrGotoNextVC() moves the internal index backward so
        // that it points to the next value change and the time
        // where the next value change happened.
        //  
        for ( ; FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoNextVC(); ) {
            vc_trvs_hdl->ffrGetXTag(&time);
      	    vc_trvs_hdl->ffrGetVC(&vc_ptr);
      	    // __PrintTimeValChng(vc_trvs_hdl, &time, vc_ptr);
        }
            
        // // 
        // // ffrGotoPrevVC() moves the internal index forward so
        // // that it points to the previous value change and the time
        // // where the previous value change happened.
        // //  
        // for ( ; FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoPrevVC(); ) {
        //     vc_trvs_hdl->ffrGetXTag(&time);
      	//     vc_trvs_hdl->ffrGetVC(&vc_ptr);
      	//     __PrintTimeValChng(vc_trvs_hdl, &time, vc_ptr);
        // }
    }
    // 
    // free this value change traverse handle 
    //
    vc_trvs_hdl->ffrFree();

    //
    // We have traversed the value changes associated with the var 
    // whose idcode is max_var_idcode, now we are going to traverse
    // the value changes of the other vars. The idcode of the other
    // vars is from FSDB_MIN_VAR_IDCODE, which is 1, to 
    // (max_var_idcode - 1)  
    //
    
    for (i = FSDB_MIN_VAR_IDCODE; i <= max_var_idcode; i++) {
        //
            // create a value change traverse handle associated with
        // current traversed var.
        // 
        vc_trvs_hdl = fsdb_obj->ffrCreateVCTraverseHandle(i);
            if (NULL == vc_trvs_hdl) {
            fprintf(stderr, "Failed to create a traverse handle(%u)\n", 
                max_var_idcode);
            exit(FSDB_RC_OBJECT_CREATION_FAILED);
            }

        //
        // Check to see if this var has value changes or not.
        //
        if (FALSE == vc_trvs_hdl->ffrHasIncoreVC()) {
            fprintf(stderr, 
            "This var(%u) has no value change at all.\n", i);
            vc_trvs_hdl->ffrFree();
            continue;
        }

            //
            // Get the minimum time(xtag) where has value change. 
            // 
        vc_trvs_hdl->ffrGetMinXTag((void*)&time);

        //
        // Jump to the minimum time(xtag). 
        // 
        vc_trvs_hdl->ffrGotoXTag((void*)&time);

        //
        // Traverse all the value changes from the minimum time
        // to the maximum time.
        //
        
        memset(toggle_buff, 0, BUFFER_SIZE*sizeof(float));
        uint_T num_toggles = 0;
        uint_T time_window = td;
        uint_T idx = 0;
        byte_T *prev_vc_ptr;

        do {
            vc_trvs_hdl->ffrGetXTag(&time);
            vc_trvs_hdl->ffrGetVC(&vc_ptr);

            if (time.L >= time_window) {
                while (time.L - td > time_window) {
                    time_window += td; idx += 1;
                }
                

                // fprintf(stderr, "%u\n", idx);

                toggle_buff[idx] = (float) num_toggles/2/N_CYCLES;
                
                time_window += td; idx += 1;
                num_toggles = 1;
            } else {
                num_toggles += 1;
            }

            
            
        } while(FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoNextVC());
        
        if (idx > max_idx) max_idx = idx;


        if (time_window > max_time_window) max_time_window = time_window;
        
        // TODO (nk): technically we throw out last incomplete time window

        fprintf(fptr, "\n");
        uint_T i = 0;
        for (i = 0; i < max_idx; i++) {
            if toggle_buff[i]
                fprintf(fptr, "%f ", toggle_buff[i]);
            else
                fprintf(fptr, "0 ");
        }
        // this only works for binary file:
        // fwrite(toggle_buff, sizeof(float), max_idx, fptr);

        // 
        // free this value change traverse handle
        //
        vc_trvs_hdl->ffrFree();
    } 

    fprintf(stderr, "\nMax_time_window: %u\n", 
            max_time_window);
    
    fclose(fptr);

    fsdb_obj->ffrResetSignalList();

    fsdb_obj->ffrUnloadSignals();


    fsdb_obj->ffrClose();
    return 0;
}

static bool_T 
__ValChng(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr, 
           byte_T *prev_vc_ptr)
{ 
    static byte_T buffer[FSDB_MAX_BIT_SIZE+1];
    byte_T        *ret_vc;
    uint_T        i;
    fsdbVarType   var_type; 
    
    switch (vc_trvs_hdl->ffrGetBytesPerBit()) {
    case FSDB_BYTES_PER_BIT_1B:
        for (i = 0; i < vc_trvs_hdl->ffrGetBitSize(); i++) {
            if (vc_ptr[i] != prev_vc_ptr[i]) {
                return TRUE;
            }
        }
        return FALSE;
	break;

    case FSDB_BYTES_PER_BIT_4B:
        return FALSE;
        // var_type = vc_trvs_hdl->ffrGetVarType();
        // switch(var_type){
        // case FSDB_VT_VCD_MEMORY_DEPTH:
        // case FSDB_VT_VHDL_MEMORY_DEPTH:
	    // fprintf(stderr, "time: (%u %u)", time->H, time->L);
        //     fprintf(stderr, "  begin: %d", *((int*)vc_ptr));
        //     vc_ptr = vc_ptr + sizeof(uint_T);
        //     fprintf(stderr, "  end: %d\n", *((int*)vc_ptr));  
        //     break;
               
        // default:    
        //     vc_trvs_hdl->ffrGetVC(&vc_ptr);
	    // fprintf(stderr, "time: (%u %u)  val chg: %f\n",
        //             time->H, time->L, *((float*)vc_ptr));
	    // break;
        // }
        break;

    case FSDB_BYTES_PER_BIT_8B:
        return ((*((double*)vc_ptr)) != (*((double*)prev_vc_ptr)));
	break;
    }
    return FALSE;
}


static void 
__PrintTimeValChng(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr)
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
	fprintf(stderr, "time: (%u %u)  val chg: %s\n",
		time->H, time->L, buffer);	
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
	    fprintf(stderr, "time: (%u %u)", time->H, time->L);
            fprintf(stderr, "  begin: %d", *((int*)vc_ptr));
            vc_ptr = vc_ptr + sizeof(uint_T);
            fprintf(stderr, "  end: %d\n", *((int*)vc_ptr));  
            break;
               
        default:    
            vc_trvs_hdl->ffrGetVC(&vc_ptr);
	    fprintf(stderr, "time: (%u %u)  val chg: %f\n",
                    time->H, time->L, *((float*)vc_ptr));
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
	fprintf(stderr, "time: (%u %u)  val chg: %e\n",
		time->H, time->L, *((double*)vc_ptr));
	break;

    default:
	fprintf(stderr, "Control flow should not reach here.\n");
	break;
    }
}

static bool_T __MyTreeCB(fsdbTreeCBType cb_type, 
			 void *client_data, void *tree_cb_data)
{
    // nk - skip all of this for now
    return TRUE;
    switch (cb_type) {
    case FSDB_TREE_CBT_BEGIN_TREE:
	fprintf(stderr, "<BeginTree>\n");
	break;

    case FSDB_TREE_CBT_SCOPE:
	__DumpScope((fsdbTreeCBDataScope*)tree_cb_data);
	break;

    case FSDB_TREE_CBT_VAR:
	// __DumpVar((fsdbTreeCBDataVar*)tree_cb_data);
    __DumpVar_less((fsdbTreeCBDataVar*)tree_cb_data);
	break;

    case FSDB_TREE_CBT_UPSCOPE:
	fprintf(stderr, "<Upscope>\n");
	break;

    case FSDB_TREE_CBT_END_TREE:
	fprintf(stderr, "<EndTree>\n\n");
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
        fprintf(stderr, "<BeginArray>\n");
        break;
        
    case FSDB_TREE_CBT_ARRAY_END:
        fprintf(stderr, "<EndArray>\n\n");
        break;

    case FSDB_TREE_CBT_RECORD_BEGIN:
        fprintf(stderr, "<BeginRecord>\n");
        break;
        
    case FSDB_TREE_CBT_RECORD_END:
        fprintf(stderr, "<EndRecord>\n\n");
        break;
             
    default:
	return FALSE;
    }

    return TRUE;
}

static void 
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
	type = "unknown_scope_type";
	break;
    }

    fprintf(stderr, "<Scope> name:%s  type:%s\n", 
	    scope->name, type);
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

    fprintf(stderr,
	"<Var>  name:%s  l:%u  r:%u  type:%s  ",
	var->name, var->lbitnum, var->rbitnum, type);
    fprintf(stderr,
	"idcode:%u  dtidcode:%u  bpb:%s\n",
	var->u.idcode, var->dtidcode, bpb);
}

static void 
__DumpVar_less(fsdbTreeCBDataVar *var)
{
    fprintf(stderr,
	"%u  %s\n",
	var->u.idcode, var->name);
    
}
