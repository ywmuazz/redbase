/*
class RM_Record {
    static const int INVALID_RECORD_SIZE = -1;
    friend class RM_FileHandle;
public:
    RM_Record ();
    ~RM_Record();
    RM_Record& operator= (const RM_Record &record);

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;

    // Sets the record with an RID, data contents, and its size
    RC SetRecord (RID rec_rid, char *recData, int size);
private:
    RID rid;        // record RID
    char * data;    // pointer to record data. This is stored in the
                    // record object, where its size is malloced
    int size;       // size of the malloc
};
*/

#include <unistd.h>
#include <sys/types.h>
#include "pf.h"
#include "rm_internal.h"


RM_Record::RM_Record(){
    data=nullptr;
    size=INVALID_RECORD_SIZE;
}

RM_Record::~RM_Record(){
    if(data){
        delete [] data;
    }
}

RM_Record& RM_Record::operator= (const RM_Record &record){
    if(this!=&record){
        if(this->data){
            delete [] data;
        }
        this->size=record.size;
        //这样做的好处是,当stringlen小于size时,可以new出末尾\0
        this->data=new char[size]; 
        memcpy(this->data,record.data,this->size);
        this->rid=record.rid;
    }
    return *this;
}

RC RM_Record::GetData(char *&pData) const {
  if(data == NULL || size == INVALID_RECORD_SIZE)
    return (RM_INVALIDRECORD);
  pData = data;
  return (0);
}

RC RM_Record::GetRid (RID &rid) const {
  RC rc;
  if((rc = (this->rid).isValidRID()))
    return rc;
  rid = this->rid;
  return (0);
}

RC RM_Record::SetRecord(RID rec_rid, char *recData, int rec_size){
    RC rc;
    if((rc=rec_rid.isValidRID())){
        return RM_INVALIDRID;
    }
    rid=rec_rid;
    if(rec_size<=0){
        return RM_BADRECORDSIZE;
    }
    size=rec_size;
    if(recData==nullptr){
        return RM_INVALIDRECORD;
    }
    if(data){
        delete [] data;
    }
    data=new char[size];
    memcpy(data,recData,size);
    return 0;
}






