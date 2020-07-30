/*
class QL_NodeRel : public QL_Node {
    friend class QL_Manager;
    friend class QL_NodeJoin;

public:
    QL_NodeRel(QL_Manager& qlm, RelCatEntry* rEntry);
    ~QL_NodeRel();

    RC OpenIt();
    RC GetNext(char* data);
    RC CloseIt();
    RC GetNextRec(RM_Record& rec);
    RC DeleteNodes();
    RC PrintNode(int numTabs);
    bool IsRelNode();

    RC SetUpNode(int* attrs, int attrlistSize);
    RC UseIndex(int attrNum, int indexNumber, void* data);
    RC OpenIt(void* data);

private:
    RC RetrieveNextRec(RM_Record& rec, char*& recData);
    // relation name, and indicator for whether it's been malloced
    char* relName;
    bool relNameInitialized;

    bool useIndex; // whether to use the index
    int indexNo; // index number to use
    void* value; // equality value for index
    int indexAttr; // index of attribute for the index

    RM_FileHandle fh; // filehandle/scans for retrieving records from relation
    IX_IndexHandle ih;
    RM_FileScan fs;
    IX_IndexScan is;
};
*/

#include "comparators.h"
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

QL_NodeRel::QL_NodeRel(QL_Manager& qlm, RelCatEntry* rEntry)
    : QL_Node(qlm)
{
    relName = (char*)malloc(MAXNAME + 1);
    memcpy(this->relName, rEntry->relName, strlen(rEntry->relName) + 1);
    relNameInitialized = true;
    listsInitialized = false;
    isOpen = false;
    tupleLength = rEntry->tupleLength;

    useIndex = false;
    indexNo = 0;
    indexAttr = 0;
    void* value = nullptr;
    useIndexJoin = false;
}

//free内存
QL_NodeRel::~QL_NodeRel()
{
    if (relNameInitialized == true) {
        free(relName);
        relNameInitialized = false;
    }
    if (listsInitialized == true) {
        free(attrsInRec);
        listsInitialized = false;
    }
}

RC QL_NodeRel::SetUpNode(int* attrs, int attrlistSize)
{
    RC rc = 0;
    //这个size只是个数
    attrsInRecSize = attrlistSize;
    attrsInRec = (int*)malloc(attrlistSize * sizeof(int));
    for (int i = 0; i < attrlistSize; i++)
        attrsInRec[i] = attrs[i];
    listsInitialized = true;
    return rc;
}

RC QL_NodeRel::DeleteNodes()
{
    if (relNameInitialized == true) {
        free(relName);
        relNameInitialized = false;
    }
    if (listsInitialized == true) {
        free(attrsInRec);
        listsInitialized = false;
    }
    return 0;
}

bool QL_NodeRel::IsRelNode()
{
    return true;
}

RC QL_NodeRel::PrintNode(int numTabs)
{
    for (int i = 0; i < numTabs; i++) {
        cout << "\t";
    }
    cout << "--REL: " << relName;
    if (useIndex && !useIndexJoin) {
        cout << " using index on attribute " << qlm.attrEntries[indexAttr].attrName;
        if (value == NULL) {
            cout << endl;
        } else {
            cout << " = ";

            if (qlm.attrEntries[indexAttr].attrType == INT) {
                print_int(value, 4);
            } else if (qlm.attrEntries[indexAttr].attrType == FLOAT) {
                print_float(value, 4);
            } else {
                print_string(value, strlen((char*)value));
            }
            cout << "\n";
        }
    } else if (useIndexJoin && useIndex) {
        cout << " using index join on attribute " << qlm.attrEntries[indexAttr].attrName << endl;
    } else {
        cout << " using filescan." << endl;
    }
    return 0;
}

RC QL_NodeRel::UseIndex(int attrNum, int indexNumber, void* data)
{
    indexNo = indexNumber;
    value = data;
    useIndex = true;
    //记录使用index的attr的idx
    indexAttr = attrNum;
    return (0);
}

RC QL_NodeRel::GetNextRec(RM_Record& rec)
{
    RC rc = 0;
    char* recData;
    if ((rc = RetrieveNextRec(rec, recData))) {
        if (rc == RM_EOF || rc == IX_EOF)
            return QL_EOI;
        return rc;
    }
    return 0;
}

//打开迭代器
RC QL_NodeRel::OpenIt()
{
    RC rc = 0;
    isOpen = true;
    if (useIndex) {
        //useindex则使用索引
        if (rc = qlm.ixm.OpenIndex(relName, indexNo, ih))
            return rc;
        if (rc = is.OpenScan(ih, EQ_OP, value))
            return rc;
        if (rc = qlm.rmm.OpenFile(relName, fh))
            return rc;
    } else {
        //不是有cond?
        if (rc = qlm.rmm.OpenFile(relName, fh))
            return rc;
        if (rc = fs.OpenScan(fh, INT, 4, 0, NO_OP, NULL))
            return rc;
    }
    return rc;
}

RC QL_NodeRel::OpenIt(void* data)
{
    RC rc = 0;
    isOpen = true;
    value = data;
    if (rc = qlm.ixm.OpenIndex(relName, indexNo, ih))
        return rc;
    if (rc = is.OpenScan(ih, EQ_OP, value))
        return rc;
    if (rc = qlm.rmm.OpenFile(relName, fh))
        return rc;
    return 0;
}

//只取pData
RC QL_NodeRel::GetNext(char* data)
{
    RC rc = 0;
    char* recData;
    RM_Record rec;
    if (rc = RetrieveNextRec(rec, recData)) {
        if ((rc == RM_EOF) || (rc == IX_EOF))
            return QL_EOI;
        return rc;
    }
    //rec里面的内存会被自动析构
    memcpy(data, recData, tupleLength);
    return 0;
}

RC QL_NodeRel::CloseIt()
{
    RC rc = 0;
    if (useIndex) {
        if (rc = qlm.rmm.CloseFile(fh))
            return rc;
        if (rc = is.CloseScan())
            return rc;
        if (rc = qlm.ixm.CloseIndex(ih))
            return rc;
    } else {
        if ((rc = fs.CloseScan()) || (rc = qlm.rmm.CloseFile(fh)))
            return rc;
    }
    isOpen = false;
    return rc;
}

RC QL_NodeRel::RetrieveNextRec(RM_Record& rec, char*& recData)
{
    RC rc = 0;
    if (useIndex) {
        RID rid;
        if (rc = is.GetNextEntry(rid))
            return rc;
        if (rc = fh.GetRec(rid, rec))
            return rc;
    } else {
        if (rc = fs.GetNextRec(rec))
            return rc;
    }
    if (rc = rec.GetData(recData))
        return rc;
    return 0;
}
