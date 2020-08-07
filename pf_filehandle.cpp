#include <sys/types.h>
#include <unistd.h>
#include<cstdio>

#include "pf_buffermgr.h"
#include "pf_internal.h"

// class PF_BufferMgr;
/*
   PF_BufferMgr *pBufferMgr;                      // pointer to buffer manager
   PF_FileHdr hdr;                                // file
   header,空页链表+总分页数 int bFileOpen;                                 //
   file open flag int bHdrChanged;                               // dirty flag
   for file hdr int unixfd;
*/

/*
class PF_FileHandle {
  friend class PF_Manager;

 public:
  PF_FileHandle();   // Default constructor
  ~PF_FileHandle();  // Destructor

  // Copy constructor
  PF_FileHandle(const PF_FileHandle& fileHandle);

  // Overload =
  PF_FileHandle& operator=(const PF_FileHandle& fileHandle);

  // Get the first page
  RC GetFirstPage(PF_PageHandle& pageHandle) const;
  // Get the next page after current
  RC GetNextPage(PageNum current, PF_PageHandle& pageHandle) const;
  // Get a specific page
  RC GetThisPage(PageNum pageNum, PF_PageHandle& pageHandle) const;
  // Get the last page
  RC GetLastPage(PF_PageHandle& pageHandle) const;
  // Get the prev page after current
  RC GetPrevPage(PageNum current, PF_PageHandle& pageHandle) const;

  RC AllocatePage(PF_PageHandle& pageHandle);  // Allocate a new page
  RC DisposePage(PageNum pageNum);             // Dispose of a page
  RC MarkDirty(PageNum pageNum) const;         // Mark page as dirty
  RC UnpinPage(PageNum pageNum) const;         // Unpin the page

  // Flush pages from buffer pool.  Will write dirty pages to disk.
  RC FlushPages() const;

  // Force a page or pages to disk (but do not remove from the buffer pool)
  RC ForcePages(PageNum pageNum = ALL_PAGES) const;

 private:
  // IsValidPageNum will return TRUE if page number is valid and FALSE
  // otherwise
  int IsValidPageNum(PageNum pageNum) const;

  PF_BufferMgr* pBufferMgr;  // pointer to buffer manager
  PF_FileHdr hdr;            // file header
  int bFileOpen;             // file open flag
  int bHdrChanged;           // dirty flag for file hdr
  int unixfd;                // OS file descriptor
};
*/

PF_FileHandle::PF_FileHandle() {
  bFileOpen = false;
  pBufferMgr = nullptr;
}

PF_FileHandle::~PF_FileHandle() {}

PF_FileHandle::PF_FileHandle(const PF_FileHandle& fileHandle) {
  pBufferMgr = fileHandle.pBufferMgr;
  hdr = fileHandle.hdr;
  bFileOpen = fileHandle.bFileOpen;
  bHdrChanged = fileHandle.bHdrChanged;
  unixfd = fileHandle.unixfd;
}

PF_FileHandle& PF_FileHandle::operator=(const PF_FileHandle& fileHandle) {
  if (this != &fileHandle) {
    pBufferMgr = fileHandle.pBufferMgr;
    hdr = fileHandle.hdr;
    bFileOpen = fileHandle.bFileOpen;
    bHdrChanged = fileHandle.bHdrChanged;
    unixfd = fileHandle.unixfd;
  }
  return *this;
}

RC PF_FileHandle::GetFirstPage(PF_PageHandle& pageHandle) const {
  return (GetNextPage((PageNum)-1, pageHandle));
}

RC PF_FileHandle::GetLastPage(PF_PageHandle& pageHandle) const {
  return (GetPrevPage((PageNum)hdr.numPages, pageHandle));
}

//因为要跳过空页
RC PF_FileHandle::GetNextPage(PageNum current,
                              PF_PageHandle& pageHandle) const {
  int rc;
  if (!bFileOpen) return PF_CLOSEDFILE;
  //除了-1以外都要检查合法
  if (current != -1 && !IsValidPageNum(current)) return PF_INVALIDPAGE;
  for (++current; current < hdr.numPages; ++current) {
    rc = GetThisPage(current, pageHandle);
    if (rc == 0) return 0;
    if (rc != PF_INVALIDPAGE) return rc;
  }
  return PF_EOF;
}

RC PF_FileHandle::GetPrevPage(PageNum current,
                              PF_PageHandle& pageHandle) const {
  int rc;
  if (!bFileOpen) return PF_CLOSEDFILE;
  //除了num以外都要检查合法
  if (current != hdr.numPages && !IsValidPageNum(current))
    return PF_INVALIDPAGE;
  for (current--; current >= 0; current--) {
    if (!(rc = GetThisPage(current, pageHandle))) return (0);
    if (rc != PF_INVALIDPAGE) return (rc);
  }
  return (PF_EOF);
}

//很暴力的方法，直接读进来，看看nextFree标志是否为已用
RC PF_FileHandle::GetThisPage(PageNum pageNum,
                              PF_PageHandle& pageHandle) const {
  int rc;          // return code
  char* pPageBuf;  // address of page in buffer pool

  // File must be open
  if (!bFileOpen) return (PF_CLOSEDFILE);

  // Validate page number
  if (!IsValidPageNum(pageNum)) return (PF_INVALIDPAGE);

  // Get this page from the buffer manager
  if ((rc = pBufferMgr->GetPage(unixfd, pageNum, &pPageBuf))) return (rc);

  // If the page is valid, then set pageHandle to this page and return ok
  if (((PF_PageHdr*)pPageBuf)->nextFree == PF_PAGE_USED) {
    // Set the pageHandle local variables
    pageHandle.pageNum = pageNum;
    pageHandle.pPageData = pPageBuf + sizeof(PF_PageHdr);

    // Return ok
    return (0);
  }

  // If the page is *not* a valid one, then unpin the page
  if ((rc = UnpinPage(pageNum))) return (rc);

  return (PF_INVALIDPAGE);
}

RC PF_FileHandle::AllocatePage(PF_PageHandle& pageHandle) {
  int rc;          // return code
  int pageNum;     // new-page number
  char* pPageBuf;  // address of page in buffer pool

  // File must be open
  if (!bFileOpen) return (PF_CLOSEDFILE);

  // If the free list isn't empty...
  if (hdr.firstFree != PF_PAGE_LIST_END) {
    pageNum = hdr.firstFree;

    // Get the first free page into the buffer
    if ((rc = pBufferMgr->GetPage(unixfd, pageNum, &pPageBuf))) return (rc);

    // Set the first free page to the next page on the free list
    hdr.firstFree = ((PF_PageHdr*)pPageBuf)->nextFree;
  } else {
    // The free list is empty...
    pageNum = hdr.numPages;

    // Allocate a new page in the file
    if ((rc = pBufferMgr->AllocatePage(unixfd, pageNum, &pPageBuf)))
      return (rc);

    // Increment the number of pages for this file
    hdr.numPages++;
  }

  // Mark the header as changed
  bHdrChanged = true;

  // Mark this page as used
  ((PF_PageHdr*)pPageBuf)->nextFree = PF_PAGE_USED;

  // Zero out the page data
  memset(pPageBuf + sizeof(PF_PageHdr), 0, PF_PAGE_SIZE);

  // Mark the page dirty because we changed the next pointer
  if ((rc = MarkDirty(pageNum))) return (rc);

  // Set the pageHandle local variables
  pageHandle.pageNum = pageNum;
  pageHandle.pPageData = pPageBuf + sizeof(PF_PageHdr);

  // Return ok
  return (0);
}

RC PF_FileHandle::DisposePage(PageNum pageNum) {
  // printf("DisposePage call. pagenum:%d\n",pageNum);


  int rc;          // return code
  char* pPageBuf;  // address of page in buffer pool

  // File must be open
  if (!bFileOpen) return (PF_CLOSEDFILE);

  // Validate page number
  if (!IsValidPageNum(pageNum)) return (PF_INVALIDPAGE);

  // Get the page (but don't re-pin it if it's already pinned)
  if ((rc = pBufferMgr->GetPage(unixfd, pageNum, &pPageBuf, FALSE)))
    return (rc);

  // Page must be valid (used)
  if (((PF_PageHdr*)pPageBuf)->nextFree != PF_PAGE_USED) {
    // Unpin the page
    if ((rc = UnpinPage(pageNum))) return (rc);

    // Return page already free
    return (PF_PAGEFREE);
  }

  // Put this page onto the free list
  ((PF_PageHdr*)pPageBuf)->nextFree = hdr.firstFree;
  hdr.firstFree = pageNum;
  bHdrChanged = TRUE;

  // Mark the page dirty because we changed the next pointer
  if ((rc = MarkDirty(pageNum))) return (rc);

  // Unpin the page
  if ((rc = UnpinPage(pageNum))) return (rc);

  // Return ok
  return (0);
}

RC PF_FileHandle::MarkDirty(PageNum pageNum) const {
  // File must be open
  if (!bFileOpen) return (PF_CLOSEDFILE);

  // Validate page number
  if (!IsValidPageNum(pageNum)) return (PF_INVALIDPAGE);

  // Tell the buffer manager to mark the page dirty
  return (pBufferMgr->MarkDirty(unixfd, pageNum));
}

RC PF_FileHandle::UnpinPage(PageNum pageNum) const {
  // File must be open
  if (!bFileOpen) return (PF_CLOSEDFILE);

  // Validate page number
  if (!IsValidPageNum(pageNum)) return (PF_INVALIDPAGE);

  // Tell the buffer manager to unpin the page
  return (pBufferMgr->UnpinPage(unixfd, pageNum));
}

//把文件头也刷进磁盘,这里会释放分页，就是不在缓冲区中了
RC PF_FileHandle::FlushPages() const {
  // File must be open
  if (!bFileOpen) return (PF_CLOSEDFILE);

  // If the file header has changed, write it back to the file
  if (bHdrChanged) {
    // First seek to the appropriate place
    if (lseek(unixfd, 0, L_SET) < 0) return (PF_UNIX);

    // Write header
    int numBytes = write(unixfd, (char*)&hdr, sizeof(PF_FileHdr));
    if (numBytes < 0) return (PF_UNIX);
    if (numBytes != sizeof(PF_FileHdr)) return (PF_HDRWRITE);

    // This function is declared const, but we need to change the
    // bHdrChanged variable.  Cast away the constness
    PF_FileHandle* dummy = (PF_FileHandle*)this;
    dummy->bHdrChanged = FALSE;
  }

  // Tell Buffer Manager to flush pages
  return (pBufferMgr->FlushPages(unixfd));
}

///刷单页 ,这里不会释放，仅写入distk,也会刷文件头
RC PF_FileHandle::ForcePages(PageNum pageNum) const
{
   // File must be open
   if (!bFileOpen)
      return (PF_CLOSEDFILE);

   // If the file header has changed, write it back to the file
   if (bHdrChanged) {

      // First seek to the appropriate place
      if (lseek(unixfd, 0, L_SET) < 0)
         return (PF_UNIX);

      // Write header
      int numBytes = write(unixfd,
            (char *)&hdr,
            sizeof(PF_FileHdr));
      if (numBytes < 0)
         return (PF_UNIX);
      if (numBytes != sizeof(PF_FileHdr))
         return (PF_HDRWRITE);

      // This function is declared const, but we need to change the
      // bHdrChanged variable.  Cast away the constness
      PF_FileHandle *dummy = (PF_FileHandle *)this;
      dummy->bHdrChanged = FALSE;
   }

   // Tell Buffer Manager to Force the page
   return (pBufferMgr->ForcePages(unixfd, pageNum));
}



int PF_FileHandle::IsValidPageNum(PageNum pageNum) const
{
   return (bFileOpen &&
         pageNum >= 0 &&
         pageNum < hdr.numPages);
}

RC PF_FileHandle:: GetFileHdr(PF_FileHdr& fhdr)const{
  fhdr=this->hdr;
  return 0;
}
