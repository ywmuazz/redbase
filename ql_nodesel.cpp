/*
class QL_NodeSel : public QL_Node {
    friend class QL_Manager;

public:
    QL_NodeSel(QL_Manager& qlm, QL_Node& prevNode);
    ~QL_NodeSel();

    RC OpenIt();
    RC GetNext(char* data);
    RC CloseIt();
    RC GetNextRec(RM_Record& rec);
    RC DeleteNodes();
    RC PrintNode(int numTabs);
    bool IsRelNode();
    RC OpenIt(void* data);
    RC UseIndex(int attrNum, int indexNumber, void* data);

    RC AddCondition(const Condition conditions, int condNum);
    RC SetUpNode(int numConds);

private:
    QL_Node& prevNode;

    char* buffer;
};
*/

#include "ix.h"
#include "ql.h"
#include "ql_node.h"
#include "redbase.h"
#include "rm.h"
#include "sm.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace std;

QL_NodeSel::QL_NodeSel(QL_Manager &qlm, QL_Node &prevNode) : QL_Node(qlm), prevNode(prevNode) {
    isOpen=false;
    listsInitialized=false;
    tupleLength=0;
    attrsInRecSize=0;
    condIndex=0;
}

QL_NodeSel::~QL_NodeSel(){
  if(listsInitialized == true){
    free(condList);
    free(attrsInRec);
    free(buffer);
    free(condsInNode);
  }
  listsInitialized = false;
}


RC QL_NodeSel::SetUpNode(int numConds){
    RC rc=0;
    //copy attrIDXList过来
    int *attrListPtr;
    if(rc=prevNode.GetAttrList(attrListPtr,attrsInRecSize))return rc;
    attrsInRec=(int*)malloc(attrsInRecSize*sizeof(int));
    for(int i=0;i<attrsInRecSize;i++)attrsInRec[i]=attrListPtr[i];
    //分配cond
    condList=(Cond*)malloc(numConds * sizeof(Cond));
    for(int i=0;i<numConds;i++)condList[i]={0, NULL, true, NULL, 0, 0, INT};
    //分配关联cond的idx数组
    condsInNode=(int*)malloc(numConds * sizeof(int));
    bzero(condsInNode,numConds * sizeof(int));
    //分配buffer存tuple
    prevNode.GetTupleLength(tupleLength);
    buffer=(char*)malloc(tupleLength);
    bzero(buffer,tupleLength);
    listsInitialized=true;
    return 0;
}

RC QL_NodeSel::OpenIt(){
  RC rc = 0;
  if(rc = prevNode.OpenIt())
    return rc;
  return 0;
}

RC QL_NodeSel::GetNext(char *data){
  RC rc = 0;
  while(true){
    if(rc = prevNode.GetNext(buffer)){
      return rc;
    }
    RC ok = CheckConditions(buffer);
    if(ok == 0)
      break;
  }
  memcpy(data, buffer, tupleLength);
  return 0;
}

RC QL_NodeSel::CloseIt(){
    RC rc=0;
    if(rc=prevNode.CloseIt())return rc;
    return 0;
}

RC QL_NodeSel::GetNextRec(RM_Record &rec){
  RC rc = 0;
  while(true){
    if((rc = prevNode.GetNextRec(rec)))
      return (rc);

    char *pData;
    if((rc = rec.GetData(pData)))
      return (rc);

    RC cond = CheckConditions(pData);
    if(cond ==0)
      break;
  }

  return (0);
}

//会递归打印前一个节点
RC QL_NodeSel::PrintNode(int numTabs){
  for(int i=0; i < numTabs; i++){
    cout << "\t";
  }
  cout << "--SEL: " << endl;
  for(int i = 0; i < condIndex; i++){
    for(int j=0; j <numTabs; j++){
      cout << "\t";
    }
    PrintCondition(qlm.condptr[condsInNode[i]]);
    cout << "\n";
  }
  prevNode.PrintNode(numTabs + 1);

  return (0);
}


RC QL_NodeSel::DeleteNodes(){
  prevNode.DeleteNodes();
  delete &prevNode;
  if(listsInitialized == true){
    free(condList);
    free(attrsInRec);
    free(buffer);
    free(condsInNode);
  }
  listsInitialized = false;
  return (0);
}

bool QL_NodeSel::IsRelNode(){
  return false;
}

RC QL_NodeSel::OpenIt(void *data){
  return (QL_BADCALL);
}

RC QL_NodeSel::UseIndex(int attrNum, int indexNumber, void *data){
  return (QL_BADCALL);
}