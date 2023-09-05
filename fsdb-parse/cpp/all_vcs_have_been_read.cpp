/* *****************************************************************************
// [all_vcs_have_been_read.cpp]
//
//  Copyright 2001-2009 SPRINGSOFT. All Rights Reserved.
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

static bool_T __MyTreeCB_2(fsdbTreeCBType cb_type, 
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


int 
main(int argc, char *argv[])
{
    if (2 != argc) {
	fprintf(stderr, "usage: all_vcs_have_been_read verilog_type_fsdb\n");
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
    // This function is similar to ffrReadScopeVarTree with the difference that caller could ask the function
    // to use tree_cb_func and tree_client_data instead of that of the fsdb reader object to handle scope/signal
    // information. This is useful when caller needs to handle scope/signal information in its own way but does
    // not want to change the tree function pointer and tree callback data of the fsdb reader object. Note that
    // the tree function pointer and tree callback data of the fsdb reader object
    // remains intact after the function finishes.
    //
    fsdb_obj->ffrReadScopeVarTree2(__MyTreeCB_2, NULL);
    
    //
    // get fsdb's callback function, should be __MyTreeCB
    //
    fsdbTreeCBFunc ret_tree_cb_func;
    void            *ret_tree_client_data;
    fsdb_obj->ffrGetTreeCBFunc(&ret_tree_cb_func, &ret_tree_client_data);
    if(ret_tree_cb_func == __MyTreeCB)
        fprintf(stderr, "After ReadScopeVarTree2, the callback function shouldn't change\n\n");
    else {
        fprintf(stderr, "Error!! After ReadScopeVarTree2, the callback function shouldn't change\n");
        return FSDB_RC_FAILURE;
    }
    

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
    fsdb_obj->ffrAddToSignalList(1);
    fsdb_obj->ffrAddToSignalList(2);
    fsdb_obj->ffrAddToSignalList(3);

    fsdb_obj->ffrLoadSignals();

    fsdb_obj->ffrAddToSignalList(4);
    fsdb_obj->ffrLoadSignals();


    fsdb_obj->ffrAddToSignalList(5);
    fsdb_obj->ffrAddToSignalList(6);
    fsdb_obj->ffrAddToSignalList(7);
    fsdb_obj->ffrLoadSignals();

    fprintf(stderr, "idcode 1 has %u vcs.\n", fsdb_obj->ffrGetVCCount(1));
    fprintf(stderr, "idcode 2 has %u vcs.\n", fsdb_obj->ffrGetVCCount(2));
    fprintf(stderr, "idcode 3 has %u vcs.\n", fsdb_obj->ffrGetVCCount(3));
    fprintf(stderr, "idcode 4 has %u vcs.\n", fsdb_obj->ffrGetVCCount(4));
    fprintf(stderr, "idcode 5 has %u vcs.\n", fsdb_obj->ffrGetVCCount(5));
    fprintf(stderr, "idcode 6 has %u vcs.\n", fsdb_obj->ffrGetVCCount(6));
    fprintf(stderr, "idcode 7 has %u vcs.\n", fsdb_obj->ffrGetVCCount(7));

    fsdb_obj->ffrClose();
    return 0;
}

static bool_T __MyTreeCB_2(fsdbTreeCBType cb_type, 
			 void *client_data, void *tree_cb_data)
{
    switch (cb_type) {
    case FSDB_TREE_CBT_BEGIN_TREE:
	fprintf(stderr, "CB2_<BeginTree>\n");
	break;

    case FSDB_TREE_CBT_SCOPE:
	__DumpScope((fsdbTreeCBDataScope*)tree_cb_data);
	break;

    case FSDB_TREE_CBT_VAR:
	__DumpVar((fsdbTreeCBDataVar*)tree_cb_data);
	break;

    case FSDB_TREE_CBT_UPSCOPE:
	fprintf(stderr, "CB2_<Upscope>\n");
	break;

    case FSDB_TREE_CBT_END_TREE:
	fprintf(stderr, "CB_2<EndTree>\n\n");
	break;

    case FSDB_TREE_CBT_FILE_TYPE:
    case FSDB_TREE_CBT_SIMULATOR_VERSION:
    case FSDB_TREE_CBT_SIMULATION_DATE:
    case FSDB_TREE_CBT_X_AXIS_SCALE:
    case FSDB_TREE_CBT_END_ALL_TREE:
        break;
    default:
	return FALSE;
    }

    return TRUE;
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
