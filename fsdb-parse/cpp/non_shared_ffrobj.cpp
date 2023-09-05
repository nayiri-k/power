/* *****************************************************************************
// [non_shared_ffrobj.cpp]
//
// Except as specified in the license terms of Synopsys, this material may not be copied, modified,
// re-published, uploaded, executed, or distributed in any way, in any medium,
// in whole or in part, without prior written permission from SPRINGSOFT.
// ****************************************************************************/
//
// Program Name	: non_shared_ffrobj.cpp
//
// Purpose	: Demonstrate how to call fsdb reader APIs to get 
//		  non-shared fsdb reader object.
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

bool 
isSameReaderObj(ffrObject *obj1, ffrObject  *obj2)
{
    bool temp  = false;
    char *str  = "!=";
    if (obj1 == obj2)
    {
        temp = true;
        str = "==";
    }    
    fprintf(stderr, "\t%s\n", str);
    return temp;
}


int
main(int argc, char** argv)
{
    ffrObject* ffr_obj = 
        ffrObject::ffrOpenNonSharedObj("non_shared_ffrobj.fsdb");
    if (NULL == ffr_obj) {
	fprintf(stderr, "ffrObject::ffrOpen() failed.\n");
	exit(FSDB_RC_OBJECT_CREATION_FAILED);
    }

    ffrObject *obj1, *obj2, *obj3, *obj4, *obj5, *obj6, *obj7;
    obj1 = ffrObject::ffrOpenNonSharedObj("non_shared_ffrobj.fsdb");
    obj2 = ffrObject::ffrOpen3("non_shared_ffrobj.fsdb");    
    obj3 = ffrObject::ffrOpen3("non_shared_ffrobj.fsdb");
    obj4 = ffrObject::ffrOpenNonSharedObj("non_shared_ffrobj.fsdb");
    obj5 = ffrObject::ffrOpen3("non_shared_ffrobj.fsdb");
    obj6 = ffrObject::ffrOpen3("non_shared_ffrobj.fsdb");
    obj7 = ffrObject::ffrOpenNonSharedObj("non_shared_ffrobj.fsdb");

    ffr_obj->ffrReadScopeVarTree();

    ffr_obj->ffrClose();
    obj7->ffrClose();
    obj1->ffrClose();
    obj6->ffrClose();
    obj5->ffrClose();
    obj2->ffrClose();
    obj3->ffrClose();
    obj4->ffrClose();
    
    isSameReaderObj(obj2, obj3);
    bool sharedObj_check = false;
    bool nonSharedObj_check = false;
    sharedObj_check     = (isSameReaderObj(obj2, obj3) && 
                           isSameReaderObj(obj3, obj5) && 
                           isSameReaderObj(obj3, obj6));
    nonSharedObj_check  = (isSameReaderObj(ffr_obj, obj2) || 
                            isSameReaderObj(ffr_obj, obj4) || 
                            isSameReaderObj(obj4, obj7)); 

    if ((!sharedObj_check) || nonSharedObj_check)
        fprintf(stderr, "Failed in using the API!");

    return 0;
}

