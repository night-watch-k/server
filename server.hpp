#ifndef __M_SRV_H_
#define  __M_SRV_H_
#include"tcpsocket.hpp"
#include"epollwait.hpp"
#include"http.hpp"
#include"threadpool.hpp"
#include<fstream>
#include<boost/filesystem.hpp>
#define WWW_ROOT "./www"
    class Server
   {
       
        public:
        bool Start(int port)//服务器开始跑，其实这个start就是阻塞的，调用后一直监听，等待客户端
       {
         
          bool ret;
          ret=_lst_sock.SocketInit(port);//创建一个套接字，需要一个端口，绑定地址信息，开始监听
         if(ret==false)
         {
           return false;
         }
         
         ret=_pool.PoolInit();
         if(ret==false)
         {
           return false;
         }
         
         ret=_epoll.Init();//初始化一个epoll
         if(ret==false)
         {
           return false;
         }
        
         
         _epoll.Add(_lst_sock);//开始监控
         //要想成功，去掉Add的边缘出发
         
       
         while(1)//开始获取新连接                                                    
         {
           
              
          std::vector<TcpSocket> list;//定义一个列表
           ret=_epoll.Wait(list);//开始等待，获取到信息，获取已完成列表

            if(ret==false)//监控失败，超时或者描述符出错
           {
             continue;
           }
           for(int i=0;i<list.size();i++)
           {
             
             if(list[i].GetFd()==_lst_sock.GetFd())//监听套结字
             {
                TcpSocket cli_sock;
                ret=_lst_sock.Accept(cli_sock);//获取
                if(ret==false)
                {
                  continue;//重新获取
                }
                cli_sock.SetNonBlock();//非阻塞，设置不设置无所谓
                _epoll.Add(cli_sock);
  
             }
             else//普通的，有事件到来
             {
               ThreadTask tt(list[i].GetFd(),ThreadHandler);//组织一个任务？
	           
               _pool.TaskPush(tt);//入队操作  ？
               _epoll.Del(list[i]);//移除掉
             }
           }
            
  
         }
         _lst_sock.Close();//完毕
         return true;
       }
       public:
          static void ThreadHandler(int sockfd)//除掉this指针用static，因为对方只有一个参数即笔记里的那个函数指针  ？
          {
            TcpSocket sock;
            sock.SetFd(sockfd);
        
            
            HttpRequest req;//接受数据，保存请求的所有信息，解析结果放到req；解析请求，将所有的正文以及请求头信息放到req  
            HttpResponse rsp;//实例化一个响应对象，包含所有响应信息
            
            int status=req.RequestParse(sock);//需要接受信息，返回状态码  ？
            //从sock接受请求数据，进行解析
            
            if(status!=200)//
            {
              //解析失败则直接响应错误
              rsp._status=status;
              rsp.ErrorProcess(sock);
              sock.Close();
              return;
            }      
             std::cout<<"----\n";            
          
            
            HttpProcess(req,rsp);//通过req填充rsp
            ////根据请求，进行处理，将处理结果填充到rsp
            
            rsp.NormalProcess(sock);    ？
           
            //将处理结果（响应结果）反映给客户端
            sock.Close();
            //当前采用短连接，直接处理完毕后关闭套接字
            return;
          }
       
       
       static bool HttpProcess(HttpRequest&req,HttpResponse&rsp)
       {
         //若请求是一个POST请求--则多进程CGI进行处理
         //若请求是一个GET请求--但是查询字符串不为空
         //否则，若请求是GET，并且查询字符串为空
         //若请求的是一个目录
         //若请求是一个文件
         std::string realpath=WWW_ROOT+req._path;
         if(!boost::filesystem::exists(realpath))
         {
           rsp._status=404;
           std::cerr<<"realpath:["<<realpath<<"]\n";
           return false;
         }
         std::cout<<"realpath:["<<realpath<<"]\n";
         std::cout<<"method:["<<req._method<<"]\n";
         if((req._method=="GET"&&req._param.size()!=0)||req._method=="POST")
         {
           //对于当前来说，则是一个文件上传请求
           CGIProcess(req,rsp);
          
         }
         else
         {
           //则是一个基本的文件下载/目录列表请求
           if(boost::filesystem::is_directory(realpath))
           {
              std::cerr<<"into is_directory\n";
             //查看目录列表请求
             ListShow(realpath,rsp._body);
             rsp.SetHeader("Content-Type","text/html");
           }
           else
           {
            //普通的文件下载请求
            for(auto i:req._headers)
            {
             std::cout<<"["<<i.first<<"]---["<<i.second"]\n";
            }
            auto it=req._headers.find("Range");
            if(it==req._headers.end())
            {
             Download(realpath,rsp._body);
            rsp.SetHeader("ContentType","application/octet-stream");
            rsp.SetHeader("Accept-Ranges","bytes");
            rsp.SetHeader("ETag","abcdefg");
            }
            else
            {
             std::string range=it->second;
             RangeDownload(realpath,range,rsp._body);
             rsp.SetHeader("ContentType","application/octet-stream");
             std::string unit="bytes=";
             size_t pos=range.find(unit);
             if(pos==std::string::npos)
             {
              return false;
             }
             int64_t  len=boost::filesystem::file_size(realpath);
             std::stringstream tmp;
             tmp<<range.substr(pos+unit.size());
             tmp<<"/";
             tmp<<len;
             
          
             tsp.SetHeader("Content-Range",tmp.str());
             rsp._status=206;
             return true;
            }
            
           }
         }
         rsp._status=200;
         return true;
      }
      static bool RangeDownload(std::string &path,std::string &range,std::string &body)
      {
       //Range:bytes=start-end
       std::string unit="bytes=";
       size_t pos=range.find(unit);
       if(pos==std::string::npos)
       {
         return false;
       }
       pos+=unit.size();
       size_t pos2=range.find("-",pos);
       if(pos2==std::string::npos)
       {
         return false;
       }
       std::string start=range.substr(pos,pos2-pos);
       std::string end=range.substr(pos2+1);
       std::cout<<"range:["<<start<<"]---["<<end<<"]\n";
       std::stringstream tmp;
       int64_t dig_start,dig_end;
       tmp<<start;
       tmp>>dig_start;
       tmp.clear();
       if(end.size()==0)
       {
         dig_end=boost::filesystem::file_size(path)-1;
       }
       else
       {
        tmp<<end;
        tmp>>dig_end;
       }
       int64_t len=dig_end-dig_start+1;
       std::cout<<dig_start<<"*****"<<dig_end<<"\n";
       body.resize(len);
       std::ifstream file(path);
       if(!file.is_open())
       {
        return false;
       }
       file.seekg(std::ios::beg,dig_start);
       file.read(&body[0],len);
       if(!file.good())
       {
        std::cerr<<"read error\n";
        return false;
        
       }
       file.close();
       return true;
      }
      
       static bool CGIProcess(HttpRequest &req,HttpResponse &rsp)
       {
         int pipe_in[2],pipe_out[2];
         if(pipe(pipe_in)<0||pipe(pipe_out)<0)
         {
           std::cerr<<"create pipe error\n";
           return false;
         }
         int pid=fork();
         if(pid<0)
         {
           return false;
         }
         else if(pid==0)
         {
           close(pipe_in[0]);//用于父进程读，子进程写，关闭读端
           close(pipe_out[1]);//用于父进程写，子进程读，关闭写端
           dup2(pipe_in[1],1);
           dup2(pipe_out[0],0);
           
           setenv("METHOD",req._method.c_str(),1);
           setenv("PATH",req._path.c_str(),1);
           for(auto i:req._headers)
           {
            setenv(i.first.c_str(),i.second.c_str(),1);
            
           }
           std::string realpath =WWW_ROOT +req._path;
           execl(realpath.c_str(),realpath.c_str(),NULL);
           exit(0);
         }
         
         close(pipe_in[1]);
         close(pipe_out[0]);
         write(pipe_out[1],&req._body[0],req._body.size());
         while(1)
         {
           char buf[1024]={0};
           int ret=read(pipe_in[0],buf,1024);
           if(ret==0)
           {
            break;
           }
           buf[ret]='\0';
           rsp._body+=buf;
         }
         
         close(pipe_in[0]);
         close(pipe_out[1]);
         return true;
       }
       static bool Download(std::string &path,std::string &body)
       {
        int64_t fsize=boost::filesystem::file_size(path);
        body.resize(fsize);
        std::ifstream file(path);
        if(!file.is_open())
        {
         std::cerr<<"open file error\n";
         return false;
        }
        file.read(&body[0],fsize);
        if(!file.good())
        {
         std::cerr<<"read file data error\n";
         return false;
        }
        file.close();
        return true;
       }
       static bool ListShow(std::string &path,std::string &body)
       {
           //./www/testdir/a.txt ->/testdir/a.txt
           
           std::string www = WWW_ROOT;
           size_t pos=path.find(www);
           if(pos==std::string::npos)
           {
             return false;
           }
           std::string req_path =path.substr(www.size());
           
          std::stringstream tmp;
          tmp<<"<html><head><style>";
          tmp<<"*{margin:0;}";
          tmp<<".main-window{height:100%;width:80%;margin:0  auto;}";
          tmp<<".upload{position : relative;height : 20%;width :100%;background-color:#33c0b9;text-align:center;}";
          tmp<<".listshow{position:relative;height:80%;width:100%;background : #6fcad6;}";
          tmp<<"</style></head>";
          tmp<<"<body><div class='main-window'>";
          tmp<<"<div class ='upload'>";
          tmp<<"<form action='/upload'method='POST'>";
          tmp<<"<div class='upload-btn'>";
          tmp<<"<input type='file'name='fileupload'>";
          tmp<<"<input typr='submit'name='submit'>";
          tmp<<"</div></form>"
          tmp<<"</div><hr />";
          tmp<<"<div class ='listshow'><ol>"
          //......组织每个文件信息节点
          boost::filesystem::directory_iterator begin(path);
          boost::filesystem::directory_iterator end;
          for(;begin!=end;++begin)
          {
            int64_t mtime;
            std::string pathname=begin->path().string();
            std::string name = begin->path().filename().string();
            std::string uri=req_path+name;
            
            if(boost::filesystem::is_directory(pathname))
            {
            tmp<<"<li><strong><a href='";
            tmp<<uri<<"'>";
            tmp<< name<<"/";
            tmp<<"</a><br /></strong>";
            tmp<<"<small>filetype:directory ";
            tmp<<"</small></li>";
            }
            else
            
            {
             int64_t ssize;
             mtime=boost::filesystem::last_write_time(begin->path());
            ssize=boost::filesystem::file_size(begin->path());
          
             tmp<<"<li><strong><a href='";
             tmp<<uri<<"'>";
            tmp<< name;
            tmp<<"</a><br /></strong>";
            tmp<<"<small>modified:"; 
            tmp<<mtime;
            tmp<<"<br />filetype:application-octstream ";
            tmp<<ssize/1024<<"kbytes";
            tmp<<"</small></li>";
            
            
          }
        }
         
          
          //...组织每个文件信息节点
          tmp<<"</ol></div><hr /></div></body></html>";
          body=tmp.str();
          return true;
       }
       private:
         TcpSocket _lst_sock;//监听套接字
         ThreadPool _pool;//线程池
         Epoll _epoll;//多路转接模型epoll
  };  

