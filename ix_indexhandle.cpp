/*

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

#include "comparators.h"
#include "ix_internal.h"
#include "pf.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>

//打印叶子节点
//涵盖了基本操作
RC IX_IndexHandle::PrintIndex()
{
    RC rc;
    PageNum leafPage;
    PF_PageHandle ph;
    struct IX_NodeHeader_L* lheader;
    struct Node_Entry* entries;
    char* keys;
    if ((rc = GetFirstLeafPage(ph, leafPage) || (rc = pfh.UnpinPage(leafPage))))
        return (rc);
    while (leafPage != NO_MORE_PAGES) { // print this page
        if ((rc = pfh.GetThisPage(leafPage, ph)) || (rc = ph.GetData((char*&)lheader)))
            return (rc);
        entries = (struct Node_Entry*)((char*)lheader + header.entryOffset_N);
        keys = (char*)lheader + header.keysOffset_N;

        int prev_idx = BEGINNING_OF_SLOTS;
        int curr_idx = lheader->firstSlotIndex;
        while (curr_idx != NO_MORE_SLOTS) {
            printf("\n");
            printer((void*)keys + curr_idx * header.attr_length, header.attr_length);
            if (entries[curr_idx].isValid == OCCUPIED_DUP) {
                //printf("is a duplicate\n");
                PageNum bucketPage = entries[curr_idx].page;
                PF_PageHandle bucketPH;
                struct IX_BucketHeader* bheader;
                struct Bucket_Entry* bEntries;
                while (bucketPage != NO_MORE_PAGES) {
                    if ((rc = pfh.GetThisPage(bucketPage, bucketPH)) || (rc = bucketPH.GetData((char*&)bheader))) {
                        return (rc);
                    }
                    bEntries = (struct Bucket_Entry*)((char*)bheader + header.entryOffset_B);
                    int currIdx = bheader->firstSlotIndex;
                    int prevIdx = BEGINNING_OF_SLOTS;
                    while (currIdx != NO_MORE_SLOTS) {
                        //printf("currIdx: %d ", currIdx);
                        printf("rid: %d, page %d | ", bEntries[currIdx].page, bEntries[currIdx].slot);
                        prevIdx = currIdx;
                        currIdx = bEntries[prevIdx].nextSlot;
                    }
                    PageNum nextBucketPage = bheader->nextBucket;
                    if ((rc = pfh.UnpinPage(bucketPage)))
                        return (rc);
                    bucketPage = nextBucketPage;
                }

            } else {
                printf("rid: %d, page %d | ", entries[curr_idx].page, entries[curr_idx].slot);
            }
            prev_idx = curr_idx;
            curr_idx = entries[prev_idx].nextSlot;
        }
        PageNum nextPage = lheader->nextPage;
        if (leafPage != header.rootPage) {
            if ((rc = pfh.UnpinPage(leafPage)))
                return (rc);
        }
        leafPage = nextPage;
    }
    puts("");
    return (0);
}


RC IX_IndexHandle::PrintIndex(std::map<int,std::vector<RID>>& mp)
{
    RC rc;
    PageNum leafPage;
    PF_PageHandle ph;
    struct IX_NodeHeader_L* lheader;
    struct Node_Entry* entries;
    char* keys;
    if ((rc = GetFirstLeafPage(ph, leafPage) || (rc = leafPage==header.rootPage?0:pfh.UnpinPage(leafPage))))
        return (rc);
    while (leafPage != NO_MORE_PAGES) { // print this page
        if ((rc = pfh.GetThisPage(leafPage, ph)) || (rc = ph.GetData((char*&)lheader)))
            return (rc);
        entries = (struct Node_Entry*)((char*)lheader + header.entryOffset_N);
        keys = (char*)lheader + header.keysOffset_N;

        int prev_idx = BEGINNING_OF_SLOTS;
        int curr_idx = lheader->firstSlotIndex;
        while (curr_idx != NO_MORE_SLOTS) {
            printf("\n");
            int* kp=(int*)(keys + curr_idx * header.attr_length);
            printer((void*)keys + curr_idx * header.attr_length, header.attr_length);
            if (entries[curr_idx].isValid == OCCUPIED_DUP) {
                //printf("is a duplicate\n");
                PageNum bucketPage = entries[curr_idx].page;
                PF_PageHandle bucketPH;
                struct IX_BucketHeader* bheader;
                struct Bucket_Entry* bEntries;
                while (bucketPage != NO_MORE_PAGES) {
                    if ((rc = pfh.GetThisPage(bucketPage, bucketPH)) || (rc = bucketPH.GetData((char*&)bheader))) {
                        return (rc);
                    }
                    bEntries = (struct Bucket_Entry*)((char*)bheader + header.entryOffset_B);
                    int currIdx = bheader->firstSlotIndex;
                    int prevIdx = BEGINNING_OF_SLOTS;
                    while (currIdx != NO_MORE_SLOTS) {
                        //printf("currIdx: %d ", currIdx);
                        printf("rid: %d, page %d | ", bEntries[currIdx].page, bEntries[currIdx].slot);

                        mp[*kp].push_back(RID(bEntries[currIdx].page, bEntries[currIdx].slot));

                        prevIdx = currIdx;
                        currIdx = bEntries[prevIdx].nextSlot;
                    }
                    PageNum nextBucketPage = bheader->nextBucket;
                    if ((rc = pfh.UnpinPage(bucketPage)))
                        return (rc);
                    bucketPage = nextBucketPage;
                }

            } else {
                printf("rid: %d, page %d | ", entries[curr_idx].page, entries[curr_idx].slot);
                mp[*kp].push_back(RID(entries[curr_idx].page, entries[curr_idx].slot));
            }
            prev_idx = curr_idx;
            curr_idx = entries[prev_idx].nextSlot;
        }
        PageNum nextPage = lheader->nextPage;
        if (leafPage != header.rootPage) {
            if ((rc = pfh.UnpinPage(leafPage)))
                return (rc);
        }
        leafPage = nextPage;
    }
    puts("");
    return (0);
}


IX_IndexHandle::IX_IndexHandle()
{
    isOpenHandle = false; // indexhandle is initially closed
    header_modified = false;
}

IX_IndexHandle::~IX_IndexHandle()
{
}

//第一个版本，先使用向下判断子节点是否满，满则分裂子节点
//会造成空间的浪费，
RC IX_IndexHandle::InsertEntry(void* pData, const RID& rid)
{
    if (!isValidIndexHeader() || isOpenHandle == false)
        return (IX_INVALIDINDEXHANDLE);
    RC rc = 0;
    IX_NodeHeader* rHeader;
    if (rc = rootPH.GetData((char*&)rHeader))
        return rc;

    if (rHeader->num_keys == header.maxKeys_N) {
        PF_PageHandle newRootPH;
        PageNum newRootPage;
        char* newRootData;

        if (rc = CreateNewNode(newRootPH, newRootPage, newRootData, false)) {
            return (rc);
        }

        IX_NodeHeader_I* newRootHeader = (IX_NodeHeader_I*)newRootData;
        newRootHeader->isEmpty = false;
        newRootHeader->firstPage = header.rootPage;

        //新页(右)在父节点的index
        int unused;
        //新页的pageNum
        PageNum unusedPage;

        if (rc = SplitNode((IX_NodeHeader*&)newRootHeader, (IX_NodeHeader*&)rHeader, header.rootPage,
                BEGINNING_OF_SLOTS, unused, unusedPage))
            return rc;
        // if((rc = SplitNode((IX_NodeHeader *&)newRootData, (IX_NodeHeader *&)rHeader, header.rootPage,
        //     BEGINNING_OF_SLOTS, unused, unusedPage)))
        //     return (rc);
        if ((rc = pfh.MarkDirty(header.rootPage)) || (rc = pfh.UnpinPage(header.rootPage)))
            return rc;

        this->rootPH = newRootPH;
        this->header_modified = true;
        header.rootPage = newRootPage;
        // printf("new root page:%d\n",header.rootPage);

        IX_NodeHeader* rh;
        if (rc = newRootPH.GetData((char*&)rh))
            return rc;
        if (rc = InsertIntoNonFullNode(rh, newRootPage, pData, rid))
            return rc;
    } else {
        if ((rc = InsertIntoNonFullNode(rHeader, header.rootPage, pData, rid))) {
            return (rc);
        }
    }
    if ((rc = pfh.MarkDirty(header.rootPage)))
        return (rc);
    return rc;
}

//分裂节点
//pHeader - 分裂节点的父节点
//oldHeader - 需分裂节点
//oldPage - 分裂页号
//index - 插到父节点的新key的index-1
//newKeyIndex - 返回：新key在父index
//newPageNum - 返回：新节点的pagenum
//!!因为index只是个句柄slot，所以并不能反映该key是在所有key中第几个位置
RC IX_IndexHandle::SplitNode(IX_NodeHeader* pHeader, IX_NodeHeader* oldHeader,
    PageNum oldPage, int index, int& newKeyIndex, PageNum& newPageNum)
{
    RC rc = 0;
    PF_PageHandle newPH;
    PageNum newPage;
    IX_NodeHeader* newHeader;

    if (rc = CreateNewNode(newPH, newPage, (char*&)newHeader, oldHeader->isLeafNode))
        return rc;

    newPageNum = newPage;

    Node_Entry* pEntries = (Node_Entry*)((char*)pHeader + header.entryOffset_N);
    Node_Entry* oldEntries = (Node_Entry*)((char*)oldHeader + header.entryOffset_N);
    Node_Entry* newEntries = (Node_Entry*)((char*)newHeader + header.entryOffset_N);
    char* pKeys = (char*)pHeader + header.keysOffset_N;
    char* newKeys = (char*)newHeader + header.keysOffset_N;
    char* oldKeys = (char*)oldHeader + header.keysOffset_N;

    //其实把插入节点删除节点的方法封装一下更好
    int prev_idx1 = BEGINNING_OF_SLOTS;
    int curr_idx1 = oldHeader->firstSlotIndex;
    for (int i = 0; i < header.maxKeys_N / 2; i++) {
        prev_idx1 = curr_idx1;
        curr_idx1 = oldEntries[prev_idx1].nextSlot;
    }
    oldEntries[prev_idx1].nextSlot = NO_MORE_SLOTS;

    char* parentKey = oldKeys + curr_idx1 * header.attr_length;

    if (!oldHeader->isLeafNode) {
        struct IX_NodeHeader_I* newIHeader = (struct IX_NodeHeader_I*)newHeader;
        newIHeader->firstPage = oldEntries[curr_idx1].page;
        newIHeader->isEmpty = false;
        prev_idx1 = curr_idx1;
        curr_idx1 = oldEntries[prev_idx1].nextSlot;
        oldEntries[prev_idx1].nextSlot = oldHeader->freeSlotIndex;
        oldHeader->freeSlotIndex = prev_idx1;
        oldHeader->num_keys--;
    }

    // Now, move the remaining header.maxKeys_N/2 values into the new node
    int prev_idx2 = BEGINNING_OF_SLOTS;
    int curr_idx2 = newHeader->freeSlotIndex;
    while (curr_idx1 != NO_MORE_SLOTS) {
        newEntries[curr_idx2].page = oldEntries[curr_idx1].page;
        newEntries[curr_idx2].slot = oldEntries[curr_idx1].slot;
        newEntries[curr_idx2].isValid = oldEntries[curr_idx1].isValid;
        memcpy(newKeys + curr_idx2 * header.attr_length, oldKeys + curr_idx1 * header.attr_length, header.attr_length);
        if (prev_idx2 == BEGINNING_OF_SLOTS) {
            newHeader->freeSlotIndex = newEntries[curr_idx2].nextSlot;
            newEntries[curr_idx2].nextSlot = newHeader->firstSlotIndex;
            newHeader->firstSlotIndex = curr_idx2;
        } else {
            newHeader->freeSlotIndex = newEntries[curr_idx2].nextSlot;
            newEntries[curr_idx2].nextSlot = newEntries[prev_idx2].nextSlot;
            newEntries[prev_idx2].nextSlot = curr_idx2;
        }
        prev_idx2 = curr_idx2;
        curr_idx2 = newHeader->freeSlotIndex; // update insert index

        prev_idx1 = curr_idx1;
        curr_idx1 = oldEntries[prev_idx1].nextSlot;
        oldEntries[prev_idx1].nextSlot = oldHeader->freeSlotIndex;
        oldHeader->freeSlotIndex = prev_idx1;
        oldHeader->num_keys--;
        newHeader->num_keys++;
    }

    newKeyIndex = pHeader->freeSlotIndex;
    pHeader->freeSlotIndex = pEntries[newKeyIndex].nextSlot;
    memcpy(pKeys+newKeyIndex*header.attr_length,parentKey,header.attr_length);
    pEntries[newKeyIndex].page=newPage;
    pEntries[newKeyIndex].isValid=OCCUPIED_NEW;

    if (index == BEGINNING_OF_SLOTS) {
        pEntries[newKeyIndex].nextSlot = pHeader->firstSlotIndex;
        pHeader->firstSlotIndex = newKeyIndex;
    } else {
        pEntries[newKeyIndex].nextSlot = pEntries[index].nextSlot;
        pEntries[index].nextSlot = newKeyIndex;
    }
    pHeader->num_keys++;

    //更新叶子节点左右指针

    if (oldHeader->isLeafNode) {
        struct IX_NodeHeader_L* newLHeader = (struct IX_NodeHeader_L*)newHeader;
        struct IX_NodeHeader_L* oldLHeader = (struct IX_NodeHeader_L*)oldHeader;
        newLHeader->nextPage = oldLHeader->nextPage;
        newLHeader->prevPage = oldPage;
        oldLHeader->nextPage = newPage;
        if (newLHeader->nextPage != NO_MORE_PAGES) {
            PF_PageHandle nextPH;
            struct IX_NodeHeader_L* nextHeader;
            if ((rc = pfh.GetThisPage(newLHeader->nextPage, nextPH)) || (nextPH.GetData((char*&)nextHeader)))
                return (rc);
            nextHeader->prevPage = newPage;
            if ((rc = pfh.MarkDirty(newLHeader->nextPage)) || (rc = pfh.UnpinPage(newLHeader->nextPage)))
                return (rc);
        }
    }

    if ((rc = pfh.MarkDirty(newPage)) || (rc = pfh.UnpinPage(newPage))) {
        return (rc);
    }
    return (rc);
}

//往溢出页插入rid
//大概步骤就是先查重,再(直接)插到页的末尾
RC IX_IndexHandle::InsertIntoBucket(PageNum page, const RID& rid)
{
    RC rc = 0;
    PageNum ridPage; // Gets the Page and Slot Number from this rid object
    SlotNum ridSlot;
    if ((rc = rid.GetPageNum(ridPage)) || (rc = rid.GetSlotNum(ridSlot))) {
        return (rc);
    }

    bool notEnd = true; // for searching for an empty spot in the bucket
    PageNum currPage = page;
    PF_PageHandle bucketPH;
    struct IX_BucketHeader* bucketHeader;
    while (notEnd) {
        // Get the contents of this bucket
        if ((rc = pfh.GetThisPage(currPage, bucketPH)) || (rc = bucketPH.GetData((char*&)bucketHeader))) {
            return (rc);
        }
        // Try to find the entry in the database
        struct Bucket_Entry* entries = (struct Bucket_Entry*)((char*)bucketHeader + header.entryOffset_B);
        int prev_idx = BEGINNING_OF_SLOTS;
        int curr_idx = bucketHeader->firstSlotIndex;
        while (curr_idx != NO_MORE_SLOTS) {
            // If we found a duplicate entry, then return an error
            if (entries[curr_idx].page == ridPage && entries[curr_idx].slot == ridSlot) {
                RC rc2 = 0;
                if ((rc2 = pfh.UnpinPage(currPage)))
                    return (rc2);
                return (IX_DUPLICATEENTRY);
            }
            prev_idx = curr_idx;
            curr_idx = entries[prev_idx].nextSlot;
        }
        // If this is the last bucket in the string of buckets, and it's full, create a new bucket
        if (bucketHeader->nextBucket == NO_MORE_PAGES && bucketHeader->num_keys == header.maxKeys_B) {
            notEnd = false; // end the search
            PageNum newBucketPage;
            PF_PageHandle newBucketPH;
            if ((rc = CreateNewBucket(newBucketPage)))
                return (rc);
            bucketHeader->nextBucket = newBucketPage;
            if ((rc = pfh.MarkDirty(currPage)) || (rc = pfh.UnpinPage(currPage))) // unpin previous bucket
                return (rc);
            // Get the contents of the new bucket
            currPage = newBucketPage;
            if ((rc = pfh.GetThisPage(currPage, bucketPH)) || (rc = bucketPH.GetData((char*&)bucketHeader)))
                return (rc);
            entries = (struct Bucket_Entry*)((char*)bucketHeader + header.entryOffset_B);
        }
        // If it's the last bucket in the string of buckets, insert the value in
        if (bucketHeader->nextBucket == NO_MORE_PAGES) {
            notEnd = false; // end search
            int loc = bucketHeader->freeSlotIndex; // Insert into this location
            entries[loc].slot = ridSlot;
            entries[loc].page = ridPage;
            bucketHeader->freeSlotIndex = entries[loc].nextSlot;
            entries[loc].nextSlot = bucketHeader->firstSlotIndex;
            bucketHeader->firstSlotIndex = loc;
            bucketHeader->num_keys++;
        }

        // Update the currPage pointer to the next bucket in the sequence
        PageNum nextPage = bucketHeader->nextBucket;
        if ((rc = pfh.MarkDirty(currPage)) || (rc = pfh.UnpinPage(currPage)))
            return (rc);
        currPage = nextPage;
    }
    return (0);
}

//断言节点非满，插入新key
//自顶而下，先判断下层节点是否满，满则先分裂(会有空间浪费)，再递归插入
RC IX_IndexHandle::InsertIntoNonFullNode(struct IX_NodeHeader* nHeader, PageNum thisNodeNum, void* pData,
    const RID& rid)
{
    RC rc = 0;

    // Retrieve contents of this node
    struct Node_Entry* entries = (struct Node_Entry*)((char*)nHeader + header.entryOffset_N);
    char* keys = (char*)nHeader + header.keysOffset_N;

    // If it is a leaf node, then insert into it
    if (nHeader->isLeafNode) {
        int prevInsertIndex = BEGINNING_OF_SLOTS;
        bool isDup = false;
        if ((rc = FindNodeInsertIndex(nHeader, pData, prevInsertIndex, isDup))) // get appropriate index
            return (rc);
        // If it's not a duplicate, then insert a new key for it, and update
        // the slot and page values.
        if (!isDup) {
            int index = nHeader->freeSlotIndex;
            memcpy(keys + header.attr_length * index, (char*)pData, header.attr_length);
            entries[index].isValid = OCCUPIED_NEW; // mark it as a single entry
            if ((rc = rid.GetPageNum(entries[index].page)) || (rc = rid.GetSlotNum(entries[index].slot)))
                return (rc);
            nHeader->isEmpty = false;
            nHeader->num_keys++;
            nHeader->freeSlotIndex = entries[index].nextSlot;
            if (prevInsertIndex == BEGINNING_OF_SLOTS) {
                entries[index].nextSlot = nHeader->firstSlotIndex;
                nHeader->firstSlotIndex = index;
            } else {
                entries[index].nextSlot = entries[prevInsertIndex].nextSlot;
                entries[prevInsertIndex].nextSlot = index;
            }
        }

        // if it is a duplicate, add a new page if new, or add it to existing bucket:
        else {
            PageNum bucketPage;
            if (isDup && entries[prevInsertIndex].isValid == OCCUPIED_NEW) {
                if ((rc = CreateNewBucket(bucketPage))) // create a new bucket
                    return (rc);
                entries[prevInsertIndex].isValid = OCCUPIED_DUP;
                RID rid2(entries[prevInsertIndex].page, entries[prevInsertIndex].slot);
                // Insert this new RID, and the existing entry into the bucket
                if ((rc = InsertIntoBucket(bucketPage, rid2)) || (rc = InsertIntoBucket(bucketPage, rid)))
                    return (rc);
                entries[prevInsertIndex].page = bucketPage; // page now points to bucket
            } else {
                bucketPage = entries[prevInsertIndex].page;
                if ((rc = InsertIntoBucket(bucketPage, rid))) // insert in existing bucket
                    return (rc);
            }
        }

    } else { // Otherwise, this is a internal node
        // Get its contents, and find the insert location
        struct IX_NodeHeader_I* nIHeader = (struct IX_NodeHeader_I*)nHeader;
        PageNum nextNodePage;
        int prevInsertIndex = BEGINNING_OF_SLOTS;
        bool isDup;
        if ((rc = FindNodeInsertIndex(nHeader, pData, prevInsertIndex, isDup)))
            return (rc);
        if (prevInsertIndex == BEGINNING_OF_SLOTS)
            nextNodePage = nIHeader->firstPage;
        else {
            nextNodePage = entries[prevInsertIndex].page;
        }

        // Read this next page to insert into.
        PF_PageHandle nextNodePH;
        struct IX_NodeHeader* nextNodeHeader;
        int newKeyIndex;
        PageNum newPageNum;
        if ((rc = pfh.GetThisPage(nextNodePage, nextNodePH)) || (rc = nextNodePH.GetData((char*&)nextNodeHeader)))
            return (rc);
        // If this next node is full, the split the node
        if (nextNodeHeader->num_keys == header.maxKeys_N) {
            if ((rc = SplitNode(nHeader, nextNodeHeader, nextNodePage, prevInsertIndex, newKeyIndex, newPageNum)))
                return (rc);
            char* value = keys + newKeyIndex * header.attr_length;

            // check which of the two split nodes to insert into.
            int compared = comparator(pData, (void*)value, header.attr_length);
            if (compared >= 0) {
                PageNum nextPage = newPageNum;
                if ((rc = pfh.MarkDirty(nextNodePage)) || (rc = pfh.UnpinPage(nextNodePage)))
                    return (rc);
                if ((rc = pfh.GetThisPage(nextPage, nextNodePH)) || (rc = nextNodePH.GetData((char*&)nextNodeHeader)))
                    return (rc);
                nextNodePage = nextPage;
            }
        }
        // Insert into the following node, then mark it dirty and unpin it
        if ((rc = InsertIntoNonFullNode(nextNodeHeader, nextNodePage, pData, rid)))
            return (rc);
        if ((rc = pfh.MarkDirty(nextNodePage)) || (rc = pfh.UnpinPage(nextNodePage)))
            return (rc);
    }
    return (rc);
}

//输入key，找到要插入的位置index(key应放在index之后),或找中间节点的分支
RC IX_IndexHandle::FindNodeInsertIndex(struct IX_NodeHeader* nHeader,
    void* pData, int& index, bool& isDup)
{
    // Setup
    struct Node_Entry* entries = (struct Node_Entry*)((char*)nHeader + header.entryOffset_N);
    char* keys = ((char*)nHeader + header.keysOffset_N);

    // Search until we reach a key in the node that is greater than the pData entered
    int prev_idx = BEGINNING_OF_SLOTS;
    int curr_idx = nHeader->firstSlotIndex;
    isDup = false;
    while (curr_idx != NO_MORE_SLOTS) {
        char* value = keys + header.attr_length * curr_idx;
        int compared = comparator(pData, (void*)value, header.attr_length);
        if (compared == 0)
            isDup = true;
        if (compared < 0)
            break;
        prev_idx = curr_idx;
        curr_idx = entries[prev_idx].nextSlot;
    }
    index = prev_idx;
    return (0);
}

//删除key，如果把当前节点给删光了，要将当前节点变为叶子header->isLeafNode
RC IX_IndexHandle::DeleteEntry(void* pData, const RID& rid)
{
    RC rc = 0;
    if (!isValidIndexHeader() || isOpenHandle == false)
        return (IX_INVALIDINDEXHANDLE);

    // get root page
    struct IX_NodeHeader* rHeader;
    if ((rc = rootPH.GetData((char*&)rHeader))) {
        printf("failing here\n");
        return (rc);
    }

    // If the root page is empty, then no entries can exist
    if (rHeader->isEmpty && (!rHeader->isLeafNode))
        return (IX_INVALIDENTRY);
    if (rHeader->num_keys == 0 && rHeader->isLeafNode)
        return (IX_INVALIDENTRY);

    // toDelete is an indicator for whether to delete this current node
    // because it has no more contents
    bool toDelete = false;
    if ((rc = DeleteFromNode(rHeader, pData, rid, toDelete))) // Delete the value from this node
        return (rc);

    // If the tree is empty, set the current node to a leaf node.
    if (toDelete) {
        rHeader->isLeafNode = true;
    }

    return (rc);
}

//必须不能是firstPage的
//不包含从free链上取下来的过程
RC IX_IndexHandle::LinkSlot(IX_NodeHeader* nHeader, int preindex, int newindex)
{
    assert(newindex >= 0);
    RC rc = 0;
    Node_Entry* entries = (Node_Entry*)((char*)nHeader + header.entryOffset_N);
    if (preindex == BEGINNING_OF_SLOTS) {
        entries[newindex].nextSlot = nHeader->firstSlotIndex;
        nHeader->firstSlotIndex = newindex;
    } else {
        entries[newindex].nextSlot = entries[preindex].nextSlot;
        entries[preindex].nextSlot = newindex;
    }
    return rc;
}

//不包含归还freelist的过程
//需要注意的是如果要删掉firstSlot，但是外面之后还需要用到这个slot,在外面保存原值，之后不要直接用
//nheader->firstSlotIndex
RC IX_IndexHandle::UnlinkSlot(IX_NodeHeader* nHeader, int preindex, int delindex)
{
    assert(delindex >= 0);
    RC rc = 0;
    Node_Entry* entries = (Node_Entry*)((char*)nHeader + header.entryOffset_N);
    if (preindex == BEGINNING_OF_SLOTS) {
        nHeader->firstSlotIndex = entries[delindex].nextSlot;
    } else {
        entries[preindex].nextSlot = entries[delindex].nextSlot;
    }
    return rc;
}

RC IX_IndexHandle::LinkFreeList(IX_NodeHeader* nHeader, int index)
{
    assert(index >= 0);
    RC rc = 0;
    Node_Entry* entries = (Node_Entry*)((char*)nHeader + header.entryOffset_N);
    entries[index].nextSlot = nHeader->freeSlotIndex;
    nHeader->freeSlotIndex = index;
    return rc;
}
RC IX_IndexHandle::GetFreeSlot(IX_NodeHeader* nHeader, int& index)
{
    RC rc = 0;
    if (nHeader->freeSlotIndex == NO_MORE_SLOTS)
        return IX_NODEFULL;
    Node_Entry* entries = (Node_Entry*)((char*)nHeader + header.entryOffset_N);
    index = nHeader->freeSlotIndex;
    nHeader->freeSlotIndex = entries[index].nextSlot;
    return rc;
}

RC IX_IndexHandle::DeleteFromNode(struct IX_NodeHeader* nHeader, void* pData, const RID& rid, bool& toDelete)
{
    RC rc = 0;
    toDelete = false;
    if (nHeader->isLeafNode) { // If it's a leaf node, delete it from there
        if ((rc = DeleteFromLeaf((struct IX_NodeHeader_L*)nHeader, pData, rid, toDelete))) {
            return (rc);
        }
    } else {
        int index, preindex;
        bool isDup;
        bool useFirstPage = false;
        if (rc = FindNodeInsertIndex(nHeader, pData, index, isDup))
            return rc;

        PageNum nextNodePage;
        IX_NodeHeader_I* iHeader = (struct IX_NodeHeader_I*)nHeader;
        Node_Entry* entries = (struct Node_Entry*)((char*)nHeader + header.entryOffset_N);

        if (index == BEGINNING_OF_SLOTS) {
            useFirstPage = true;
            nextNodePage = iHeader->firstPage;
            // PF_PageHandle ph;
            // IX_NodeHeader* sheader;
            // if((rc=pfh.GetThisPage(((IX_NodeHeader_I*)nHeader)->firstPage,ph))||
            //   (rc=ph.GetData((char*&)sheader)))return rc;
            // if(rc=DeleteFromNode(sheader,pData,rid,toDelete))
        } else {
            nextNodePage = entries[index].page;
            if ((rc = FindPrevIndex(nHeader, index, preindex)))
                return (rc);
        }
        PF_PageHandle nextNodePH;
        IX_NodeHeader* nextHeader;
        if ((rc = pfh.GetThisPage(nextNodePage, nextNodePH)) || (rc = nextNodePH.GetData((char*&)nextHeader)))
            return rc;
        bool nextToDelete = false;
        rc = DeleteFromNode(nextHeader, pData, rid, nextToDelete);

        RC rc2 = 0;
        if ((rc2 = pfh.MarkDirty(nextNodePage)) || (rc2 = pfh.UnpinPage(nextNodePage)))
            return (rc2);
        if (rc == IX_INVALIDENTRY)
            return rc;

        //子节点删光了
        if (nextToDelete) {
            //丢弃子节点的操作直接由pf层管理
            if ((rc = pfh.DisposePage(nextNodePage))) {
                return (rc);
            }
            //把第一个slot移到firstPage
            if (useFirstPage) {
                int firstindex = nHeader->firstSlotIndex;
                if (rc = UnlinkSlot(nHeader, BEGINNING_OF_SLOTS, firstindex))
                    return rc;
                iHeader->firstPage = entries[firstindex].page;
                if (rc = LinkFreeList(nHeader, firstindex))
                    return rc;
                // entries[firstindex].nextSlot=nHeader->freeSlotIndex;
                // nHeader->freeSlotIndex=firstindex;
            } else {
                if (rc = UnlinkSlot(nHeader, preindex, index))
                    return rc;
                if (rc = LinkFreeList(nHeader, index))
                    return rc;
            }
            if (nHeader->num_keys == 0) {
                nHeader->isEmpty = true;
                toDelete = true;
            } else {
                nHeader->num_keys--;
            }
        }
    }
    return rc;
}

RC IX_IndexHandle::FindPrevIndex(struct IX_NodeHeader* nHeader, int thisIndex, int& prevIndex)
{
    struct Node_Entry* entries = (struct Node_Entry*)((char*)nHeader + header.entryOffset_N);
    int prev_idx = BEGINNING_OF_SLOTS;
    int curr_idx = nHeader->firstSlotIndex;
    while (curr_idx != thisIndex) {
        prev_idx = curr_idx;
        curr_idx = entries[prev_idx].nextSlot;
    }
    prevIndex = prev_idx;
    return (0);
}

RC IX_IndexHandle::DeleteFromLeaf(struct IX_NodeHeader_L* nHeader, void* pData, const RID& rid, bool& toDelete)
{
    RC rc = 0;
    int prevIndex, currIndex;
    bool isDup; // find index of deletion
    if ((rc = FindNodeInsertIndex((struct IX_NodeHeader*)nHeader, pData, currIndex, isDup)))
        return (rc);
    if (isDup == false) // If this entry exists, a key should exist for it in the leaf
        return (IX_INVALIDENTRY);

    // Setup
    struct Node_Entry* entries = (struct Node_Entry*)((char*)nHeader + header.entryOffset_N);
    char* key = (char*)nHeader + header.keysOffset_N;

    if (currIndex == nHeader->firstSlotIndex) // Set up previous index for deletion of key
        prevIndex = currIndex; // purposes
    else {
        if ((rc = FindPrevIndex((struct IX_NodeHeader*)nHeader, currIndex, prevIndex)))
            return (rc);
    }

    // if only entry, delete it from the leaf
    if (entries[currIndex].isValid == OCCUPIED_NEW) {
        PageNum ridPage;
        SlotNum ridSlot;
        if ((rc = rid.GetPageNum(ridPage)) || (rc = rid.GetSlotNum(ridSlot))) {
            return (rc);
        }
        // If this RID and key value don't match, then the entry is not there. Return IX_INVALIDENTRY
        int compare = comparator((void*)(key + header.attr_length * currIndex), pData, header.attr_length);
        if (ridPage != entries[currIndex].page || ridSlot != entries[currIndex].slot || compare != 0)
            return (IX_INVALIDENTRY);

        // Otherwise, delete from leaf page
        if (currIndex == nHeader->firstSlotIndex) {
            nHeader->firstSlotIndex = entries[currIndex].nextSlot;
        } else
            entries[prevIndex].nextSlot = entries[currIndex].nextSlot;

        entries[currIndex].nextSlot = nHeader->freeSlotIndex;
        nHeader->freeSlotIndex = currIndex;
        entries[currIndex].isValid = UNOCCUPIED;
        nHeader->num_keys--; // update the key counter
    }
    // if duplicate, delete it from the corresponding bucket
    else if (entries[currIndex].isValid == OCCUPIED_DUP) {
        PageNum bucketNum = entries[currIndex].page;
        PF_PageHandle bucketPH;
        struct IX_BucketHeader* bHeader;
        bool deletePage = false;
        RID lastRID;
        PageNum nextBucketNum; // Retrieve the bucket header
        if ((rc = pfh.GetThisPage(bucketNum, bucketPH)) || (rc = bucketPH.GetData((char*&)bHeader))) {
            return (rc);
        }
        rc = DeleteFromBucket(bHeader, rid, deletePage, lastRID, nextBucketNum);
        RC rc2 = 0;
        if ((rc2 = pfh.MarkDirty(bucketNum)) || (rc = pfh.UnpinPage(bucketNum)))
            return (rc2);

        if (rc == IX_INVALIDENTRY) // If it doesnt exist in the bucket, notify the function caller
            return (IX_INVALIDENTRY);

        if (deletePage) { // If we need to delete the bucket,
            if ((rc = pfh.DisposePage(bucketNum)))
                return (rc);
            // If there are no more buckets, then place the last RID into the leaf page, and
            // update the isValid flag to OCCUPIED_NEW
            if (nextBucketNum == NO_MORE_PAGES) {
                entries[currIndex].isValid = OCCUPIED_NEW;
                if ((rc = lastRID.GetPageNum(entries[currIndex].page)) || (rc = lastRID.GetSlotNum(entries[currIndex].slot)))
                    return (rc);
            } else // otherwise, set the bucketPointer to the next bucket
                entries[currIndex].page = nextBucketNum;
        }
    }
    if (nHeader->num_keys == 0) { // If the leaf is now empty,
        toDelete = true; // return the indicator to delete
        // Update the leaf pointers of its previous and next neighbors
        PageNum prevPage = nHeader->prevPage;
        PageNum nextPage = nHeader->nextPage;
        PF_PageHandle leafPH;
        struct IX_NodeHeader_L* leafHeader;
        if (prevPage != NO_MORE_PAGES) {
            if ((rc = pfh.GetThisPage(prevPage, leafPH)) || (rc = leafPH.GetData((char*&)leafHeader)))
                return (rc);
            leafHeader->nextPage = nextPage;
            if ((rc = pfh.MarkDirty(prevPage)) || (rc = pfh.UnpinPage(prevPage)))
                return (rc);
        }
        if (nextPage != NO_MORE_PAGES) {
            if ((rc = pfh.GetThisPage(nextPage, leafPH)) || (rc = leafPH.GetData((char*&)leafHeader)))
                return (rc);
            leafHeader->prevPage = prevPage;
            if ((rc = pfh.MarkDirty(nextPage)) || (rc = pfh.UnpinPage(nextPage)))
                return (rc);
        }
    }
    return (0);
}

RC IX_IndexHandle::DeleteFromBucket(struct IX_BucketHeader* bHeader, const RID& rid,
    bool& deletePage, RID& lastRID, PageNum& nextPage)
{
    RC rc = 0;
    PageNum nextPageNum = bHeader->nextBucket;
    nextPage = bHeader->nextBucket; // set the nextBucket pointer

    struct Bucket_Entry* entries = (struct Bucket_Entry*)((char*)bHeader + header.entryOffset_B);

    if ((nextPageNum != NO_MORE_PAGES)) { // If there's a bucket after this one, search in it first
        bool toDelete = false; // whether to delete the following bucket
        PF_PageHandle nextBucketPH;
        struct IX_BucketHeader* nextHeader;
        RID last;
        PageNum nextNextPage; // Get this next bucket
        if ((rc = pfh.GetThisPage(nextPageNum, nextBucketPH)) || (rc = nextBucketPH.GetData((char*&)nextHeader)))
            return (rc);
        // Recursively call go delete from this bucket
        rc = DeleteFromBucket(nextHeader, rid, toDelete, last, nextNextPage);

        int numKeysInNext = nextHeader->num_keys;
        RC rc2 = 0;
        if ((rc2 = pfh.MarkDirty(nextPageNum)) || (rc2 = pfh.UnpinPage(nextPageNum)))
            return (rc2);

        // If the following bucket only has one key remaining, and there is space in this
        // bucket, then put the lastRID into this bucket, and delete the following bucket
        if (toDelete && bHeader->num_keys < header.maxKeys_B && numKeysInNext == 1) {
            int loc = bHeader->freeSlotIndex;
            if ((rc2 = last.GetPageNum(entries[loc].page)) || (rc2 = last.GetSlotNum(entries[loc].slot)))
                return (rc2);

            bHeader->freeSlotIndex = entries[loc].nextSlot;
            entries[loc].nextSlot = bHeader->firstSlotIndex;
            bHeader->firstSlotIndex = loc;

            bHeader->num_keys++;
            numKeysInNext = 0;
        }
        if (toDelete && numKeysInNext == 0) {
            if ((rc2 = pfh.DisposePage(nextPageNum)))
                return (rc2);
            bHeader->nextBucket = nextNextPage; // make this bucket point to the bucket
                // the deleted bucket pointed to
        }

        if (rc == 0) // if we found the value, return
            return (0);
    }

    // Otherwise, search in this bucket
    PageNum ridPage;
    SlotNum ridSlot;
    if ((rc = rid.GetPageNum(ridPage)) || (rc = rid.GetSlotNum(ridSlot)))
        return (rc);

    // Search through entire values
    int prevIndex = BEGINNING_OF_SLOTS;
    int currIndex = bHeader->firstSlotIndex;
    bool found = false;
    while (currIndex != NO_MORE_SLOTS) {
        if (entries[currIndex].page == ridPage && entries[currIndex].slot == ridSlot) {
            found = true;
            break;
        }
        prevIndex = currIndex;
        currIndex = entries[prevIndex].nextSlot;
    }

    // if found, delete from it
    if (found) {
        if (prevIndex == BEGINNING_OF_SLOTS)
            bHeader->firstSlotIndex = entries[currIndex].nextSlot;
        else
            entries[prevIndex].nextSlot = entries[currIndex].nextSlot;
        entries[currIndex].nextSlot = bHeader->freeSlotIndex;
        bHeader->freeSlotIndex = currIndex;

        bHeader->num_keys--;
        // If there is one or less keys in this bucket, mark it for deletion.
        if (bHeader->num_keys == 1 || bHeader->num_keys == 0) {
            int firstSlot = bHeader->firstSlotIndex;
            RID last(entries[firstSlot].page, entries[firstSlot].slot);
            lastRID = last; // Return the last RID to move to the previous bucket
            deletePage = true;
        }

        return (0);
    }
    return (IX_INVALIDENTRY); // if not found, return IX_INVALIDENTRY
}

RC IX_IndexHandle::CreateNewNode(PF_PageHandle& ph, PageNum& page, char*& nData, bool isLeaf)
{
    RC rc = 0;
    if ((rc = pfh.AllocatePage(ph)) || (rc = ph.GetPageNum(page)) || (rc = ph.GetData(nData)))
        return rc;
    IX_NodeHeader* nHeader = (IX_NodeHeader*)nData;
    nHeader->firstSlotIndex = NO_MORE_SLOTS;
    nHeader->freeSlotIndex = 0;
    nHeader->isEmpty = true;
    nHeader->num_keys = 0;
    nHeader->isLeafNode = isLeaf;
    nHeader->invalid1 = NO_MORE_PAGES;
    nHeader->invalid2 = NO_MORE_PAGES;

    Node_Entry* entries = (struct Node_Entry*)((char*)nHeader + header.entryOffset_N);
    for (int i = 0; i < header.maxKeys_N; i++) {
        entries[i].isValid = UNOCCUPIED;
        entries[i].page = NO_MORE_PAGES;
        if (i == (header.maxKeys_N - 1))
            entries[i].nextSlot = NO_MORE_SLOTS;
        else
            entries[i].nextSlot = i + 1;
    }
    return rc;
}

RC IX_IndexHandle::CreateNewBucket(PageNum& page)
{
    char* nData;
    PF_PageHandle ph;
    RC rc = 0;
    if ((rc = pfh.AllocatePage(ph)) || (rc = ph.GetPageNum(page))) {
        return (rc);
    }
    if ((rc = ph.GetData(nData))) {
        RC rc2;
        if (rc2 = pfh.UnpinPage(page))
            return rc2;
        return rc;
    }
    IX_BucketHeader* bHeader = (struct IX_BucketHeader*)nData;
    bHeader->num_keys = 0;
    bHeader->freeSlotIndex = 0;
    bHeader->firstSlotIndex = NO_MORE_SLOTS;
    bHeader->nextBucket = NO_MORE_PAGES;

    Bucket_Entry* entries = (struct Bucket_Entry*)((char*)bHeader + header.entryOffset_B);
    for (int i = 0; i < header.maxKeys_B; i++) {
        if (i == (header.maxKeys_B - 1))
            entries[i].nextSlot = NO_MORE_SLOTS;
        else
            entries[i].nextSlot = i + 1;
    }
    if ((rc = pfh.MarkDirty(page)) || (rc = pfh.UnpinPage(page)))
        return (rc);

    return (rc);
}

RC IX_IndexHandle::ForcePages()
{
    RC rc = 0;
    if (isOpenHandle == false)
        return (IX_INVALIDINDEXHANDLE);
    pfh.ForcePages();
    return (rc);
}

RC IX_IndexHandle::GetFirstLeafPage(PF_PageHandle& leafPH, PageNum& leafPage)
{
    RC rc;
    PageNum nowpage = header.rootPage;
    IX_NodeHeader* rHeader;
    PF_PageHandle nowPH = rootPH;
    if (rc = rootPH.GetData((char*&)rHeader))
        return rc;

    while (rHeader->isLeafNode == false) {
        PageNum nextpage = ((IX_NodeHeader_I*)rHeader)->firstPage;
        if (nextpage == NO_MORE_PAGES)
            return IX_EOF;
        if ((rc = pfh.GetThisPage(nextpage, nowPH)) || (rc = nowPH.GetData((char*&)rHeader)))
            return rc;
        if ((nowpage!=header.rootPage)&&(rc = pfh.UnpinPage(nowpage)))
            return rc;
        nowpage = nextpage;
    }
    leafPH = nowPH;
    leafPage = nowpage;
    return rc;
}

int IX_IndexHandle::CalcNumKeysBucket(int attrLength)
{
    return (PF_PAGE_SIZE - sizeof(IX_BucketHeader)) / sizeof(Bucket_Entry);
}

int IX_IndexHandle::CalcNumKeysNode(int attrLength)
{
    return (PF_PAGE_SIZE - sizeof(IX_NodeHeader)) / (sizeof(Node_Entry) + attrLength);
}

bool IX_IndexHandle::isValidIndexHeader() const
{
    if (header.maxKeys_N <= 0 || header.maxKeys_B <= 0) {
        printf("error 1");
        return false;
    }
    if (header.entryOffset_N != sizeof(struct IX_NodeHeader) || header.entryOffset_B != sizeof(struct IX_BucketHeader)) {
        printf("error 2");
        return false;
    }

    int attrLength2 = (header.keysOffset_N - header.entryOffset_N) / (header.maxKeys_N);
    if (attrLength2 != sizeof(struct Node_Entry)) {
        printf("error 3");
        return false;
    }
    if ((header.entryOffset_B + header.maxKeys_B * sizeof(Bucket_Entry) > PF_PAGE_SIZE) || header.keysOffset_N + header.maxKeys_N * header.attr_length > PF_PAGE_SIZE)
        return false;
    return true;
}

RC IX_IndexHandle::FindRecordPage(PF_PageHandle& leafPH, PageNum& leafPage, void* key)
{
    RC rc = 0;
    IX_NodeHeader* rHeader;
    PF_PageHandle nowPH = rootPH;
    PageNum nowPage = header.rootPage;
    if ((rc = rootPH.GetData((char*&)rHeader)))
        return rc;

    while (rHeader->isLeafNode == false) {
        int index;
        bool isDup;
        PageNum nextPage;
        Node_Entry* entries = (struct Node_Entry*)((char*)rHeader + header.entryOffset_N);
        if (rc = FindNodeInsertIndex(rHeader, key, index, isDup))
            return rc;
        if (index == BEGINNING_OF_SLOTS) {
            nextPage = ((IX_NodeHeader_I*)rHeader)->firstPage;
        } else {
            nextPage = entries[index].page;
        }
        assert(nextPage != NO_MORE_PAGES);
        if ((nowPage != header.rootPage)&&(rc = pfh.UnpinPage(nowPage)))
            return rc;
        if ((rc = pfh.GetThisPage(nextPage, nowPH)) || (rc = nowPH.GetData((char*&)rHeader)))
            return rc;
        nowPage = nextPage;
    }
    leafPH = nowPH;
    leafPage = nowPage;
    return rc;
}