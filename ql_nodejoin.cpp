/*
class QL_NodeJoin : public QL_Node {
    friend class QL_Manager;

public:
    QL_NodeJoin(QL_Manager& qlm, QL_Node& node1, QL_Node& node2);
    ~QL_NodeJoin();

    RC OpenIt();
    RC GetNext(char* data);
    RC CloseIt();
    RC GetNextRec(RM_Record& rec);
    RC DeleteNodes();
    RC PrintNode(int numTabs);
    bool IsRelNode();
    RC OpenIt(void* data);
    RC UseIndex(int attrNum, int indexNumber, void* data);

    RC UseIndexJoin(int indexAttr, int subNodeAttr, int indexNumber);
    RC AddCondition(const Condition conditions, int condNum);
    RC SetUpNode(int numConds);

private:
    QL_Node& node1;
    QL_Node& node2;
    int firstNodeSize;
    char* buffer;

    bool gotFirstTuple;

    bool useIndexJoin;
    int indexAttr;
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

QL_NodeJoin::QL_NodeJoin(QL_Manager& qlm, QL_Node& node1, QL_Node& node2)
    : QL_Node(qlm)
    , node1(node1)
    , node2(node2)
{
    isOpen = false;
    listsInitialized = false;
    attrsInRecSize = 0;
    tupleLength = 0;
    condIndex = 0;
    firstNodeSize = 0;
    gotFirstTuple = false;
    useIndexJoin = false;
}

QL_NodeJoin::~QL_NodeJoin()
{
    if (listsInitialized = true) {
        free(attrsInRec);
        free(condList);
        free(buffer);
        free(condsInNode);
    }
}

RC QL_NodeJoin::SetUpNode(int numConds)
{
    RC rc = 0;
    int *attrList1, *attrList2;
    int attrListSize1, attrListSize2;
    if ((rc = node1.GetAttrList(attrList1, attrListSize1)) || (rc = node2.GetAttrList(attrList2, attrListSize2)))
        return rc;
    attrsInRecSize = attrListSize1 + attrListSize2;
    for (int i = 0; i < attrListSize1; i++)
        attrsInRec[i] = attrList1[1];
    for (int i = 0; i < attrListSize2; i++)
        attrsInRec[i + attrListSize1] = attrList2[i];
    condList = (Cond*)malloc(numConds * sizeof(Cond));
    for (int i = 0; i < numConds; i++)
        condList[i] = { 0, NULL, true, NULL, 0, 0, INT };
    condsInNode = (int*)malloc(numConds * sizeof(int));
    int tupleLength1, tupleLength2;
    node1.GetTupleLength(tupleLength1);
    node2.GetTupleLength(tupleLength2);
    tupleLength = tupleLength1 + tupleLength2;
    firstNodeSize = tupleLength1;
    buffer = (char*)malloc(tupleLength);
    bzero(buffer, tupleLength);
    listsInitialized = true;
    return 0;
}

RC QL_NodeJoin::OpenIt()
{
    RC rc = 0;
    if (!useIndexJoin) {
        if ((rc = node1.OpenIt()) || (rc = node2.OpenIt()))
            return rc;
    } else {
        if (rc = node1.OpenIt())
            return rc;
    }
    gotFirstTuple = false;
    return 0;
}

RC QL_NodeJoin::GetNext(char* data)
{
    RC rc = 0;
    if (gotFirstTuple == false && !useIndexJoin) {
        if (rc = node1.GetNext(buffer))
            return rc;
    } else if (gotFirstTuple == false && useIndexJoin) {
        if (rc = node1.GetNext(buffer))
            return rc;
        int offset, length;
        IndexToOffset(indexAttr, offset, length);
        if (rc = node2.OpenIt(buffer + offset))
            return rc;
    }
    gotFirstTuple = true;
    if (!useIndexJoin) {
        for (;;) {
            if ((rc = node2.GetNext(buffer + firstNodeSize)) == QL_EOI) {
                if (rc = node1.GetNext(buffer))
                    return rc;
                //二重循环
                if ((rc = node2.CloseIt()) || (rc = node2.OpenIt()))
                    return rc;
                if (rc = node2.GetNext(buffer + firstNodeSize))
                    return rc;
            }
            if (CheckConditions(buffer) == 0)
                break;
        }
    } else {
        for (;;) {
            if (rc = node2.GetNext(buffer + firstNodeSize)) {
                int found = false;
                while (found == false) {
                    if (rc = node1.GetNext(buffer))
                        return rc;
                    int offset, length;
                    IndexToOffset(indexAttr, offset, length);
                    if ((rc = node2.CloseIt()) || (rc = node2.OpenIt(buffer + offset)))
                        return (rc);
                    if(rc=node2.GetNext(buffer+firstNodeSize)){
                        if(rc!=QL_EOI)return rc;
                    }else{
                        found=true;
                    }
                }
            }
            if(CheckConditions(buffer))break;
        }
    }
    memcpy(data,buffer,tupleLength);
    return 0;
}

RC QL_NodeJoin::GetNextRec(RM_Record &rec)
{
  return (QL_BADCALL);
}

RC QL_NodeJoin::CloseIt()
{
  RC rc = 0;
  if ((rc = node1.CloseIt()) || (rc = node2.CloseIt()))
    return (rc);
  gotFirstTuple = false;
  return 0;
}

RC QL_NodeJoin::PrintNode(int numTabs)
{
  for (int i = 0; i < numTabs; i++)
  {
    cout << "\t";
  }
  cout << "--JOIN: \n";
  for (int i = 0; i < condIndex; i++)
  {
    for (int j = 0; j < numTabs; j++)
    {
      cout << "\t";
    }
    PrintCondition(qlm.condptr[condsInNode[i]]);
    cout << "\n";
  }
  node1.PrintNode(numTabs + 1);
  node2.PrintNode(numTabs + 1);
  return (0);
}

RC QL_NodeJoin::DeleteNodes()
{
  node1.DeleteNodes();
  node2.DeleteNodes();
  delete &node1;
  delete &node2;
  if (listsInitialized == true)
  {
    free(attrsInRec);
    free(condList);
    free(condsInNode);
    free(buffer);
  }
  listsInitialized = false;
  return 0;
}

RC QL_NodeJoin::UseIndexJoin(int indexAttr, int subNodeAttr, int indexNumber)
{
  useIndexJoin = true;
  this->indexAttr = indexAttr;
  node2.UseIndex(subNodeAttr, indexNumber, NULL);
  node2.useIndexJoin = true;
  return 0;
}
bool QL_NodeJoin::IsRelNode()
{
  return false;
}

RC QL_NodeJoin::OpenIt(void *data)
{
  return QL_BADCALL;
}

RC QL_NodeJoin::UseIndex(int attrNum, int indexNumber, void *data)
{
  return QL_BADCALL;
}