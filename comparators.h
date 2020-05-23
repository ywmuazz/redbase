#include<cstdlib>
#include<cstring>
#include<cstdio>

static int compare_string(void *value1, void* value2, int attrLength){
    return strncmp((char*)value1,(char*)value2,attrLength);
}

static int compare_int(void *value1, void* value2, int attrLength){
    if((*(int *)value1 < *(int *)value2))
    return -1;
  else if((*(int *)value1 > *(int *)value2))
    return 1;
  else
    return 0;
}

static int compare_float(void *value1, void* value2, int attrLength){
  if((*(float *)value1 < *(float *)value2))
    return -1;
  else if((*(float *)value1 > *(float *)value2))
    return 1;
  else
    return 0;
}

static bool print_string(void *value, int attrLength){
    char* s=static_cast<char*>(value);
    for(int i=0;i<attrLength;i++)putchar(s[i]);
    return true;
}

static bool print_int(void *value, int attrLength){
    printf("%d ",*static_cast<int*>(value));
    return true;
}

static bool print_float(void *value, int attrLength){
  printf("%f ",*static_cast<float*>(value) );
  return true;
}
