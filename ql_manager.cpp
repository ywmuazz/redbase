/*
class QL_Manager {
    friend class QL_Node;
    friend class Node_Rel;
    friend class QL_NodeJoin;
    friend class QL_NodeRel;
    friend class QL_NodeSel;
    friend class QL_NodeProj;
    friend class QO_Manager;
public:
    QL_Manager (SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm);
    ~QL_Manager();                       // Destructor

    RC Select  (int nSelAttrs,           // # attrs in select clause
        const RelAttr selAttrs[],        // attrs in select clause
        int   nRelations,                // # relations in from clause
        const char * const relations[],  // relations in from clause
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

    RC Insert  (const char *relName,     // relation to insert into
        int   nValues,                   // # values
        const Value values[]);           // values to insert

    RC Delete  (const char *relName,     // relation to delete from
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

    RC Update  (const char *relName,     // relation to update
        const RelAttr &updAttr,          // attribute to update
        const int bIsValue,              // 1 if RHS is a value, 0 if attribute
        const RelAttr &rhsRelAttr,       // attr on RHS to set LHS equal to
        const Value &rhsValue,           // or value to set attr equal to
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

private:
  // Resets the class variables for the next query command
  RC Reset();
  // Checks whether an attribute is valid for a query command
  bool IsValidAttr(const RelAttr attr);
  // Parses the relations, making sure they're in the database, and there aren't duplicates
  RC ParseRelNoDup(int nRelations, const char * const relations[]);
  
  // Parses the select attributes, making sure they're valid
  RC ParseSelectAttrs(int nSelAttrs, const RelAttr selAttrs[]);
  // Gets the attribute attrCatEntry associated with a specific attribute
  RC GetAttrCatEntry(const RelAttr attr, AttrCatEntry *&entry);
  // Get the attribute attrEntries position associated with a specific attribute
  RC GetAttrCatEntryPos(const RelAttr attr, int &index);
  // Parses the conditions to make sure that they're valid
  RC ParseConditions(int nConditions, const Condition conditions[]);

  // Set up the global relation and attribute lists (attrEntries and relEntries)
  // for one relation (used in Delete and Update)
  RC SetUpOneRelation(const char *relName);
  // Check that the update attributes are valid
  RC CheckUpdateAttrs(const RelAttr &updAttr,
                      const int bIsValue,
                      const RelAttr &rhsRelAttr,
                      const Value &rhsValue);
  // Run delete given the top node
  RC RunDelete(QL_Node *topNode);
  // Run update given the top node
  RC RunUpdate(QL_Node *topNode, const RelAttr &updAttr,
                      const int bIsValue,
                      const RelAttr &rhsRelAttr,
                      const Value &rhsValue);
  // Runs select given the top node
  RC RunSelect(QL_Node *topNode);
  // Inserts a set of values into a relation
  RC InsertIntoRelation(const char *relName, int tupleLength, int nValues, const Value values[]);
  // Inserts a new record in all the indices belonging to that relation
  RC InsertIntoIndex(char *recbuf, RID recRID);
  // Creates a record from the values entered via the query command
  RC CreateRecord(char *recbuf, AttrCatEntry *aEntries, int nValues, const Value values[]);


  // Used for update for keeping track of attributes that have indices.
  RC CleanUpRun(Attr* attributes, RM_FileHandle &relFH);
  RC SetUpRun(Attr* attributes, RM_FileHandle &relFH);

  // Cleans up the nodes after a run
  RC CleanUpNodes(QL_Node *topNode);
  // Count the number of conditions associated with a relation
  RC CountNumConditions(int relIndex, int &numConds);
  // Set up the first node in creating the query tree
  RC SetUpFirstNode(QL_Node *&topNode);
  // Sets up the entire query tree and returns the top node in topNode
  RC SetUpNodes(QL_Node *&topNode, int nSelAttrs, const RelAttr selAttrs[]);
  // Sets up the entire query tree based on the return of the Query optimizer and returns the 
  // top node in topNode
  RC SetUpNodesWithQO(QL_Node *&topNode, QO_Rel* qorels, int nSelAttrs, const RelAttr selAttrs[]);
  RC SetUpFirstNodeWithQO(QL_Node *&topNode, QO_Rel* qorels);
  RC JoinRelationWithQO(QL_Node *&topNode, QO_Rel* qorels, QL_Node *currNode, int relIndex);
  RC RecalcCondToRel(QO_Rel* qorels);
  RC AttrToRelIndex(const RelAttr attr, int& relIndex);
  // Creates a join node and a relation node for the relation specified, and
  // returns the top node in topNode
  RC JoinRelation(QL_Node *&topNode, QL_Node *currNode, int relIndex);

  // Set up the printing parameters
  RC SetUpPrinter(QL_Node *topNode, DataAttrInfo *attributes);
  RC SetUpPrinterInsert(DataAttrInfo *attributes);



  RM_Manager &rmm;
  IX_Manager &ixm;
  SM_Manager &smm;

  // helpers for query parsing
  // maps from relation name to its index number in relEntries
  std::map<std::string, int> relToInt;
  // maps from attribute name to a set of its relation names
  std::map<std::string, std::set<std::string> > attrToRel;
  // maps from relation name to the index of its first relation
  std::map<std::string, int> relToAttrIndex;
  // maps from condition to the relation it's meant to be grouped with
  std::map<int, int> conditionToRel;

  // Holds the relation relCat entries
  RelCatEntry *relEntries;
  // Holds the attribute attrCat entries. The attributes are ordered in the
  // appropriate order following the relation, in the order of the relations
  AttrCatEntry *attrEntries;

  // number of attributes, relations and conditions
  int nAttrs;
  int nRels;
  int nConds;
  // whether it's an update or not (whether to use indices or not)
  bool isUpdate;

  // pointer to the condition list
  const Condition *condptr;
  

};
*/

#include "ix.h"
#include "ql.h"
#include "ql_node.h"
#include "redbase.h"
#include "rm.h"
#include "sm.h"
#include <cassert>
#include <cfloat>
#include <cstdio>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#undef max
#undef min
#include <algorithm>

using namespace std;

QL_Manager::QL_Manager(SM_Manager& smm, IX_Manager& ixm, RM_Manager& rmm)
    : rmm(rmm)
    , ixm(ixm)
    , smm(smm)
{
    assert(&smm && &ixm && &rmm);
}

QL_Manager::~QL_Manager()
{
}

RC QL_Manager::Reset()
{
    relToInt.clear();
    relToAttrIndex.clear();
    attrToRel.clear();
    conditionToRel.clear();
    nAttrs = 0;
    nRels = 0;
    nConds = 0;
    condptr = NULL;
    isUpdate = false;
    return (0);
}

//attr rel cond
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
    int nRelations, const char* const relations[],
    int nConditions, const Condition conditions[])
{
    int i;
    RC rc = 0;
    if (smm.printPageStats) {
        smm.ResetPageStats();
    }

    cout << "Select\n";

    cout << "   nSelAttrs = " << nSelAttrs << "\n";
    for (i = 0; i < nSelAttrs; i++)
        cout << "   selAttrs[" << i << "]:" << selAttrs[i] << "\n";

    cout << "   nRelations = " << nRelations << "\n";
    for (i = 0; i < nRelations; i++)
        cout << "   relations[" << i << "] " << relations[i] << "\n";

    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    // 去重rel
    if ((rc = ParseRelNoDup(nRelations, relations)))
        return (rc);

    Reset();
    nRels = nRelations;
    nConds = nConditions;
    condptr = conditions;

    //取得rel entry 以及relname->在entry数组的index 的map。
    relEntries = (RelCatEntry*)malloc(sizeof(RelCatEntry) * nRelations);
    bzero(relEntries, sizeof(RelCatEntry) * nRelations);
    if ((rc = smm.GetAllRels(relEntries, nRelations, relations, nAttrs, relToInt))) {
        free(relEntries);
        return (rc);
    }

    //取得attr entry
    //处理出rel->index的映射，index是该rel在attrEntries数组中第一个属于该rel的attr的index
    //处理出attr->rel的映射，注意rel可能有多个
    attrEntries = (AttrCatEntry*)malloc(sizeof(AttrCatEntry) * nAttrs);
    int slot = 0;
    for (int i = 0; i < nRelations; i++) {
        string relString(relEntries[i].relName);
        relToAttrIndex[relString] = slot;
        if (rc = smm.GetAttrForRel(relEntries + i, attrEntries + slot, attrToRel)) {
            free(relEntries);
            free(attrEntries);
            return rc;
        }
        slot += relEntries[i].attrCount;
    }

    //验证attr和cond的有效性
    if (rc = ParseSelectAttrs(nSelAttrs, selAttrs)) {
        free(relEntries);
        free(attrEntries);
        return rc;
    }

    if (rc = ParseConditions(nConditions, conditions)) {
        free(relEntries);
        free(attrEntries);
        return rc;
    }

    QL_Node* rootNode;
    float cost, tupleEst;
    //USEQO
    if (rc = SetUpNodes(rootNode, nSelAttrs, selAttrs)) {
        return rc;
    }
    if (rc = RunSelect(rootNode))
        return rc;
    //USEQO
    if (bQueryPlans) {
        cout << "PRINTING QUERY PLAN" << endl;
        ;
        rootNode->PrintNode(0);
    }

    if (rc = CleanUpNodes(rootNode))
        return rc;
    if (smm.printPageStats) {
        puts("");
        smm.PrintPageStats();
    }
    free(relEntries);
    free(attrEntries);

    return rc;
}

RC QL_Manager::RunSelect(QL_Node* topNode)
{
    RC rc = 0;
    int finalTupLength;
    topNode->GetTupleLength(finalTupLength);
    int* attrList;
    int attrListSize;
    if (rc = topNode->GetAttrList(attrList, attrListSize))
        return rc;
    //搞个用来存打印信息的结构体数组
    //设置一下printer,然后就可以开始打印结果了
    DataAttrInfo* attributes = (DataAttrInfo*)malloc(attrListSize * sizeof(DataAttrInfo));
    if (rc = SetUpPrinter(topNode, attributes))
        return rc;
    Printer printer(attributes, attrListSize);
    printer.PrintHeader(cout);

    //一边获取结果一边输出
    if (rc = topNode->OpenIt())
        return rc;
    RC it_rc = 0;
    char* buffer = (char*)malloc(finalTupLength);
    it_rc = topNode->GetNext(buffer);
    while (it_rc == 0) {
        printer.Print(cout, buffer);
        it_rc = topNode->GetNext(buffer);
    }
    free(buffer);
    if (rc = topNode->CloseIt())
        return rc;
    printer.PrintFooter(cout);
    free(attributes);
    return 0;
}

//检验带rel的attr存在于rel中，或不带rel的attr只存在某个rel
bool QL_Manager::IsValidAttr(const RelAttr attr)
{
    if (attr.relName) {
        string relString(attr.relName);
        string attrString(attr.attrName);
        auto it1 = relToInt.find(relString);
        auto it2 = attrToRel.find(attrString);
        if (it1 == relToInt.end() || it2 == attrToRel.end())
            return false;
        auto it3 = (it2->second).find(relString);
        if (it3 != (it2->second).end())
            return true;
        return false;
    } else {
        string attrString(attr.attrName);
        auto it = attrToRel.find(attrString);
        //没有这个属性或有这个属性但rel size不是1
        if (it == attrToRel.end() || (it->second).size() != 1)
            return false;
        return true;
    }
}

RC QL_Manager::SetUpNodes(QL_Node*& topNode, int nSelAttrs, const RelAttr selAttrs[])
{
    RC rc = 0;
    if (rc = SetUpFirstNode(topNode))
        return rc;
    QL_Node* currNode = topNode;
    //nRels为成员变量
    for (int i = 1; i < nRels; i++) {
        if (rc = JoinRelation(topNode, currNode, i))
            return rc;
        currNode = topNode;
    }

    if (nSelAttrs == 1 && strncmp(selAttrs[0].attrName, "*", strlen(selAttrs[0].attrName)) == 0)
        return 0;
    QL_NodeProj* projNode = new QL_NodeProj(*this, *currNode);
    projNode->SetUpNode(nSelAttrs);
    for (int i = 0; i < nSelAttrs; i++) {
        int attrIndex = 0;
        //看看selAttrs[i]在这次询问处理出来的attrCatEntry中是第几个
        if (rc = GetAttrCatEntryPos(selAttrs[i], attrIndex))
            return rc;
        if (rc = projNode->AddProj(attrIndex))
            return rc;
    }
    topNode = projNode;
    return rc;
}

//USEQO
//RC QL_Manager::SetUpNodesWithQO(QL_Node *&topNode, QO_Rel* qorels, int nSelAttrs, const RelAttr selAttrs[]){

//relIndex是该rel在relCatEntries中的index
//将该rel与currNode进行join，然后返回新node存在topNode中
RC QL_Manager::JoinRelation(QL_Node*& topNode, QL_Node* currNode, int relIndex)
{
    RC rc = 0;
    bool useIndex = false;
    QL_NodeRel* relNode = new QL_NodeRel(*this, relEntries + relIndex);
    RelCatEntry& relEntry = relEntries[relIndex];
    int attrCount = relEntry.attrCount;
    //attrList是个存index的数组，对应该rel每个属性在attrcatentries中的index
    int* attrList = (int*)malloc(attrCount * sizeof(int));
    bzero(attrList, attrCount * sizeof(int));
    string relString(relEntry.relName);
    int start = relToAttrIndex[relString];
    for (int i = 0; i < attrCount; i++)
        attrList[i] = i + start;
    //relNode需要这个attrlist来初始化节点
    relNode->SetUpNode(attrList, attrCount);
    free(attrList);
    int numConds;
    CountNumConditions(relIndex, numConds);

    QL_NodeJoin* joinNode = new QL_NodeJoin(*this, *currNode, *relNode);
    if (rc = joinNode->SetUpNode(numConds))
        return rc;
    topNode = joinNode;

    for (int i = 0; i < nConds; i++) {
        if (conditionToRel[i] == relIndex) {
            bool added = false;
            //右为value,符号为equal,尝试用索引去获取左边的匹配元组
            if (condptr[i].op == EQ_OP && !condptr[i].bRhsIsAttr && useIndex == false) {
                int index = 0;
                //求出左属性在attr数组中的index
                if (rc = GetAttrCatEntryPos(condptr[i].lhsAttr, index))
                    return rc;
                if (attrEntries[index].indexNo != -1) {
                    if (rc = relNode->UseIndex(index, attrEntries[index].indexNo, condptr[i].rhsValue.data))
                        return rc;
                    added = true;
                    useIndex = true;
                }
            } //eq，右为attr
            else if (condptr[i].op == EQ_OP && condptr[i].bRhsIsAttr && useIndex == false && !relNode->useIndex) {
                int index1, index2;
                if ((rc = GetAttrCatEntryPos(condptr[i].lhsAttr, index1)))
                    return rc;
                if ((rc = GetAttrCatEntryPos(condptr[i].rhsAttr, index2)))
                    return rc;
                //找出两个attr所属rel在relcatentries中的idx
                int relIdx1, relIdx2;
                AttrToRelIndex(condptr[i].lhsAttr, relIdx1);
                AttrToRelIndex(condptr[i].rhsAttr, relIdx2);
                if (relIdx2 == relIndex && attrEntries[index2].indexNo != -1) {
                    if (rc = joinNode->UseIndexJoin(index1, index2, attrEntries[index2].indexNo))
                        return rc;
                    added = true;
                    useIndex = true;
                } else if (relIdx1 == relIndex && attrEntries[index1].indexNo != -1) {
                    if (rc = joinNode->UseIndexJoin(index2, index1, attrEntries[index1].indexNo))
                        return rc;
                    added = true;
                    useIndex = true;
                }
            }
            //没有用上index优化join过程，就暴力
            if (!added) {
                if (rc = topNode->AddCondition(condptr[i], i))
                    return rc;
            }
        }
    }
    return rc;
}

//USEQO
//RC QL_Manager::RecalcCondToRel(QO_Rel* qorels)

//求attr所在的rel在catentries中的idx
RC QL_Manager::AttrToRelIndex(const RelAttr attr, int& relIndex)
{
    if (attr.relName != NULL) {
        string relName(attr.relName);
        relIndex = relToInt[relName];
    } else {
        string attrName(attr.attrName);
        set<string> relSet = attrToRel[attrName];
        relIndex = relToInt[*relSet.begin()];
    }

    return (0);
}

//USEQO
//RC QL_Manager::JoinRelationWithQO(QL_Node *&topNode, QO_Rel* qorels, QL_Node *currNode, int qoIdx)

RC QL_Manager::SetUpFirstNode(QL_Node*& topNode)
{
    RC rc = 0;
    bool useSelNode = false;
    bool useIndex = false;
    int relIndex = 0;

    QL_NodeRel* relNode = new QL_NodeRel(*this, relEntries);
    topNode = relNode;

    RelCatEntry& relEntry = relEntries[relIndex];
    //处理出该rel的attr在attrcatentries中的index
    int* attrList = (int*)malloc(relEntry.attrCount * sizeof(int));
    for (int i = 0; i < relEntry.attrCount; i++) {
        attrList[i] = 0;
    }
    string relString(relEntry.relName);
    int start = relToAttrIndex[relString];
    for (int i = 0; i < relEntry.attrCount; i++)
        attrList[i] = start + i;
    //设置node节点
    relNode->SetUpNode(attrList, relEntry.attrCount);
    free(attrList);

    //求出与某rel相关的cond的个数，传入rel的index
    int numConds;
    CountNumConditions(0, numConds);

    for (int i = 0; i < nConds; i++) {
        if (conditionToRel[i] == 0) {
            bool added = false;
            //等值、右为attr、未用索引、非update操作
            if (condptr[i].op == EQ_OP && !condptr[i].bRhsIsAttr && useIndex == false && isUpdate == false) {
                int index = 0;
                if ((rc = GetAttrCatEntryPos(condptr[i].lhsAttr, index)))
                    return (rc);
                if ((attrEntries[index].indexNo != -1)) { // add only if there is an index on this attribute
                    if ((rc = relNode->UseIndex(index, attrEntries[index].indexNo, condptr[i].rhsValue.data)))
                        return (rc);
                    added = true;
                    useIndex = true;
                }
            }
            //建立select节点?
            if (!added && !useSelNode) {
                QL_NodeSel* selNode = new QL_NodeSel(*this, *relNode);
                if ((rc = selNode->SetUpNode(numConds)))
                    return (rc);
                topNode = selNode;
                useSelNode = true;
            }
            //直接加condition
            if (!added) {
                if ((rc = topNode->AddCondition(condptr[i], i)))
                    return (rc);
            }
        }
    }
    return 0;
}

//USEQO
//RC QL_Manager::SetUpFirstNodeWithQO(QL_Node *&topNode, QO_Rel* qorels)

//计算与rel关联的cond的个数，注意关联是指该cond以它为max
RC QL_Manager::CountNumConditions(int relIndex, int& numConds)
{
    RC rc = 0;
    numConds = 0;
    for (int i = 0; i < nConds; i++) {
        if (conditionToRel[i] == relIndex)
            numConds++;
    }
    return (0);
}

//给relattr取entry，本质就类型转换
RC QL_Manager::GetAttrCatEntry(const RelAttr attr, AttrCatEntry*& entry)
{
    RC rc = 0;
    int index = 0;
    if ((rc = GetAttrCatEntryPos(attr, index)))
        return (rc);
    entry = attrEntries + index;
    return (0);
}

//给relattr取entry的index
RC QL_Manager::GetAttrCatEntryPos(const RelAttr attr, int& index)
{
    string relString = attr.relName ? attr.relName : *(attrToRel[attr.attrName].begin());
    int relNum = relToInt[relString];
    int numAttrs = relEntries[relNum].attrCount;
    int start = relToAttrIndex[relString];
    for (int i = start; i < start + numAttrs; i++) {
        if (strcmp(attr.attrName, attrEntries[i].attrName) == 0) {
            index = i;
            return 0;
        }
    }
    return QL_ATTRNOTFOUND;
}

//检查属性的有效性
RC QL_Manager::ParseSelectAttrs(int nSelAttrs, const RelAttr selAttrs[])
{
    if ((nSelAttrs == 1 && strncmp(selAttrs[0].attrName, "*", strlen(selAttrs[0].attrName)) == 0))
        return (0);
    for (int i = 0; i < nSelAttrs; i++) {
        if (!IsValidAttr(selAttrs[i]))
            return (QL_ATTRNOTFOUND);
    }
    return (0);
}

//检查cond的有效性
RC QL_Manager::ParseConditions(int nConditions, const Condition conditions[])
{
    RC rc = 0;
    for (int i = 0; i < nConditions; i++) {
        if (!IsValidAttr(conditions[i].lhsAttr))
            return QL_ATTRNOTFOUND;
        AttrCatEntry* lhsEntry;
        if (rc = GetAttrCatEntry(conditions[i].lhsAttr, lhsEntry))
            return rc;
        if (!conditions[i].bRhsIsAttr) {
            if (lhsEntry->attrType != conditions[i].rhsValue.type)
                return QL_BADCOND;
            conditionToRel[i] = relToInt[lhsEntry->relName];
        } else {
            if (!IsValidAttr(conditions[i].rhsAttr))
                return QL_ATTRNOTFOUND;
            AttrCatEntry* rhsEntry;
            if (rc = GetAttrCatEntry(conditions[i].rhsAttr, rhsEntry))
                return rc;
            if (lhsEntry->attrType != rhsEntry->attrType)
                return QL_BADCOND;
            conditionToRel[i] = max(relToInt[lhsEntry->relName], relToInt[rhsEntry->relName]);
        }
    }
    return 0;
}

//检查有无重，重就返回RC但不在这做处理
RC QL_Manager::ParseRelNoDup(int nRelations, const char* const relations[])
{
    set<string> s;
    for (int i = 0; i < nRelations; i++) {
        string relString(relations[i]);
        if (s.find(relString) != s.end())
            return QL_DUPRELATION;
        s.insert(relString);
    }
    return 0;
}

//插入
RC QL_Manager::Insert(const char* relName, int nValues, const Value values[])
{
    cout << "Insert\n";
    cout << "   relName = " << relName << "\n";
    cout << "   nValues = " << nValues << "\n";
    for (int i = 0; i < nValues; i++)
        cout << "   values[" << i << "]:" << values[i] << "\n";

    RC rc = 0;
    Reset();
    //仅一个rel 
    relEntries = (RelCatEntry *)malloc(sizeof(RelCatEntry));
    *relEntries = (RelCatEntry) {"\0", 0, 0, 0, 0, 0, false};

    //会在setup里面去把relcatentry读进来
    if ((rc = SetUpOneRelation(relName))) {
        free(relEntries);
        return (rc);
    }

    //attr个数不对
    if (relEntries->attrCount != nValues) {
        free(relEntries);
        return (QL_BADINSERT);
    }

    attrEntries = (AttrCatEntry*)malloc(relEntries->attrCount * sizeof(AttrCatEntry));
    for (int i = 0; i < relEntries->attrCount; i++)
        attrEntries[i] = (AttrCatEntry) { "\0", "\0", 0, INT, 0, 0, 0, 0, FLT_MIN, FLT_MAX };

    //跟select一样，先拿个attrcatentries数组，只不过这里只有一个rel的所有attr，而select有多个rel的attr
    if ((rc = smm.GetAttrForRel(relEntries, attrEntries, attrToRel))) {
        free(relEntries);
        free(attrEntries);
        return (rc);
    }

    bool badFormat = false;
    for (int i = 0; i < nValues; i++) {
        if (values[i].type != attrEntries[i].attrType)
            badFormat = true;
        if (attrEntries[i].attrType == STRING && strlen((char*)values[i].data) > attrEntries[i].attrLength)
            badFormat = true;
        if (badFormat)
            break;
    }
    if (badFormat) {
        free(relEntries);
        free(attrEntries);
        return (QL_BADINSERT);
    }
    rc = InsertIntoRelation(relName, relEntries->tupleLength, nValues, values);

    free(relEntries);
    free(attrEntries);

    return (rc);
}

//经过调用者对tuple的验证后，调用该方法进行插入
RC QL_Manager::InsertIntoRelation(const char* relName, int tupleLength, int nValues, const Value values[])
{
    RC rc = 0;

    //处理出打印信息的数组给printer使用
    DataAttrInfo* printAttributes = (DataAttrInfo*)malloc(relEntries->attrCount * sizeof(DataAttrInfo));
    if ((rc = SetUpPrinterInsert(printAttributes)))
        return (rc);
    Printer printer(printAttributes, relEntries->attrCount);
    printer.PrintHeader(cout);

    RM_FileHandle relfh;
    //文件就是relname直接打开拿filehandle
    if (rc = rmm.OpenFile(relName, relfh))
        return rc;
    char* recbuf = (char*)malloc(tupleLength);
    CreateRecord(recbuf, attrEntries, nValues, values);

    //插入
    RID rid;
    if (rc = relfh.InsertRec(recbuf, rid)) {
        free(recbuf);
        return rc;
    }
    printer.Print(cout, recbuf);
    //attrcatentries是成员变量，直接遍历找有无索引，有直接更新
    if (rc = InsertIntoIndex(recbuf, rid)) {
        free(recbuf);
        return rc;
    }
    printer.PrintFooter(cout);
    free(recbuf);
    free(printAttributes);
    rc = rmm.CloseFile(relfh);
    return rc;
}

RC QL_Manager::InsertIntoIndex(char* recbuf, RID rid)
{
    RC rc = 0;
    for (int i = 0; i < relEntries->attrCount; i++) {
        AttrCatEntry& entry = attrEntries[i];
        if (entry.indexNo != -1) {
            IX_IndexHandle ih;
            //打开索引 插 关
            if (rc = ixm.OpenIndex(relEntries->relName, entry.indexNo, ih))
                return rc;
            if (rc = ih.InsertEntry(recbuf + entry.offset, rid))
                return rc;
            if (rc = ixm.CloseIndex(ih))
                return rc;
        }
    }
    return rc;
}

//insert会调，用来make一段内存存values
//recbuf已经申请好空间了
RC QL_Manager::CreateRecord(char* recbuf, AttrCatEntry* aEntries, int nValues, const Value values[])
{
    for (int i = 0; i < nValues; i++) {
        memcpy(recbuf + aEntries[i].offset, values[i].data, aEntries[i].attrLength);
    }
    return 0;
}

//把relcatentry读进relEntries
RC QL_Manager::SetUpOneRelation(const char* relName)
{
    RC rc = 0;
    RelCatEntry* rEntry;
    RM_Record relRec;
    if ((rc = smm.GetRelEntry(relName, relRec, rEntry))) {
        return (rc);
    }
    memcpy(relEntries, rEntry, sizeof(RelCatEntry));
    nRels = 1;
    nAttrs = rEntry->attrCount;
    string relString(relName);
    relToInt[relString] = 0;
    relToAttrIndex[relString] = 0;
    return 0;
}

RC QL_Manager::Delete(const char* relName, int nConditions, const Condition conditions[])
{
    cout << "Delete\n";

    cout << "   relName = " << relName << "\n";
    cout << "   nCondtions = " << nConditions << "\n";
    for (int i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    RC rc = 0;
    Reset();
    condptr = conditions;
    nConds = nConditions;

    relEntries = (RelCatEntry*)malloc(sizeof(RelCatEntry));
    *relEntries = (RelCatEntry) { "\0", 0, 0, 0, 0, 0, true };

    if (rc = SetUpOneRelation(relName)) {
        free(relEntries);
        return rc;
    }

    attrEntries = (AttrCatEntry*)malloc(relEntries->attrCount * sizeof(AttrCatEntry));
    for (int i = 0; i < relEntries->attrCount; i++)
        *(attrEntries + i) = (AttrCatEntry) { "\0", "\0", 0, INT, 0, 0, 0, 0, FLT_MIN, FLT_MAX };

    //这个函数仅获取一个rel的attr
    if ((rc = smm.GetAttrForRel(relEntries, attrEntries, attrToRel))) {
        free(relEntries);
        free(attrEntries);
        return (rc);
    }
    //check以及处理出condToRel
    if (rc = ParseConditions(nConditions, conditions)) {
        free(relEntries);
        free(attrEntries);
        return (rc);
    }

    QL_Node* topNode;
    if (rc = SetUpFirstNode(topNode))
        return rc;

    if (bQueryPlans) {
        cout << "PRINTING QUERY PLAN" << endl;
        topNode->PrintNode(0);
    }

    if ((rc = RunDelete(topNode)))
        return (rc);
    //清除节点
    if (rc = CleanUpNodes(topNode))
        return (rc);

    free(relEntries);
    free(attrEntries);

    return 0;
}

RC QL_Manager::RunDelete(QL_Node* topNode)
{
    RC rc = 0;
    int finalTupLength;
    topNode->GetTupleLength(finalTupLength);
    int attrListSize, *attrList;
    if (rc = topNode->GetAttrList(attrList, attrListSize))
        return rc;
    DataAttrInfo* printAttributes = (DataAttrInfo*)malloc(attrListSize * sizeof(DataAttrInfo));
    if (rc = SetUpPrinter(topNode, printAttributes))
        return rc;
    Printer printer(printAttributes, attrListSize);
    printer.PrintHeader(cout);

    RM_FileHandle relfh;
    Attr* attributes = (Attr*)malloc(sizeof(Attr) * relEntries->attrCount);
    //打开index处理出attributes数组,因为可能要多次删除，防止频繁打开关闭index，事先开着
    if ((rc = SetUpRun(attributes, relfh))) {
        smm.CleanUpAttr(attributes, relEntries->attrCount);
        return (rc);
    }

    if (rc = topNode->OpenIt())
        return rc;

    RM_Record rec;
    RID rid;

    while (true) {
        if (rc = topNode->GetNextRec(rec)) {
            if (rc == QL_EOI)
                break;
            return rc;
        }
        char* pData;
        if ((rc = rec.GetRid(rid)) || (rc = rec.GetData(pData)))
            return rc;
        printer.Print(cout, pData);
        if (rc = relfh.DeleteRec(rid))
            return rc;
        for (int i = 0; i < relEntries->attrCount; i++) {
            if ((attributes[i].indexNo != -1) && (rc = attributes[i].ih.DeleteEntry(pData + attributes[i].offset, rid)))
                return rc;
        }
    }

    if ((rc = topNode->CloseIt()))
        return (rc);
    if ((rc = CleanUpRun(attributes, relfh)))
        return (rc);
    printer.PrintFooter(cout);
    free(printAttributes);

    return 0;
}

RC QL_Manager::CleanUpNodes(QL_Node* topNode)
{
    RC rc = 0;
    if ((rc = topNode->DeleteNodes()))
        return (rc);
    delete topNode;
    return (0);
}

//处理出一个Attr数组，里面存有属性的索引
RC QL_Manager::SetUpRun(Attr* attributes, RM_FileHandle& relFH)
{
    RC rc = 0;
    for (int i = 0; i < relEntries->attrCount; i++) {
        IX_IndexHandle ih;
        attributes[i] = (Attr) { 0, 0, 0, 0, ih, NULL };
    }
    //打开所有索引
    if ((rc = smm.PrepareAttr(relEntries, attributes)))
        return rc;
    //打开rmfile
    if ((rc = rmm.OpenFile(relEntries->relName, relFH)))
        return rc;
    return 0;
}

RC QL_Manager::CleanUpRun(Attr* attributes, RM_FileHandle& relFH)
{
    RC rc = 0;
    if (rc = rmm.CloseFile(relFH))
        return rc;
    if (rc = smm.CleanUpAttr(attributes, relEntries->attrCount))
        return rc;
    return 0;
}

RC QL_Manager::CheckUpdateAttrs(const RelAttr& updAttr,
    const int bIsValue,
    const RelAttr& rhsRelAttr,
    const Value& rhsValue)
{
    RC rc = 0;
    if (!IsValidAttr(updAttr))
        return QL_ATTRNOTFOUND;
    if (bIsValue) {
        AttrCatEntry* entry;
        if (rc = GetAttrCatEntry(updAttr, entry))
            return rc;
        if (entry->attrType != rhsValue.type)
            return QL_BADUPDATE;
        if (entry->attrType == STRING) {
            int newValueSize = strlen((char*)rhsValue.data);
            if (newValueSize > entry->attrLength)
                return QL_BADUPDATE;
        }
    } else {
        if (!IsValidAttr(rhsRelAttr))
            return QL_ATTRNOTFOUND;
        AttrCatEntry *lentry, *rentry;
        if ((rc = GetAttrCatEntry(updAttr, lentry)) || (rc = GetAttrCatEntry(rhsRelAttr, rentry)))
            return rc;
        if (lentry->attrType != rentry->attrType)
            return QL_BADUPDATE;
        if (lentry->attrType == STRING) {
            if (rentry->attrLength > lentry->attrLength)
                return QL_BADUPDATE;
        }
    }
    return rc;
}

//注意rhs可以是属性，这个属性可以是同一个元组的非本次更新的其他同类型属性
RC QL_Manager::RunUpdate(QL_Node* topNode, const RelAttr& updAttr,
    const int bIsValue,
    const RelAttr& rhsRelAttr,
    const Value& rhsValue)
{

    RC rc = 0;
    int finalTupLength, attrListSize;
    topNode->GetTupleLength(finalTupLength);
    int* attrList;
    if ((rc = topNode->GetAttrList(attrList, attrListSize)))
        return (rc);
    //设置一下printer,只有insert是不用node来设置的
    DataAttrInfo* attributes = (DataAttrInfo*)malloc(attrListSize * sizeof(DataAttrInfo));
    if ((rc = SetUpPrinter(topNode, attributes)))
        return (rc);
    Printer printer(attributes, attrListSize);
    printer.PrintHeader(cout);

    RM_FileHandle relfh;
    if (rc = rmm.OpenFile(relEntries->relName, relfh))
        return rc;
    int idx1, idx2;
    if (rc = GetAttrCatEntryPos(updAttr, idx1))
        return rc;
    if (!bIsValue && (rc = GetAttrCatEntryPos(rhsRelAttr, idx2)))
        return rc;

    IX_IndexHandle ih;
    if (attrEntries[idx1].indexNo != -1 && (rc = ixm.OpenIndex(relEntries->relName, attrEntries[idx1].indexNo, ih)))
        return rc;

    //遍历更新
    if (rc = topNode->OpenIt())
        return rc;
    RM_Record rec;
    RID rid;
    while (true) {
        if (rc = topNode->GetNextRec(rec)) {
            if (rc == QL_EOI)
                break;
            return rc;
        }
        char* pData;
        if ((rc = rec.GetRid(rid)) || (rc = rec.GetData(pData)))
            return rc;
        if (attrEntries[idx1].indexNo != -1 && (rc = ih.DeleteEntry(pData + attrEntries[idx1].offset, rid)))
            return rc;
        if (bIsValue) {
            if (attrEntries[idx1].attrType == STRING) {
                int len = min(attrEntries[idx1].attrLength, (int)strlen((char*)rhsValue.data) + 1);
                memcpy(pData + attrEntries[idx1].offset, (char*)rhsValue.data, len);
            } else {
                memcpy(pData + attrEntries[idx1].offset, (char*)rhsValue.data, attrEntries[idx1].attrLength);
            }
        } else {
            if (attrEntries[idx2].attrLength >= attrEntries[idx1].attrLength) {
                memcpy(pData + attrEntries[idx1].offset, pData + attrEntries[idx2].offset, attrEntries[idx1].attrLength);
            } else {
                memcpy(pData + attrEntries[idx1].offset, pData + attrEntries[idx2].offset, attrEntries[idx2].attrLength);
                pData[attrEntries[idx1].offset + attrEntries[idx2].attrLength] = '\0';
            }
        }
        if (rc = relfh.UpdateRec(rec))
            return rc;
        printer.Print(cout, pData);
        //前面已经删除了该entry了，现在插入新的
        if (attrEntries[idx1].indexNo != -1) {
            if (ih.InsertEntry(pData + attrEntries[idx1].offset, rid))
                return rc;
        }
    }

    if ((rc = topNode->CloseIt()))
        return (rc);

    if ((attrEntries[idx1].indexNo != -1)) {
        if ((rc = ixm.CloseIndex(ih)))
            return (rc);
    }
    if ((rc = rmm.CloseFile(relfh)))
        return (rc);

    printer.PrintFooter(cout);
    free(attributes);

    return 0;
}

RC QL_Manager::Update(const char* relName,
    const RelAttr& updAttr,
    const int bIsValue,
    const RelAttr& rhsRelAttr,
    const Value& rhsValue,
    int nConditions, const Condition conditions[])
{

    cout << "Update\n";

    cout << "   relName = " << relName << "\n";
    cout << "   updAttr:" << updAttr << "\n";
    if (bIsValue)
        cout << "   rhs is value: " << rhsValue << "\n";
    else
        cout << "   rhs is attribute: " << rhsRelAttr << "\n";

    cout << "   nCondtions = " << nConditions << "\n";
    for (int i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    RC rc = 0;
    Reset();
    condptr = conditions;
    nConds = nConditions;
    isUpdate = true;

    //读取relEntries和attrEntries(成员变量)

    relEntries = (RelCatEntry*)malloc(sizeof(RelCatEntry));
    *relEntries = (RelCatEntry) { "\0", 0, 0, 0, 0, 0, true };

    if (rc = SetUpOneRelation(relName)) {
        free(relEntries);
        return rc;
    }

    attrEntries = (AttrCatEntry*)malloc(relEntries->attrCount * sizeof(AttrCatEntry));
    for (int i = 0; i < relEntries->attrCount; i++) {
        attrEntries[i] = (AttrCatEntry) { "\0", "\0", 0, INT, 0, 0, 0, 0, FLT_MIN, FLT_MAX };
    }
    if (rc = smm.GetAttrForRel(relEntries, attrEntries, attrToRel)) {
        free(relEntries);
        free(attrEntries);
        return rc;
    }

    if (rc = ParseConditions(nConditions, conditions)) {
        free(relEntries);
        free(attrEntries);
        return rc;
    }

    QL_Node* topNode;

    if ((rc = SetUpFirstNode(topNode)))
        return (rc);

    if (bQueryPlans) {
        cout << "PRINTING QUERY PLAN" << endl;
        topNode->PrintNode(0);
    }

    if ((rc = RunUpdate(topNode, updAttr, bIsValue, rhsRelAttr, rhsValue)))
        return (rc);
    if (rc = CleanUpNodes(topNode))
        return (rc);

    free(relEntries);
    free(attrEntries);

    return 0;
}

//可能是join之后的rel，动态生成的表，offset也是动态生成的，生成attrlist的算法在node中
RC QL_Manager::SetUpPrinter(QL_Node* topNode, DataAttrInfo* attributes)
{
    RC rc = 0;
    int* attrList;
    int attrListSize;
    if (rc = topNode->GetAttrList(attrList, attrListSize))
        return rc;
    for (int i = 0; i < attrListSize; i++) {
        int index = attrList[i];
        memcpy(attributes[i].relName, attrEntries[index].relName, MAXNAME + 1);
        memcpy(attributes[i].attrName, attrEntries[index].attrName, MAXNAME + 1);
        attributes[i].attrType = attrEntries[index].attrType;
        attributes[i].attrLength = attrEntries[index].attrLength;
        attributes[i].indexNo = attrEntries[index].indexNo;
        int offset, length;
        //每次都暴力算offset
        if (rc = topNode->IndexToOffset(index, offset, length))
            return rc;
        attributes[i].offset = offset;
    }
    return 0;
}

//对于insert操作没node,就是原本的rel,设置printer
RC QL_Manager::SetUpPrinterInsert(DataAttrInfo* attributes)
{
    RC rc = 0;
    for (int i = 0; i < relEntries->attrCount; i++) {
        memcpy(attributes[i].relName, attrEntries[i].relName, MAXNAME + 1);
        memcpy(attributes[i].attrName, attrEntries[i].attrName, MAXNAME + 1);
        attributes[i].attrType = attrEntries[i].attrType;
        attributes[i].attrLength = attrEntries[i].attrLength;
        attributes[i].indexNo = attrEntries[i].indexNo;
        attributes[i].offset = attrEntries[i].offset;
    }
    return 0;
}