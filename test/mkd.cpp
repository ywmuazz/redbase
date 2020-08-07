#include<bits/stdc++.h>
using namespace std;


int main(int argc,char* argv[]){
    string tbName("tb3484");
    freopen(tbName.c_str(),"w",stdout);
    srand(time(0));
    // printf("create table %s (id i,name c10,age i,money f);\n",tbName.c_str());
    // printf("create index %s(id);\n",tbName.c_str());
    // int numRecs=stoi(argv[1]);
    // for(int i=0;i<numRecs;i++){
    //     printf("insert into %s values(%d,\"%s\",%d,%f);\n",
    //     tbName.c_str(),i,to_string(i).c_str(),i/10,i+0.5);
    // }

    // printf("select * from %s;",tbName.c_str());
    int numRecs=10000;
    for(int i=0;i<numRecs;i++){
        printf("select * from %s where id=%d;\n",tbName.c_str(),rand()%10000);
    }


    return 0;
}