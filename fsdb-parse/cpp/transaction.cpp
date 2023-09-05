/* *****************************************************************************
// [transaction.cpp]
//
//  Copyright 2009-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// NAME : transaction.cpp
//
// PURPOSE : Test transaction recording functions.
//
// DESCRIPTION : Test the following properties:
//
//  1. Value
//      - All data types
//      - Different bit size of attributes
//      - All time combinations(including overlapping transactions)
//
//  2. Relations
//
//
// CREATION DATE :2009.08
//

#ifndef NOVAS_FSDB
    #undef NOVAS_FSDB
#endif

#include "ffrAPI.h"
#include "stdio.h"
#include "stdlib.h"

#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE	1
#endif

//
// Print out the Transaction info,
// include begin_time, end_time, label...
//
static void
__PrintTransInfo(ffrTransInfo *info);

//
// Main Program
//
int
main(int argc, char *argv[])
{
    if (2 != argc) {
        printf("usage: read transaction file\n");
	return FSDB_RC_FAILURE;
    }

    fsdbRC rc;
    ffrObject       *ffr_obj = NULL;
    fsdbVarIdcode   var_idcode;

    // 
    // check the file to see if it's a fsdb file or not.
    //
    if (FALSE == ffrObject::ffrIsFSDB(argv[1])) {
        fprintf(stderr, "%s is not an fsdb file.\n", argv[1]);
        return FSDB_RC_FAILURE;
    }
    
    //  
    // Open the file.
    //
    ffr_obj = ffrObject::ffrOpen3(argv[1]);
    if (NULL == ffr_obj) {
        fprintf(stderr, "ffrObject::ffrOpen3() failed.\n");
        return FSDB_RC_FAILURE;
    }

    //
    // Remember to call ffrReadDataTypeDefByBlkIdx() to read attribute 
    // definitions before reading transactions!!
    //
    uint_T blk_idx = 0;
    ffr_obj->ffrReadDataTypeDefByBlkIdx(blk_idx);
    ffr_obj->ffrReadScopeVarTree();

    for (var_idcode = FSDB_MIN_VAR_IDCODE; 
         var_idcode <= ffr_obj->ffrGetMaxVarIdcode(); var_idcode++) {
        ffr_obj->ffrAddToSignalList(var_idcode);
    }
    ffr_obj->ffrLoadSignals();

    ffrTransInfo *info;
    str_T relation_name;
    for (int idx = 0; idx < ffr_obj->ffrGetMaxVarIdcode(); idx++) {
        if(FSDB_RC_FAILURE == ffr_obj->ffrGetTransInfo((fsdbTransId)idx, info))
            return FSDB_RC_FAILURE;
        printf("Var(%d) has transaction:\n", idx);
        __PrintTransInfo(info);
        if(FSDB_RC_FAILURE == ffr_obj->ffrGetRelationName(*(info->relation_hdl_arr), relation_name))
            return FSDB_RC_FAILURE;
        printf("\t relation_name: %s\n", relation_name);
    }
    
    ffrVCTrvsHdl trvs_hdl;
    for (int idx = 0; idx < ffr_obj->ffrGetMaxVarIdcode(); idx++) {
        trvs_hdl = ffr_obj->ffrCreateVCTrvsHdl(idx);

        if(FSDB_RC_FAILURE == trvs_hdl->ffrGetTransInfo(info))
            continue;
        printf("Var(%d) has transaction:\n", idx);
        __PrintTransInfo(info);
    }

    return rc;
}

static void
__PrintTransInfo(ffrTransInfo *info)
{
    printf("\t trans_id : %d\n", info->trans_id);
    printf("\t attr_count : %d\n", info->attr_count);
    printf("\t relation_count : %d\n",info->relation_count);
    printf("\t status:\n");
    printf("\t label : %s\n", info->label);
    printf("\t begin : (%u, %u)\n", info->begin_time.hltag.H, info->begin_time.hltag.L);
    printf("\t end : (%u, %u)\n", info->end_time.hltag.H, info->end_time.hltag.L);
}
