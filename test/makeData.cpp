#include<bits/stdc++.h>
using namespace std;

int main(){
    int seed=time(0)%10000;
    string tbName="tb"+to_string(seed);
    freopen((tbName+".txt").c_str(),"w",stdout);
    cout<<"create table "+tbName+" (id i,name c10,age i,money f);"<<endl;
    cout<<"create index "+tbName+" (id);"<<endl;
    auto numRecs=10000;
    for(int i=0;i<numRecs;i++){
        int id=i,age=i/10;
        float f=i;
        f+=0.5;
        string name=to_string(i);
        printf("insert into %s values(%d,\"%s\",%d,%f);\n",
        tbName.c_str(),id,name.c_str(),age,f);
    }
    return 0;
}