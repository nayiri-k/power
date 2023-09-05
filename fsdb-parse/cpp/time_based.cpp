/* *****************************************************************************
// [time_based.cpp]
//
//  Copyright 2009-2009 SPRINGSOFT. All Rights Reserved.
//
// Except as specified in the license terms of SPRINGSOFT, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// NAME : time_based.cpp
//
// PURPOSE : A regression test for time-based vc trvs hdl.
//
// DESCRIPTION : 
//               1. Test the combination of sequence number, view window, and
//                  using one or many APIs to get vc.
//
//                  sequence number: if the fsdb file has sequence number, the
//                                   vc will be returned in ascending sequence 
//                                   number. Otherwise, the order will be
//                                   undetermined.
//
//                  view window: test if only vc within the view window are
//                               returned when view window is set.
//
//                  one/many APIs: 
//                      one API  : ffrGetVarIdcodeXTagVCSeqNum();
//                      many APIs: ffrGetVarIdcode(&var_idcode);
//                                 ffrGetXTag(&time);
//                                 ffrGetSeqNum(&seq_num);
//                      the differnece is that ffrGetVarIdcodeXTagVCSeqNum()
//                      won't align time to view window. However, ffrGetXTag()
//                      does.
//
//               2. Test if time-based vc trvs hdl free is ok.
//
// CREATION DATE : 02/07/2003
//
#ifdef NOVAS_FSDB
    #undef NOVAS_FSDB
#endif

#include <stdlib.h>
#include <stdio.h>
#include "ffrAPI.h"

#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE	1
#endif

/*
#include "string.h"
#include "stdarg.h"
#include "assert.h"
#include "reg_util.h"*/

//
// Constants
//
int       sig_num         = 5;

static bool_T
__TBTraversal(bool_T is_vw_set, str_T file_name);

static fsdbRC
__VerifyVC(ffrObject* ffr_obj);

static void
__PrintVar(ffrTimeBasedVCTrvsHdl tb_vc_trvs_hdl);

static void
PrintAsVerilog(byte_T *ptr, uint_T size);
//
// Main Program
//
int
main(int argc, char *argv[])
{

    //
    // When view window is set, if vc fall out of view window, they will be
    // aligned to view window boundary in ffrCreateTimeBasedVCTrvsHdl().
    //
    if(TRUE != __TBTraversal(TRUE, argv[1]))
        fprintf(stderr, "Test case with view-window : %s passed.\n", argv[1]);
    else {
        fprintf(stderr, "Test case with view_sindow : %s failed.\n", argv[1]);
        return FALSE;
    }
    
    if(TRUE != __TBTraversal(FALSE, argv[1]))
        fprintf(stderr, "Test case with no view-window : %s passed.\n", argv[1]);
    else {
        fprintf(stderr, "Test case with no view_sindow : %s failed.\n", argv[1]);
        return FALSE;
    }

    return TRUE;
}

//
// __TBTraversal()
//
bool_T
__TBTraversal(bool_T is_vw_set, str_T file_name)
{
    ffrObject   *ffr_obj = NULL;
    fsdbRC      rc = FSDB_RC_SUCCESS;
    fsdbXTag    start_xtag, close_xtag;

    //
    // Open the fsdb file for read
    //
    ffr_obj = ffrObject::ffrOpen3(file_name);
    if (NULL == ffr_obj) {
        fprintf(stderr, "Fsdb file(%s) failed to be opened for read.\n", file_name);
        return FALSE;
    }

    if (TRUE == is_vw_set) {
        start_xtag.hltag.H = 0;
        start_xtag.hltag.L = 600;
        close_xtag.hltag.H = 0;
        close_xtag.hltag.L = 800;
        ffr_obj->ffrSetViewWindow(&start_xtag, &close_xtag);
    }

    //
    // Verify the vc in the fsdb file
    //
    rc = __VerifyVC(ffr_obj);
    if (FSDB_RC_SUCCESS == rc)
        fprintf(stderr, "Time-based traversal check passed!!.\n");
    else
        fprintf(stderr, "Time-based traversal check failed!!\n");

    ffr_obj->ffrClose();

    return rc;
}

static fsdbRC
__VerifyVC(ffrObject* ffr_obj)
{
    fsdbVarIdcode           sig_arr[sig_num];
    ffrTimeBasedVCTrvsHdl   tb_vc_trvs_hdl;
    byte_T                  *vc_ptr;
    fsdbRC                  rc = FSDB_RC_SUCCESS;
    uint_T idx;
                                    
    fsdbVarIdcode max_var_idcode = ffr_obj->ffrGetMaxVarIdcode();
    if(sig_num > max_var_idcode)
        sig_num = max_var_idcode-1;
    
    // Read all vars
    ffr_obj->ffrReadScopeVarTree();
        
    for(idx = 0; idx < sig_num; idx++) {
        // Load vc
        ffr_obj->ffrAddToSignalList(idx);
        sig_arr[idx] = idx+1;
    }
    ffr_obj->ffrLoadSignals();

    //
    // Create a time-based traverse handle to encapsulate the signals to be 
    // traversed.
    //
    tb_vc_trvs_hdl = ffr_obj->ffrCreateTimeBasedVCTrvsHdl(sig_num, sig_arr);

    if (NULL == tb_vc_trvs_hdl) {
        fprintf(stderr, "Fail to create time based vc trvs hdl!\n");
        return FSDB_RC_FAILURE;
    }

    if (FSDB_RC_SUCCESS == tb_vc_trvs_hdl->ffrGetVC(&vc_ptr))
        __PrintVar(tb_vc_trvs_hdl);

    //
    // Iterate until no more vc
    //
    while(FSDB_RC_SUCCESS == tb_vc_trvs_hdl->ffrGotoNextVC())
        __PrintVar(tb_vc_trvs_hdl);

    tb_vc_trvs_hdl->ffrFree();
    fprintf(stderr, "Time based vc trvs hdl free test ok\n");

    ffr_obj->ffrUnloadSignals();

    return rc;
}

void
__PrintVar(ffrTimeBasedVCTrvsHdl tb_vc_trvs_hdl)
{
    byte_T                  *vc_ptr;
    fsdbVarIdcode           var_idcode;
    fsdbXTag                time;
    fsdbSeqNum              seq_num;

    bool_T  is_prop, is_transaction = FALSE;
    
    if(TRUE == (is_prop = tb_vc_trvs_hdl->ffrIsItPropVar()))
        fprintf(stderr, " it is  a Propetery Var\n");
    else if(TRUE == (is_transaction = tb_vc_trvs_hdl->ffrIsItStreamVar()))
        fprintf(stderr, " it is  a Transaction Var\n");

    
    if(!is_transaction) {
    //else {
        tb_vc_trvs_hdl->ffrGetVarIdcodeXTagVCSeqNum(&var_idcode, &time, 
                &vc_ptr, &seq_num);

        fprintf(stderr, "(%u %u) => var(%u): seq_num: %u val: ", time.hltag.H, 
            time.hltag.L, var_idcode, seq_num);
        PrintAsVerilog(vc_ptr, 1);
        fprintf(stderr, "bit_size = %d bpb = %d byte_count = %d var_type = ",
                tb_vc_trvs_hdl->ffrGetBitSize(),
                tb_vc_trvs_hdl->ffrGetBytesPerBit(),
                tb_vc_trvs_hdl->ffrGetByteCount());
                
        switch (tb_vc_trvs_hdl->ffrGetVarType()) {
        case FSDB_VT_VCD_EVENT:
    	    fprintf(stderr, "event\n"); 
      	break;
    
        case FSDB_VT_VCD_INTEGER:
    	    fprintf(stderr, "integer\n"); 
    	break;
    
        case FSDB_VT_VCD_PARAMETER:
    	    fprintf(stderr, "parameter\n"); 
    	break;
    
        case FSDB_VT_VCD_REAL:
    	    fprintf(stderr, "real\n"); 
    	break;
    
        case FSDB_VT_VCD_REG:
    	    fprintf(stderr, "reg\n"); 
    	break;
    
        case FSDB_VT_VCD_SUPPLY0:
    	    fprintf(stderr, "supply0\n"); 
    	break;
    
        case FSDB_VT_VCD_SUPPLY1:
    	    fprintf(stderr, "supply1\n"); 
    	break;
    
        case FSDB_VT_VCD_TIME:
    	    fprintf(stderr, "time\n");
    	break;
    
        case FSDB_VT_VCD_TRI:
    	    fprintf(stderr, "tri\n");
    	break;
    
        case FSDB_VT_VCD_TRIAND:
    	    fprintf(stderr, "triand\n");
    	break;
    
        case FSDB_VT_VCD_TRIOR:
    	    fprintf(stderr, "trior\n");
    	break;
    
        case FSDB_VT_VCD_TRIREG:
    	    fprintf(stderr, "trireg\n");
    	break;
    
        case FSDB_VT_VCD_TRI0:
    	    fprintf(stderr, "tri0\n");
    	break;
    
        case FSDB_VT_VCD_TRI1:
    	    fprintf(stderr, "tri1\n");
    	break;
    
        case FSDB_VT_VCD_WAND:
    	    fprintf(stderr, "wand\n");
    	break;
    
        case FSDB_VT_VCD_WIRE:
    	    fprintf(stderr, "wire\n");
    	break;
    
        case FSDB_VT_VCD_WOR:
    	    fprintf(stderr, "wor\n");
    	break;
    
        case FSDB_VT_VHDL_SIGNAL:
    	    fprintf(stderr, "signal\n");
    	break;
    
        case FSDB_VT_VHDL_VARIABLE:
    	    fprintf(stderr, "variable\n");
    	break;
    
        case FSDB_VT_VHDL_CONSTANT:
    	    fprintf(stderr, "constant\n");
    	break;
    
        case FSDB_VT_VHDL_FILE:
    	    fprintf(stderr, "file\n");
    	break;
    
        default:
    	    fprintf(stderr, "unknown_var_type\n");
    	break;
        }
    }
}
/*
** Print as verilog standard values.
** x0z1-z10z 0x0x-1011...
*/
void
PrintAsVerilog(byte_T *ptr, uint_T size) 
{
    const int VALUES_IN_A_SEG          = 8;
    const int VALUES_DUMPED_IN_A_LINE  = 40;
    int i, j, end_idx;
    byte_T a_byte; 
    char val_tbl[] = "01xz"; 

    /*
    ** Dump like:
    ** (0000~0019) z1x1-0001 00z0-11z0 01z1-0x00 0011-0101 0110-0000
    ** (0020~0039) 10z0-0101 x0x0-11x0 0x01-01x1 1011-1101 0110-0110
    */
    for(i = 0; i < size; i++) {
        //
        // Don't print starting and ending indices.
        //
        //if (0 == (i % VALUES_DUMPED_IN_A_LINE)) {
        //    end_idx = MIN(size, i+(VALUES_DUMPED_IN_A_LINE-1));
        //    fprintf(stderr, "(%04u~%04u) ", i, end_idx);
        //}
        //

        a_byte = *(ptr+i);

        if (a_byte < 4)
            fprintf(stderr, "%c", val_tbl[a_byte]);
        else
            fprintf(stderr, "?");

        if (3 == (i % VALUES_IN_A_SEG) && (i < size-1))
            fprintf(stderr, "-");

        if ((VALUES_IN_A_SEG - 1) == (i % VALUES_IN_A_SEG)) {
            /* Dump line break */
            if ((VALUES_DUMPED_IN_A_LINE - 1) == 
                (i % VALUES_DUMPED_IN_A_LINE)) {
                fprintf(stderr, "\n");
            }
            else {
                /* Dump a separating space */
                fprintf(stderr, " ");
            }
        }
    }

    if ((VALUES_DUMPED_IN_A_LINE - 1) != (i % VALUES_DUMPED_IN_A_LINE)) {
        fprintf(stderr, "\n");
    }
}

