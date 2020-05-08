/*
class RM_Manager {
public:
    RM_Manager    (PF_Manager &pfm);
    ~RM_Manager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);
    RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);

    RC CloseFile  (RM_FileHandle &fileHandle);
private:
    // helper method for open scan which sets up private variables of
    // RM_FileHandle.
    RC SetUpFH(RM_FileHandle& fileHandle, PF_FileHandle &fh, struct
RM_FileHeader* header);
    // helper method for close scan which sets up private variables of
    // RM_FileHandle
    RC CleanUpFH(RM_FileHandle &fileHandle);

    PF_Manager &pfm; // reference to program's PF_Manager
};

//RM_FileHandle 变量
    bool openedFH;
    struct RM_FileHeader header;
    PF_FileHandle pfh;
    bool header_modified;

//RM_FileHeader
struct RM_FileHeader {
  int recordSize;           // record size in file
  int numRecordsPerPage;    // calculated max # of recs per page
  int numPages;             // number of pages
  PageNum firstFreePage;    // pointer to first free object

  int bitmapOffset;         // location in bytes of where the bitmap starts
                            // in the page headers
  int bitmapSize;           // size of bitmaps in the page headers
};
*/

#include <sys/types.h>
#include <unistd.h>

#include "pf.h"
#include "rm_internal.h"

RM_Manager::RM_Manager(PF_Manager& pfm) : pfm(pfm) {}

RM_Manager::~RM_Manager() {}

// RM_FileHeader就在第0个分页的头(当然前面有个pf分页头再前面pf文件头)
RC RM_Manager::CreateFile(const char* fileName, int recordSize) {
    RC rc = 0;
    if (fileName == NULL)
        return (RM_BADFILENAME);
    if (recordSize <= 0 || recordSize > PF_PAGE_SIZE)
        return RM_BADRECORDSIZE;
    int numRecordsPerPage = RM_FileHandle::CalcNumRecPerPage(recordSize);
    int bitmapSize = RM_FileHandle::NumBitsToCharSize(numRecordsPerPage);
    int bitmapOffset = sizeof(RM_PageHeader);
    if (numRecordsPerPage <= 0)
        return RM_BADRECORDSIZE;
    if ((rc = pfm.CreateFile(fileName)))
        return (rc);
    PF_PageHandle ph;
    PF_FileHandle fh;
    RM_FileHeader* header;
    if (rc = pfm.OpenFile(fileName, fh))
        return rc;
    PageNum page;

    if ((rc = fh.AllocatePage(ph) || (rc = ph.GetPageNum(page))))
        return rc;
    char* pData;
    if (rc = ph.GetData(pData)) {
        goto retclean;
    }
    header = (RM_FileHeader*)pData;
    header->recordSize = recordSize;
    header->numRecordsPerPage = numRecordsPerPage;
    //这里把分页数设置为1,即把rmfileheader也算上了
    header->numPages = 1;
    header->bitmapSize = bitmapSize;
    header->bitmapOffset = bitmapOffset;
    header->firstFreePage = NO_MORE_FREE_PAGES;

retclean:
    RC rc2;
    if ((rc2 = fh.MarkDirty(page)) || (rc2 = fh.UnpinPage(page)) ||
        (rc2 = pfm.CloseFile(fh)))
        return rc2;
    return rc;
}

RC RM_Manager::DestroyFile(const char* fileName) {
    if (fileName == nullptr)
        return (RM_BADFILENAME);
    RC rc;
    if ((rc = pfm.DestroyFile(fileName)))
        return (rc);
    return (0);
}

RC RM_Manager::SetUpFH(RM_FileHandle& fileHandle,
                       PF_FileHandle& fh,
                       struct RM_FileHeader* header) {
    fileHandle.header = *header;
    fileHandle.pfh = fh;
    fileHandle.header_modified = false;
    fileHandle.openedFH = true;
    if (!fileHandle.isValidFileHeader()) {
        fileHandle.openedFH = false;
        return (RM_INVALIDFILE);
    }
    return (0);
}

/*
//RM_FileHandle 变量
    bool openedFH;
    struct RM_FileHeader header;
    PF_FileHandle pfh;
    bool header_modified;
*/
// TODO this func is totally made by myself
RC RM_Manager::OpenFile(const char* fileName, RM_FileHandle& fileHandle) {
    if (fileName == nullptr)
        return RM_BADFILENAME;
    if (fileHandle.openedFH)
        return RM_INVALIDFILEHANDLE;
    RC rc = 0;
    PF_FileHandle fh;
    if (rc = pfm.OpenFile(fileName, fh))
        return rc;
    PF_PageHandle ph;
    if (rc = fh.GetFirstPage(ph))
        return rc;
    PageNum page;
    if (rc = ph.GetPageNum(page))
        goto errclean;

    char* header;
    if (rc = ph.GetData(header))
        goto errclean;
    if (rc = SetUpFH(fileHandle, fh, (RM_FileHeader*)header))
        goto errclean;

    if ((rc = fh.UnpinPage(page)))
        return rc;
    return rc;

errclean:
    RC rc2;
    if ((rc2 = fh.UnpinPage(page)) || (rc2 = pfm.CloseFile(fh)))
        return rc2;
    return rc;
}

RC RM_Manager::CleanUpFH(RM_FileHandle& fileHandle) {
    if (fileHandle.openedFH == false)
        return (RM_INVALIDFILEHANDLE);
    fileHandle.openedFH = false;
    return (0);
}

RC RM_Manager::CloseFile(RM_FileHandle& fileHandle) {
    RC rc = 0;
    PF_PageHandle ph;
    PageNum page;
    char* pData;

    if (!fileHandle.openedFH)
        return RM_INVALIDFILEHANDLE;
    if (fileHandle.header_modified) {
        if (rc = fileHandle.pfh.GetFirstPage(ph))
            return rc;
        if (rc = ph.GetPageNum(page))
            goto errclean;
        if (rc = ph.GetData(pData))
            goto errclean;
        *((RM_FileHeader*)pData) = fileHandle.header;
        if ((rc = fileHandle.pfh.MarkDirty(page)) ||
            (rc = fileHandle.pfh.UnpinPage(page)))
            return rc;
    }
    if (rc = pfm.CloseFile(fileHandle.pfh))
        return rc;
    if (rc = CleanUpFH(fileHandle))
        return rc;
    return 0;

errclean:
    RC rc2;
    //说实话getPageNum的时候出错了这里是不是bug了
    if ((rc2 = fileHandle.pfh.UnpinPage(page)))
        return rc2;
    return rc;
}