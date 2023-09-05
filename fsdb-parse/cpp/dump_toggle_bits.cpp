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
uint_T CLOCK_TICK = CLOCK_PERIOD/2; // length of clock "tick" is half of a period
uint_T N_CYCLES = 100;
uint_T TW_L = CLOCK_PERIOD*N_CYCLES;  // time window length
uint_T TW_TICKS = 2*N_CYCLES; // number of R+F edges in a time window
uint_T MAX_TIME=0;
uint_T N_ROWS;
uint_T M_COLS;

// #define BUFFER_TYPE uint_T
#define BUFFER_TYPE unsigned short int
BUFFER_TYPE* toggles_buff;
BUFFER_TYPE* ones_buff;



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
    // fprintf(stderr, "Size of uint16: %u, Size of uint32: %u\n",
    //     sizeof(unsigned short int), sizeof(uint_T));
    
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

    // char *output_file = strcat("bin/",strcat(strtok(basename(argv[1]), "."),".bin"));
    char *bin_filename = strcat(strtok(basename(argv[1]), "."),".bin");
    char fpath_toggles[50]; strcpy(fpath_toggles,"./out/dump-toggle_bits/");
    strcat(fpath_toggles, bin_filename);
    FILE *f_toggles;
    f_toggles = fopen(fpath_toggles, "wb");
    if (f_toggles == NULL) {
        fprintf(stderr, "ERROR: File does not exist, %s\n", fpath_toggles);
        return 0;
    }
    char fpath_ones[50]; strcpy(fpath_ones,"./out/dump-ones/");
    strcat(fpath_ones, bin_filename);
    FILE *f_ones;
    f_ones = fopen(fpath_ones, "wb");
    if (f_ones == NULL) {
        fprintf(stderr, "ERROR: File does not exist, %s\n", fpath_ones);
        return 0;
    }

    char *txt_filename = strcat(strtok(basename(argv[1]), "."),".txt");
    char fpath_idcodes[50]; strcpy(fpath_idcodes,"./out/idcodes/");
    strcat(fpath_idcodes, txt_filename);
    FILE *f_idcodes;
    f_idcodes = fopen(fpath_idcodes, "r");
    if (f_idcodes == NULL) {
        fprintf(stderr, "ERROR: File does not exist, %s\n", fpath_idcodes);
        return 0;
    } 
    char fpath_widths[50]; strcpy(fpath_widths,"./out/signal_widths/");
    strcat(fpath_widths, txt_filename);
    FILE *f_widths;
    f_widths = fopen(fpath_widths, "r");
    if (f_widths == NULL) {
        fprintf(stderr, "ERROR: File does not exist, %s\n", fpath_widths);
        return 0;
    } 

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
    // id codes range from 1 - max_var_idcode
    // N_ROWS = (uint_T) max_var_idcode;  // number of rows = # variables

    //
    // In order to load value changes of vars onto memory, application
    // has to tell fsdb reader about what vars it's interested in. 
    // Application selects the interested vars by giving their idcodes
    // to fsdb reader, this is done by the following API:
    //
    //		ffrAddToSignalList()
    // 
    int id_code;
    for (id_code = FSDB_MIN_VAR_IDCODE; id_code <= max_var_idcode; id_code++)
    	fsdb_obj->ffrAddToSignalList(id_code);

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

    fsdbTag64 time;
    int	      glitch_num;
    byte_T    *vc_ptr;

    uint_T cntr = 0;
    for (id_code = FSDB_MIN_VAR_IDCODE; id_code <= max_var_idcode; id_code++) {

        
        vc_trvs_hdl = fsdb_obj->ffrCreateVCTraverseHandle(id_code);
            if (NULL == vc_trvs_hdl) {
            fprintf(stderr, "Failed to create a traverse handle(%u)\n", 
                id_code);
            exit(FSDB_RC_OBJECT_CREATION_FAILED);
            }
        // Get the maximim time(xtag) where has value change. 
        vc_trvs_hdl->ffrGetMaxXTag((void*)&time);
        if (time.L > MAX_TIME) {
            MAX_TIME = time.L;
        }
    }

    // first value of idcodes file is the number of idcodes
    fscanf(f_idcodes, "%u ", &N_ROWS);
    M_COLS = (uint_T) MAX_TIME/CLOCK_PERIOD/N_CYCLES;  // # columns = # time entries

    toggles_buff = (BUFFER_TYPE*) malloc(M_COLS*sizeof(BUFFER_TYPE));
    ones_buff = (BUFFER_TYPE*) malloc(M_COLS*sizeof(BUFFER_TYPE));

    fprintf(stderr, "Max time: %u\n", MAX_TIME);
    fprintf(stderr, "Number of signals to process: %u\n", N_ROWS);
    fprintf(stderr, "Number of time windows per signal: %u\n", M_COLS);
    fprintf(stderr, "Array shape: (%u,%u)\n", 
            N_ROWS, M_COLS);
    fprintf(stderr, "Writing to files: %s\n", fpath_toggles);
    fprintf(stderr, "                  %s\n", fpath_ones);
    

    // since we know both N and M, write these to file first
    // write NxM array dimensions
    // need 4 bytes bc N_ROWS could exceed max value of uint16
    
    //
    // We have traversed the value changes associated with the var 
    // whose idcode is max_var_idcode, now we are going to traverse
    // the value changes of the other vars. The idcode of the other
    // vars is from FSDB_MIN_VAR_IDCODE, which is 1, to 
    // max_var_idcode 
    //
    
    // for (id_code = FSDB_MIN_VAR_IDCODE; id_code <= max_var_idcode; id_code++) {
    fprintf(stderr, "Reading idcodes from: %s\n", fpath_idcodes);
    fprintf(stderr, "Reading signal bit widths from: %s\n", fpath_widths);
    uint_T idcode;
    uint_T msb;
    uint_T bit_idx;
    uint_T num_signal_bits = 0;
    while (fscanf(f_idcodes, "%u ", &idcode) == 1) {
        uint_T res = fscanf(f_widths, "%u ", &msb);
        if (res != 1) {
            fprintf(stderr, "Widths file is shorter than idcodes file.\n");
            return 0;
        }
        // dump MSB --> LSB (e.g. 31 downto 0)
        for (bit_idx=msb; bit_idx <= msb; bit_idx--) { // after 0 bit_idx will roll over to huge #
            // create a value change traverse handle associated with
            // current traversed var.
            vc_trvs_hdl = fsdb_obj->ffrCreateVCTraverseHandle(idcode);
                if (NULL == vc_trvs_hdl) {
                fprintf(stderr, "Failed to create a traverse handle(%u)\n", idcode);
                exit(FSDB_RC_OBJECT_CREATION_FAILED);
                }

            // Get the minimum time(xtag) where has value change. 
            vc_trvs_hdl->ffrGetMinXTag((void*)&time);

            // Jump to the minimum time(xtag). 
            vc_trvs_hdl->ffrGotoXTag((void*)&time);

            // Traverse all the value changes from the minimum time
            // to the maximum time.
            
            uint_T num_ones = 0;
            uint_T num_toggles = 0;  // range is 0-N_CYCLES, or 0-2*N_CYCLES for clk (R+F edge)
            uint_T time_window = TW_L;
            uint_T idx_buff = 0;
            uint_T prev_bit_val = FSDB_BT_VCD_X;  // uninitialized
            uint_T prev_vchg_time = 0;
            memset(toggles_buff, (BUFFER_TYPE) 0, M_COLS*sizeof(BUFFER_TYPE)); // zero out buffers
            memset(ones_buff,    (BUFFER_TYPE) 0, M_COLS*sizeof(BUFFER_TYPE));
            do {
                vc_trvs_hdl->ffrGetXTag(&time);
                vc_trvs_hdl->ffrGetVC(&vc_ptr);  // get next value change
                // initial state (don't include in # toggles)
                if (time.L == 0) {
                    prev_bit_val = vc_ptr[bit_idx];
                    continue;
                }
                // not a value change for this bit of the signal
                if (vc_ptr[bit_idx] == prev_bit_val) {
                    continue; 
                }
                if (time.L >= time_window) {
                    // signal was one for the reaminder of the previous time window
                    if (prev_bit_val == FSDB_BT_VCD_1)   { 
                        num_ones += (uint_T) (time_window - prev_vchg_time)/CLOCK_TICK;
                        if (num_ones > TW_TICKS) num_ones = TW_TICKS;
                    }
                    // val chg is outside the buff time window, so write previous window's # toggles/ones
                    toggles_buff[idx_buff] = (BUFFER_TYPE) num_toggles;
                    ones_buff[idx_buff] = (BUFFER_TYPE) num_ones;
                    while (time.L >= time_window) {
                        // advance the time window to catch up to current value change
                        time_window += TW_L; idx_buff += 1;
                        // signal was 1 the entire window
                        if (prev_bit_val == FSDB_BT_VCD_1)   { ones_buff[idx_buff] = (BUFFER_TYPE) TW_TICKS; }
                    }
                    // now time window includes current value change
                    // reset num_toggles/num_ones
                    num_toggles = 1;  // include current value change (which is now in time window)
                    num_ones = 0; // only count ones when prev signal == 1
                } else {
                    num_toggles += 1;
                    uint_T prev_time = time_window - TW_L;  // 1 was from previous window
                    if (prev_time < prev_vchg_time) prev_time = prev_vchg_time; // 1 was from previous value change
                    if (prev_bit_val == FSDB_BT_VCD_1) num_ones += (uint_T) (time.L - prev_vchg_time)/CLOCK_TICK;
                }
                prev_bit_val = vc_ptr[bit_idx];
                prev_vchg_time = time.L;
                
            } while(FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoNextVC());
            
            // NOTE (nk): technically we throw out last incomplete time window

            // free this value change traverse handle
            vc_trvs_hdl->ffrFree();

            // write data to buffer
            fwrite(toggles_buff, sizeof(BUFFER_TYPE), M_COLS, f_toggles);
            fwrite(ones_buff, sizeof(BUFFER_TYPE), M_COLS, f_ones);
            num_signal_bits += 1;
        }
    }
    

    free(toggles_buff);
    free(ones_buff);

    fwrite(&num_signal_bits, sizeof(uint_T), 1, f_toggles);
    fwrite(&num_signal_bits, sizeof(uint_T), 1, f_ones);
    fwrite(&M_COLS, sizeof(uint_T), 1, f_toggles);
    fwrite(&M_COLS, sizeof(uint_T), 1, f_ones);
    
    fclose(f_toggles);
    fclose(f_ones);

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
	type = (str_T) "unknown_scope_type";
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
