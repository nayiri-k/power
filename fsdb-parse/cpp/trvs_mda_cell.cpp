/* *****************************************************************************
// [trvs_mda_cell.cpp]
//
//  Copyright 2002-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// Program Name	: trvs_mem_cell.cpp
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

#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE	1
#endif


typedef struct MyTreeClientData	MyTreeClientData;
struct MyTreeClientData {
    ffrObject		*fsdb_obj;
    char		*cell_name;
    fsdbVarIdcode 	cell_idcode;
};

//
// The tree callback function, it's used to traverse the design 
// hierarchies. 
//
static bool_T 
__MyTreeCBFunc(fsdbTreeCBType cb_type, void *client_data, void *tree_cb_data);


static void 
__PrintTimeValue(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr);

int 
main(int argc, char *argv[])
{
    if (4 != argc) {
	fprintf(stderr, "usage example: trvs_md_cell mda.fsdb -cell cell_name.\n");
	return FSDB_RC_FAILURE;
    }

    int  idx;
    char *ptr;

    for (idx = 0; idx < argc; idx++) {
	ptr = argv[idx];
	if ('-' != *ptr)
	    continue;

	ptr++;
        if (0 != strcmp("cell", ptr)) {
	    fprintf(stderr, "option is wrong, it should be -cell.\n");
	    return (-1);
	}
	else {
	    idx++;
	    if (idx >= argc) {
		fprintf(stderr, "cell_name not specified.\n");
		return (-1);
	    }

	    ptr = argv[idx];
	    break;
	}
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
    // Open FSDB file.
    // 
    ffrObject *fsdb_obj = ffrObject::ffrOpen3(argv[1]);
    if (NULL == fsdb_obj) {
	fprintf(stderr, "ffrObject::ffrOpen() failed.\n");
	return FSDB_RC_OBJECT_CREATION_FAILED;
    }

    //
    // Activate the tree callback funciton to read the design 
    // hierarchies. Based on the callback type, the Application 
    // performs proper cast on tree callback data, then it can
    // access the right data.
    //
    MyTreeClientData my_tree_client_data;
    
    memset((void*)&my_tree_client_data, 0, sizeof(MyTreeClientData));
    my_tree_client_data.fsdb_obj = fsdb_obj;
    my_tree_client_data.cell_name = ptr;
    // 
    // We will figure out the var idcode of the cell in the tree callback 
    // function.
    //
    fsdb_obj->ffrReadScopeVarTree2(__MyTreeCBFunc, (void*)&my_tree_client_data);
    if (0 == my_tree_client_data.cell_idcode) {
	fprintf(stderr, "cell idcode not found.\n");
	return FSDB_RC_FAILURE;
    }

    //
    // add the wanted signals' idcode into signal list
    //
    fsdb_obj->ffrAddToSignalList(my_tree_client_data.cell_idcode); 

    //
    // load the value changes of wanted signals onto memory
    //
    fsdb_obj->ffrLoadSignals();

    //
    // Create a traverse object associated with a specific signal.
    //
    ffrVCTrvsHdl vc_trvs_hdl = 
	fsdb_obj->ffrCreateVCTraverseHandle(my_tree_client_data.cell_idcode); 
    if (NULL == vc_trvs_hdl) {
	fprintf(stderr, "Failed to create a traverse object.\n");
	return FSDB_RC_OBJECT_CREATION_FAILED;
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
		my_tree_client_data.cell_idcode);
    }
    else {
        //
        // Get the maximum time(xtag) where has value change. 
        //
        if (FSDB_RC_SUCCESS != vc_trvs_hdl->ffrGetMaxXTag((void*)&time)) {
	    fprintf(stderr, "It's not supposed to happen.");
	    return FSDB_RC_FAILURE;
 	}
       	fprintf(stderr, "maximum time is (%u %u).\n", time.H, time.L);
            
        //
        // Get the minimum time(xtag) where has value change. 
        // 
        if (FSDB_RC_SUCCESS != vc_trvs_hdl->ffrGetMinXTag((void*)&time)) {
	    fprintf(stderr, "It's not supposed to happen.");
	    return FSDB_RC_FAILURE;
 	}
       	fprintf(stderr, "minimum time is (%u %u).\n", time.H, time.L);
   

	//
	// Traverse all the value changes from the start to the end.
	// 
        if (FSDB_RC_SUCCESS != vc_trvs_hdl->ffrGotoTheFirstVC()) {
	    fprintf(stderr, "It's not supposed to happen.");
	    return FSDB_RC_FAILURE;
        }	

   	do {
            vc_trvs_hdl->ffrGetXTag(&time);
      	    vc_trvs_hdl->ffrGetVC(&vc_ptr);
      	    __PrintTimeValue(vc_trvs_hdl, &time, vc_ptr);
	} while (FSDB_RC_SUCCESS == vc_trvs_hdl->ffrGotoNextVC()); 

    }
    // 
    // free this traverse object 
    //
    vc_trvs_hdl->ffrFree();

    fsdb_obj->ffrClose();

    return FSDB_RC_SUCCESS;
}

static bool_T __MyTreeCBFunc(fsdbTreeCBType cb_type, 
			 void *tree_client_data, void *tree_cb_data)
{
    switch (cb_type) {
    case FSDB_TREE_CBT_BEGIN_TREE:
	// fprintf(stderr, "<BeginTree>\n");
	break;

    case FSDB_TREE_CBT_SCOPE:
	break;

    case FSDB_TREE_CBT_VAR: 
	// fprintf(stderr, "<Var>\n");
        {
	    fsdbTreeCBDataVar *var_ptr = (fsdbTreeCBDataVar*)tree_cb_data;
	    MyTreeClientData *my_tree_client_data = (MyTreeClientData*)tree_client_data;

	    // fprintf(stderr, "var name: %s\n", var_ptr->name);
	    if (0 == strcmp(var_ptr->name, my_tree_client_data->cell_name)) {
		fprintf(stderr, "cell idcode found, it's %u.\n", var_ptr->u.idcode);
		my_tree_client_data->cell_idcode = var_ptr->u.idcode;
	    }
   	} 
	break;

    case FSDB_TREE_CBT_UPSCOPE:
	// fprintf(stderr, "<Upscope>\n");
	break;

    case FSDB_TREE_CBT_END_TREE:
	// fprintf(stderr, "<EndTree>\n\n");
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
__PrintTimeValue(ffrVCTrvsHdl vc_trvs_hdl, 
		   fsdbTag64 *time, byte_T *vc_ptr)
{ 
    static byte_T buffer[FSDB_MAX_BIT_SIZE + 1];
    byte_T *ret_vc;
    uint_T i;

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
	fprintf(stderr, "time: (%u %u)  value: %s\n",
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
	fprintf(stderr, "time: (%u %u)  value: %f\n",
		time->H, time->L, *((float*)vc_ptr));
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
	fprintf(stderr, "time: (%u %u)  value: %e\n",
		time->H, time->L, *((double*)vc_ptr));
	break;

    default:
	fprintf(stderr, "Control flow should not reach here.\n");
	break;
    }
}
