/* 
class IX_Manager {
    static const char UNOCCUPIED = 'u';

public:
    IX_Manager(PF_Manager& pfm);
    ~IX_Manager();

    // Create a new Index
    RC CreateIndex(const char* fileName, int indexNo,
        AttrType attrType, int attrLength);

    // Destroy and Index
    RC DestroyIndex(const char* fileName, int indexNo);

    // Open an Index
    RC OpenIndex(const char* fileName, int indexNo,
        IX_IndexHandle& indexHandle);

    // Close an Index
    RC CloseIndex(IX_IndexHandle& indexHandle);

private:
    // Checks that the index parameters given (attrtype and length) make
    // a valid index
    bool IsValidIndex(AttrType attrType, int attrLength);

    // Creates the index file name from the filename and index number, and
    // returns it as a string in indexname
    RC GetIndexFileName(const char* fileName, int indexNo, std::string& indexname);
    // Sets up the IndexHandle internal varables when opening an index
    RC SetUpIH(IX_IndexHandle& ih, PF_FileHandle& fh, struct IX_IndexHeader* header);
    // Modifies th IndexHandle internal variables when closing an index
    RC CleanUpIH(IX_IndexHandle& indexHandle);

    PF_Manager& pfm; // The PF_Manager associated with this index.
};

class IX_IndexHandle
    friend class IX_Manager;
    friend class IX_IndexScan;
    static const int BEGINNING_OF_SLOTS = -2; // class constants
    static const int END_OF_SLOTS = -3;
    static const char UNOCCUPIED = 'u';
    static const char OCCUPIED_NEW = 'n';
    static const char OCCUPIED_DUP = 'r';

public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // Insert a new index entry
    RC InsertEntry(void* pData, const RID& rid);

    // Delete a new index entry
    RC DeleteEntry(void* pData, const RID& rid);

    // Force index files to disk
    RC ForcePages();
    RC PrintIndex();

class IX_IndexScan 
    static const char UNOCCUPIED = 'u'; // class constants to check whether
    static const char OCCUPIED_NEW = 'n'; // a slot in a node is valid
    static const char OCCUPIED_DUP = 'r';

public:
    IX_IndexScan();
    ~IX_IndexScan();

    // Open index scan
    RC OpenScan(const IX_IndexHandle& indexHandle,
        CompOp compOp,
        void* value,
        ClientHint pinHint = NO_HINT);

    // Get the next matching entry return IX_EOF if no more matching
    // entries.
    RC GetNextEntry(RID& rid);

    // Close index scan
    RC CloseScan();

*/

#include "ix.h"
#include <bits/stdc++.h>
#include <unistd.h>

#include <cassert>
#include <string>
#include <cstdio>


#include "pf.h"
#include "rm.h"

using namespace std;

PF_Manager pfm;
IX_Manager ixm(pfm);

string indexName("ixtest");
int indexNo = 0;

//测试manager的index创建打开关闭删除
RC testINT1();
//随机生成的插入删除序列
RC testINT2();
//测试索引扫描
RC testINT3();
int main()
{
    srand(time(0));
    assert(0==testINT1());
    assert(0==testINT2());
    assert(0==testINT3());
    return 0;
}

RC testINT1()
{
    RC rc = 0;
    IX_IndexHandle ixh;
    ixm.DestroyIndex(indexName.c_str(), indexNo);
    if (rc = ixm.CreateIndex(indexName.c_str(), indexNo, INT, 4))
        return rc;
    if (rc = ixm.OpenIndex(indexName.c_str(), indexNo, ixh))
        return rc;
    // if(rc=ixm.DestroyIndex(indexName.c_str(),indexNo))return rc;

    int maxTestKeys = 100;

    for (int i = 0; i < maxTestKeys; i++) {
        if (rc = ixh.InsertEntry((void*)&i, RID(i, i)))
            return rc;
    }
    puts("insert done.");
    if (rc = ixh.PrintIndex())
        return rc;
    for (int i = 0; i < maxTestKeys; i++) {
        if (rc = ixh.DeleteEntry((void*)&i, RID(i, i)))
            return rc;
    }
    puts("delete all done.");
    if (rc = ixh.PrintIndex())
        return rc;
    for (int i = 0; i < maxTestKeys; i++) {
        if (rc = ixh.InsertEntry((void*)&i, RID(i, i)))
            return rc;
    }
    puts("insert again.");
    if (rc = ixh.PrintIndex())
        return rc;

    for (int i = 0; i < maxTestKeys; i++) {
        if (i & 1)
            continue;
        if (rc = ixh.DeleteEntry((void*)&i, RID(i, i)))
            return rc;
    }
    puts("delete half done.");
    if (rc = ixh.PrintIndex())
        return rc;

    if (rc = ixm.CloseIndex(ixh))
        return rc;

    puts("test ixm done.");
    return rc;
}

int getkey(std::map<int, vector<RID>>& mp)
{
    if(mp.empty())assert(0);
    int mx=(--mp.end())->first;
    int x = rand() % (mx+1);
    return mp.lower_bound(x)->first;
    // int i = 0;
    // for (auto& p : mp) {
    //     if (i == step)
    //         return p.first;
    //     i++;
    // }
    // assert(0);
}
RID getrid(vector<RID>& v){
    if(v.empty())return RID(-1,-1);
    return v[rand()%v.size()];
}
bool check(std::map<int, vector<RID>>& a,std::map<int, vector<RID>>& b){
    // {
    //     vector<int> rm;
    //     for(auto& p:a){
    //         if(p.second.empty())rm.push_back(p.first);
    //     }
    //     for(auto i:rm){
    //         a.erase(i);
    //     }
    // }

    auto it1=a.begin();
    auto it2=b.begin();
    if(a.size()!=b.size()){printf("map size diff mp:%d b+%d\n",a.size(),b.size());return false;}
    for(;it1!=a.end()&&it2!=b.end();++it1,++it2){
        int k1=it1->first,k2=it2->first;
        vector<RID> &v1=it1->second,&v2=it2->second;
        if(k1!=k2||v1.size()!=v2.size()){
            printf("key diff or vec size diff. ka:%d kb:%d\n",k1,k2);
            return false;
        }
        // sort(v1.begin(),v1.end());
        reverse(v1.begin(),v1.end());
        // sort(v2.begin(),v2.end());
        for(int i=0;i<v1.size();i++){

            if(!(v1[i]==v2[i])){
                printf("rid diff. key:%d ra:%s rb:%s\n",k1,
                v1[i].toString().c_str(),v2[i].toString().c_str());
                return false;
            }
        }
    }
    return true;
}

RC testINT2()
{
    RC rc = 0;
    IX_IndexHandle ixh;
    ixm.DestroyIndex(indexName.c_str(), indexNo);
    if (rc = ixm.CreateIndex(indexName.c_str(), indexNo, INT, 4))
        return rc;
    if (rc = ixm.OpenIndex(indexName.c_str(), indexNo, ixh))
        return rc;

    const int ops = 500000;
    const int maxkeyvalue=ops;
    
    double rate = 0.7;

    std::map<int, vector<RID>> mp;
    std::map<int, vector<RID>> testmp;
    for (int i = 0; i < ops; i++) {
        int op = (rand() % 100 < 100 * rate ? 0 : 1);
        // printf("%s\t%d",(op == 0 ? "insert" : "remove"),i);
        if (op == 0) {
            int key = rand()%maxkeyvalue;
            RID rid(i, i);
            mp[key].push_back(rid);
            if(rc=ixh.InsertEntry(&key, rid)){
                PF_PrintError(rc);
                return rc;
            }
            // printf("%d:\tinsert\tkey:%d\trid:(%d,%d)\n",i,key,i,i);
        } else {
            if (mp.empty()) {
                puts("no key to del.");
                continue;
            }
            int key = getkey(mp);
            RID rid=getrid(mp[key]);
            if(rid==RID(-1,-1)){
                puts("no rid to del.");
                continue;
            }
            auto it=find(mp[key].begin(),mp[key].end(),rid);
            if(it==mp[key].end()){
                assert(0);
            }
            mp[key].erase(it);
            if(mp[key].empty()){
                mp.erase(key);
            }

            if(rc=ixh.DeleteEntry(&key,rid)){
                puts("del");
                PF_PrintError(rc);
                return rc;
            }
            PageNum ra;
            SlotNum rb;
            rid.GetPageNum(ra),rid.GetSlotNum(rb);
            // printf("%d:\terase\tkey:%d\trid:(%d,%d)\n",i,key,ra,rb);
        }
    }
    ixh.PrintIndex(testmp);

    if (rc = ixm.CloseIndex(ixh)){
        PF_PrintError(rc);
        return rc;
    }
        
    if(check(mp,testmp)){
        puts("ok. same");
    }else{
        puts("err. diff");
    }
    return rc;
}


RC testINT3(){
    RC rc = 0;
    IX_IndexHandle ixh;
    ixm.DestroyIndex(indexName.c_str(), indexNo);
    if (rc = ixm.CreateIndex(indexName.c_str(), indexNo, INT, 4))
        return rc;
    if (rc = ixm.OpenIndex(indexName.c_str(), indexNo, ixh))
        return rc;

    const int totRecs=100;

    for(int i=0;i<totRecs;i++){
        int key=i;
        RID rid(i,i);
        if(rc=ixh.InsertEntry(&key,rid)){
            return rc;
        }
        // if(i&1){
        //     if(rc=ixh.DeleteEntry(&key,rid))
        //         return rc;
        // }
    }
    for(int i=20;i<=80;i++){

        if(rc=ixh.DeleteEntry(&i,RID(i,i)))return rc;
    }


    IX_IndexScan is;
    int value=10;
    if(rc=is.OpenScan(ixh,GE_OP,&value)){
        return rc;
    }

    RID rid;
    int cnt=0;
    while((rc=is.GetNextEntry(rid))==0){
        int p;
        rid.GetPageNum(p);
        assert(p>=value);
        printf("%d\t%s\n",++cnt,rid.toString().c_str());
    }
    if(rc==IX_EOF){
        puts("scan done.");
    }else{
        puts("scan error.");
        IX_PrintError(rc);
        return rc;
    }

    if(rc=ixm.CloseIndex(ixh)){
        return rc;
    }

    puts("index closed.");

    return rc==IX_EOF?0:rc;

}


