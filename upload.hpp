#include<iostream>
#include<sstream>
#include<vector>
#include<fstream>
#include<boost/filesystem.hpp>
#include<boost/algorithm/string.hpp>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#define WWW_ROOT "./www/"
class Boundary
{
  public:
    int64_t _start_addr;
    int64_t _data_len;
    std::string _name;
    std::string _filename;
    
};
bool GetHeader(const std::string &key,std::string &val)
{
 std::string body;
 char *ptr=getenv(key.c_str());
 if(ptr==NULL)
 {
   return false;
 }
 val=ptr;
 return true;
}
