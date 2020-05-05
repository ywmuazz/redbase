#include "pf_internal.h"

#define INVALID_PAGE (-1)
// PF_PageHandle

/*
private:
   int  pageNum;                                  // page number
   char *pPageData;
*/

PF_PageHandle ::PF_PageHandle() {
  pageNum = INVALID_PAGE;
  pPageData = nullptr;
}

// Default constructor
PF_PageHandle::~PF_PageHandle() {}  // Destructor

// Copy constructor
PF_PageHandle::PF_PageHandle(const PF_PageHandle &pageHandle) {
  pageNum = pageHandle.pageNum;
  pPageData = pageHandle.pPageData;
}

// Overloaded =
PF_PageHandle &PF_PageHandle::operator=(const PF_PageHandle &pageHandle) {
  if (this != &pageHandle) {
    this->pageNum = pageHandle.pageNum;
    this->pPageData = pageHandle.pPageData;
  }
  return *this;
}

RC PF_PageHandle::GetData(char *&pData) const  // Set pData to point to
{
  if (pPageData == nullptr) return PF_PAGEUNPINNED;
  pData = pPageData;
  return 0;
}

// the page contents
RC PF_PageHandle::GetPageNum(PageNum &_pageNum) const  // Return the page number
{
  if (pPageData == nullptr) return PF_PAGEUNPINNED;
  _pageNum = this->pageNum;
  return 0;
}