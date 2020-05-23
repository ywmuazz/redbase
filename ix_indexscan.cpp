/*
class IX_IndexScan {
    static const char UNOCCUPIED = 'u'; // class constants to check whether
    static const char OCCUPIED_NEW = 'n'; // a slot in a node is valid
    static const char OCCUPIED_DUP = 'r';

public:
    IX_IndexScan();
    ~IX_IndexScan();

    // Open index scan
    RC OpenScan(const IX_IndexHandle& indexHandle,
        CompOp compOp,
        void* value,
        ClientHint pinHint = NO_HINT);

    // Get the next matching entry return IX_EOF if no more matching
    // entries.
    RC GetNextEntry(RID& rid);

    // Close index scan
    RC CloseScan();

private:
    RC BeginScan(PF_PageHandle& leafPH, PageNum& pageNum);
    // Sets up the scan private variables to the first entry within the given leaf
    RC GetFirstEntryInLeaf(PF_PageHandle& leafPH);
    // Sets up the scan private variables to the appropriate entry within the given leaf
    RC GetAppropriateEntryInLeaf(PF_PageHandle& leafPH);
    // Sets up the scan private variables to the first entry within this bucket
    RC GetFirstBucketEntry(PageNum nextBucket, PF_PageHandle& bucketPH);
    // Sets up the scan private variables to the next entry in the index
    RC FindNextValue();

    // Sets the RID
    RC SetRID(bool setCurrent);
//######################################
    bool openScan; // Indicator for whether the scan is being used
    IX_IndexHandle* indexHandle; // Pointer to the indexHandle that modifies the
        // file that the scan will try to traverse

    // The comparison to determine whether a record satisfies given scan conditions
    bool (*comparator)(void*, void*, AttrType, int);
    int attrLength; // Comparison type, and information about the value
    void* value; // to compare to
    AttrType attrType;
    CompOp compOp;

    bool scanEnded; // Indicators for whether the scan has started or
    bool scanStarted; // ended

    PF_PageHandle currLeafPH; // Currently pinned Leaf and Bucket PageHandles
    PF_PageHandle currBucketPH; // that the scan is accessing

    char* currKey; // the keys of the current record, and the following
    char* nextKey; // two records after that
    char* nextNextKey;
    struct IX_NodeHeader_L* leafHeader; // the scan's current leaf and bucket header
    struct IX_BucketHeader* bucketHeader; // and entry and key pointers
    struct Node_Entry* leafEntries;
    struct Bucket_Entry* bucketEntries;
    char* leafKeys;
    int leafSlot; // the current leaf and bucket slots of the scan
    int bucketSlot;
    PageNum currLeafNum; // the current and next bucket slots of the scan
    PageNum currBucketNum;
    PageNum nextBucketNum;

    RID currRID; // the current RID and the next RID in the scan
    RID nextRID;

    bool hasBucketPinned; // whether the scan has pinned a bucket or a leaf page
    bool hasLeafPinned;
    bool initializedValue; // Whether value variable has been initialized (malloced)
    bool endOfIndexReached; // Whether the end of the scan has been reached

    bool foundFirstValue;
    bool foundLastValue;
    bool useFirstLeaf;
};

*/

#include "ix_internal.h"
#include "pf.h"
#include <cstdio>
#include <sys/types.h>
#include <unistd.h>

bool iequal(void* value1, void* value2, AttrType attrtype, int attrLength)
{
    switch (attrtype) {
    case FLOAT:
        return (*(float*)value1 == *(float*)value2);
    case INT:
        return (*(int*)value1 == *(int*)value2);
    default:
        return (strncmp((char*)value1, (char*)value2, attrLength) == 0);
    }
}

bool iless_than(void* value1, void* value2, AttrType attrtype, int attrLength)
{
    switch (attrtype) {
    case FLOAT:
        return (*(float*)value1 < *(float*)value2);
    case INT:
        return (*(int*)value1 < *(int*)value2);
    default:
        return (strncmp((char*)value1, (char*)value2, attrLength) < 0);
    }
}

bool igreater_than(void* value1, void* value2, AttrType attrtype, int attrLength)
{
    switch (attrtype) {
    case FLOAT:
        return (*(float*)value1 > *(float*)value2);
    case INT:
        return (*(int*)value1 > *(int*)value2);
    default:
        return (strncmp((char*)value1, (char*)value2, attrLength) > 0);
    }
}

bool iless_than_or_eq_to(void* value1, void* value2, AttrType attrtype, int attrLength)
{
    switch (attrtype) {
    case FLOAT:
        return (*(float*)value1 <= *(float*)value2);
    case INT:
        return (*(int*)value1 <= *(int*)value2);
    default:
        return (strncmp((char*)value1, (char*)value2, attrLength) <= 0);
    }
}

bool igreater_than_or_eq_to(void* value1, void* value2, AttrType attrtype, int attrLength)
{
    switch (attrtype) {
    case FLOAT:
        return (*(float*)value1 >= *(float*)value2);
    case INT:
        return (*(int*)value1 >= *(int*)value2);
    default:
        return (strncmp((char*)value1, (char*)value2, attrLength) >= 0);
    }
}

bool inot_equal(void* value1, void* value2, AttrType attrtype, int attrLength)
{
    switch (attrtype) {
    case FLOAT:
        return (*(float*)value1 != *(float*)value2);
    case INT:
        return (*(int*)value1 != *(int*)value2);
    default:
        return (strncmp((char*)value1, (char*)value2, attrLength) != 0);
    }
}

IX_IndexScan::IX_IndexScan()
{
    openScan = false; // Initialize all values
    value = nullptr;
    initializedValue = false;
    hasBucketPinned = false;
    hasLeafPinned = false;
    scanEnded = true;
    scanStarted = false;
    endOfIndexReached = true;
    attrLength = 0;
    attrType = INT;

    foundFirstValue = false;
    foundLastValue = false;
    useFirstLeaf = false;
}

IX_IndexScan::~IX_IndexScan()
{
    if (scanEnded == false && hasBucketPinned == true)
        indexHandle->pfh.UnpinPage(currBucketNum);
    if (scanEnded == false && hasLeafPinned == true && (currLeafNum != (indexHandle->header).rootPage))
        indexHandle->pfh.UnpinPage(currLeafNum);
    if (initializedValue == true) {
        free(value);
        initializedValue = false;
    }
}

//不允许传入!=
RC IX_IndexScan::OpenScan(const IX_IndexHandle& indexHandle,
    CompOp compOp,
    void* value,
    ClientHint pinHint)
{
    RC rc = 0;
    if (openScan == true || compOp == NE_OP) {
        return IX_INVALIDSCAN;
    }
    if (indexHandle.isValidIndexHeader()) // makes sure that the indexHanlde is valid
        this->indexHandle = const_cast<IX_IndexHandle*>(&indexHandle);
    else
        return (IX_INVALIDSCAN);
    useFirstLeaf = true;
    this->compOp = compOp;
    //大于号、等于号不找第一个叶子
    switch (compOp) {
    case EQ_OP:
        comparator = &iequal;
        useFirstLeaf = false;
        break;
    case LT_OP:
        comparator = &iless_than;
        break;
    case GT_OP:
        comparator = &igreater_than;
        useFirstLeaf = false;
        break;
    case LE_OP:
        comparator = &iless_than_or_eq_to;
        break;
    case GE_OP:
        comparator = &igreater_than_or_eq_to;
        useFirstLeaf = false;
        break;
    case NO_OP:
        comparator = NULL;
        break;
    default:
        return (IX_INVALIDSCAN);
    }

    this->attrType = (indexHandle.header).attr_type;
    attrLength = ((this->indexHandle)->header).attr_length;
    if (compOp != NO_OP) {
        this->value = (void*)malloc(attrLength); // sets up the value
        memcpy(this->value, value, attrLength);
        initializedValue = true;
    }

    openScan = true; // sets up all indicators
    scanEnded = false;
    hasLeafPinned = false;
    scanStarted = false;
    endOfIndexReached = false;
    foundFirstValue = false;
    foundLastValue = false;

    return rc;
}

RC IX_IndexScan::BeginScan(PF_PageHandle& leafPH, PageNum& pageNum)
{
    RC rc = 0;
    if (useFirstLeaf) {
        if (rc = indexHandle->GetFirstLeafPage(leafPH, pageNum))
            return rc;
        if (rc = GetFirstEntryInLeaf(currLeafPH)) {
            if (rc = IX_EOF) {
                scanEnded = true;
            }
            return rc;
        }
    } else { //大于号等于号(无符号不在此)
        if (rc = indexHandle->FindRecordPage(leafPH, pageNum, value))
            return rc;
        if (rc = GetAppropriateEntryInLeaf(currLeafPH)) {
            if (rc == IX_EOF) {
                scanEnded = true;
            }
            return (rc);
        }
    }
}

RC IX_IndexScan::GetNextEntry(RID& rid)
{
    RC rc = 0;
    if (openScan == false)
        return IX_INVALIDSCAN;
    if (scanEnded == true)
        return IX_EOF;
    if (foundLastValue == true)
        return IX_EOF;
    bool notFound = true;

    while (notFound) {
        if (openScan && scanEnded == false && scanStarted == false) {
            if (rc = BeginScan(currLeafPH, currLeafNum))
                return rc;
            currKey = nextNextKey;
            scanStarted = true;
            SetRID(true);
            if (FindNextValue() == IX_EOF) {
                endOfIndexReached = true;
            }
        } else {
            currKey = nextKey;
            currRID = nextRID;
        }
        SetRID(false);
        nextKey = nextNextKey;
        if (FindNextValue() == IX_EOF) {
            endOfIndexReached = true;
        }

        PageNum thispage;
        if (rc = currRID.GetPageNum(thispage))
            return rc;
        if (thispage == -1) {
            scanEnded = true;
            return IX_EOF;
        }
        if (compOp == NO_OP) {
            rid = currRID;
            notFound = false;
            foundFirstValue = true;
        } else if (comparator((void*)currKey, value, attrType, attrLength)) {
            rid = currRID;
            notFound = false;
            foundFirstValue = true;
        } else if (foundFirstValue == true) {
            //比较不通过,说明已经到结尾了
            //?? < <=那种运算符，从第一个叶子开始，开始就不满足，难道扫完?
            //似乎不会，因为beginscan的时候就已经判断第一个存不存在了
            foundLastValue = true;
            return IX_EOF;
        }
    }
    return rc;
}

RC IX_IndexScan::SetRID(bool setCurrent)
{
    if (setCurrent) {
        if (hasBucketPinned) {
            currRID = RID(bucketEntries[bucketSlot].page, bucketEntries[bucketSlot].slot);
        } else if (hasLeafPinned) {
            currRID = RID(leafEntries[leafSlot].page, leafEntries[leafSlot].slot);
        }
    } else {
        if (endOfIndexReached) {
            nextRID = RID(-1, -1);
            return 0;
        }
        if (hasBucketPinned) {
            RID rid1(bucketEntries[bucketSlot].page, bucketEntries[bucketSlot].slot);
            nextRID = rid1;
        } else if (hasLeafPinned) {
            RID rid1(leafEntries[leafSlot].page, leafEntries[leafSlot].slot);
            nextRID = rid1;
        }
    }
    return 0;
}

RC IX_IndexScan::FindNextValue()
{
    RC rc = 0;
    if (hasBucketPinned) { // If a bucket is pinned, then search in this bucket
        int prevSlot = bucketSlot;
        bucketSlot = bucketEntries[prevSlot].nextSlot; // update the slot index
        if (bucketSlot != NO_MORE_SLOTS) {
            return (0); // found next bucket slot, so stop searching
        }
        // otherwise, unpin this bucket
        PageNum nextBucket = bucketHeader->nextBucket;
        if ((rc = (indexHandle->pfh).UnpinPage(currBucketNum)))
            return (rc);
        hasBucketPinned = false;

        if (nextBucket != NO_MORE_PAGES) { // If this is a valid bucket, open it up, and get
            // the first entry
            if ((rc = GetFirstBucketEntry(nextBucket, currBucketPH)))
                return (rc);
            currBucketNum = nextBucket;
            return (0);
        }
    }
    // otherwise, deal with leaf level.
    int prevLeafSlot = leafSlot;
    leafSlot = leafEntries[prevLeafSlot].nextSlot; // update to the next leaf slot

    // If the leaf slot containts duplicate entries, open up the bucket associated
    // with it, and update the key.
    if (leafSlot != NO_MORE_SLOTS && leafEntries[leafSlot].isValid == OCCUPIED_DUP) {
        nextNextKey = leafKeys + leafSlot * attrLength;
        currBucketNum = leafEntries[leafSlot].page;

        if ((rc = GetFirstBucketEntry(currBucketNum, currBucketPH)))
            return (rc);
        return (0);
    }
    // Otherwise, stay update the key.
    if (leafSlot != NO_MORE_SLOTS && leafEntries[leafSlot].isValid == OCCUPIED_NEW) {
        nextNextKey = leafKeys + leafSlot * attrLength;
        return (0);
    }

    // otherwise, get the next page
    PageNum nextLeafPage = leafHeader->nextPage;

    // if it's not the root page, unpin it:
    if ((currLeafNum != (indexHandle->header).rootPage)) {
        if ((rc = (indexHandle->pfh).UnpinPage(currLeafNum))) {
            return (rc);
        }
    }
    hasLeafPinned = false;

    // If the next page is a valid page, then retrieve the first entry in this leaf page
    if (nextLeafPage != NO_MORE_PAGES) {
        currLeafNum = nextLeafPage;
        if ((rc = (indexHandle->pfh).GetThisPage(currLeafNum, currLeafPH)))
            return (rc);
        if ((rc = GetFirstEntryInLeaf(currLeafPH)))
            return (rc);
        return (0);
    }

    return (IX_EOF); // Otherwise, no more elements
}

RC IX_IndexScan::GetFirstEntryInLeaf(PF_PageHandle& leafPH)
{
    RC rc = 0;
    hasLeafPinned = true;
    if ((rc = leafPH.GetData((char*&)leafHeader)))
        return (rc);

    if (leafHeader->num_keys == 0) // no keys in the leaf... return IX_EOF
        return (IX_EOF);

    leafEntries = (struct Node_Entry*)((char*)leafHeader + (indexHandle->header).entryOffset_N);
    leafKeys = (char*)leafHeader + (indexHandle->header).keysOffset_N;

    leafSlot = leafHeader->firstSlotIndex;
    if (leafSlot == NO_MORE_SLOTS)
        return IX_INVALIDSCAN;
    nextNextKey = leafKeys + attrLength * leafSlot;
    if (leafEntries[leafSlot].isValid == OCCUPIED_DUP) { // if it's a duplciate value, go into the bucket
        currBucketNum = leafEntries[leafSlot].page; // to retrieve the first entry
        if ((rc = GetFirstBucketEntry(currBucketNum, currBucketPH)))
            return (rc);
    }
    return (0);
}

RC IX_IndexScan::GetAppropriateEntryInLeaf(PF_PageHandle& leafPH)
{
    RC rc = 0;
    hasLeafPinned = true;
    if ((rc = leafPH.GetData((char*&)leafHeader)))
        return (rc);

    if (leafHeader->num_keys == 0)
        return (IX_EOF);

    leafEntries = (struct Node_Entry*)((char*)leafHeader + (indexHandle->header).entryOffset_N);
    leafKeys = (char*)leafHeader + (indexHandle->header).keysOffset_N;
    int index = 0;
    bool isDup = false;
    if ((rc = indexHandle->FindNodeInsertIndex((struct IX_NodeHeader*)leafHeader, value, index, isDup)))
        return (rc);

    leafSlot = index;

    //猜想这里会出现多个bug： FindNodeInsertIndex找<=value的最大index， 仅当compOp为>=且找到的就是==时无误
    //当compOp为>= / >且找到的是< 都会出错
    if ((leafSlot != NO_MORE_SLOTS))
        nextNextKey = leafKeys + attrLength * leafSlot;
    else
        return (IX_INVALIDSCAN);

    if (leafEntries[leafSlot].isValid == OCCUPIED_DUP) { // if it's a duplciate value, go into the bucket
        currBucketNum = leafEntries[leafSlot].page; // to retrieve the first entry
        if ((rc = GetFirstBucketEntry(currBucketNum, currBucketPH)))
            return (rc);
    }
    return (0);
}

/* 
 * 无脑get即可，不管合不合法，外面会判断
 */
RC IX_IndexScan::GetFirstBucketEntry(PageNum nextBucket, PF_PageHandle& bucketPH)
{
    RC rc = 0;
    if ((rc = (indexHandle->pfh).GetThisPage(nextBucket, currBucketPH))) // pin the bucket
        return (rc);
    hasBucketPinned = true;
    if ((rc = bucketPH.GetData((char*&)bucketHeader))) // retrieve bucket contents
        return (rc);

    bucketEntries = (struct Bucket_Entry*)((char*)bucketHeader + (indexHandle->header).entryOffset_B);
    bucketSlot = bucketHeader->firstSlotIndex; // set the current scan to the first slot in bucket

    return (0);
}

RC IX_IndexScan::CloseScan()
{
    RC rc = 0;
    if (openScan == false)
        return (IX_INVALIDSCAN);
    if (scanEnded == false && hasBucketPinned == true)
        indexHandle->pfh.UnpinPage(currBucketNum);
    if (scanEnded == false && hasLeafPinned == true && (currLeafNum != (indexHandle->header).rootPage))
        indexHandle->pfh.UnpinPage(currLeafNum);
    if (initializedValue == true) {
        free(value);
        initializedValue = false;
    }
    openScan = false;
    scanStarted = false;

    return (rc);
}