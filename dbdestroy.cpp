#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "rm.h"
#include "sm.h"
#include "redbase.h"
#include <climits>

using namespace std;

int main(int argc,char *argv[]){
    const int maxCommLen=255;
    string comm("rm -r ");
    RC rc;
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " dbname \n";
        exit(1);
    }
    string dbname(argv[1]);
    if(comm.size()+dbname.size()>maxCommLen){
        cerr << argv[1] << " length exceeds maximum allowed, cannot delete database\n";
        exit(1);
    }
    comm+=dbname;
    int error = system (comm.c_str());

    if(error){
        cerr << "system call to destroy directory exited with error code " << error << endl;
        exit(1);
    }

}