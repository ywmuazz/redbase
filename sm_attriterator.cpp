#include <cstdio>
#include <iostream>
#include "rm.h"
#include "sm.h"

using namespace std;

SM_AttrIterator::SM_AttrIterator(){
  validIterator = false;
}


SM_AttrIterator::~SM_AttrIterator(){
  if(validIterator == true){
    fs.CloseScan();
  }
  validIterator = false;

}


RC SM_AttrIterator::OpenIterator(RM_FileHandle &fh, char *relName){
  RC rc = 0;
  validIterator = true;
  if((rc = fs.OpenScan(fh, STRING, MAXNAME+1, 0, EQ_OP, relName)))
    return (rc);

  return (0);
}


RC SM_AttrIterator::GetNextAttr(RM_Record &attrRec, AttrCatEntry *&entry){
  RC rc = 0;
  if((rc = fs.GetNextRec(attrRec)))
    return (rc);
  if((rc = attrRec.GetData((char *&)entry)))
    return (rc);
  return (0);
}

/*
 * Close the scan associated with this
 */
RC SM_AttrIterator::CloseIterator(){
  RC rc = 0;
  if((rc = fs.CloseScan()))
    return (rc);
  validIterator = false;
  return (0);
}