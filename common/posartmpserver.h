/*
    this is created by wangxiaohui 2014-3-13
*/
#ifndef _KDM_RTMP_SERVER_H_
#define _KDM_RTMP_SERVER_H_

#include "posa_linux.h"
#include <string>
using namespace std;

#ifdef WIN32

#ifdef KDMRTMPSERVER_EXPORTS
#define POSA_RTMP_SERVER_API __declspec(dllexport)
#define RTMP_SERVER_API extern "C" __declspec(dllexport)
#else
#define POSA_RTMP_SERVER_API __declspec(dllimport)
#define RTMP_SERVER_API extern "C" __declspec(dllimport)
#pragma comment(lib, "kdmrtmpserver.lib")
#endif

#pragma comment(lib, "Winmm.lib")

// if u cannot find this please install openssl 0.9.8
// add path C:\OpenSSL-Win32\lib\VC to 
// your visual studio tools-->options-->directory-->library files
#pragma comment(lib, "libeay32MD.lib")

#else 

#define POSA_RTMP_SERVER_API
#define RTMP_SERVER_API extern "C"

#endif

class CPosaRtmpServerInstance;
typedef void (*RtmpNewInstance)(CPosaRtmpServerInstance* pInstance);

class POSA_RTMP_SERVER_API CPosaRtmpServer : public IPosaNetHandler
{
public:
    CPosaRtmpServer();
    virtual ~CPosaRtmpServer();
    int Open(CPosaNetProactor* pProactor, int dwLocalPort);
    virtual int Close();    
    RtmpNewInstance m_pFun;
protected:
    virtual int HandleAccept(int sclientfd);
    
};

class CPosaRtmpProtocol;
class POSA_RTMP_SERVER_API CPosaRtmpServerInstance : public IPosaNetHandler
{
public:
    CPosaRtmpServerInstance();
    virtual ~CPosaRtmpServerInstance();    
    int SendH264Frame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp);
    void SetCallback(RtmpNewInstance);
protected:
    virtual int HandleRead(CPosaBuffer& cBuffer);
    virtual int HandleSend(int nOperateType);
    virtual int HandleClose(int nOperateType);
private:
    CPosaRtmpProtocol* m_pRtmpProtocol;        
};

#endif
