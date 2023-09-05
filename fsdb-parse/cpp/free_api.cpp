/* *****************************************************************************
// [free_api.cpp]
//
//  Copyright 2001-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// Program Name	: free_api.cpp
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
static bool_T 
__MyTreeCB(fsdbTreeCBType cb_type, void *client_data, void *tree_cb_data);

byte_T logic_0 = FSDB_BT_VCD_0;
byte_T logic_1 = FSDB_BT_VCD_1;
byte_T logic_x = FSDB_BT_VCD_X;
byte_T logic_z = FSDB_BT_VCD_Z;

int 
main(int argc, char *argv[])
{
  if (2 != argc) {
      fprintf(stderr, "usage: free_api verilog_type_fsdb\n");
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


  ffrObject *ffr_obj = ffrObject::ffrOpen3(argv[1]);
  if (NULL == ffr_obj) {
      fprintf(stderr, "ffrObject::ffrOpen3() failed.\n");
      exit(FSDB_RC_OBJECT_CREATION_FAILED);
  }
  ffr_obj->ffrSetTreeCBFunc(__MyTreeCB, NULL);

  if (FSDB_FT_VERILOG != ffr_obj->ffrGetFileType()) {
      fprintf(stderr, 
      	"%s is not verilog type fsdb, just return.\n", argv[1]);
      ffr_obj->ffrClose();
      return FSDB_RC_SUCCESS;
  }

  ffr_obj->ffrReadScopeVarTree();


  const fsdbVarIdcode max_var_idcode = ffr_obj->ffrGetMaxVarIdcode();
  ffrVCTrvsHdl *trvs_hdl_arr = (ffrVCTrvsHdl*)calloc(max_var_idcode + 1, sizeof(ffrVCTrvsHdl));
  fsdbVarIdcode var_idcode;
  
  for (var_idcode = FSDB_MIN_VAR_IDCODE; var_idcode <= max_var_idcode; var_idcode++) {
    ffr_obj->ffrAddToSignalList(var_idcode);
    trvs_hdl_arr[var_idcode] = ffr_obj->ffrCreateVCTraverseHandle(var_idcode);
  }
  ffr_obj->ffrLoadSignals();

  ffrVCTrvsHdl trvs_hdl = ffr_obj->ffrCreateVCTraverseHandle(1);
  fprintf(stderr, "reference_count of variable(1) is %u\n", trvs_hdl->ffrGetRefCount());
  trvs_hdl->ffrFree();
  getchar();

  // 
  // the traverse object pointed by trvs_hdl is freed, and the reference count of the
  // associated variable is subtracted by one.
  //
  fprintf(stderr, "reference_count of variable(1) is %u\n", trvs_hdl_arr[1]->ffrGetRefCount());
  ffr_obj->ffrUnloadSignals();
  getchar();


  for (var_idcode = FSDB_MIN_VAR_IDCODE; var_idcode < max_var_idcode; var_idcode++)
    trvs_hdl_arr[var_idcode]->ffrFree();
  ffr_obj->ffrUnloadSignals();


  ffr_obj->ffrLoadSignals();
  for (var_idcode = FSDB_MIN_VAR_IDCODE; var_idcode < max_var_idcode; var_idcode++)
    trvs_hdl_arr[var_idcode] = ffr_obj->ffrCreateVCTraverseHandle(var_idcode);

  trvs_hdl_arr[1]->ffrFree();
  trvs_hdl_arr[3]->ffrFree();
  trvs_hdl_arr[5]->ffrFree();
  fsdbVarIdcode var_idcode_arr[1024];
  var_idcode_arr[0] = 1;  
  var_idcode_arr[1] = 3;  
  var_idcode_arr[2] = 5;  
  ffr_obj->ffrUnloadSignals(var_idcode_arr, 3);
  getchar();


//  ffr_obj->ffrPurgeSignals();


  ffr_obj->ffrClose();
  return FSDB_RC_SUCCESS;
}

static bool_T __MyTreeCB(fsdbTreeCBType cb_type, 
			 void *client_data, void *tree_cb_data)
{
#if 0
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
#endif

    return TRUE;
}

