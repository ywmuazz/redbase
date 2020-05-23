/*
#define INVALID_SLOT  (-1)

//
// PF_BufPageDesc - struct containing data about a page in the buffer
//
struct PF_BufPageDesc {
    char       *pData;      // page contents
    int        next;        // next in the linked list of buffer pages
    int        prev;        // prev in the linked list of buffer pages
    int        bDirty;      // TRUE if page is dirty
    short int  pinCount;    // pin count
    PageNum    pageNum;     // page number for this page
    int        fd;          // OS file descriptor of this page
};

//
// PF_BufferMgr - manage the page buffer
//
class PF_BufferMgr {
public:

    PF_BufferMgr     (int numPages);             // Constructor - allocate
                                                  // numPages buffer pages
    ~PF_BufferMgr    ();                         // Destructor

    // Read pageNum into buffer, point *ppBuffer to location
    RC  GetPage      (int fd, PageNum pageNum, char **ppBuffer,
                      int bMultiplePins = TRUE);
    // Allocate a new page in the buffer, point *ppBuffer to its location
    RC  AllocatePage (int fd, PageNum pageNum, char **ppBuffer);

    RC  MarkDirty    (int fd, PageNum pageNum);  // Mark page dirty
    RC  UnpinPage    (int fd, PageNum pageNum);  // Unpin page from the buffer
    RC  FlushPages   (int fd);                   // Flush pages for file

    // Force a page to the disk, but do not remove from the buffer pool
    RC ForcePages    (int fd, PageNum pageNum);


    // Remove all entries from the Buffer Manager.
    RC  ClearBuffer  ();
    // Display all entries in the buffer
    RC PrintBuffer   ();

    // Attempts to resize the buffer to the new size
    RC ResizeBuffer  (int iNewSize);

    // Three Methods for manipulating raw memory buffers.  These memory
    // locations are handled by the buffer manager, but are not
    // associated with a particular file.  These should be used if you
    // want memory that is bounded by the size of the buffer pool.

    // Return the size of the block that can be allocated.
    RC GetBlockSize  (int &length) const;

    // Allocate a memory chunk that lives in buffer manager
    RC AllocateBlock (char *&buffer);
    // Dispose of a memory chunk managed by the buffer manager.
    RC DisposeBlock  (char *buffer);

private:
    RC  InsertFree   (int slot);                 // Insert slot at head of free
    RC  LinkHead     (int slot);                 // Insert slot at head of used
    RC  Unlink       (int slot);                 // Unlink slot
    RC  InternalAlloc(int &slot);                // Get a slot to use

    // Read a page
    RC  ReadPage     (int fd, PageNum pageNum, char *dest);

    // Write a page
    RC  WritePage    (int fd, PageNum pageNum, char *source);

    // Init the page desc entry
    RC  InitPageDesc (int fd, PageNum pageNum, int slot);

    PF_BufPageDesc *bufTable;                     // info on buffer pages
    PF_HashTable   hashTable;                     // Hash table object
    int            numPages;                      // # of pages in the buffer
    int            pageSize;                      // Size of pages in the buffer
    int            first;                         // MRU page slot
    int            last;                          // LRU page slot
    int            free;                          // head of free list
};
*/

#include "pf_buffermgr.h"

#include <unistd.h>

#include <cstdio>
#include <iostream>

using namespace std;

PF_BufferMgr::PF_BufferMgr(int _numPages) : hashTable(PF_HASH_TBL_SIZE) {
  numPages = _numPages;
  pageSize = PF_PAGE_SIZE + sizeof(PF_PageHdr);
  bufTable = new PF_BufPageDesc[numPages];
  for (int i = 0; i < numPages; i++) {
    bufTable[i].pData = new char[pageSize];
    if (!bufTable[i].pData) {
      ERREXIT("Not enough memory for buffer");
    }
    bzero(bufTable[i].pData, sizeof(pageSize));
    bufTable[i].prev = i - 1;
    bufTable[i].next = i + 1;
  }
  bufTable[0].prev = bufTable[numPages - 1].next = INVALID_SLOT;
  free = 0;
  first = last = INVALID_SLOT;
}

PF_BufferMgr::~PF_BufferMgr() {
  // Free up buffer pages and tables
  for (int i = 0; i < this->numPages; i++) delete[] bufTable[i].pData;

  delete[] bufTable;
}

RC PF_BufferMgr::GetPage(int fd, PageNum pageNum, char **ppBuffer,
                         int bMultiplePins) {
  RC rc;
  int slot;

  if ((rc = hashTable.Find(fd, pageNum, slot)) && (rc != PF_HASHNOTFOUND))
    return (rc);  // unexpected error

  // If page not in buffer...
  if (rc == PF_HASHNOTFOUND) {
    // Allocate an empty page, this will also promote the newly allocated
    // page to the MRU slot
    if ((rc = InternalAlloc(slot))) return (rc);

    // read the page, insert it into the hash table,
    // and initialize the page description entry
    if ((rc = ReadPage(fd, pageNum, bufTable[slot].pData)) ||
        (rc = hashTable.Insert(fd, pageNum, slot)) ||
        (rc = InitPageDesc(fd, pageNum, slot))) {
      // Put the slot back on the free list before returning the error
      Unlink(slot);
      InsertFree(slot);
      return (rc);
    }
  } else {
    // Error if we don't want to get a pinned page
    if (!bMultiplePins && bufTable[slot].pinCount > 0) return (PF_PAGEPINNED);

    // Page is alredy in memory, just increment pin count
    bufTable[slot].pinCount++;
    if ((rc = Unlink(slot)) || (rc = LinkHead(slot))) return (rc);
  }

  *ppBuffer = bufTable[slot].pData;
  return 0;
}

// Alloc 分配一个未在文件中存在的页
//但其实只是分配一块0内存,到时候换出时再写入到disk
//所以这里没有用getPage，因为它要从硬盘读
RC PF_BufferMgr::AllocatePage(int fd, PageNum pageNum, char **ppBuffer) {
  RC rc;
  int slot;
  // If page is already in buffer, return an error
  if (!(rc = hashTable.Find(fd, pageNum, slot)))
    return (PF_PAGEINBUF);
  else if (rc != PF_HASHNOTFOUND)
    return (rc);  // unexpected error

  if ((rc = InternalAlloc(slot))) return (rc);

  // Insert the page into the hash table,
  // and initialize the page description entry
  if ((rc = hashTable.Insert(fd, pageNum, slot)) ||
      (rc = InitPageDesc(fd, pageNum, slot))) {
    // Put the slot back on the free list before returning the error
    Unlink(slot);
    InsertFree(slot);
    return (rc);
  }
  // Point ppBuffer to page
  *ppBuffer = bufTable[slot].pData;

  // Return ok
  return (0);
}

//只有pin的时候才能mark
//而且mark也会放到mru
RC PF_BufferMgr::MarkDirty(int fd, PageNum pageNum) {
  RC rc;     // return code
  int slot;  // buffer slot where page is located

  // The page must be found and pinned in the buffer
  if ((rc = hashTable.Find(fd, pageNum, slot))) {
    if ((rc == PF_HASHNOTFOUND))
      return (PF_PAGENOTINBUF);
    else
      return (rc);  // unexpected error
  }

  if (bufTable[slot].pinCount == 0) return (PF_PAGEUNPINNED);

  // Mark this page dirty
  bufTable[slot].bDirty = TRUE;

  // Make this page the most recently used page
  if ((rc = Unlink(slot)) || (rc = LinkHead(slot))) return (rc);

  // Return ok
  return (0);
}

// unpined到0才会换到mru
RC PF_BufferMgr::UnpinPage(int fd, PageNum pageNum) {
  RC rc;     // return code
  int slot;  // buffer slot where page is located


  // The page must be found and pinned in the buffer
  if ((rc = hashTable.Find(fd, pageNum, slot))) {
    if ((rc == PF_HASHNOTFOUND))
      return (PF_PAGENOTINBUF);
    else
      return (rc);  // unexpected error
  }

  if (bufTable[slot].pinCount == 0) return (PF_PAGEUNPINNED);

  // If unpinning the last pin, make it the most recently used page
  if (--(bufTable[slot].pinCount) == 0) {
    if ((rc = Unlink(slot)) || (rc = LinkHead(slot))) return (rc);
  }

  // Return ok
  return (0);
}

//遍历查找某个文件的所有分页
//有个rcWarn,当某个分页pin的时候不写(不是退出哦)
RC PF_BufferMgr::FlushPages(int fd) {
  RC rc, rcWarn = 0;  // return codes
  // Do a linear scan of the buffer to find pages belonging to the file
  int slot = first;
  while (slot != INVALID_SLOT) {
    int next = bufTable[slot].next;

    // If the page belongs to the passed-in file descriptor
    if (bufTable[slot].fd == fd) {
      // Ensure the page is not pinned
      if (bufTable[slot].pinCount) {
        rcWarn = PF_PAGEPINNED;
      } else {
        // Write the page if dirty
        if (bufTable[slot].bDirty) {
          if ((rc =
                   WritePage(fd, bufTable[slot].pageNum, bufTable[slot].pData)))
            return (rc);
          bufTable[slot].bDirty = FALSE;
        }

        // Remove page from the hash table and add the slot to the free list
        if ((rc = hashTable.Delete(fd, bufTable[slot].pageNum)) ||
            (rc = Unlink(slot)) || (rc = InsertFree(slot)))
          return (rc);
      }
    }
    slot = next;
  }

  // Return warning or ok
  return (rcWarn);
}

//仅写分页,不放到free
//而且这里连hashtable都不用,说实话写单个的时候可以用,写全部的时候因为不知道一共多少分页所以还是遍历好
RC PF_BufferMgr::ForcePages(int fd, PageNum pageNum) {
  RC rc;  // return codes
  // Do a linear scan of the buffer to find the page for the file
  int slot = first;
  while (slot != INVALID_SLOT) {
    int next = bufTable[slot].next;

    // If the page belongs to the passed-in file descriptor
    if (bufTable[slot].fd == fd &&
        (pageNum == ALL_PAGES || bufTable[slot].pageNum == pageNum)) {
      // I don't care if the page is pinned or not, just write it if
      // it is dirty.
      if (bufTable[slot].bDirty) {
        if ((rc = WritePage(fd, bufTable[slot].pageNum, bufTable[slot].pData)))
          return (rc);
        bufTable[slot].bDirty = FALSE;
      }
    }
    slot = next;
  }
  return 0;
}

RC PF_BufferMgr::PrintBuffer() {
  cout << "Buffer contains " << numPages << " pages of size " << pageSize
       << ".\n";
  cout << "Contents in order from most recently used to "
       << "least recently used.\n";

  int slot, next;
  slot = first;
  while (slot != INVALID_SLOT) {
    next = bufTable[slot].next;
    cout << slot << " :: \n";
    cout << "  fd = " << bufTable[slot].fd << "\n";
    cout << "  pageNum = " << bufTable[slot].pageNum << "\n";
    cout << "  bDirty = " << bufTable[slot].bDirty << "\n";
    cout << "  pinCount = " << bufTable[slot].pinCount << "\n";
    slot = next;
  }

  if (first == INVALID_SLOT)
    cout << "Buffer is empty!\n";
  else
    cout << "All remaining slots are free.\n";

  return 0;
}

//清掉非pin的缓冲块，不过它不写回disk
RC PF_BufferMgr::ClearBuffer() {
  RC rc;

  int slot, next;
  slot = first;
  while (slot != INVALID_SLOT) {
    next = bufTable[slot].next;
    if (bufTable[slot].pinCount == 0)
      if ((rc = hashTable.Delete(bufTable[slot].fd, bufTable[slot].pageNum)) ||
          (rc = Unlink(slot)) || (rc = InsertFree(slot)))
        return (rc);
    slot = next;
  }

  return 0;
}

//此函数只通过命令行调用
//感觉这个函数很废,首先你把pdata都重新分配的话,那pagehandle都废了,又无法更新他们
//自己写的话realloc再把新的插入free链即可
RC PF_BufferMgr::ResizeBuffer(int iNewSize) {
  int i;
  RC rc;

  // First try and clear out the old buffer!
  ClearBuffer();

  // Allocate memory for a new buffer table
  PF_BufPageDesc *pNewBufTable = new PF_BufPageDesc[iNewSize];

  // Initialize the new buffer table and allocate memory for buffer
  // pages.  Initially, the free list contains all pages
  for (i = 0; i < iNewSize; i++) {
    if ((pNewBufTable[i].pData = new char[pageSize]) == NULL) {
      cerr << "Not enough memory for buffer\n";
      exit(1);
    }

    memset((void *)pNewBufTable[i].pData, 0, pageSize);

    pNewBufTable[i].prev = i - 1;
    pNewBufTable[i].next = i + 1;
  }
  pNewBufTable[0].prev = pNewBufTable[iNewSize - 1].next = INVALID_SLOT;

  // Now we must remember the old first and last slots and (of course)
  // the buffer table itself.  Then we use insert methods to insert
  // each of the entries into the new buffertable
  int oldFirst = first;
  PF_BufPageDesc *pOldBufTable = bufTable;

  // Setup the new number of pages,  first, last and free
  numPages = iNewSize;
  first = last = INVALID_SLOT;
  free = 0;

  // Setup the new buffer table
  bufTable = pNewBufTable;

  // We must first remove from the hashtable any possible entries
  int slot, next, newSlot;
  slot = oldFirst;
  while (slot != INVALID_SLOT) {
    next = pOldBufTable[slot].next;

    // Must remove the entry from the hashtable from the
    if ((rc = hashTable.Delete(pOldBufTable[slot].fd,
                               pOldBufTable[slot].pageNum)))
      return (rc);
    slot = next;
  }

  // Now we traverse through the old buffer table and copy any old
  // entries into the new one
  slot = oldFirst;
  while (slot != INVALID_SLOT) {
    next = pOldBufTable[slot].next;
    // Allocate a new slot for the old page
    if ((rc = InternalAlloc(newSlot))) return (rc);

    // Insert the page into the hash table,
    // and initialize the page description entry
    if ((rc = hashTable.Insert(pOldBufTable[slot].fd,
                               pOldBufTable[slot].pageNum, newSlot)) ||
        (rc = InitPageDesc(pOldBufTable[slot].fd, pOldBufTable[slot].pageNum,
                           newSlot)))
      return (rc);

    // Put the slot back on the free list before returning the error
    Unlink(newSlot);
    InsertFree(newSlot);

    slot = next;
  }

  // Finally, delete the old buffer table
  delete[] pOldBufTable;

  return 0;
}

RC PF_BufferMgr::InsertFree(int slot) {
  bufTable[slot].next = free;
  free = slot;

  // Return ok
  return (0);
}
RC PF_BufferMgr::LinkHead(int slot) {
  // Set next and prev pointers of slot entry
  bufTable[slot].next = first;
  bufTable[slot].prev = INVALID_SLOT;

  // If list isn't empty, point old first back to slot
  if (first != INVALID_SLOT) bufTable[first].prev = slot;

  first = slot;

  // if list was empty, set last to slot
  if (last == INVALID_SLOT) last = first;

  // Return ok
  return (0);
}

RC PF_BufferMgr::Unlink(int slot) {
  // If slot is at head of list, set first to next element
  if (first == slot) first = bufTable[slot].next;

  // If slot is at end of list, set last to previous element
  if (last == slot) last = bufTable[slot].prev;

  // If slot not at end of list, point next back to previous
  if (bufTable[slot].next != INVALID_SLOT)
    bufTable[bufTable[slot].next].prev = bufTable[slot].prev;

  // If slot not at head of list, point prev forward to next
  if (bufTable[slot].prev != INVALID_SLOT)
    bufTable[bufTable[slot].prev].next = bufTable[slot].next;

  // Set next and prev pointers of slot entry
  bufTable[slot].prev = bufTable[slot].next = INVALID_SLOT;

  // Return ok
  return (0);
}

// lru的置换就在这里
//写脏页也是在这
RC PF_BufferMgr::InternalAlloc(int &slot) {
  RC rc;  // return code

  // If the free list is not empty, choose a slot from the free list
  if (free != INVALID_SLOT) {
    slot = free;
    free = bufTable[slot].next;
  } else {
    // Choose the least-recently used page that is unpinned
    for (slot = last; slot != INVALID_SLOT; slot = bufTable[slot].prev) {
      if (bufTable[slot].pinCount == 0) break;
    }

    // Return error if all buffers were pinned
    if (slot == INVALID_SLOT) return (PF_NOBUF);

    // Write out the page if it is dirty
    if (bufTable[slot].bDirty) {
      if ((rc = WritePage(bufTable[slot].fd, bufTable[slot].pageNum,
                          bufTable[slot].pData)))
        return (rc);

      bufTable[slot].bDirty = FALSE;
    }

    // Remove page from the hash table and slot from the used buffer list
    if ((rc = hashTable.Delete(bufTable[slot].fd, bufTable[slot].pageNum)) ||
        (rc = Unlink(slot)))
      return (rc);
  }

  // Link slot at the head of the used list
  if ((rc = LinkHead(slot))) return (rc);

  // Return ok
  return (0);
}

RC PF_BufferMgr::ReadPage(int fd, PageNum pageNum, char *dest) {
  // seek to the appropriate place (cast to long for PC's)
  long offset = pageNum * (long)pageSize + PF_FILE_HDR_SIZE;
  if (lseek(fd, offset, L_SET) < 0) return (PF_UNIX);

  // Read the data
  int numBytes = read(fd, dest, pageSize);
  if (numBytes < 0)
    return (PF_UNIX);
  else if (numBytes != pageSize)
    return (PF_INCOMPLETEREAD);
  else
    return (0);
}

RC PF_BufferMgr::WritePage(int fd, PageNum pageNum, char *source) {
  // seek to the appropriate place (cast to long for PC's)
  long offset = pageNum * (long)pageSize + PF_FILE_HDR_SIZE;
  if (lseek(fd, offset, L_SET) < 0) return (PF_UNIX);

  // Read the data
  int numBytes = write(fd, source, pageSize);
  if (numBytes < 0)
    return (PF_UNIX);
  else if (numBytes != pageSize)
    return (PF_INCOMPLETEWRITE);
  else
    return (0);
}

RC PF_BufferMgr::InitPageDesc(int fd, PageNum pageNum, int slot) {
  // set the slot to refer to a newly-pinned page
  bufTable[slot].fd = fd;
  bufTable[slot].pageNum = pageNum;
  bufTable[slot].bDirty = FALSE;
  bufTable[slot].pinCount = 1;

  // Return ok
  return (0);
}

#define MEMORY_FD -1
RC PF_BufferMgr::GetBlockSize(int &length) const {
  length = pageSize;
  return OK_RC;
}

//分配一块unpin就废的分页，不与特定文件关联，可以说是临时内存吧
//注意64bit机上用指针低32bit做pageNum还是有概率出错的，万一两个分页指针低32bit相同呢
//不过如果当初pData都是一块连续内存上的分段点,那应该是不会出错的,可以这样改进
RC PF_BufferMgr::AllocateBlock(char *&buffer) {
  RC rc = OK_RC;

  // Get an empty slot from the buffer pool
  int slot;
  if ((rc = InternalAlloc(slot)) != OK_RC) return rc;

  // Create artificial page number (just needs to be unique for hash table)
  PageNum pageNum = (PageNum) reinterpret_cast<long long>(
      bufTable[slot].pData);  // PageNum(bufTable[slot].pData);

  // Insert the page into the hash table, and initialize the page description
  // entry
  if ((rc = hashTable.Insert(MEMORY_FD, pageNum, slot) != OK_RC) ||
      (rc = InitPageDesc(MEMORY_FD, pageNum, slot)) != OK_RC) {
    // Put the slot back on the free list before returning the error
    Unlink(slot);
    InsertFree(slot);
    return rc;
  }

  // Return pointer to buffer
  buffer = bufTable[slot].pData;

  // Return success code
  return OK_RC;
}

///其实就是unpin
RC PF_BufferMgr::DisposeBlock(char *buffer) {
  return UnpinPage(MEMORY_FD, (PageNum) reinterpret_cast<long long>(buffer));
}