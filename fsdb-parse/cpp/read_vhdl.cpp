/* *****************************************************************************
// [read_vhdl.cpp]
//
//  Copyright 2003-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// Program Name	: read_vhdl.cpp
//
// Purpose	: Demonstrate how to call fsdb reader APIs to access 
//		  VHDL type fsdb.
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

#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE	1
#endif


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

//
// dump value change definition
// 
static void
__PrintTimeValChng(ffrVCTrvsHdl vc_trvs_hdl,
   fsdbTag64 *time, byte_T *vc_ptr);

int 
main(int argc, char *argv[])
{
    if (2 != argc) {
	fprintf(stderr, "usage: read_vhdl vhdl_type_fsdb\n");
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
    if (FSDB_FT_VHDL != fsdb_info.file_type) {
  	fprintf(stderr, "File type is not VHDL.\n");
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
	fprintf(stderr, "ffrObject::ffrOpene() failed.\n");
	exit(FSDB_RC_OBJECT_CREATION_FAILED);
    }
    fsdb_obj->ffrSetTreeCBFunc(__MyTreeCB, NULL);

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
    //          ffrGetMaxVarIdcode()
    //
    fsdbVarIdcode max_var_idcode = fsdb_obj->ffrGetMaxVarIdcode();


    //
    // In order to load value changes of vars onto memory, application
    // has to tell fsdb reader about what vars it's interested in. 
    // Application selects the interested vars by giving their idcodes
    // to fsdb reader, this is done by the following API:
    //
    //          ffrAddToSignalList()
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
    ffrVCTrvsHdl vc_trvs_hdl =
        fsdb_obj->ffrCreateVCTraverseHandle(max_var_idcode);
    if (NULL == vc_trvs_hdl) {
        fprintf(stderr, "Failed to create a traverse handle(%u)\n",
                max_var_idcode);
        exit(FSDB_RC_OBJECT_CREATION_FAILED);
    }


    fsdbTag64 time;
    int       glitch_num;
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
        //
        // Get the maximum time(xtag) where has value change. 
        //
        if (FSDB_RC_SUCCESS !=
            vc_trvs_hdl->ffrGetMaxXTag((void*)&time)) {
            fprintf(stderr, "should not happen.\n");
            exit(FSDB_RC_FAILURE);
        }
        fprintf(stderr, "trvs hdl(%u): maximum time is (%u %u).\n",
                max_var_idcode, time.H, time.L);

        //
        // Get the minimum time(xtag) where has value change. 
        // 
        if (FSDB_RC_SUCCESS !=
            vc_trvs_hdl->ffrGetMinXTag((void*)&time)) {
            fprintf(stderr, "should not happen.\n");
            exit(FSDB_RC_FAILURE);
        }
        fprintf(stderr, "trvs hdl(%u): minimum time is (%u %u).\n",
                max_var_idcode, time.H, time.L);

        //
        // Jump to the specific time specified by the parameter of 
        // ffrGotoXTag(). The specified time may have or have not 
        // value change; if it has value change, then the return time 
        // is exactly the same as the specified time; if it has not 
        // value change, then the return time will be aligned forward
        // (toward smaller time direction). 
        //
        // There is an exception for the jump alignment: If the 
        // specified time is smaller than the minimum time where has 
        // value changes, then the return time will be aligned to the 
        // minimum time.
        //
        if (FSDB_RC_SUCCESS != vc_trvs_hdl->ffrGotoXTag((void*)&time)) {
            fprintf(stderr, "should not happen.\n");
            exit(FSDB_RC_FAILURE);
        }

        //
        // Get the value change. 
        //
        if (FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGetVC(&vc_ptr))
            __PrintTimeValChng(vc_trvs_hdl, &time, vc_ptr);


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
            __PrintTimeValChng(vc_trvs_hdl, &time, vc_ptr);
        }

        // 
        // ffrGotoPrevVC() moves the internal index forward so
        // that it points to the previous value change and the time
        // where the previous value change happened.
        //  
        for ( ; FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoPrevVC(); ) {
            vc_trvs_hdl->ffrGetXTag(&time);
            vc_trvs_hdl->ffrGetVC(&vc_ptr);
            __PrintTimeValChng(vc_trvs_hdl, &time, vc_ptr);
        }
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
    for (i = FSDB_MIN_VAR_IDCODE; i < max_var_idcode; i++) {
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
        fprintf(stderr, "\n");
        fprintf(stderr, "Current traversed var idcode is %u\n", i);

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
        do {
            vc_trvs_hdl->ffrGetXTag(&time);
            vc_trvs_hdl->ffrGetVC(&vc_ptr);
            __PrintTimeValChng(vc_trvs_hdl, &time, vc_ptr);
        } while(FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoNextVC());

        // 
        // free this value change traverse handle
        //
        vc_trvs_hdl->ffrFree();
    }

    fprintf(stderr, "Watch Out Here!\n");
    fprintf(stderr, "We are going to reset the signal list.\n");
    fprintf(stderr, "Press enter to continue running.");
    getchar();

    fsdb_obj->ffrResetSignalList();
    for (i = FSDB_MIN_VAR_IDCODE; i <= max_var_idcode; i++) {
        if (TRUE == fsdb_obj->ffrIsInSignalList(i))
            fprintf(stderr, "var idcode %d is in signal list.\n", i);
        else
            fprintf(stderr, "var idcode %d is not in signal list.\n", i);
    }
    fsdb_obj->ffrUnloadSignals();

    fsdb_obj->ffrClose();
    return 0;
}

static void
__PrintTimeValChng(ffrVCTrvsHdl vc_trvs_hdl,
                   fsdbTag64 *time, byte_T *vc_ptr)
{
    static byte_T buffer[FSDB_MAX_BIT_SIZE+1];
    byte_T *ret_vc;
    uint_T i;
    
    switch (vc_trvs_hdl->ffrGetBytesPerBit()) {
    case FSDB_BYTES_PER_BIT_1B:
        //
        // Convert each VHDL bit type to corresponding
        // character.
        //
        for (i = 0; i < vc_trvs_hdl->ffrGetBitSize(); i++) {
            switch(vc_ptr[i]) {
            case FSDB_BT_VHDL_STD_ULOGIC_U:
                buffer[i] = 'u';
                break;
            
            case FSDB_BT_VHDL_STD_ULOGIC_X:
                buffer[i] = 'x';
                break;
            
            case FSDB_BT_VHDL_STD_ULOGIC_0:
                buffer[i] = '0';
                break;
            
            case FSDB_BT_VHDL_STD_ULOGIC_1:
                buffer[i] = '1';
                break;
            
            case FSDB_BT_VHDL_STD_ULOGIC_Z:
                buffer[i] = 'z';
                break;
            
            case FSDB_BT_VHDL_STD_ULOGIC_W:
                buffer[i] = 'w';
                break;
            
            case FSDB_BT_VHDL_STD_ULOGIC_L:
                buffer[i] = 'l';
                break;
            
            case FSDB_BT_VHDL_STD_ULOGIC_H:
                buffer[i] = 'h';
                break;
            
            case FSDB_BT_VHDL_STD_ULOGIC_DASH:
                buffer[i] = '-';
                break;
            
            default:
                //
                // unknown verilog bit type found.
                //
                buffer[i] = '?';
            }
        }
        buffer[i] = '\0';
        fprintf(stderr, "time: (%u %u)  val chg: %s\n",
                time->H, time->L, buffer);
        break;

    case FSDB_BYTES_PER_BIT_4B:
    {
        //
        // May be an array of float or integer. Could also be enum.
        //
        float* ptr = (float*)vc_ptr;

        for (i = 0; i < vc_trvs_hdl->ffrGetBitSize(); i++) {
            fprintf(stderr, "time: (%u %u)  val chg: %f(or %d)\n",
                    time->H, time->L, *((float*)ptr), *((int*)ptr));
            ptr++;
        }
        break;
    }
    case FSDB_BYTES_PER_BIT_8B:
    {
        //
        // May be an array of double.
        //
        double* ptr = (double*)vc_ptr;

        for (i = 0; i < vc_trvs_hdl->ffrGetBitSize(); i++) {
            fprintf(stderr, "time: (%u %u)  val chg: %e\n",
                    time->H, time->L, *((double*)ptr));
            ptr++;
        }
        break;
    }
    default:
        fprintf(stderr, "Control flow should not reach here.\n");
        break;
    }
}

static bool_T __MyTreeCB(fsdbTreeCBType cb_type, 
			 void *client_data, void *tree_cb_data)
{
    switch (cb_type) {
    case FSDB_TREE_CBT_BEGIN_TREE:
	fprintf(stderr, "<BeginTree>\n");
	break;

    case FSDB_TREE_CBT_SCOPE:
	__DumpScope((fsdbTreeCBDataScope*)tree_cb_data);
	break;

    case FSDB_TREE_CBT_VAR:
	__DumpVar((fsdbTreeCBDataVar*)tree_cb_data);
	break;

    case FSDB_TREE_CBT_UPSCOPE:
	fprintf(stderr, "<Upscope>\n");
	break;

    case FSDB_TREE_CBT_END_TREE:
	fprintf(stderr, "<EndTree>\n\n");
	break;

    case FSDB_TREE_CBT_END_ALL_TREE:
	break;

    case FSDB_TREE_CBT_FILE_TYPE:
	break;

    case FSDB_TREE_CBT_SIMULATOR_VERSION:
	break;

    case FSDB_TREE_CBT_SIMULATION_DATE:
	break;

    case FSDB_TREE_CBT_X_AXIS_SCALE:
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
    case FSDB_ST_VHDL_ARCHITECTURE:
	type = (str_T) "architecture"; 
	break;

    case FSDB_ST_VHDL_PROCEDURE:
	type = (str_T) "procedure"; 
	break;

    case FSDB_ST_VHDL_FUNCTION:
	type = (str_T) "function"; 
	break;

    case FSDB_ST_VHDL_RECORD:
	type = (str_T) "record"; 
	break;

    case FSDB_ST_VHDL_PROCESS:
	type = (str_T) "process"; 
	break;

    case FSDB_ST_VHDL_BLOCK:
	type = (str_T) "block"; 
	break;

    case FSDB_ST_VHDL_FOR_GENERATE:
	type = (str_T) "for generate"; 
	break;

    case FSDB_ST_VHDL_IF_GENERATE:
	type = (str_T) "if generate"; 
	break;

    default:
	type = "unknown_scope_type";
	break;
    }

    fprintf(stderr, "<Scope> name:%s type:%s\n", 
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
