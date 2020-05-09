/*
// RM_FileHeader: Header for each file
struct RM_FileHeader {
  int recordSize;           // record size in file
  int numRecordsPerPage;    // calculated max # of recs per page
  int numPages;             // number of pages
  PageNum firstFreePage;    // pointer to first free object

  int bitmapOffset;         // location in bytes of where the bitmap starts
                            // in the page headers
  int bitmapSize;           // size of bitmaps in the page headers
};

class RM_FileHandle {
    static const PageNum NO_FREE_PAGES = -1;
    friend class RM_Manager;
    friend class RM_FileScan;
public:
    RM_FileHandle ();
    ~RM_FileHandle();
    RM_FileHandle& operator= (const RM_FileHandle &fileHandle);

    // Given a RID, return the record
    RC GetRec     (const RID &rid, RM_Record &rec) const;

    RC InsertRec  (const char *pData, RID &rid);       // Insert a new record

    RC DeleteRec  (const RID &rid);                    // Delete a record
    RC UpdateRec  (const RM_Record &rec);              // Update a record

    // Forces a page (along with any contents stored in this class)
    // from the buffer pool to disk.  Default value forces all pages.
    RC ForcePages (PageNum pageNum = ALL_PAGES);
    //RC UnpinPage(PageNum page);
private:
    // Converts from the number of bits to the appropriate char size
    // to store those bits
    static int NumBitsToCharSize(int size);

    // Calculate the number of records per page based on the size of the
    // record to be stored
    static int CalcNumRecPerPage(int recSize);

    // Checks that the file header is valid
    bool isValidFileHeader() const;
    int getRecordSize(); // returns the record size

    // Returns the next record in rec, and the corresponding PF_PageHandle
    // in ph, given the current page and slot number from where to start
    // the search, and whether the next page should be used
    RC GetNextRecord(PageNum page, SlotNum slot, RM_Record &rec, PF_PageHandle
&ph, bool nextPage);

    // Allocates a new page, and returns its page number in page, and the
    // pinned PageHandle in ph
    RC AllocateNewPage(PF_PageHandle &ph, PageNum &page);

    // returns true if this FH is associated with an open file
    bool isValidFH() const;

    // Wrapper around retrieving the bitmap of records and data from a
pageHandle RC GetPageDataAndBitmap(PF_PageHandle &ph, char *&bitmap, struct
RM_PageHeader *&pageheader) const;
    // Wrapper around retrieving the page and slot number from an RID
    RC GetPageNumAndSlot(const RID &rid, PageNum &page, SlotNum &slot) const;

    //These tests are helpers with bit manipulation on the page headers
    // which keep track of which slots have records and which don't
    RC ResetBitmap(char * bitmap, int size);
    RC SetBit(char * bitmap, int size, int bitnum);
    RC ResetBit(char * bitmap, int size, int bitnum);
    RC CheckBitSet(char *bitmap, int size, int bitnum, bool &set) const;
    RC GetFirstZeroBit(char *bitmap, int size, int &location);
    RC GetNextOneBit(char *bitmap, int size, int start, int &location);

    bool openedFH;
    struct RM_FileHeader header;
    PF_FileHandle pfh;
    bool header_modified;
};
struct RM_PageHeader {
  PageNum nextFreePage;
  int numRecords;
};
*/

#include <math.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>

#include "pf.h"
#include "rm_internal.h"

RM_FileHandle::RM_FileHandle() {
    // initially, it is not associated with an open file.
    header_modified = false;
    openedFH = false;
}

RM_FileHandle::~RM_FileHandle() {
    openedFH = false;  // disassociate from fileHandle from an open file
}

RM_FileHandle& RM_FileHandle::operator=(const RM_FileHandle& fileHandle) {
    // sets all contents equal to another RM_FileHandle object
    if (this != &fileHandle) {
        this->openedFH = fileHandle.openedFH;
        this->header_modified = fileHandle.header_modified;
        this->pfh = fileHandle.pfh;
        header = fileHandle.header;
        // memcpy(&this->header, &fileHandle.header, sizeof(struct
        // RM_FileHeader));
    }
    return (*this);
}

// RM层的分配新分页不仅仅是调用pf的alloc那么简单，还要初始化rm分页头以及位图
RC RM_FileHandle::AllocateNewPage(PF_PageHandle& ph, PageNum& page) {
    RC rc;
    // allocate the page
    if ((rc = pfh.AllocatePage(ph))) {
        return (rc);
    }
    // get the page number of this allocated page
    if ((rc = ph.GetPageNum(page)))
        return (rc);

    char* bitmap;
    struct RM_PageHeader* pageheader;
    if ((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
        return (rc);
    pageheader->nextFreePage = header.firstFreePage;
    pageheader->numRecords = 0;
    if ((rc = ResetBitmap(bitmap, header.numRecordsPerPage)))
        return (rc);
    header.numPages++;
    header.firstFreePage = page;
    header_modified = true;
    return (0);
}

//计算出rm分页头以及位图偏移而已
RC RM_FileHandle::GetPageDataAndBitmap(
    PF_PageHandle& ph,
    char*& bitmap,
    struct RM_PageHeader*& pageheader) const {
    RC rc;
    char* pData;
    if ((rc = ph.GetData(pData)))
        return rc;
    pageheader = (RM_PageHeader*)pData;
    bitmap = pData + header.bitmapOffset;
    return 0;
}

RC RM_FileHandle::GetPageNumAndSlot(const RID& rid,
                                    PageNum& page,
                                    SlotNum& slot) const {
    RC rc;
    if ((rc = rid.isValidRID()))
        return (rc);
    if ((rc = rid.GetPageNum(page)))
        return (rc);
    if ((rc = rid.GetSlotNum(slot)))
        return (rc);
    return (0);
}

char* RM_FileHandle::GetRecPtr(char* p, int slot) const {
    if (slot >= header.numRecordsPerPage)
        return nullptr;
    return p + header.bitmapOffset + header.bitmapSize +
           header.recordSize * slot;
}

char* RM_FileHandle::GetRecPtrByBitmap(char* bitmap, int slot) const {
    if (slot >= header.numRecordsPerPage)
        return nullptr;
    return bitmap + (header.bitmapSize) + slot * header.recordSize;
}

//根据索引获取record
RC RM_FileHandle::GetRec(const RID& rid, RM_Record& rec) const {
    if (!isValidFH())
        return (RM_INVALIDFILE);
    RC rc = 0;
    PageNum page;
    SlotNum slot;
    if ((rc = GetPageNumAndSlot(rid, page, slot)))
        return (rc);

    PF_PageHandle ph;
    if ((rc = pfh.GetThisPage(page, ph)))
        return rc;
    char* bitmap;
    struct RM_PageHeader* pageheader;
    if ((rc = GetPageDataAndBitmap(ph, bitmap, pageheader))) {
        goto reterr;
    }
    bool recordExists;
    if (rc =
            CheckBitSet(bitmap, header.numRecordsPerPage, slot, recordExists)) {
        goto reterr;
    }
    if (!recordExists) {
        rc = RM_INVALIDRECORD;
        goto reterr;
    }

    char* pData;
    if ((rc = ph.GetData(pData))) {
        goto reterr;
    }

    if (rc = rec.SetRecord(rid, GetRecPtr(pData, slot), header.recordSize)) {
        goto reterr;
    }

reterr:
    RC rc2;
    if (rc2 = pfh.UnpinPage(page))
        return rc2;
    return rc;
}

RC RM_FileHandle::CheckBitSet(char* bitmap,
                              int size,
                              int bitnum,
                              bool& set) const {
    if (bitnum > size)
        return (RM_INVALIDBITOPERATION);
    int b = bitnum / 8;
    int offset = bitnum & 7;
    set = (bitmap[b] >> offset) & 1;
    return 0;
}

RC RM_FileHandle::InsertRec(const char* pData, RID& rid) {
    if (!isValidFH())
        return (RM_INVALIDFILE);
    RC rc = 0;
    if (pData == NULL)
        return RM_INVALIDRECORD;
    PF_PageHandle ph;
    PageNum page;
    if (header.firstFreePage == NO_FREE_PAGES) {
        // Alloc这里会把新页挂在freelist上
        AllocateNewPage(ph, page);
    } else {
        if ((rc = pfh.GetThisPage(header.firstFreePage, ph)))
            return (rc);
        page = header.firstFreePage;
    }
    char* bitmap;
    struct RM_PageHeader* pageheader;
    int slot;
    if ((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
        goto retclean;
    if ((rc = GetFirstZeroBit(bitmap, header.numRecordsPerPage, slot)))
        goto retclean;
    if ((rc = SetBit(bitmap, header.numRecordsPerPage, slot)))
        goto retclean;

    char* recPtr;
    recPtr = nullptr;
    recPtr = GetRecPtrByBitmap(bitmap, slot);
    if (recPtr == nullptr)
        return RM_INVALIDRID;
    memcpy(recPtr, pData, header.recordSize);
    pageheader->numRecords++;
    rid = RID(page, slot);
    if (pageheader->numRecords == header.numRecordsPerPage) {
        header.firstFreePage = pageheader->nextFreePage;
        header_modified = true;
    }
retclean:
    RC rc2;
    if ((rc2 = pfh.MarkDirty(page)) || (rc2 = pfh.UnpinPage(page)))
        return rc2;
    return rc;
}

RC RM_FileHandle::DeleteRec(const RID& rid) {
    if (!isValidFH())
        return (RM_INVALIDFILE);
    RC rc = 0;
    PageNum page;
    SlotNum slot;
    if ((rc = GetPageNumAndSlot(rid, page, slot)))
        return (rc);
    PF_PageHandle ph;
    if (rc = pfh.GetThisPage(page, ph))
        return rc;
    char* bitmap;
    RM_PageHeader* pageheader;
    if (rc = GetPageDataAndBitmap(ph, bitmap, pageheader))
        goto retclean;
    bool recordExists;
    if (rc = CheckBitSet(bitmap, header.numRecordsPerPage, slot, recordExists))
        goto retclean;
    if (!recordExists) {
        rc = RM_INVALIDRECORD;
        goto retclean;
    }
    if (rc = ResetBit(bitmap, header.numRecordsPerPage, slot))
        goto retclean;
    pageheader->numRecords--;
    if (pageheader->numRecords == header.numRecordsPerPage - 1) {
        pageheader->nextFreePage = header.firstFreePage;
        header.firstFreePage = page;
        header_modified = true;
    }
retclean:
    RC rc2;
    if ((rc2 = pfh.MarkDirty(page)) || (rc2 = pfh.UnpinPage(page)))
        return rc2;
    return rc;
}

RC RM_FileHandle::UpdateRec(const RM_Record& rec) {
    if (!isValidFH())
        return (RM_INVALIDFILE);
    RC rc = 0;
    RID rid;
    if ((rc = rec.GetRid(rid)))
        return (rc);
    PageNum page;
    SlotNum slot;
    if ((rc = GetPageNumAndSlot(rid, page, slot)))
        return (rc);
    PF_PageHandle ph;
    if (rc = pfh.GetThisPage(page, ph))
        return rc;
    char* bitmap;
    RM_PageHeader* pageheader;
    if (rc = GetPageDataAndBitmap(ph, bitmap, pageheader))
        goto retclean;
    bool exists;
    if (rc = CheckBitSet(bitmap, header.numRecordsPerPage, slot, exists))
        goto retclean;
    if (!exists) {
        rc = RM_INVALIDRECORD;
        goto retclean;
    }
    char* recData;
    recData = GetRecPtrByBitmap(bitmap, slot);
    if (recData == nullptr) {
        rc = RM_INVALIDRECORD;
        goto retclean;
    }
    char* pData;
    if (rc = rec.GetData(pData))
        goto retclean;
    memcpy(recData, pData, header.recordSize);
retclean:
    RC rc2;
    if ((rc2 = pfh.MarkDirty(page)) || (rc2 = pfh.UnpinPage(page)))
        return rc2;
    return rc;
}

RC RM_FileHandle::ForcePages(PageNum pageNum) {
    // only proceed if this filehandle is associated with an open file
    if (!isValidFH())
        return (RM_INVALIDFILE);
    pfh.ForcePages(pageNum);
    return (0);
}

// TODO
//取得下一条记录，这里还用了nextPage
//感觉不是很好的实现,但也是可以接受的实现
RC RM_FileHandle::GetNextRecord(PageNum page,
                                SlotNum slot,
                                RM_Record& rec,
                                PF_PageHandle& ph,
                                bool nextPage) {
    RC rc = 0;
    char* bitmap;
    struct RM_PageHeader* pageheader;
    int nextRec;
    PageNum nextRecPage = page;
    SlotNum nextRecSlot;

    // If we are looking in the next page, keep running GetNextPage until
    // we reach a page that has some records in it.
    if (nextPage) {
        while (true) {
            // printf("Getting next page\n");
            if ((PF_EOF == pfh.GetNextPage(nextRecPage, ph)))
                return (RM_EOF);  // reached the end of file

            // retrieve page and bitmap information
            if ((rc = ph.GetPageNum(nextRecPage)))
                return (rc);
            if ((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
                return (rc);
            // search for the next record
            if (GetNextOneBit(bitmap, header.numRecordsPerPage, 0, nextRec) !=
                RM_ENDOFPAGE)
                break;
            // if there are no records, on the page, unpin and get the next page
            // printf("Unpinning page\n");
            if ((rc = pfh.UnpinPage(nextRecPage)))
                return (rc);
        }
    } else {
        // get the bitmap for this page, and the next record location
        if ((rc = GetPageDataAndBitmap(ph, bitmap, pageheader)))
            return (rc);
        if (GetNextOneBit(bitmap, header.numRecordsPerPage, slot + 1,
                          nextRec) == RM_ENDOFPAGE)
            return (RM_EOF);
    }
    // set the RID, and data contents of this record
    nextRecSlot = nextRec;
    RID rid(nextRecPage, nextRecSlot);
    if ((rc = rec.SetRecord(
             rid,
             bitmap + (header.bitmapSize) + (nextRecSlot) * (header.recordSize),
             header.recordSize)))
        return (rc);

    return (0);
}

bool RM_FileHandle::isValidFH() const {
    if (openedFH == true)
        return true;
    return false;
}

bool RM_FileHandle::isValidFileHeader() const {
    if (!isValidFH()) {
        // puts("FH.");
        return false;
    }
    if (header.recordSize <= 0 || header.numRecordsPerPage <= 0 ||
        header.numPages <= 0) {
        // puts("recordSize nums pages.");
        return false;
    }
    if ((header.bitmapOffset + header.bitmapSize +
         header.recordSize * header.numRecordsPerPage) > PF_PAGE_SIZE) {
        return false;
    }

#ifdef DEBUG
    printf("bitmap off:%d\nbitmap size:%d recsize:%d nums:%d\n",
           header.bitmapOffset, header.bitmapSize, header.recordSize,
           header.numRecordsPerPage);
    printf("sum:%d \n", header.bitmapOffset + header.bitmapSize +
                            header.recordSize * header.numRecordsPerPage);
#endif
    return true;
}

int RM_FileHandle::getRecordSize() {
    return this->header.recordSize;
}

RC RM_FileHandle::ResetBitmap(char* bitmap, int size) {
    int char_num = NumBitsToCharSize(size);
    bzero(bitmap, char_num);
    // for (int i = 0; i < char_num; i++)
    //     bitmap[i] = bitmap[i] ^ bitmap[i];
    return (0);
}

RC RM_FileHandle::SetBit(char* bitmap, int size, int bitnum) {
    if (bitnum > size)
        return (RM_INVALIDBITOPERATION);
    int b = bitnum / 8;
    int offset = bitnum & 7;
    bitmap[b] |= 1 << offset;
    return 0;
}

RC RM_FileHandle::ResetBit(char* bitmap, int size, int bitnum) {
    if (bitnum > size)
        return (RM_INVALIDBITOPERATION);
    int b = bitnum / 8;
    int offset = bitnum & 7;
    bitmap[b] &= ~(1 << offset);
    return 0;
}

RC RM_FileHandle::GetFirstZeroBit(char* bitmap, int size, int& location) {
    RC rc = 0;
    for (int i = 0; i < size; i++) {
        bool set;
        if (rc = CheckBitSet(bitmap, size, i, set))
            return rc;
        if (!set) {
            location = i;
            return 0;
        }
    }
    return RM_PAGEFULL;
}

//这里传入的start是起点,即返回值可以是start
//返回下一个bit为1的位
RC RM_FileHandle::GetNextOneBit(char* bitmap,
                                int size,
                                int start,
                                int& location) {
    RC rc;
    for (int i = start; i < size; i++) {
        bool set;
        if (rc = CheckBitSet(bitmap, size, i, set))
            return rc;
        if (set) {
            location = i;
            return 0;
        }
    }
    return RM_ENDOFPAGE;
}

int RM_FileHandle::NumBitsToCharSize(int size) {
    return (size - 1) / 8 + 1;
}

//这个除了PF_PAGE_SIZE以外是对的，因为没有减去头
//佩服，学习了
//我改成用int
int RM_FileHandle::CalcNumRecPerPage(int recSize) {
    // return floor((PF_PAGE_SIZE * 1.0) / (1.0 * recSize + 1.0 / 8));
    return (PF_PAGE_SIZE - sizeof(RM_PageHeader)) * 8 / (1 + recSize * 8);
}
