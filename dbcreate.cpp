#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include "rm.h"
#include "sm.h"
#include "redbase.h"
#include <climits>


using namespace std;

int main(int argc ,char* argv[]){
    const int maxCommLen=255;
    string comm("mkdir ");
    RC rc;
    if(argc!=2){
        cerr<<"Usage: "<<argv[0]<<" dbname"<<"\n";
        exit(1);
    }
    string dbname(argv[1]);
    if(dbname.size()+comm.size()>maxCommLen){
        cerr << argv[1] << " length exceeds maximum allowed, cannot create database\n";
        exit(1);
    }
    comm+=dbname;
    int err=system(comm.c_str());
    if(err){
        cerr << "system call to create directory exited with error code " << err << endl;
        exit(1);
    }
    if(chdir(dbname.c_str())<0){
        cerr << argv[0] << " chdir error to " << dbname << "\n";
        exit(1);
    }

    PF_Manager pfm;
    RM_Manager rmm(pfm);
    int relCatRecSize = sizeof(RelCatEntry);
    int attrCatRecSize = sizeof(AttrCatEntry);

    if((rc = rmm.CreateFile("relcat", relCatRecSize))){
        cerr << "Trouble creating relcat. Exiting" <<endl;
        exit(1);
    }

    if((rc = rmm.CreateFile("attrcat", attrCatRecSize))){
        cerr << "Trouble creating attrcat. Exiting" <<endl;
        exit(1);
    }
    return 0;
}