/*
#define BEGIN_SCAN  -1 //default slot number before scan begins
class RM_FileScan {
public:
    RM_FileScan  ();
    ~RM_FileScan ();

    RC OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint = NO_HINT); // Initialize a file scan
    RC GetNextRec(RM_Record &rec);               // Get next matching record
    RC CloseScan ();                             // Close the scan

private:
    // Retrieves the number of records on ph's page, and returns it in
    // numRecords
    RC GetNumRecOnPage(PF_PageHandle& ph, int &numRecords);

    bool openScan; // whether this instance is currently a valid, open scan

    // save the parameters of the scan:
    RM_FileHandle* fileHandle;
    bool (*comparator) (void * , void *, AttrType, int);
    int attrOffset;
    int attrLength;
    void *value;
    AttrType attrType;
    CompOp compOp;

    // whether the scan has ended or not. This dictages whether to unpin the
    // page that the scan is on (currentPH)
    bool scanEnded;

    // The current state of the scan. currentPH is the page that's pinned
    PageNum scanPage;
    SlotNum scanSlot;
    PF_PageHandle currentPH;
    // Dictates whether to seek a record on the same page, or unpin it and
    // seek a record on the following page
    int numRecOnPage;
    int numSeenOnPage;
    bool useNextPage;
    bool hasPagePinned;
    bool initializedValue;
};
*/

#include <sys/types.h>
#include <unistd.h>

#include <cassert>
#include <cstdlib>

#include "pf.h"
#include "rm_internal.h"

RM_FileScan::RM_FileScan() {
    openScan = false;
    value = nullptr;
    initializedValue = false;
    hasPagePinned = false;
    scanEnded = true;
}

RM_FileScan::~RM_FileScan() {
    if (openScan == true && scanEnded == false && hasPagePinned == true) {
        fileHandle->pfh.UnpinPage(scanPage);
    }
    if (initializedValue == true) {  // free any memory not freed
        free(value);
        initializedValue = false;
    }
}

bool equal(void* value1, void* value2, AttrType attrtype, int attrLength) {
    switch (attrtype) {
        case FLOAT:
            return (*(float*)value1 == *(float*)value2);
        case INT:
            return (*(int*)value1 == *(int*)value2);
        default:
            return (strncmp((char*)value1, (char*)value2, attrLength) == 0);
    }
}

bool less_than(void* value1, void* value2, AttrType attrtype, int attrLength) {
    switch (attrtype) {
        case FLOAT:
            return (*(float*)value1 < *(float*)value2);
        case INT:
            return (*(int*)value1 < *(int*)value2);
        default:
            return (strncmp((char*)value1, (char*)value2, attrLength) < 0);
    }
}

bool greater_than(void* value1,
                  void* value2,
                  AttrType attrtype,
                  int attrLength) {
    switch (attrtype) {
        case FLOAT:
            return (*(float*)value1 > *(float*)value2);
        case INT:
            return (*(int*)value1 > *(int*)value2);
        default:
            return (strncmp((char*)value1, (char*)value2, attrLength) > 0);
    }
}

bool less_than_or_eq_to(void* value1,
                        void* value2,
                        AttrType attrtype,
                        int attrLength) {
    switch (attrtype) {
        case FLOAT:
            return (*(float*)value1 <= *(float*)value2);
        case INT:
            return (*(int*)value1 <= *(int*)value2);
        default:
            return (strncmp((char*)value1, (char*)value2, attrLength) <= 0);
    }
}

bool greater_than_or_eq_to(void* value1,
                           void* value2,
                           AttrType attrtype,
                           int attrLength) {
    switch (attrtype) {
        case FLOAT:
            return (*(float*)value1 >= *(float*)value2);
        case INT:
            return (*(int*)value1 >= *(int*)value2);
        default:
            return (strncmp((char*)value1, (char*)value2, attrLength) >= 0);
    }
}

bool not_equal(void* value1, void* value2, AttrType attrtype, int attrLength) {
    switch (attrtype) {
        case FLOAT:
            return (*(float*)value1 != *(float*)value2);
        case INT:
            return (*(int*)value1 != *(int*)value2);
        default:
            return (strncmp((char*)value1, (char*)value2, attrLength) != 0);
    }
}

RC RM_FileScan::OpenScan(const RM_FileHandle& fileHandle,
                         AttrType attrType,
                         int attrLength,
                         int attrOffset,
                         CompOp compOp,
                         void* value,
                         ClientHint pinHint) {
    if (openScan)
        return RM_INVALIDSCAN;
    if (fileHandle.isValidFileHeader())
        this->fileHandle = const_cast<RM_FileHandle*>(&fileHandle);
    else
        return (RM_INVALIDFILE);
    this->value = nullptr;
    if (compOp > GE_OP)
        return RM_INVALIDSCAN;
    this->compOp = compOp;
    static bool (*funcs[])(void*, void*, AttrType, int) = {
        nullptr,
        equal,
        not_equal,
        less_than,
        greater_than,
        less_than_or_eq_to,
        greater_than_or_eq_to};
    this->comparator = funcs[compOp];
    int recSize = (this->fileHandle)->getRecordSize();
    if (this->compOp != NO_OP) {
        if ((attrOffset + attrLength) > recSize || attrOffset < 0 ||
            attrOffset > MAXSTRINGLEN)
            return (RM_INVALIDSCAN);
        this->attrOffset = attrOffset;
        this->attrLength = attrLength;
        if (attrType == FLOAT || attrType == INT) {
            if (attrLength != 4)
                return (RM_INVALIDSCAN);
            this->value = (void*)malloc(4);
            memcpy(this->value, value, 4);
            initializedValue = true;
        } else if (attrType == STRING) {
            this->value = (void*)malloc(attrLength);
            memcpy(this->value, value, attrLength);
            initializedValue = true;
        } else {
            return (RM_INVALIDSCAN);
        }
        this->attrType = attrType;
    }
    openScan = true;
    scanEnded = false;
    numRecOnPage = 0;
    numSeenOnPage = 0;
    useNextPage = true;
    scanPage = 0;
    scanSlot = BEGIN_SCAN;
    numSeenOnPage = 0;
    hasPagePinned = false;
    return 0;
}

RC RM_FileScan::GetNumRecOnPage(PF_PageHandle& ph, int& numRecords) {
    RC rc = 0;
    char* pData;
    if (rc = ph.GetData(pData))
        return rc;
    char* bitmap;
    RM_PageHeader* header;
    if (rc = this->fileHandle->GetPageDataAndBitmap(ph, bitmap, header))
        return rc;
    numRecords = header->numRecords;
    return 0;
}

/*
    RC GetNumRecOnPage(PF_PageHandle& ph, int &numRecords);
    bool openScan;
    RM_FileHandle* fileHandle;
    bool (*comparator) (void * , void *, AttrType, int);
    int attrOffset;
    int attrLength;
    void *value;
    AttrType attrType;
    CompOp compOp;

    bool scanEnded;
    PageNum scanPage;
    SlotNum scanSlot;
    PF_PageHandle currentPH;
    int numRecOnPage;
    int numSeenOnPage;
    bool useNextPage;
    bool hasPagePinned;
    bool initializedValue;
*/

RC RM_FileScan::GetNextRec(RM_Record& rec) {
    if (scanEnded == true)
        return (RM_EOF);
    if (openScan == false)
        return (RM_INVALIDSCAN);

    RC rc = 0;
    RM_Record retrec;
    for (;;) {
        if (rc = fileHandle->GetNextRecord(scanPage, scanSlot, retrec,
                                           currentPH, useNextPage)) {
            //获取了分页但err怎么办?要不要unpin?
            //如果在这里unpin,但是里面却没有pin成功过怎么办
            //可以直接unpin，检测rc，如果是未在缓冲区那ok
            if (rc == RM_EOF) {
                hasPagePinned = false;
                scanEnded = true;
                return rc;
            } else {
                fileHandle->pfh.UnpinPage(scanPage);
                hasPagePinned = false;
                return rc;
            }
        }
        hasPagePinned = true;
        if (useNextPage) {
            useNextPage = false;
            if (rc = GetNumRecOnPage(currentPH, numRecOnPage))
                return rc;
            numSeenOnPage = 0;
        }
        numSeenOnPage++;

        RID rid;
        if (rc = retrec.GetRid(rid))
            return rc;
        if (rc = rid.GetPageNum(scanPage))
            return rc;
        if (rc = rid.GetSlotNum(scanSlot))
            return rc;

        if (numSeenOnPage == numRecOnPage) {
            useNextPage = true;
            if (rc = fileHandle->pfh.UnpinPage(scanPage))
                return rc;
            hasPagePinned = false;
        }

        if (compOp == NO_OP) {
            rec = retrec;
            return 0;
        }

        //比较一下
        char* pData;
        if (rc = retrec.GetData(pData))
            return rc;
        if (comparator(pData + attrOffset, value, attrType, attrLength)) {
            rec = retrec;
            return 0;
        }
    }

    assert(0);
}
