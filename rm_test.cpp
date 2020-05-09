#include "rm.h"

#include <bits/stdc++.h>
#include <unistd.h>

#include <string>

#include "pf.h"

using namespace std;

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
RM_FileHandle
    RC GetRec     (const RID &rid, RM_Record &rec) const;

    RC InsertRec  (const char *pData, RID &rid);       // Insert a new record

    RC DeleteRec  (const RID &rid);                    // Delete a record
    RC UpdateRec  (const RM_Record &rec);              // Update a record


RC RM_FileScan::OpenScan(const RM_FileHandle& fileHandle,
                         AttrType attrType,
                         int attrLength,
                         int attrOffset,
                         CompOp compOp,
                         void* value,
                         ClientHint pinHint) ;

*/

const char* filename1 = "rm_testfile";
PF_Manager pfm;
RM_Manager rmm(pfm);
int namecnt = 0;

template <class T>
void test(AttrType type) {
    puts("\n\n\n");
    RC rc=0;
    int recordSize=sizeof(T);
    namecnt++;
    string name=string(filename1)+to_string(namecnt);
    unlink(name.c_str());
    if(rc=rmm.CreateFile(name.c_str(),recordSize)){
        puts("create err.");
    }
    RM_FileHandle fh;
    if(rc=rmm.OpenFile(name.c_str(),fh)){
        RM_PrintError(rc);

        puts("open err.");
    }

    vector<RID> rids;
    const int num=50;
    for(int i=0;i<num;i++){
        T data=i;
        RID rid;
        if(fh.InsertRec((const char*)&data,rid)){
            puts("insert error.");
        }
        rids.push_back(rid);
    }

    for(int i=0;i<num;i++){
        // if((i+1)%100!=0)continue;
        RM_Record rec;
        if(fh.GetRec(rids[i],rec)){
            puts("getrec error.");
        }

        int page,slot;
        rids[i].GetPageNum(page),rids[i].GetSlotNum(slot);
        char* data;
        rec.GetData(data);

        printf("rec i:%d rid:%d %d data:",i,page,slot);
        cout<<(*(T*)data)<<endl;

        T& v=*(T*)data;
        v*=2;
        fh.UpdateRec(rec);
        if(((int)v)%4==0)if(rc=fh.DeleteRec(rids[i])){
            puts("del err.");
        }
    }
    
    T cmpval=num;

    puts("RM filescan start.");
    RM_FileScan fs;
    fs.OpenScan(fh,type,sizeof(T),0,LE_OP,&cmpval);
    
    for(int i=0;;i++){
        RM_Record rec;
        if(rc=fs.GetNextRec(rec)){
            if(rc==RM_EOF)break;
            else puts("scan err.");
        }
        
        RID rid;
        rec.GetRid(rid);
        int page,slot;
        rid.GetPageNum(page),rid.GetSlotNum(slot);
        char* data;
        rec.GetData(data);

        printf("rec i:%d rid:%d %d data:",i,page,slot);
        cout<<(*(T*)data)<<endl; 
    }
    fs.CloseScan();
    
    puts("\nInsert again.\n");

    vector<RID> rids2;
    for(int i=1;i<=num;i++){
        T val=-i;
        RID rid;
        if(rc=fh.InsertRec((const char*)&val,rid))puts("insert err.");
        rids2.push_back(rid);
    }

    fs.OpenScan(fh,type,sizeof(T),0,NO_OP,nullptr);
    for(int i=0;;i++){
        RM_Record rec;
        if(rc=fs.GetNextRec(rec)){
            if(rc==RM_EOF)break;
            else puts("scan err.");
        }
        
        RID rid;
        rec.GetRid(rid);
        int page,slot;
        rid.GetPageNum(page),rid.GetSlotNum(slot);
        char* data;
        rec.GetData(data);

        printf("rec i:%d rid:%d %d data:",i,page,slot);
        cout<<(*(T*)data)<<endl; 
    }
    fs.CloseScan();



    if(rmm.CloseFile(fh))puts("close err.");
    // if(rmm.DestroyFile(name.c_str()))puts("destroy err.");
}

void testString(int recordSize=10);

int main() {
    test<int>(INT);
    // test<float>(FLOAT);
    // testString(10);

    return 0;
}


void testString(int recordSize){
    puts("\n\n\n");
    RC rc=0;
    
    namecnt++;
    string name=string(filename1)+to_string(namecnt);
    unlink(name.c_str());
    if(rc=rmm.CreateFile(name.c_str(),recordSize)){
        puts("create err.");
    }
    RM_FileHandle fh;
    if(rc=rmm.OpenFile(name.c_str(),fh)){
        RM_PrintError(rc);

        puts("open err.");
    }

    vector<RID> rids;
    const int num=50;
    for(int i=0;i<num;i++){
        string data;
        data="string"+to_string(i);
        RID rid;
        if(fh.InsertRec(data.c_str(),rid)){
            puts("insert error.");
        }
        rids.push_back(rid);
    }

    for(int i=0;i<num;i++){
        // if((i+1)%100!=0)continue;
        RM_Record rec;
        if(fh.GetRec(rids[i],rec)){
            puts("getrec error.");
        }

        int page,slot;
        rids[i].GetPageNum(page),rids[i].GetSlotNum(slot);
        char* data;
        rec.GetData(data);

        printf("rec i:%d rid:%d %d data:%s\n",i,page,slot,data);
        string ts(data);
        int v=atoi(ts.substr(strlen("string")).c_str());
        v*=2;
        ts="string"+to_string(v);
        strncpy(data,ts.c_str(),10);
        fh.UpdateRec(rec);
        if(((int)v)%4==0)if(rc=fh.DeleteRec(rids[i])){
            puts("del err.");
        }
    }
    
    string cmpval="string"+to_string(num);

    puts("RM filescan start.");
    RM_FileScan fs;
    fs.OpenScan(fh,STRING,recordSize,0,LE_OP,(void*)cmpval.c_str());
    
    for(int i=0;;i++){
        RM_Record rec;
        if(rc=fs.GetNextRec(rec)){
            if(rc==RM_EOF)break;
            else puts("scan err.");
        }
        
        RID rid;
        rec.GetRid(rid);
        int page,slot;
        rid.GetPageNum(page),rid.GetSlotNum(slot);
        char* data;
        rec.GetData(data);

        printf("rec i:%d rid:%d %d data:%s\n",i,page,slot,data);

    }
    fs.CloseScan();
    
    if(rmm.CloseFile(fh))puts("close err.");
    // if(rmm.DestroyFile(name.c_str()))puts("destroy err.");

}