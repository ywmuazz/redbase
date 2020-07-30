/*
class QL_NodeProj : public QL_Node {
    friend class QL_Manager;

public:
    QL_NodeProj(QL_Manager& qlm, QL_Node& prevNode);
    ~QL_NodeProj();

    RC OpenIt();
    RC GetNext(char* data);
    RC CloseIt();
    RC GetNextRec(RM_Record& rec);
    RC DeleteNodes();
    RC PrintNode(int numTabs);
    bool IsRelNode();
    RC OpenIt(void* data);
    RC UseIndex(int attrNum, int indexNumber, void* data);

    // Add a projection by specifying the index of the attribute to keep
    RC AddProj(int attrIndex);
    RC ReconstructRec(char* data); // reconstruct the record, and put it in data
    RC SetUpNode(int numAttrToKeep);

private:
    QL_Node& prevNode; // previous node

    int numAttrsToKeep; // # of attributes to keep

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

QL_NodeProj::QL_NodeProj(QL_Manager& qlm, QL_Node& prevNode)
    : QL_Node(qlm)
    , prevNode(prevNode)
{
    isOpen = false;
    listsInitialized = false;
    attrsInRecSize = 0;
    tupleLength = 0;
}

QL_NodeProj::~QL_NodeProj()
{
    if (listsInitialized == true) {
        free(attrsInRec);
        free(buffer);
    }
    listsInitialized = false;
}

RC QL_NodeProj::SetUpNode(int numAttrToKeep)
{
    RC rc = 0;
    attrsInRec = (int*)malloc(sizeof(int) * numAttrsToKeep);
    bzero(attrsInRec, sizeof(int) * numAttrsToKeep);
    attrsInRecSize = 0;
    int bufLength;
    prevNode.GetTupleLength(bufLength);
    buffer = (char*)malloc(bufLength);
    bzero(buffer, bufLength);
    listsInitialized = true;
    return 0;
}

//proj和sele都只调前一节点的it
RC QL_NodeProj::OpenIt()
{
    RC rc = 0;
    if (rc = prevNode.OpenIt())
        return rc;
    return 0;
}

RC QL_NodeProj::GetNext(char* data)
{
    RC rc = 0;
    if (rc = prevNode.GetNext(buffer))
        return rc;

    ReconstructRec(data);
    return 0;
}

RC QL_NodeProj::CloseIt()
{
    RC rc = 0;
    if (rc = prevNode.CloseIt())
        return rc;
    return 0;
}

//把attr的len加起来，并在内部数组记录下proj到哪些attr(记idx)
RC QL_NodeProj::AddProj(int attrIndex)
{
    RC rc = 0;
    tupleLength += qlm.attrEntries[attrIndex].attrLength;
    attrsInRec[attrsInRecSize] = attrIndex;
    attrsInRecSize++;
    return 0;
}

RC QL_NodeProj::ReconstructRec(char* data)
{
    RC rc = 0;
    int currIdx = 0;
    int* attrsInPrevNode;
    int numAttrsInPrevNode;
    if (rc = prevNode.GetAttrList(attrsInPrevNode, numAttrsInPrevNode))
        return rc;
    for (int i = 0; i < attrsInRecSize; i++) {
        int bufIdx = 0;
        //计算这个attr在原tuple中的offset,其实有O(N)算法的
        for (int j = 0; j < numAttrsInPrevNode; j++) {
            if (attrsInRec[i] == attrsInPrevNode[j]) {
                break;
            }
            int prevNodeIdx = attrsInPrevNode[j];
            bufIdx += qlm.attrEntries[prevNodeIdx].attrLength;
        }
        int attrIdx = attrsInRec[i];
        memcpy(data + currIdx, buffer + bufIdx, qlm.attrEntries[attrIdx].attrLength);
        currIdx += qlm.attrEntries[attrIdx].attrLength;
    }
    return 0;
}


RC QL_NodeProj::PrintNode(int numTabs){
  for(int i=0; i < numTabs; i++){
    cout << "\t";
  }
  cout << "--PROJ: \n";
  for(int i = 0; i < attrsInRecSize; i++){
    int index = attrsInRec[i];
    cout << " " << qlm.attrEntries[index].relName << "." << qlm.attrEntries[index].attrName;
  }
  cout << "\n";
  prevNode.PrintNode(numTabs + 1);

  return 0;
}

RC QL_NodeProj::GetNextRec(RM_Record &rec){
  return QL_BADCALL;
}


RC QL_NodeProj::DeleteNodes(){
  prevNode.DeleteNodes();
  delete &prevNode;
  if(listsInitialized == true){
    free(attrsInRec);
    free(buffer);
  }
  listsInitialized = false;
  return 0;
}

bool QL_NodeProj::IsRelNode(){
  return false;
}

RC QL_NodeProj::OpenIt(void *data){
  return QL_BADCALL;
}

RC QL_NodeProj::UseIndex(int attrNum, int indexNumber, void *data){
  return QL_BADCALL;
}
