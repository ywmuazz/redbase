
#ifndef RM_INTERNAL_H
#define RM_INTERNAL_H

#include <cstring>
#include "pf.h"
#include "rm.h"

#define NO_MORE_FREE_PAGES -1 // for the linked list of free pages
                              // to indicate no more free page

// Define the RM page header
struct RM_PageHeader {
  PageNum nextFreePage;
  int numRecords;
};



#endif