/*
class RID {
  static const PageNum INVALID_PAGE = -1;
  static const SlotNum INVALID_SLOT = -1;
public:
    RID();                                         // Default constructor
    RID(PageNum pageNum, SlotNum slotNum);
    ~RID();                                        // Destructor
    RID& operator= (const RID &rid);               // Copies RID
    bool operator== (const RID &rid) const;

    RC GetPageNum(PageNum &pageNum) const;         // Return page number
    RC GetSlotNum(SlotNum &slotNum) const;         // Return slot number

    RC isValidRID() const; // checks if it is a valid RID

private:
  PageNum page;
  SlotNum slot;
};
*/

#include "rm_rid.h"

#include "rm_internal.h"

RID::RID() : page(INVALID_PAGE), slot(INVALID_SLOT) {}

RID::RID(PageNum pageNum, SlotNum slotNum) : page(pageNum), slot(slotNum) {}

RID::~RID() {}

RID& RID::operator=(const RID& rid) {
    if (this != &rid) {
        page = rid.page;
        slot = rid.slot;
    }
}

bool RID::operator==(const RID& rid) const {
    return page == rid.page && slot == rid.slot;
}

RC RID::GetPageNum(PageNum& pageNum) const {
    pageNum = page;
    return 0;
}
RC RID::GetSlotNum(SlotNum& slotNum) const {
    slotNum = slot;
    return 0;
}

RC RID::isValidRID() const {
    if (page > 0 && slot >= 0)
        return 0;
    else
        return RM_INVALIDRID;
}