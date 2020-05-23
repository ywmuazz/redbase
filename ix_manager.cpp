/*
class IX_Manager
{
    static const char UNOCCUPIED = 'u';

public:
    IX_Manager(PF_Manager &pfm);
    ~IX_Manager();

    // Create a new Index
    RC CreateIndex(const char *fileName, int indexNo,
                   AttrType attrType, int attrLength);

    // Destroy and Index
    RC DestroyIndex(const char *fileName, int indexNo);

    // Open an Index
    RC OpenIndex(const char *fileName, int indexNo,
                 IX_IndexHandle &indexHandle);

    // Close an Index
    RC CloseIndex(IX_IndexHandle &indexHandle);

private:
    // Checks that the index parameters given (attrtype and length) make
    // a valid index
    bool IsValidIndex(AttrType attrType, int attrLength);

    // Creates the index file name from the filename and index number, and
    // returns it as a string in indexname
    RC GetIndexFileName(const char *fileName, int indexNo, std::string
&indexname);
    // Sets up the IndexHandle internal varables when opening an index
    RC SetUpIH(IX_IndexHandle &ih, PF_FileHandle &fh, struct IX_IndexHeader
*header);
    // Modifies th IndexHandle internal variables when closing an index
    RC CleanUpIH(IX_IndexHandle &indexHandle);

    PF_Manager &pfm; // The PF_Manager associated with this index.
};

struct IX_IndexHeader
{
    AttrType attr_type; // attribute type and length
    int attr_length;

    int entryOffset_N; // the offset from the header of the beginning of
    int entryOffset_B; // the entry list in the Bucket and Nodes

    int keysOffset_N; // the offset from the header of the beginning of
                      // the keys list in the nodes
    int maxKeys_N;    // Maximum number of entries in buckets and nodes
    int maxKeys_B;

    PageNum rootPage; // Page number associated with the root page
};

IX_IndexHandle:
bool isOpenHandle;            // Indicator for whether the indexHandle is being used 
PF_FileHandle pfh;            // The PF_FileHandle associated with this index 
bool header_modified;         // Indicator for whether the header has been modified 
PF_PageHandle rootPH;         // The PF_PageHandle associated withthe root node 
struct IX_IndexHeader header; // The header for this index int
(*comparator)(void *, void *, int); 
bool (*printer)(void *, int);

*/

#include <sys/types.h>
#include <unistd.h>

#include <climits>
#include <cstdio>
#include<cstring>
#include <sstream>
#include <string>

#include "comparators.h"
#include "ix_internal.h"
#include "pf.h"

IX_Manager::IX_Manager(PF_Manager& pfm)
    : pfm(pfm)
{
}

IX_Manager::~IX_Manager() {}

bool IX_Manager::IsValidIndex(AttrType attrType, int attrLength)
{
    if (attrType == INT && attrLength == 4)
        return true;
    else if (attrType == FLOAT && attrLength == 4)
        return true;
    else if (attrType == STRING && attrLength > 0 && attrLength <= MAXSTRINGLEN)
        return true;
    else
        return false;
}

//返回filename.indexNo的字符串
RC IX_Manager::GetIndexFileName(const char* fileName,
    int indexNo,
    std::string& indexname)
{
    std::string idx = std::to_string(indexNo);
    indexname = std::string(fileName) + "." + idx;
    if (indexname.size() > PATH_MAX || idx.size() > 10)
        return IX_BADINDEXNAME;
    return 0;
}

RC IX_Manager::CreateIndex(const char* fileName,
    int indexNo,
    AttrType attrType,
    int attrLength)
{
    if (fileName == nullptr || indexNo < 0)
        return IX_BADFILENAME;
    RC rc = 0;
    if (!IsValidIndex(attrType, attrLength))
        return IX_BADINDEXSPEC;
    std::string indexname;
    if (rc = GetIndexFileName(fileName, indexNo, indexname))
        return rc;
    if (rc = pfm.CreateFile(indexname.c_str()))
        return rc;

    PF_FileHandle fh;
    PF_PageHandle ph_header;
    PF_PageHandle ph_root;
    if (rc = pfm.OpenFile(indexname.c_str(), fh))
        return rc;
    int numKeyN = IX_IndexHandle::CalcNumKeysNode(attrLength);
    int numKeyB = IX_IndexHandle::CalcNumKeysBucket(attrLength);

    PageNum headerpage;
    PageNum rootpage;

    if ((rc = fh.AllocatePage(ph_header)) || (rc = ph_header.GetPageNum(headerpage)) || (rc = fh.AllocatePage(ph_root)) || (rc = ph_root.GetPageNum(rootpage)))
        return rc;
    IX_IndexHeader* header;
    IX_NodeHeader_L* rootheader;
    Node_Entry* entries;
    if ((rc = ph_header.GetData((char*&)header)) || (rc = ph_root.GetData((char*&)rootheader)))
        goto errclean;
    header->attr_type = attrType;
    header->attr_length = attrLength;
    header->maxKeys_N = numKeyN;
    header->maxKeys_B = numKeyB;
    header->entryOffset_N = sizeof(IX_NodeHeader_I);
    header->entryOffset_B = sizeof(IX_BucketHeader);
    // key放在指针之后
    header->keysOffset_N = header->entryOffset_N + numKeyN * sizeof(Node_Entry);
    header->rootPage = rootpage;

    rootheader->isLeafNode = true;
    rootheader->isEmpty = true;
    rootheader->num_keys = 0;
    rootheader->nextPage = NO_MORE_PAGES;
    rootheader->prevPage = NO_MORE_PAGES;
    rootheader->firstSlotIndex = NO_MORE_SLOTS;
    rootheader->freeSlotIndex = 0;
    entries = (Node_Entry*)((char*)rootheader + header->entryOffset_N);
    for (int i = 0; i < header->maxKeys_N; i++) {
        entries[i].isValid = UNOCCUPIED;
        entries[i].page = NO_MORE_PAGES;
        if (i == header->maxKeys_N - 1)
            entries[i].nextSlot = NO_MORE_SLOTS;
        else
            entries[i].nextSlot = i + 1;
    }
errclean:
    RC rc2;
    if ((rc2 = fh.MarkDirty(headerpage)) || (rc2 = fh.UnpinPage(headerpage)) || (rc2 = fh.MarkDirty(rootpage)) || (rc2 = fh.UnpinPage(rootpage)) || (rc2 = pfm.CloseFile(fh)))
        return (rc2);
    return rc;
}

RC IX_Manager::DestroyIndex(const char* fileName, int indexNo)
{
    RC rc;
    if (fileName == NULL || indexNo < 0)
        return (IX_BADFILENAME);
    std::string indexname;
    if ((rc = GetIndexFileName(fileName, indexNo, indexname)))
        return (rc);
    if ((rc = pfm.DestroyFile(indexname.c_str())))
        return (rc);
    return (0);
}

RC IX_Manager::SetUpIH(IX_IndexHandle& ih,
    PF_FileHandle& fh,
    IX_IndexHeader* header)
{
    RC rc = 0;
    memcpy(&ih.header, header, sizeof(struct IX_IndexHeader));

    // check that this is a valid index file
    if (!IsValidIndex(ih.header.attr_type, ih.header.attr_length))
        return (IX_INVALIDINDEXFILE);

    if (!ih.isValidIndexHeader()) { // check that the header is valid
        return (rc);
    }

    if ((rc = fh.GetThisPage(header->rootPage,
             ih.rootPH))) { // retrieve the root page
        return (rc);
    }

    if (ih.header.attr_type == INT) { // set up the comparator
        ih.comparator = compare_int;
        ih.printer = print_int;
    } else if (ih.header.attr_type == FLOAT) {
        ih.comparator = compare_float;
        ih.printer = print_float;
    } else {
        ih.comparator = compare_string;
        ih.printer = print_string;
    }

    ih.header_modified = false;
    ih.pfh = fh;
    ih.isOpenHandle = true;
    return (rc);
}

//rootPage常驻内存
RC IX_Manager::OpenIndex(const char* fileName, int indexNo,
    IX_IndexHandle& indexHandle)
{
    if (fileName == NULL || indexNo < 0) { // check for valid filename, and valid indexHandle
        return (IX_BADFILENAME);
    }
    if (indexHandle.isOpenHandle == true) {
        return (IX_INVALIDINDEXHANDLE);
    }
    RC rc = 0;

    PF_FileHandle fh;
    std::string indexname;
    if ((rc = GetIndexFileName(fileName, indexNo, indexname)) || (rc = pfm.OpenFile(indexname.c_str(), fh)))
        return (rc);

    // Get first page, and set up the indexHandle
    PF_PageHandle ph;
    PageNum firstpage;
    char* pData;
    if ((rc = fh.GetFirstPage(ph)) || (ph.GetPageNum(firstpage)) || (ph.GetData(pData))) {
        fh.UnpinPage(firstpage);
        pfm.CloseFile(fh);
        return (rc);
    }
    struct IX_IndexHeader* header = (struct IX_IndexHeader*)pData;

    rc = SetUpIH(indexHandle, fh, header);
    RC rc2;
    if ((rc2 = fh.UnpinPage(firstpage)))
        return (rc2);

    if (rc != 0) {
        pfm.CloseFile(fh);
    }
    return (rc);
}

RC IX_Manager::CleanUpIH(IX_IndexHandle& indexHandle)
{
    if (indexHandle.isOpenHandle == false)
        return (IX_INVALIDINDEXHANDLE);
    indexHandle.isOpenHandle = false;
    return (0);
}

RC IX_Manager::CloseIndex(IX_IndexHandle& indexHandle)
{
    RC rc = 0;
    PF_PageHandle ph;
    PageNum page;
    char* pData;
    
    if (indexHandle.isOpenHandle == false) {
        return (IX_INVALIDINDEXHANDLE);
    }
    PageNum root = indexHandle.header.rootPage;


    if ((rc = indexHandle.pfh.MarkDirty(root)) || (rc = indexHandle.pfh.UnpinPage(root)))
        return (rc);

    if (indexHandle.header_modified == true) {
        if ((rc = indexHandle.pfh.GetFirstPage(ph)) || ph.GetPageNum(page))
            return (rc);
        if ((rc = ph.GetData(pData))) {
            RC rc2;
            if ((rc2 = indexHandle.pfh.UnpinPage(page)))
                return (rc2);
            return (rc);
        }
        memcpy(pData, &indexHandle.header, sizeof(struct IX_IndexHeader));
        if ((rc = indexHandle.pfh.MarkDirty(page)) || (rc = indexHandle.pfh.UnpinPage(page)))
            return (rc);
    }
    if ((rc = pfm.CloseFile(indexHandle.pfh)))
        return (rc);

    if ((rc = CleanUpIH(indexHandle)))
        return (rc);

    return (rc);
}