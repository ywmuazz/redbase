#include "ix.h"
#include "pf.h"
#include "redbase.h"
#include "rm.h"
#include "sm.h"
#include "statistics.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>
#undef max
#include "stddef.h"
#include <cfloat>
#include <set>
#include <string>
#include <vector>

using namespace std;

extern StatisticsMgr* pStatisticsMgr;
extern void PF_Statistics();

//一些util,类型转换
bool recInsert_int(char* location, string value, int length)
{
    int num;
    istringstream ss(value);
    ss >> num;
    if (ss.fail())
        return false;
    memcpy(location, (char*)&num, length);
    return true;
}

bool recInsert_float(char* location, string value, int length)
{
    float num;
    istringstream ss(value);
    ss >> num;
    if (ss.fail())
        return false;
    memcpy(location, (char*)&num, length);
    return true;
}

bool recInsert_string(char* location, string value, int length)
{
    if (value.length() >= length) {
        memcpy(location, value.c_str(), length);
        return true;
    }
    memcpy(location, value.c_str(), value.length() + 1);
    return true;
}

SM_Manager::SM_Manager(IX_Manager& ixm, RM_Manager& rmm)
    : ixm(ixm)
    , rmm(rmm)
{
    printIndex = false;
    useQO = true;
    calcStats = false;
    //未实现pfpagestat,防止空指针
    printPageStats = false;
}

SM_Manager::~SM_Manager()
{
}

RC SM_Manager::OpenDb(const char* dbName)
{
    RC rc = 0;
    if (strlen(dbName) > MAX_DB_NAME) {
        return SM_INVALIDDB;
    }
    if (chdir(dbName) < 0) {
        cerr << "Cannot chdir to " << dbName << "\n";
        return (SM_INVALIDDB);
    }
    if (rc = rmm.OpenFile("relcat", relcatFH)) {
        return SM_INVALIDDB;
    }
    if (rc = rmm.OpenFile("attrcat", attrcatFH)) {
        return SM_INVALIDDB;
    }
    return 0;
}

RC SM_Manager::CloseDb()
{

    RC rc = 0;
    if ((rc = rmm.CloseFile(relcatFH))) {
        return (rc);
    }
    if ((rc = rmm.CloseFile(attrcatFH))) {
        return (rc);
    }

    return (0);
}

bool SM_Manager::isValidAttrType(AttrInfo attribute)
{

    AttrType type = attribute.attrType;
    int length = attribute.attrLength;
    if (type == INT && length == 4)
        return true;
    if (type == FLOAT && length == 4)
        return true;
    if (type == STRING && (length > 0) && length < MAXSTRINGLEN)
        return true;

    return false;
}

RC SM_Manager::CreateTable(const char* relName,
    int attrCount,
    AttrInfo* attributes)
{
    cout << "CreateTable\n"
         << "   relName     =" << relName << "\n"
         << "   attrCount   =" << attrCount << "\n";
    for (int i = 0; i < attrCount; i++)
        cout << "   attributes[" << i << "].attrName=" << attributes[i].attrName
             << "   attrType="
             << (attributes[i].attrType == INT ? "INT" : attributes[i].attrType == FLOAT ? "FLOAT" : "STRING")
             << "   attrLength=" << attributes[i].attrLength << "\n";

    RC rc = 0;
    set<string> relAttributes;
    if (attrCount > MAXATTRS || attrCount < 1) {
        return SM_BADREL;
    }
    if (strlen(relName) > MAXNAME) {
        return SM_BADRELNAME;
    }

    int totalRecSize = 0;
    for (int i = 0; i < attrCount; i++) {
        if (strlen(attributes[i].attrName) > MAXNAME) {
            return SM_BADATTR;
        }
        if (!isValidAttrType(attributes[i])) {
            return SM_BADATTR;
        }
        totalRecSize += attributes[i].attrLength;
        string attrString(attributes[i].attrName);
        bool exists = (relAttributes.find(attrString) != relAttributes.end());
        if (exists)
            return SM_BADREL;
        else
            relAttributes.insert(attrString);
    }

    if (rc = rmm.CreateFile(relName, totalRecSize))
        return SM_BADRELNAME;
    RID rid;
    int currOffset = 0;
    for (int i = 0; i < attrCount; i++) {
        AttrInfo attr = attributes[i];
        if (rc = InsertAttrCat(relName, attr, currOffset, i))
            return rc;
        currOffset += attr.attrLength;
    }
    if (rc = InsertRelCat(relName, attrCount, totalRecSize))
        return rc;
    if ((rc = attrcatFH.ForcePages()) || (rc = relcatFH.ForcePages()))
        return rc;
    return 0;
}

//插个表得到基本信息进去，包含元组大小属性个数索引个数元组个数等
RC SM_Manager::InsertRelCat(const char* relName, int attrCount, int recSize)
{
    RC rc = 0;
    RelCatEntry* rEntry = (RelCatEntry*)malloc(sizeof(RelCatEntry));
    memset(rEntry, 0, sizeof(RelCatEntry));
    *rEntry = (RelCatEntry) { "\0", 0, 0, 0, 0 };
    memcpy(rEntry->relName, relName, MAXNAME + 1);
    rEntry->tupleLength = recSize;
    rEntry->attrCount = attrCount;
    rEntry->indexCount = 0;
    rEntry->numTuples = 0;
    rEntry->statsInitialized = false;
    RID relRID;
    rc = relcatFH.InsertRec((char*)rEntry, relRID);
    free(rEntry);
    return rc;
}

RC SM_Manager::InsertAttrCat(const char* relName, AttrInfo attr, int offset, int attrNum)
{
    RC rc;
    AttrCatEntry* aEntry = (AttrCatEntry*)malloc(sizeof(AttrCatEntry));
    memset((void*)aEntry, 0, sizeof(*aEntry));
    *aEntry = (AttrCatEntry) { "\0", "\0", 0, INT, 0, 0, 0 };
    memcpy(aEntry->relName, relName, MAXNAME + 1);
    memcpy(aEntry->attrName, attr.attrName, MAXNAME + 1);
    aEntry->offset = offset;
    aEntry->attrType = attr.attrType;
    aEntry->attrLength = attr.attrLength;
    aEntry->indexNo = NO_INDEXES; // 索引号
    aEntry->attrNum = attrNum; // 它在所有属性中的下标
    // For EX component
    aEntry->numDistinct = 0;
    aEntry->maxValue = FLT_MIN;
    aEntry->minValue = FLT_MAX;

    RID attrRID;
    rc = attrcatFH.InsertRec((char*)aEntry, attrRID);
    free(aEntry);

    return rc;
}

//删表 先获取所有属性并遍历检查有无索引有就删，然后在attr文件中把该table的该属性删掉，最后在rel文件中把该table删掉
RC SM_Manager::DropTable(const char* relName)
{
    cout << "DropTable\n   relName=" << relName << "\n";
    RC rc = 0;

    if (strlen(relName) > MAXNAME) // check for whether this is a valid name
        return (SM_BADRELNAME);
    // Try to destroy the table. This should detect whether the file is there
    if ((rc = rmm.DestroyFile(relName))) {
        return (SM_BADRELNAME);
    }

    // Retrieve the record associated with the relation
    RM_Record relRec;
    RelCatEntry* relEntry;
    if ((rc = GetRelEntry(relName, relRec, relEntry)))
        return (rc);
    int numAttr = relEntry->attrCount;

    // Retrieve all its attributes
    SM_AttrIterator attrIt;
    if ((rc = attrIt.OpenIterator(attrcatFH, const_cast<char*>(relName))))
        return (rc);
    AttrCatEntry* attrEntry;
    RM_Record attrRec;
    for (int i = 0; i < numAttr; i++) {
        // Get the next attribute
        if ((rc = attrIt.GetNextAttr(attrRec, attrEntry))) {
            return (rc);
        }
        // Check whether it has indices. If so, delete it
        if ((attrEntry->indexNo != NO_INDEXES)) {
            if ((rc = DropIndex(relName, attrEntry->attrName)))
                return (rc);
        }
        // Delete that attribute record
        RID attrRID;
        if ((rc = attrRec.GetRid(attrRID)) || (rc = attrcatFH.DeleteRec(attrRID)))
            return (rc);
    }
    if ((rc = attrIt.CloseIterator()))
        return (rc);

    // Delete the record associated with the relation
    RID relRID;
    if ((rc = relRec.GetRid(relRID)) || (rc = relcatFH.DeleteRec(relRID)))
        return (rc);

    return (0);
}

//通过name把这个relentry拿到
RC SM_Manager::GetRelEntry(const char* relName, RM_Record& relRec, RelCatEntry*& entry)
{
    RC rc = 0;
    // Use a scan to search for it
    RM_FileScan fs;
    if ((rc = fs.OpenScan(relcatFH, STRING, MAXNAME + 1, 0, EQ_OP, const_cast<char*>(relName))))
        return (rc);
    if ((rc = fs.GetNextRec(relRec))) // there should be only one entry
        return (SM_BADRELNAME);

    if ((rc = fs.CloseScan()))
        return (rc);

    if ((rc = relRec.GetData((char*&)entry))) // retrieve its data contents
        return (rc);

    return (0);
}

//两个功能看返回值，一个是返回relEntries(通过给定的rel名读entry进内存)，另一个是搞出map 关系名->它在给定的rels中的下标
RC SM_Manager::GetAllRels(RelCatEntry* relEntries, int nRelations, const char* const relations[],
    int& attrCount, map<string, int>& relToInt)
{
    RC rc = 0;
    for (int i = 0; i < nRelations; i++) {
        RelCatEntry* rEntry;
        RM_Record rec;
        if ((rc = GetRelEntry(relations[i], rec, rEntry))) // retrieve this entry
            return (rc);
        *(relEntries + i) = (RelCatEntry) { "\0", 0, 0, 0, 0 };
        memcpy((char*)(relEntries + i), (char*)rEntry, sizeof(RelCatEntry)); // copy it into appropraite spot
        attrCount += relEntries[i].attrCount;

        // create a map from relation name to # in order
        string relString(relEntries[i].relName);
        relToInt.insert({ relString, i });
    }
    return (rc);
}

//给定一个relEntry，返回一个数组aEntry表示具有的列，并把attr->rel的映射加进map里
RC SM_Manager::GetAttrForRel(RelCatEntry* relEntry, AttrCatEntry* aEntry, std::map<std::string, std::set<std::string>>& attrToRel)
{
    RC rc = 0;
    SM_AttrIterator attrIt;
    if (rc = attrIt.OpenIterator(attrcatFH, relEntry->relName))
        return rc;
    RM_Record attrRec;
    AttrCatEntry* attrEntry;
    for (int i = 0; i < relEntry->attrCount; i++) {
        if (rc = attrIt.GetNextAttr(attrRec, attrEntry))
            return rc;
        int slot = attrEntry->attrNum;
        aEntry[slot] = *attrEntry; //(AttrCatEntry) {"\0", "\0", 0, INT, 0, 0, 0};
        attrToRel[aEntry[slot].attrName].insert(relEntry->relName);
    }
    if (rc = attrIt.CloseIterator())
        return rc;
    return 0;
}

RC SM_Manager::CreateIndex(const char* relName,
    const char* attrName)
{
    RC rc = 0;
    RM_Record relRec;
    RelCatEntry* rEntry;
    if ((rc = GetRelEntry(relName, relRec, rEntry))) // get the relation info
        return (rc);

    //找出attrentry
    RM_Record attrRec;
    AttrCatEntry* aEntry;
    if ((rc = FindAttr(relName, attrName, attrRec, aEntry))) {
        return (rc);
    }

    //确认无索引
    if (aEntry->indexNo != NO_INDEXES)
        return (SM_INDEXEDALREADY);

    //创建索引
    if ((rc = ixm.CreateIndex(relName, rEntry->indexCurrNum, aEntry->attrType, aEntry->attrLength)))
        return (rc);

    //打开准备写入
    IX_IndexHandle ih;
    RM_FileHandle fh;
    RM_FileScan fs;
    if ((rc = ixm.OpenIndex(relName, rEntry->indexCurrNum, ih)))
        return (rc);
    if ((rc = rmm.OpenFile(relName, fh)))
        return (rc);

    //扫描rm文件插进索引
    if ((rc = fs.OpenScan(fh, INT, 4, 0, NO_OP, NULL))) {
        return (rc);
    }
    RM_Record rec;
    while (fs.GetNextRec(rec) != RM_EOF) {
        char* pData;
        RID rid;
        if ((rc = rec.GetData(pData) || (rc = rec.GetRid(rid)))) // retrieve the record
            return (rc);
        if ((rc = ih.InsertEntry(pData + aEntry->offset, rid))) // insert into index
            return (rc);
    }
    if ((rc = fs.CloseScan()) || (rc = rmm.CloseFile(fh)) || (rc = ixm.CloseIndex(ih)))
        return (rc);

    //更新一下entry并立即持久化
    aEntry->indexNo = rEntry->indexCurrNum;
    rEntry->indexCurrNum++;
    rEntry->indexCount++;

    if ((rc = relcatFH.UpdateRec(relRec)) || (rc = attrcatFH.UpdateRec(attrRec)))
        return (rc);
    if ((rc = relcatFH.ForcePages() || (rc = attrcatFH.ForcePages())))
        return (rc);

    return (0);
}

//所有rm_record的都是外面传入的，由调用者管理record的生命周期
RC SM_Manager::FindAttr(const char* relName, const char* attrName, RM_Record& attrRec, AttrCatEntry*& entry)
{
    RC rc = 0;
    SM_AttrIterator iter;
    RM_Record relRec;
    RelCatEntry* rEntry;
    if (rc = GetRelEntry(relName, relRec, rEntry))
        return rc;
    if (rc = iter.OpenIterator(attrcatFH, const_cast<char*>(relName)))
        return rc;
    bool notFound = true;
    while (notFound) {
        if (RM_EOF == iter.GetNextAttr(attrRec, entry))
            break;
        if (strncmp(entry->attrName, attrName, MAXNAME + 1) == 0) {
            notFound = false;
            break;
        }
    }
    if (rc = iter.CloseIterator())
        return rc;
    if (notFound)
        return SM_INVALIDATTR;
    return rc;
}

//需要取entry因为要检查relname、attrName是否合法,index是否存在
RC SM_Manager::DropIndex(const char* relName, const char* attrName)
{
    cout << "DropIndex\n"
         << "   relName =" << relName << "\n"
         << "   attrName=" << attrName << "\n";

    RC rc = 0;
    RM_Record relRec;
    RelCatEntry* rEntry;
    if (rc = GetRelEntry(relName, relRec, rEntry))
        return rc;
    RM_Record attrRec;
    AttrCatEntry* aEntry;
    if (rc = FindAttr(relName, attrName, attrRec, aEntry))
        return rc;
    if (aEntry->indexNo == NO_INDEXES)
        return SM_NOINDEX;
    if (rc = ixm.DestroyIndex(relName, aEntry->indexNo))
        return rc;
    aEntry->indexNo = NO_INDEXES;
    rEntry->indexCount--;
    if ((rc = relcatFH.UpdateRec(relRec)) || (rc = attrcatFH.UpdateRec(attrRec)))
        return rc;
    if ((rc = relcatFH.ForcePages()) || (rc = attrcatFH.ForcePages()))
        return rc;
    return 0;
}

//把一个rel的所有attr转成Attr类型的数组，与attrcatentry相比有额外的变量
RC SM_Manager::PrepareAttr(RelCatEntry* rEntry, Attr* attributes)
{
    RC rc = 0;
    SM_AttrIterator attrIt;
    if ((rc = attrIt.OpenIterator(attrcatFH, rEntry->relName)))
        return (rc);
    RM_Record attrRec;
    AttrCatEntry* aEntry;
    for (int i = 0; i < rEntry->attrCount; i++) {
        if ((rc = attrIt.GetNextAttr(attrRec, aEntry)))
            return (rc);
        int slot = aEntry->attrNum;
        attributes[slot].offset = aEntry->offset;
        attributes[slot].type = aEntry->attrType;
        attributes[slot].length = aEntry->attrLength;
        attributes[slot].indexNo = aEntry->indexNo;
        attributes[slot].numDistinct = aEntry->numDistinct;
        attributes[slot].maxValue = aEntry->maxValue;
        attributes[slot].minValue = aEntry->minValue;

        if ((aEntry->indexNo != NO_INDEXES)) {
            IX_IndexHandle indexHandle;
            attributes[slot].ih = indexHandle;
            if ((rc = ixm.OpenIndex(rEntry->relName, aEntry->indexNo, attributes[slot].ih)))
                return (rc);
        }

        if (aEntry->attrType == INT) {
            attributes[slot].recInsert = &recInsert_int;
        } else if (aEntry->attrType == FLOAT)
            attributes[slot].recInsert = &recInsert_float;
        else
            attributes[slot].recInsert = &recInsert_string;
    }
    if ((rc = attrIt.CloseIterator()))
        return (rc);
    return (0);
}

//加载一个rel，这个rel原本存在，现从文件中读取数据替换掉，并更新属性的最大最小值等。
RC SM_Manager::Load(const char* relName,
    const char* fileName)
{

    cout << "Load\n"
         << "   relName =" << relName << "\n"
         << "   fileName=" << fileName << "\n";

    RC rc = 0;
    RM_Record relRec;
    RelCatEntry* rEntry;
    if ((rc = GetRelEntry(relName, relRec, rEntry)))
        return (rc);
    if (rEntry->statsInitialized == false)
        calcStats = true;

    Attr* attributes = (Attr*)malloc(sizeof(Attr) * rEntry->attrCount);
    for (int i = 0; i < rEntry->attrCount; i++) {
        attributes[i] = (Attr) { 0, 0, 0, 0, IX_IndexHandle {}, recInsert_string, 0, FLT_MAX, FLT_MIN };
    }
    if (rc = PrepareAttr(rEntry, attributes))
        return rc;
    RM_FileHandle relFH;
    if (rc = rmm.OpenFile(relName, relFH))
        return rc;
    int totalRecs = 0;
    rc = OpenAndLoadFile(relFH, fileName, attributes, rEntry->attrCount,
        rEntry->tupleLength, totalRecs);
    RC rc2;
    if (calcStats) {
        rEntry->numTuples = totalRecs;
        rEntry->statsInitialized = true;
        if ((rc = relcatFH.UpdateRec(relRec)) || (rc = relcatFH.ForcePages()))
            return (rc);

        SM_AttrIterator attrIt;
        if ((rc = attrIt.OpenIterator(attrcatFH, rEntry->relName)))
            return (rc);
        RM_Record attrRec;
        AttrCatEntry* aEntry;
        for (int i = 0; i < rEntry->attrCount; i++) {
            if ((rc = attrIt.GetNextAttr(attrRec, aEntry)))
                return (rc);
            int slot = aEntry->attrNum;
            aEntry->minValue = attributes[slot].minValue;
            aEntry->maxValue = attributes[slot].maxValue;
            aEntry->numDistinct = attributes[slot].numDistinct;
            if ((rc = attrcatFH.UpdateRec(attrRec)))
                return (rc);
        }
        if ((rc = attrIt.CloseIterator()))
            return (rc);
        if ((rc = attrcatFH.ForcePages()))
            return (rc);
        calcStats = false;
    }

    if ((rc2 = CleanUpAttr(attributes, rEntry->attrCount)))
        return (rc2);

    if ((rc2 = rmm.CloseFile(relFH)))
        return (rc2);

    return (rc);
}

//加载文件
RC SM_Manager::OpenAndLoadFile(RM_FileHandle& relFH, const char* fileName, Attr* attributes, int attrCount,
    int recLength, int& loadedRecs)
{
    RC rc = 0;
    loadedRecs = 0;

    char* record = (char*)calloc(recLength, 1);

    ifstream f(fileName);
    if (f.fail()) {
        cout << "cannot open file :( " << endl;
        free(record);
        return (SM_BADLOADFILE);
    }

    vector<set<string>> numDistinct(attrCount);

    string line, token;
    string delimiter = ",";
    while (getline(f, line)) {
        RID recRID;
        for (int i = 0; i < attrCount; i++) {
            if (line.size() == 0) {
                free(record);
                f.close();
                return (SM_BADLOADFILE);
            }
            size_t pos = line.find(delimiter);
            if (pos == string::npos)
                pos = line.size();
            token = line.substr(0, pos);
            line.erase(0, pos + delimiter.length());

            if (attributes[i].recInsert(record + attributes[i].offset, token, attributes[i].length) == false) {
                rc = SM_BADLOADFILE;
                free(record);
                f.close();
                return (rc);
            }
        }

        if ((rc = relFH.InsertRec(record, recRID))) {
            free(record);
            f.close();
            return (rc);
        }

        for (int i = 0; i < attrCount; i++) {
            if (attributes[i].indexNo != NO_INDEXES) {
                if ((rc = attributes[i].ih.InsertEntry(record + attributes[i].offset, recRID))) {
                    free(record);
                    f.close();
                    return (rc);
                }
            }
            if (calcStats) {
                int offset = attributes[i].offset;
                string attr(record + offset, record + offset + attributes[i].length);
                numDistinct[i].insert(attr);
                float attrValue = 0.0;
                if (attributes[i].type == STRING)
                    attrValue = ConvertStrToFloat(record + offset);
                else if (attributes[i].type == INT)
                    attrValue = (float)*((int*)(record + offset));
                else {
                    attrValue = *((float*)(record + offset));
                }
                if (attrValue > attributes[i].maxValue)
                    attributes[i].maxValue = attrValue;
                if (attrValue < attributes[i].minValue)
                    attributes[i].minValue = attrValue;
            }
        }
        loadedRecs++;
    }
    for (int i = 0; i < attrCount; i++) {
        attributes[i].numDistinct = numDistinct[i].size();
    }

cleanup:
    free(record);
    f.close();

    return (rc);
}

RC SM_Manager::CleanUpAttr(Attr* attributes, int attrCount)
{
    RC rc = 0;
    for (int i = 0; i < attrCount; i++) {
        if (attributes[i].indexNo != NO_INDEXES) {
            if ((rc = ixm.CloseIndex(attributes[i].ih)))
                return (rc);
        }
    }
    free(attributes);
    return (rc);
}

//打印一个rel的所有元组
RC SM_Manager::Print(const char* relName)
{
    cout << "Print\n"
         << "   relName=" << relName << "\n";

    RC rc = 0;
    RM_Record relRec;
    RelCatEntry* relEntry;
    if ((rc = GetRelEntry(relName, relRec, relEntry))) // retrieves relation info
        return (SM_BADRELNAME);
    int numAttr = relEntry->attrCount;

    // Sets up the DataAttrInfo for printing
    DataAttrInfo* attributes = (DataAttrInfo*)malloc(numAttr * sizeof(DataAttrInfo));
    if ((rc = SetUpPrint(relEntry, attributes)))
        return (rc);

    Printer printer(attributes, relEntry->attrCount);
    printer.PrintHeader(cout);

    // open the file, and a scan through the entire file
    RM_FileHandle fh;
    RM_FileScan fs;
    if ((rc = rmm.OpenFile(relName, fh)) || (rc = fs.OpenScan(fh, INT, 4, 0, NO_OP, NULL))) {
        free(attributes);
        return (rc);
    }

    // Retrieve each record and print it
    RM_Record rec;
    while (fs.GetNextRec(rec) != RM_EOF) {
        char* pData;
        if ((rec.GetData(pData))) {
            free(attributes);
            return (rc);
        }
        printer.Print(cout, pData);
    }
    fs.CloseScan();

    printer.PrintFooter(cout);
    free(attributes); // free DataAttrInfo

    return (0);
}

//生成rel的所有DataAttrInfo数组，使得attr的顺序按照定义rel时给出的顺序
RC SM_Manager::SetUpPrint(RelCatEntry* rEntry, DataAttrInfo* attributes)
{
    RC rc = 0;
    RID attrRID;
    RM_Record attrRec;
    AttrCatEntry* aEntry;

    SM_AttrIterator attrIt;
    if ((rc = attrIt.OpenIterator(attrcatFH, rEntry->relName)))
        return (rc);

    for (int i = 0; i < rEntry->attrCount; i++) {
        if ((rc = attrIt.GetNextAttr(attrRec, aEntry))) {
            return (rc);
        }
        int slot = aEntry->attrNum;

        memcpy(attributes[slot].relName, aEntry->relName, MAXNAME + 1);
        memcpy(attributes[slot].attrName, aEntry->attrName, MAXNAME + 1);
        attributes[slot].offset = aEntry->offset;
        attributes[slot].attrType = aEntry->attrType;
        attributes[slot].attrLength = aEntry->attrLength;
        attributes[slot].indexNo = aEntry->indexNo;
    }
    if ((rc = attrIt.CloseIterator()))
        return (rc);

    return (rc);
}

//设置参数
RC SM_Manager::Set(const char* paramName, const char* value)
{
    RC rc = 0;
    cout << "Set\n"
         << "   paramName=" << paramName << "\n"
         << "   value    =" << value << "\n";
    if (strncmp(paramName, "printIndex", 10) == 0 && strncmp(value, "true", 4) == 0) {
        printIndex = true;
        return (0);
    } else if (strncmp(paramName, "printIndex", 10) == 0 && strncmp(value, "false", 5) == 0) {
        printIndex = false;
        return (0);
    }
    if (strncmp(paramName, "printPageStats", 14) == 0 && strncmp(value, "true", 4) == 0) {
        printPageStats = true;
        return (0);
    }
    if (strncmp(paramName, "printPageStats", 14) == 0 && strncmp(value, "false", 4) == 0) {
        printPageStats = false;
        return (0);
    }
    if (strncmp(paramName, "printPageStats", 14) == 0) {
        int* piGP = pStatisticsMgr->Get(PF_GETPAGE);
        int* piPF = pStatisticsMgr->Get(PF_PAGEFOUND);
        int* piPNF = pStatisticsMgr->Get(PF_PAGENOTFOUND);

        cout << "PF Layer Statistics" << endl;
        cout << "-------------------" << endl;
        if (piGP)
            cout << "Total number of calls to GetPage Routine: " << *piGP << endl;
        else
            cout << "Total number of calls to GetPage Routine: None" << endl;
        if (piPF)
            cout << "  Number found: " << *piPF << endl;
        else
            cout << "  Number found: None" << endl;
        if (piPNF)
            cout << "  Number not found: " << *piPNF << endl;
        else
            cout << "  Number found: None" << endl;
        return (0);
    }
    if (strncmp(paramName, "resetPageStats", 14) == 0) {
        pStatisticsMgr->Reset();
        return (0);
    }

    if (strncmp(paramName, "useQO", 5) == 0 && strncmp(value, "true", 4) == 0) {
        cout << "Using QO" << endl;
        useQO = true;
        return (0);
    }
    if (strncmp(paramName, "useQO", 5) == 0 && strncmp(value, "false", 5) == 0) {
        cout << "disabling QO" << endl;
        useQO = false;
        return (0);
    }
    if (strncmp(paramName, "printStats", 10) == 0) {
        PrintStats(value);
        return (0);
    }
    if (strncmp(paramName, "calcStats", 9) == 0) {
        CalcStats(value);
        return (0);
    }
    return (SM_BADSET);
}

RC SM_Manager::ResetPageStats()
{
    pStatisticsMgr->Reset();
    return (0);
}

RC SM_Manager::PrintPageStats()
{
    int* piGP = pStatisticsMgr->Get(PF_GETPAGE);
    int* piPF = pStatisticsMgr->Get(PF_PAGEFOUND);
    int* piPNF = pStatisticsMgr->Get(PF_PAGENOTFOUND);

    cout << "PF Layer Statistics" << endl;
    cout << "-------------------" << endl;
    if (piGP)
        cout << "Total number of calls to GetPage Routine: " << *piGP << endl;
    else
        cout << "Total number of calls to GetPage Routine: None" << endl;
    if (piPF)
        cout << "  Number found: " << *piPF << endl;
    else
        cout << "  Number found: None" << endl;
    if (piPNF)
        cout << "  Number not found: " << *piPNF << endl;
    else
        cout << "  Number found: None" << endl;
    return (0);
}

RC SM_Manager::Help()
{
    cout << "Help\n";
    RC rc = 0;
    DataAttrInfo* attributes = (DataAttrInfo*)malloc(4 * sizeof(DataAttrInfo));
    if ((rc = SetUpRelCatAttributes(attributes)))
        return (rc);
    Printer printer(attributes, 4);
    printer.PrintHeader(cout);

    RM_FileScan fs;
    if ((rc = fs.OpenScan(relcatFH, INT, 4, 0, NO_OP, NULL))) {
        free(attributes);
        return (rc);
    }

    RM_Record rec;
    while (fs.GetNextRec(rec) != RM_EOF) {
        char* pData;
        if ((rec.GetData(pData))) {
            free(attributes);
            return (rc);
        }
        printer.Print(cout, pData);
    }

    fs.CloseScan();
    printer.PrintFooter(cout);
    free(attributes);

    return (0);
}

//打印relentry需要用到dataattrinfo对齐信息，搞一个
RC SM_Manager::SetUpRelCatAttributes(DataAttrInfo* attributes)
{
    int numAttr = 4;
    for (int i = 0; i < numAttr; i++) {
        memcpy(attributes[i].relName, "relcat", strlen("relcat") + 1);
        attributes[i].indexNo = 0;
    }

    memcpy(attributes[0].attrName, "relName", MAXNAME + 1);
    memcpy(attributes[1].attrName, "tupleLength", MAXNAME + 1);
    memcpy(attributes[2].attrName, "attrCount", MAXNAME + 1);
    memcpy(attributes[3].attrName, "indexCount", MAXNAME + 1);

    attributes[0].offset = (int)offsetof(RelCatEntry, relName);
    attributes[1].offset = (int)offsetof(RelCatEntry, tupleLength);
    attributes[2].offset = (int)offsetof(RelCatEntry, attrCount);
    attributes[3].offset = (int)offsetof(RelCatEntry, indexCount);

    attributes[0].attrType = STRING;
    attributes[1].attrType = INT;
    attributes[2].attrType = INT;
    attributes[3].attrType = INT;

    attributes[0].attrLength = MAXNAME + 1;
    attributes[1].attrLength = 4;
    attributes[2].attrLength = 4;
    attributes[3].attrLength = 4;

    return (0);
}

RC SM_Manager::Help(const char* relName)
{
    cout << "Help\n"
         << "   relName=" << relName << "\n";
    RC rc = 0;
    RM_FileScan fs;
    RM_Record rec;

    if ((rc = fs.OpenScan(relcatFH, STRING, MAXNAME + 1, 0, EQ_OP, const_cast<char*>(relName))))
        return (rc);
    if (fs.GetNextRec(rec) == RM_EOF) {
        fs.CloseScan();
        return (SM_BADRELNAME);
    }
    fs.CloseScan();

    DataAttrInfo* attributes = (DataAttrInfo*)malloc(6 * sizeof(DataAttrInfo));
    if ((rc = SetUpAttrCatAttributes(attributes)))
        return (rc);
    Printer printer(attributes, 6);
    printer.PrintHeader(cout);

    if ((rc = fs.OpenScan(attrcatFH, STRING, MAXNAME + 1, 0, EQ_OP, const_cast<char*>(relName))))
        return (rc);

    while (fs.GetNextRec(rec) != RM_EOF) {
        char* pData;
        if ((rec.GetData(pData)))
            return (rc);
        printer.Print(cout, pData);
    }

    if ((rc = fs.CloseScan()))
        return (rc);

    if ((rc = fs.OpenScan(attrcatFH, STRING, MAXNAME + 1, 0, EQ_OP, const_cast<char*>(relName))))
        return (rc);
    while (fs.GetNextRec(rec) != RM_EOF) {
        char* pData;
        if ((rec.GetData(pData)))
            return (rc);
        if (printIndex) {
            AttrCatEntry* attr = (AttrCatEntry*)pData;
            if ((attr->indexNo != NO_INDEXES)) {
                IX_IndexHandle ih;
                if ((rc = ixm.OpenIndex(relName, attr->indexNo, ih)))
                    return (rc);
                if ((rc = ih.PrintIndex()) || (rc = ixm.CloseIndex(ih)))
                    return (rc);
            }
        }
    }

    printer.PrintFooter(cout);
    free(attributes);
    return (0);
}

RC SM_Manager::SetUpAttrCatAttributes(DataAttrInfo* attributes)
{
    int numAttr = 6;
    for (int i = 0; i < numAttr; i++) {
        memcpy(attributes[i].relName, "attrcat", strlen("attrcat") + 1);
        attributes[i].indexNo = 0;
    }

    memcpy(attributes[0].attrName, "relName", MAXNAME + 1);
    memcpy(attributes[1].attrName, "attrName", MAXNAME + 1);
    memcpy(attributes[2].attrName, "offset", MAXNAME + 1);
    memcpy(attributes[3].attrName, "attrType", MAXNAME + 1);
    memcpy(attributes[4].attrName, "attrLength", MAXNAME + 1);
    memcpy(attributes[5].attrName, "indexNo", MAXNAME + 1);

    attributes[0].offset = (int)offsetof(AttrCatEntry, relName);
    attributes[1].offset = (int)offsetof(AttrCatEntry, attrName);
    attributes[2].offset = (int)offsetof(AttrCatEntry, offset);
    attributes[3].offset = (int)offsetof(AttrCatEntry, attrType);
    attributes[4].offset = (int)offsetof(AttrCatEntry, attrLength);
    attributes[5].offset = (int)offsetof(AttrCatEntry, indexNo);

    attributes[0].attrType = STRING;
    attributes[1].attrType = STRING;
    attributes[2].attrType = INT;
    attributes[3].attrType = INT;
    attributes[4].attrType = INT;
    attributes[5].attrType = INT;

    attributes[0].attrLength = MAXNAME + 1;
    attributes[1].attrLength = MAXNAME + 1;
    attributes[2].attrLength = 4;
    attributes[3].attrLength = 4;
    attributes[4].attrLength = 4;
    attributes[5].attrLength = 4;

    return (0);
}

float SM_Manager::ConvertStrToFloat(char* string)
{
    float value = (float)string[0];
    return value;
}

RC SM_Manager::PrintStats(const char* relName)
{
    RC rc = 0;
    cout << "Printing stats for relation " << relName << endl;
    if (strlen(relName) > MAXNAME)
        return (SM_BADRELNAME);

    RM_Record relRec;
    RelCatEntry* relEntry;
    if ((rc = GetRelEntry(relName, relRec, relEntry)))
        return (rc);

    cout << "Total Tuples in Relation: " << relEntry->numTuples << endl;
    cout << endl;

    AttrCatEntry* aEntry;
    SM_AttrIterator attrIt;
    RM_Record attrRec;
    if ((rc = attrIt.OpenIterator(attrcatFH, relEntry->relName)))
        return (rc);

    for (int i = 0; i < relEntry->attrCount; i++) {
        if ((rc = attrIt.GetNextAttr(attrRec, aEntry))) {
            return (rc);
        }

        cout << "  Attribute: " << aEntry->attrName << endl;
        cout << "    Num attributes: " << aEntry->numDistinct << endl;
        cout << "    Max value: " << aEntry->maxValue << endl;
        cout << "    Min value: " << aEntry->minValue << endl;
    }
    if ((rc = attrIt.CloseIterator()))
        return (rc);

    return (0);
}

RC SM_Manager::CalcStats(const char* relName)
{
    RC rc = 0;
    cout << "Calculating stats for relation " << relName << endl;
    if (strlen(relName) > MAXNAME)
        return (SM_BADRELNAME);

    RM_Record relRec;
    RelCatEntry* relEntry;
    if ((rc = GetRelEntry(relName, relRec, relEntry)))
        return (rc);

    Attr* attributes = (Attr*)malloc(sizeof(Attr) * relEntry->attrCount);
    for (int i = 0; i < relEntry->attrCount; i++) {
        memset((void*)&attributes[i], 0, sizeof(attributes[i]));
        IX_IndexHandle ih;
        attributes[i] = (Attr) { 0, 0, 0, 0, ih, recInsert_string, 0, FLT_MIN, FLT_MAX };
    }
    if ((rc = PrepareAttr(relEntry, attributes)))
        return (rc);

    vector<set<string>> numDistinct(relEntry->attrCount);
    for (int i = 0; i < relEntry->attrCount; i++) {
        attributes[i].numDistinct = 0;
        attributes[i].maxValue = FLT_MIN;
        attributes[i].minValue = FLT_MAX;
    }
    relEntry->numTuples = 0;
    relEntry->statsInitialized = true;

    RM_FileScan fs;
    RM_FileHandle fh;
    RM_Record rec;
    if ((rc = rmm.OpenFile(relName, fh)) || (rc = fs.OpenScan(fh, INT, 0, 0, NO_OP, NULL)))
        return (rc);
    while (RM_EOF != fs.GetNextRec(rec)) {
        char* recData;
        if ((rc = rec.GetData(recData)))
            return (rc);

        for (int i = 0; i < relEntry->attrCount; i++) {
            int offset = attributes[i].offset;
            string attr(recData + offset, recData + offset + attributes[i].length);
            numDistinct[i].insert(attr);
            float attrValue = 0.0;
            if (attributes[i].type == STRING)
                attrValue = ConvertStrToFloat(recData + offset);
            else if (attributes[i].type == INT)
                attrValue = (float)*((int*)(recData + offset));
            else
                attrValue = *((float*)(recData + offset));
            if (attrValue > attributes[i].maxValue)
                attributes[i].maxValue = attrValue;
            if (attrValue < attributes[i].minValue)
                attributes[i].minValue = attrValue;
        }
        relEntry->numTuples++;
    }

    if ((rc = relcatFH.UpdateRec(relRec)) || (rc = relcatFH.ForcePages()))
        return (rc);

    SM_AttrIterator attrIt;
    if ((rc = attrIt.OpenIterator(attrcatFH, relEntry->relName)))
        return (rc);
    RM_Record attrRec;
    AttrCatEntry* aEntry;
    for (int i = 0; i < relEntry->attrCount; i++) {
        if ((rc = attrIt.GetNextAttr(attrRec, aEntry)))
            return (rc);
        int slot = aEntry->attrNum;
        aEntry->minValue = attributes[slot].minValue;
        aEntry->maxValue = attributes[slot].maxValue;
        aEntry->numDistinct = numDistinct[slot].size();
        if ((rc = attrcatFH.UpdateRec(attrRec)))
            return (rc);
    }
    if ((rc = attrIt.CloseIterator()))
        return (rc);
    if ((rc = attrcatFH.ForcePages()))
        return (rc);

    return (0);
}