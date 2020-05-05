/*
//
// PF_Manager: provides PF file management
//
class PF_Manager {
public:
   PF_Manager    ();                              // Constructor
   ~PF_Manager   ();                              // Destructor
   RC CreateFile    (const char *fileName);       // Create a new file
   RC DestroyFile   (const char *fileName);       // Delete a file

   // Open and close file methods
   RC OpenFile      (const char *fileName, PF_FileHandle &fileHandle);
   RC CloseFile     (PF_FileHandle &fileHandle);

   // Three methods that manipulate the buffer manager.  The calls are
   // forwarded to the PF_BufferMgr instance and are called by parse.y
   // when the user types in a system command.
   RC ClearBuffer   ();
   RC PrintBuffer   ();
   RC ResizeBuffer  (int iNewSize);

   // Three Methods for manipulating raw memory buffers.  These memory
   // locations are handled by the buffer manager, but are not
   // associated with a particular file.  These should be used if you
   // want to create structures in memory that are bounded by the size
   // of the buffer pool.
   //
   // Note: If you lose the pointer to the buffer that is passed back
   // from AllocateBlock or do not call DisposeBlock with that pointer
   // then the internal memory will always remain "pinned" in the buffer
   // pool and you will have lost a slot in the buffer pool.

   // Return the size of the block that can be allocated.
   RC GetBlockSize  (int &length) const;

   // Allocate a memory chunk that lives in buffer manager
   RC AllocateBlock (char *&buffer);
   // Dispose of a memory chunk managed by the buffer manager.
   RC DisposeBlock  (char *buffer);

private:
   PF_BufferMgr *pBufferMgr;                      // page-buffer manager
};
*/

/*
class PF_FileHandle {
   friend class PF_Manager;
public:
   PF_FileHandle  ();                            // Default constructor
   ~PF_FileHandle ();                            // Destructor

   // Copy constructor
   PF_FileHandle  (const PF_FileHandle &fileHandle);

   // Overload =
   PF_FileHandle& operator=(const PF_FileHandle &fileHandle);

   // Get the first page
   RC GetFirstPage(PF_PageHandle &pageHandle) const;
   // Get the next page after current
   RC GetNextPage (PageNum current, PF_PageHandle &pageHandle) const;
   // Get a specific page
   RC GetThisPage (PageNum pageNum, PF_PageHandle &pageHandle) const;
   // Get the last page
   RC GetLastPage(PF_PageHandle &pageHandle) const;
   // Get the prev page after current
   RC GetPrevPage (PageNum current, PF_PageHandle &pageHandle) const;

   RC AllocatePage(PF_PageHandle &pageHandle);    // Allocate a new page
   RC DisposePage (PageNum pageNum);              // Dispose of a page
   RC MarkDirty   (PageNum pageNum) const;        // Mark page as dirty
   RC UnpinPage   (PageNum pageNum) const;        // Unpin the page

   // Flush pages from buffer pool.  Will write dirty pages to disk.
   RC FlushPages  () const;

   // Force a page or pages to disk (but do not remove from the buffer pool)
   RC ForcePages  (PageNum pageNum=ALL_PAGES) const;

private:

   // IsValidPageNum will return TRUE if page number is valid and FALSE
   // otherwise
   int IsValidPageNum (PageNum pageNum) const;

   PF_BufferMgr *pBufferMgr;                      // pointer to buffer manager
   PF_FileHdr hdr;                                // file header
   int bFileOpen;                                 // file open flag
   int bHdrChanged;                               // dirty flag for file hdr
   int unixfd;                                    // OS file descriptor
};
*/

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>

#include "pf_buffermgr.h"
#include "pf_internal.h"

//在manager这里搞个bufmgr
PF_Manager::PF_Manager() {
  // Create Buffer Manager
  pBufferMgr = new PF_BufferMgr(PF_BUFFER_SIZE);
}

PF_Manager::~PF_Manager() {
  // Destroy the buffer manager objects
  delete pBufferMgr;
}

//创建文件 写个头
RC PF_Manager::CreateFile(const char *fileName) {
  int fd;        // unix file descriptor
  int numBytes;  // return code form write syscall

  // Create file for exclusive use
  if ((fd = open(fileName, O_CREAT | O_EXCL | O_WRONLY, CREATION_MASK)) < 0)
    return (PF_UNIX);

  // Initialize the file header: must reserve FileHdrSize bytes in memory
  // though the actual size of FileHdr is smaller
  char hdrBuf[PF_FILE_HDR_SIZE];

  // So that Purify doesn't complain
  memset(hdrBuf, 0, PF_FILE_HDR_SIZE);

  PF_FileHdr *hdr = (PF_FileHdr *)hdrBuf;
  hdr->firstFree = PF_PAGE_LIST_END;
  hdr->numPages = 0;

  // Write header to file
  if ((numBytes = write(fd, hdrBuf, PF_FILE_HDR_SIZE)) != PF_FILE_HDR_SIZE) {
    // Error while writing: close and remove file
    close(fd);
    unlink(fileName);

    // Return an error
    if (numBytes < 0)
      return (PF_UNIX);
    else
      return (PF_HDRWRITE);
  }

  // Close file
  if (close(fd) < 0) return (PF_UNIX);

  // Return ok
  return (0);
}

RC PF_Manager::DestroyFile(const char *fileName) {
  // Remove the file
  if (unlink(fileName) < 0) return (PF_UNIX);

  // Return ok
  return (0);
}

//读个头，初始化一下filehandle的参数
RC PF_Manager::OpenFile(const char *fileName, PF_FileHandle &fileHandle) {
  int rc;  // return code

  // Ensure file is not already open
  if (fileHandle.bFileOpen) return (PF_FILEOPEN);

  // Open the file
  if ((fileHandle.unixfd = open(fileName,
#ifdef PC
                                O_BINARY |
#endif
                                    O_RDWR)) < 0)
    return (PF_UNIX);

  // Read the file header
  {
    int numBytes =
        read(fileHandle.unixfd, (char *)&fileHandle.hdr, sizeof(PF_FileHdr));
    if (numBytes != sizeof(PF_FileHdr)) {
      rc = (numBytes < 0) ? PF_UNIX : PF_HDRREAD;
      goto err;
    }
  }

  // Set file header to be not changed
  fileHandle.bHdrChanged = FALSE;

  // Set local variables in file handle object to refer to open file
  fileHandle.pBufferMgr = pBufferMgr;
  fileHandle.bFileOpen = TRUE;

  // Return ok
  return 0;

err:
  // Close file
  close(fileHandle.unixfd);
  fileHandle.bFileOpen = FALSE;

  // Return error
  return (rc);
}

//要flush一下
RC PF_Manager::CloseFile(PF_FileHandle &fileHandle) {
  RC rc;

  // Ensure fileHandle refers to open file
  if (!fileHandle.bFileOpen) return (PF_CLOSEDFILE);

  // Flush all buffers for this file and write out the header
  if ((rc = fileHandle.FlushPages())) return (rc);

  // Close the file
  if (close(fileHandle.unixfd) < 0) return (PF_UNIX);
  fileHandle.bFileOpen = FALSE;

  // Reset the buffer manager pointer in the file handle
  fileHandle.pBufferMgr = nullptr;

  // Return ok
  return 0;
}

//命令行调用的
RC PF_Manager::ClearBuffer() { return pBufferMgr->ClearBuffer(); }

RC PF_Manager::PrintBuffer() { return pBufferMgr->PrintBuffer(); }

RC PF_Manager::ResizeBuffer(int iNewSize) {
  return pBufferMgr->ResizeBuffer(iNewSize);
}

RC PF_Manager::GetBlockSize(int &length) const {
  return pBufferMgr->GetBlockSize(length);
}

RC PF_Manager::AllocateBlock(char *&buffer) {
  return pBufferMgr->AllocateBlock(buffer);
}

RC PF_Manager::DisposeBlock(char *buffer) {
  return pBufferMgr->DisposeBlock(buffer);
}
