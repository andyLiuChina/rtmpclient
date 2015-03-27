// this file is created by wangxiaohui may be at 2011

// changed by wxh 2014-7-17 mqueue and msg not supported by android


//
#ifndef _POSA_LINUX_H_
#define _POSA_LINUX_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <malloc.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include "sys/wait.h"
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <termios.h>
#include <signal.h>
#include "dirent.h"
#include "syslog.h"
#include <ctype.h>
#include <fcntl.h>
//#include <mqueue.h>
#include <sys/epoll.h>
#include <sys/resource.h>

typedef int s32;
typedef int BOOL32;
typedef unsigned long u32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef short s16;
typedef char s8;
typedef float fl32;

typedef long long  s64;
typedef unsigned long long u64;

#ifndef LPSTR
#define LPSTR   char *
#endif
#ifndef LPCSTR
#define LPCSTR  const char *
#endif

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

////////////////////////////////////////////////////////////////////////////
#define POSA_FD_READ         0x01
#define POSA_FD_WRITE        0x02
#define POSA_FD_ACCEPT       0x08
#define POSA_FD_CONNECT      0x10
#define POSA_FD_UDPREAD      0x80

#define SOCKHANDLE int
#ifndef SOCKADDR
#define SOCKADDR sockaddr
#endif
#define SOCKADDR_IN           sockaddr_in
#define SOCKET_ERROR          (-1)
#define INVALID_SOCKET        (-1)
#define POSA_NET_PENDING      (-3)

#define API extern "C"
#define  POSAMUTEXHANDLE  pthread_mutex_t
#define TASKHANDLE pthread_t

typedef void* (*PosaLINUXFUNC)(void*);


//print
API void kdmposaver();
API void kdmposadebugon();
API void kdmposadebugoff();
extern BOOL32 g_bKdmPosaDebug;

class CPosaNetRequest;
class CPosaNetProactor;

//init
API void KdmPosaStartup();
API void KdmPosaCleanup();

API BOOL32 KdmPosaCreateProactor(CPosaNetProactor** pProactor);
API BOOL32 KdmPosaProactorClose(CPosaNetProactor** pProactor);

API void PosaPrintf(BOOL32 bScreen, BOOL32 bFile, LPCSTR szFormat,...);

class CPosaCountMutex
{
private:
    CPosaCountMutex(const CPosaCountMutex &refMutex);
    CPosaCountMutex &operator=(const CPosaCountMutex &refMutex);
public:
    CPosaCountMutex();
    ~CPosaCountMutex();
    
    int Lock();
    int UnLock();

    // when u use Add & Release must new CPosaCountMutex,
    // pointer will be delete automatically when count is 0;
    long Add();
    long Release();
private:
    long m_lCount;
    u32   m_currentOwner;
    POSAMUTEXHANDLE m_hMutex;
};

class CPosaAutoLock
{    
private:
    CPosaAutoLock(const CPosaAutoLock &refAutoLock);
    CPosaAutoLock& operator=(const CPosaAutoLock &refAutoLock);
public:
    CPosaAutoLock(CPosaCountMutex* plock)
    {
        m_pMutex = plock;
        m_pMutex->Lock();
    };

    ~CPosaAutoLock() 
    {
        m_pMutex->UnLock();
    };
protected:
    CPosaCountMutex* m_pMutex;
}; 

class CPosaBuffer
{
public:
    CPosaBuffer();
    ~CPosaBuffer();
    void Initialize(u32 expected);	
    BOOL32 SetMaxBufferSize(u32 expected);
    BOOL32 EnsureSize(u32 expected);
    void Cleanup();
    BOOL32 MoveDataToHead();
    BOOL32 IgnoreAll();
    BOOL32 Ignore(u32 size);
    u8* GetIBPointer();
    u32 GetAvaliableBytesCount();    
    BOOL32 ReadFromBuffer(const u8 *pBuffer, const u32 size);

    void SetMinChunkSize(u32 minChunkSize);
    u32 GetMinChunkSize();    
    
    friend class CPosaNetRequest;
private:
    void Recycle();
private:
    u8* m_pBuffer;
	u32 m_size;
	u32 m_published;
	u32 m_consumed;
	u32 m_minChunkSize;    
};

class IPosaNetHandler
{
public:
    IPosaNetHandler();
    virtual ~IPosaNetHandler();    

    enum
    {
        POSA_NET_HANLDER_SERVER,
        POSA_NET_HANDLER_INSTANCE,
        POSA_NET_HANDLER_CLIENT,
        POSA_NET_HANDLER_UDP,
    };

    virtual int Open(CPosaNetProactor* pProactor, int nHanderType, char* szServerIpAddr, int dwRemotePort, int dwLocalPort, int nSocket);
    
    virtual int Close();

    // bNeedBuf TRUE this is used to keep the integrity of some tcp based protocol
    // multimedia stream data do not use this buffer (or may be can)
    int SendData(const char*const szBuffer, u32 dwLen, u32& dwSendBytes, BOOL32 bNeedBuf = FALSE);

    friend class CPosaNetRequest;
    friend class CPosaNetProactor;
protected:
    virtual int HandleAccept(int sclientfd);
    
    virtual int HandleConnect();

    virtual int HandleClose(int nOperateType);

    virtual int HandleRead(CPosaBuffer& cBuffer);// char* pBuffer, int dwBufferSize);
        
    virtual int HandleSend(int nOperateType);
        
protected:
    SOCKHANDLE m_hSocket;
    CPosaCountMutex* m_pMutex;
    CPosaNetRequest* m_pRequest;
    u32 m_dwHandlerType;
    BOOL32 m_bCanISendData;
    CPosaNetProactor* m_pNetProactor;
};


class CPosaTelnetInstance;
class CPosaTelnetServer : public IPosaNetHandler
{
public:
    CPosaTelnetServer();
    virtual ~CPosaTelnetServer();
    int Open(CPosaNetProactor* pProactor, int dwLocalPort);
    virtual int Close();
    int Print(char* strInfo);
protected:
    virtual int HandleAccept(int sclientfd);       
private:
    CPosaTelnetInstance* m_pInstance;
};

//////////////////////////////////////////////////////////////////////////
// API layer

API TASKHANDLE PosaCreateThread(PosaLINUXFUNC pvTaskEntry, u8 byPriority, u32 dwStacksize, u32 dwParam, u32* pdwTaskID);


/*class CPosaMailbox
{
public:
    CPosaMailbox();
    ~CPosaMailbox();
    BOOL32 CreateMailbox(LPSTR szName, u32 dwMsgNumber, u32 dwMsgLength);
    void CloseMailbox();
    BOOL32 RcvMsg(u32 dwTimeout, LPSTR szMsgBuf, u32 dwLen, u32* pdwLenRcved);
    BOOL32 SndMsg(LPCSTR szMsgBuf, u32 dwLen);
private:
    u32 m_dwReadID;
    u32 m_dwWriteID;
    s8 m_achMQPath[256];
};*/



#endif
