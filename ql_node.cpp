/*
typedef struct Cond{
  int offset1; // offset of LHS attribute
  // comparator - depends on which operator is used in this condition
  bool (*comparator) (void * , void *, AttrType, int); 
  bool isValue; // whether this is a value or not
  void* data; // the pointer to the data
  int offset2; // the offset of RHS attribute
  int length; // length of LHS attribute
  int length2; // length of RHS value/attribute
  AttrType type; // attribute type
} Cond;

class QL_Node {
  friend class QL_Manager;
  friend class QL_NodeJoin;
public:
  QL_Node(QL_Manager &qlm);
  ~QL_Node();

  virtual RC OpenIt() = 0;
  virtual RC GetNext(char * data) = 0;
  virtual RC GetNextRec(RM_Record &rec) = 0;
  virtual RC CloseIt() = 0;
  virtual RC DeleteNodes() = 0;
  virtual RC PrintNode(int numTabs) = 0;
  virtual bool IsRelNode() = 0;
  virtual RC OpenIt(void *data) = 0;
  virtual RC UseIndex(int attrNum, int indexNumber, void *data) = 0;
  // Prints a condition
  RC PrintCondition(const Condition condition);
  // Given a index of an attribute, returns its offset and length
  RC IndexToOffset(int index, int &offset, int &length);
  // Add a condition to the node
  RC AddCondition(const Condition conditions, int condNum);
  // Check to see if the conditions to this node are met
  RC CheckConditions(char *recData);
  // Get the attribute list for this node, and the list size
  RC GetAttrList(int *&attrList, int &attrListSize);
  // Get the tuple lenght of this node
  RC GetTupleLength(int &tupleLength);
protected:
  QL_Manager &qlm; // Reference to QL manager
  bool isOpen; // Whether the node is open or not
  int tupleLength; // the length of a tuple in this node
  int *attrsInRec; // list of indices of the attribute in this node
  int attrsInRecSize; // size of the attribute list
  bool listsInitialized; // indicator for whether memory has been initialized

  // The condition list
  Cond *condList;
  int condIndex; // the # of conditions currently in this node
  int* condsInNode; // maps the condition from the index in this list, to the 
                    // index in the list in QL
   bool useIndexJoin;
};
*/

#include "ql_node.h"
#include "comparators.h"
#include "ix.h"
#include "node_comps.h"
#include "ql.h"
#include "redbase.h"
#include "rm.h"
#include "sm.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace std;

QL_Node::QL_Node(QL_Manager& qlm)
    : qlm(qlm)
{
}

QL_Node::~QL_Node()
{
}

RC QL_Node::PrintCondition(const Condition condition)
{

    RC rc = 0;
    cout << (condition.lhsAttr.relName ? condition.lhsAttr.relName : "NULL") << "." << condition.lhsAttr.attrName;
    switch (condition.op) {
    case EQ_OP:
        cout << "=";
        break;
    case LT_OP:
        cout << "<";
        break;
    case GT_OP:
        cout << ">";
        break;
    case LE_OP:
        cout << "<=";
        break;
    case GE_OP:
        cout << ">=";
        break;
    case NE_OP:
        cout << "!=";
        break;
    default:
        return (QL_BADCOND);
    }
    if (condition.bRhsIsAttr) {
        cout << (condition.rhsAttr.relName ? condition.rhsAttr.relName : "NULL") << "." << condition.rhsAttr.attrName;
    } else {
        if (condition.rhsValue.type == INT) {
            print_int(condition.rhsValue.data, 4);
        } else if (condition.rhsValue.type == FLOAT) {
            print_float(condition.rhsValue.data, 4);
        } else {
            print_string(condition.rhsValue.data, strlen((const char*)condition.rhsValue.data));
        }
    }
    return 0;
}

//计算给一个index(是这个属性在attrCatEntry中的index)，返回它在本rel的tuple内存块中的offset
RC QL_Node::IndexToOffset(int index, int& offset, int& length)
{
    offset = 0;
    //attrsInRec是该node(rel)的所有attr的idx
    //一直累加前缀的len，直到遇到所要求的attr的index
    for (int i = 0; i < attrsInRecSize; i++) {
        if (attrsInRec[i] == index) {
            length = qlm.attrEntries[attrsInRec[i]].attrLength;
            return (0);
        }
        offset += qlm.attrEntries[attrsInRec[i]].attrLength;
    }
    return (QL_ATTRNOTFOUND);
}

//查index然后初始化condition存入node
RC QL_Node::AddCondition(const Condition condition, int condNum)
{
    RC rc = 0;
    int index1, index2;
    int offset1, offset2;
    int length1, length2;

    if ((rc = qlm.GetAttrCatEntryPos(condition.lhsAttr, index1)) || (rc = QL_Node::IndexToOffset(index1, offset1, length1)))
        return (rc);

    condList[condIndex].offset1 = offset1;
    condList[condIndex].length = length1;
    condList[condIndex].type = qlm.attrEntries[index1].attrType;

    if (condition.bRhsIsAttr) {
        if ((rc = qlm.GetAttrCatEntryPos(condition.rhsAttr, index2)) || (rc = QL_Node::IndexToOffset(index2, offset2, length2)))
            return (rc);
        condList[condIndex].offset2 = offset2;
        condList[condIndex].isValue = false;
        condList[condIndex].length2 = length2;
    } else {
        condList[condIndex].isValue = true;
        condList[condIndex].data = condition.rhsValue.data;
        condList[condIndex].length2 = strlen((char*)condition.rhsValue.data);
    }

    switch (condition.op) {
    case EQ_OP:
        condList[condIndex].comparator = &nequal;
        break;
    case LT_OP:
        condList[condIndex].comparator = &nless_than;
        break;
    case GT_OP:
        condList[condIndex].comparator = &ngreater_than;
        break;
    case LE_OP:
        condList[condIndex].comparator = &nless_than_or_eq_to;
        break;
    case GE_OP:
        condList[condIndex].comparator = &ngreater_than_or_eq_to;
        break;
    case NE_OP:
        condList[condIndex].comparator = &nnot_equal;
        break;
    default:
        return (QL_BADCOND);
    }
    condsInNode[condIndex] = condNum;
    condIndex++;

    return (0);
}

//检查是否满足条件，逻辑很简单，遍历node上每一个cond，这里面已经有该cond右值是val还是attr，左右两个值的offset
//当然如果右边是val那么就没有offset而是存在cond中
//预先已经设置好了comp，只要调用就可以返回是否满足条件
//int和float的比较没问题，让我费解的是为什么字符串要把长的截成跟短的一样长再比较，这样不就只比前缀？
RC QL_Node::CheckConditions(char* recData)
{
    RC rc = 0;
    for (int i = 0; i < condIndex; i++) {
        int offset1 = condList[i].offset1;
        if (!condList[i].isValue) {
            if (condList[i].type != STRING || condList[i].length == condList[i].length2) {
                int offset2 = condList[i].offset2;
                bool comp = condList[i].comparator((void*)(recData + offset1), (void*)(recData + offset2),
                    condList[i].type, condList[i].length);

                if (comp == false) {
                    return (QL_CONDNOTMET);
                }
            }
            else if (condList[i].length < condList[i].length2) {
                int offset2 = condList[i].offset2;
                char* shorter = (char*)malloc(condList[i].length + 1);
                memset((void*)shorter, 0, condList[i].length + 1);
                memcpy(shorter, recData + offset1, condList[i].length);
                shorter[condList[i].length] = '\0';
                bool comp = condList[i].comparator(shorter, (void*)(recData + offset2), condList[i].type, condList[i].length + 1);
                free(shorter);
                if (comp == false)
                    return (QL_CONDNOTMET);
            } else {
                int offset2 = condList[i].offset2;
                char* shorter = (char*)malloc(condList[i].length2 + 1);
                memset((void*)shorter, 0, condList[i].length2 + 1);
                memcpy(shorter, recData + offset2, condList[i].length2);
                shorter[condList[i].length2] = '\0';
                bool comp = condList[i].comparator((void*)(recData + offset1), shorter, condList[i].type, condList[i].length2 + 1);
                free(shorter);
                if (comp == false)
                    return (QL_CONDNOTMET);
            }
        }
        else {
            if (condList[i].type != STRING || condList[i].length == condList[i].length2) {
                bool comp = condList[i].comparator((void*)(recData + offset1), condList[i].data,
                    condList[i].type, condList[i].length);

                if (comp == false)
                    return (QL_CONDNOTMET);
            }
            else if (condList[i].length < condList[i].length2) {
                char* shorter = (char*)malloc(condList[i].length + 1);
                memset((void*)shorter, 0, condList[i].length + 1);
                memcpy(shorter, recData + offset1, condList[i].length);
                shorter[condList[i].length] = '\0';
                bool comp = condList[i].comparator(shorter, condList[i].data, condList[i].type, condList[i].length + 1);
                free(shorter);
                if (comp == false)
                    return (QL_CONDNOTMET);
            } else {
                char* shorter = (char*)malloc(condList[i].length2 + 1);
                memset((void*)shorter, 0, condList[i].length2 + 1);
                memcpy(shorter, condList[i].data, condList[i].length2);
                shorter[condList[i].length2] = '\0';
                bool comp = condList[i].comparator((void*)(recData + offset1), shorter, condList[i].type, condList[i].length2 + 1);
                free(shorter);
                if (comp == false)
                    return (QL_CONDNOTMET);
            }
        }
    }

    return (0);
}

RC QL_Node::GetAttrList(int*& attrList, int& attrListSize)
{
    attrList = attrsInRec;
    attrListSize = attrsInRecSize;
    return (0);
}

RC QL_Node::GetTupleLength(int& tupleLength)
{
    tupleLength = this->tupleLength;
    return (0);
}
