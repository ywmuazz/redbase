#include <bits/stdc++.h>
#include <unistd.h>

#include "pf.h"
#include "redbase.h"

using namespace std;

/*
class PF_Manager {
public:
   PF_Manager    ();                              // Constructor
   ~PF_Manager   ();                              // Destructor
   RC CreateFile    (const char *fileName);       // Create a new file
   RC DestroyFile   (const char *fileName);       // Delete a file
   RC OpenFile      (const char *fileName, PF_FileHandle &fileHandle);
   RC CloseFile     (PF_FileHandle &fileHandle);
   RC ClearBuffer   ();
   RC PrintBuffer   ();
   RC ResizeBuffer  (int iNewSize);

   RC GetBlockSize  (int &length) const;

   RC AllocateBlock (char *&buffer);

   RC DisposeBlock  (char *buffer);

private:
   PF_BufferMgr *pBufferMgr;                      // page-buffer manager
};

class PF_FileHandle {
   friend class PF_Manager;
public:
   PF_FileHandle  ();                            // Default constructor
   ~PF_FileHandle ();                            // Destructor

   PF_FileHandle  (const PF_FileHandle &fileHandle);

   PF_FileHandle& operator=(const PF_FileHandle &fileHandle);

   RC GetFirstPage(PF_PageHandle &pageHandle) const;
   RC GetNextPage (PageNum current, PF_PageHandle &pageHandle) const;
   RC GetThisPage (PageNum pageNum, PF_PageHandle &pageHandle) const;
   RC GetLastPage(PF_PageHandle &pageHandle) const;
   RC GetPrevPage (PageNum current, PF_PageHandle &pageHandle) const;
   RC AllocatePage(PF_PageHandle &pageHandle);    // Allocate a new page
   RC DisposePage (PageNum pageNum);              // Dispose of a page
   RC MarkDirty   (PageNum pageNum) const;        // Mark page as dirty
   RC UnpinPage   (PageNum pageNum) const;        // Unpin the page
   RC FlushPages  () const;
   RC ForcePages  (PageNum pageNum=ALL_PAGES) const;

private:
   int IsValidPageNum (PageNum pageNum) const;
   PF_BufferMgr *pBufferMgr;                      // pointer to buffer manager
   PF_FileHdr hdr;                                // file header
   int bFileOpen;                                 // file open flag
   int bHdrChanged;                               // dirty flag for file hdr
   int unixfd;                                    // OS file descriptor
};


class PF_PageHandle {
   friend class PF_FileHandle;
public:
   PF_PageHandle  ();                            // Default constructor
   ~PF_PageHandle ();                            // Destructor
   PF_PageHandle  (const PF_PageHandle &pageHandle);
   PF_PageHandle& operator=(const PF_PageHandle &pageHandle);
   RC GetData     (char *&pData) const;           // Set pData to point to
   RC GetPageNum  (PageNum &pageNum) const;       // Return the page number
private:
   int  pageNum;                                  // page number
   char *pPageData;                               // pointer to page data
};

struct PF_FileHdr {
   int firstFree;     // first free page in the linked list
   int numPages;      // # of pages in the file
};
*/

PF_Manager pfm;

void testPFM();
void testPFFH();
int main() {
    // testPFM();
    testPFFH();

    return 0;
}

///说实话，其实你get到指针之后并写入，并没有任何东西感知分页被写变脏，还是要自己手动
//调用markdirty，之前能够写入成功是因为最后flush了
void testPFFH() {
    const char* filename = "testfile3";
    unlink(filename);
    PF_FileHandle fh;
    pfm.CreateFile(filename);
    pfm.OpenFile(filename, fh);
    vector<PF_PageHandle> phs;
    for (int i = 0; i < 10; i++) {
        PF_PageHandle ph;
        if (fh.AllocatePage(ph)) {
            ERREXIT("分配新分页错误");
        }
        phs.push_back(ph);
        char* p;
        if (ph.GetData(p)) {
            ERREXIT("getdata error.");
        }
        sprintf(p, "\nthis is %d page.\n");
        int pagen;
        ph.GetPageNum(pagen);
        fh.UnpinPage(pagen);
    }
    RC rc = 0;
    if (rc = pfm.CloseFile(fh)) {
        PF_PrintError(rc);
        ERREXIT("关闭错误.");
    }
    cout << "关闭完成" << endl;

    pfm.OpenFile(filename, fh);
    PF_PageHandle ph;
    //  fh.GetLastPage(ph);
    //  int pagesnum = 0;
    //  ph.GetPageNum(pagesnum);
    //  fh.UnpinPage(pagesnum);
    //  pagesnum++;

    PF_FileHdr fhdr;

    fh.GetFileHdr(fhdr);
    int pagesnum = fhdr.numPages;
    printf("%s hdr\nnumpages:%d\n", filename, pagesnum);

    // PF_EOF
    for (int i = 0; i < pagesnum; i++) {
        fh.GetThisPage(i, ph);
        char* p = nullptr;
        ph.GetData(p);
        printf("page:%d %s\n", i, p);
        fh.UnpinPage(i);
    }

    for (int i = 0; i < pagesnum; i++) {
        fh.DisposePage(i);
    }
    for (int i = 0; i < pagesnum; i++) {
        fh.AllocatePage(ph);
        PageNum num = -1;
        ph.GetPageNum(num);
        cout << "i:" << i << " pagenum:" << num << endl;
        char* p = nullptr;
        ph.GetData(p);
        sprintf(p, "\nhaha %d\n", num);
    }
    pfm.CloseFile(fh);
}

void testPFM() {
    PF_FileHandle fh;
    pfm.CreateFile("testfile1");
    pfm.OpenFile("testfile1", fh);

    int len;
    pfm.GetBlockSize(len);
    cout << "block len: " << len << endl;
    char* p = nullptr;
    pfm.AllocateBlock(p);
    pfm.DisposeBlock(p);
    pfm.CloseFile(fh);
    // pfm.CloseFile(fh);
}