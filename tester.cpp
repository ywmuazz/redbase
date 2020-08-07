#include<bits/stdc++.h>
#include "redbase.h"
#include "rm.h"
#include "sm.h"
#include "ql.h"
#include <unistd.h>
#include <fcntl.h>

using namespace std;


struct CalTime {
    decltype(time(0)) st,en;
    CalTime():st(time(0)),en(0) {}
    void printTime(const char *s=nullptr) {
        en=time(0);
        if(s)
            cout<<s;
        cout<<"span: "<<(en-st)<<"s"<<endl;
    }
    ~CalTime() {
        en=time(0);
        cout<<"span: "<<(en-st)<<"s"<<endl;
    }
};

struct Init{
    Init(){
        freopen("in.txt","r",stdin);
        // fclose(stdout);
        // close(fileno(stdout));
        freopen("out.txt","w",stdout);
    }
};
Init ini;


PF_Manager pfm;
RM_Manager rmm(pfm);
IX_Manager ixm(pfm);
SM_Manager smm(ixm, rmm);
QL_Manager qlm(smm, ixm, rmm);



int main(int argc, char *argv[])
{
    CalTime cc;
    // auto st=clock();
    char *dbname;
    RC rc;

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " dbname \n";
        exit(1);
    }
 
    dbname = argv[1];
    if ((rc = smm.OpenDb(dbname))) {
        PrintError(rc);
        return (1);
    }

    RBparse(pfm, smm, qlm);

    if ((rc = smm.CloseDb())) {
        PrintError(rc);
        return (1);
    }
    
    cout << "Bye.\n";
    return 0;

}
