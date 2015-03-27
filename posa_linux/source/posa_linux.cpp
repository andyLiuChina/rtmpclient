#include "posaproactor.h"
#include "posa_linux.h"

#ifdef _NDK_POSA_PRINT_LINUX_
#include <android/log.h>
#define LOG_TAG "rtmp_wrap"
#endif

API void PosaPrintf(BOOL32 bScreen, BOOL32 bFile, LPCSTR szFormat,...)
{
    s8 strMsg[6000];
    u32 dwActLen = 0;
    va_list pvList;
    va_start(pvList, szFormat);
    dwActLen = vsnprintf(strMsg, 6000, szFormat, pvList);
    va_end(pvList);

    if (dwActLen <= 0)
    {
        printf("Osp: vsnprintf() failed in OspPrintf().\n");        
        return;
    }
    if (dwActLen >= 6000)
    {
        printf("Osp: msg's length is over MAX_LOG_MSG_LEN in OspPrintf().\n");        
        return;
    }
#ifdef _NDK_POSA_PRINT_LINUX_
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", strMsg);
#else
    printf("%s", strMsg);
    printf("\r\n");
#endif
}

int g_nRefCount = 0;
void KdmPosaStartup()
{
    if (0 == g_nRefCount) 
    {
        //g_cRequestPool.Open();
        //g_cPosaactor.Open();
    }
    g_nRefCount++;    
}
void KdmPosaCleanup()
{
    s32 nExit = g_nRefCount>0 ? g_nRefCount-- : 0;//�ж��Ƿ����һЩȫ�ֳ�Ա��
	
	if(nExit==1)
    {
        //g_cPosaactor.Close();
        //g_cRequestPool.Close();
    }    
    ::PosaPrintf(TRUE, FALSE, "[kdmposa] KdmPosaCleanup count %d\n", g_nRefCount);
}

API BOOL32 KdmPosaCreateProactor(CPosaNetProactor** pProactor)
{
    if (NULL == pProactor) 
    {
        return FALSE;
    }
    *pProactor = NULL;
    CPosaNetProactor* pNet = new CPosaNetProactor;
    if (pNet)
    {
        pNet->Open();
        *pProactor = pNet;
        return TRUE;
    }
    return FALSE;
}
API BOOL32 KdmPosaProactorClose(CPosaNetProactor** pProactor)
{
    if (NULL == pProactor)
    {
        return FALSE;
    }
    if (*pProactor)
    {
        (*pProactor)->Close();
        delete (*pProactor);
        *pProactor = NULL;
    }
    return TRUE;
}
//////////////////////////////////////////////////////////////////////////
CPosaCountMutex::CPosaCountMutex()
{
    pthread_mutexattr_t ma;
    ::pthread_mutexattr_init(&ma);
    ::pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE_NP);
    ::pthread_mutex_init(&m_hMutex, &ma);
    ::pthread_mutexattr_destroy(&ma);
    m_lCount = 0;
}

CPosaCountMutex::~CPosaCountMutex()
{
    ::pthread_mutex_destroy(&m_hMutex);
    ::PosaPrintf(TRUE, FALSE, "[kdmposa]destroy posacountmutex\n");
}

int CPosaCountMutex::Lock()
{
    return pthread_mutex_lock(&m_hMutex);
}

int CPosaCountMutex::UnLock()
{
    return pthread_mutex_unlock(&m_hMutex);
}

long CPosaCountMutex::Add()
{
    int nCount = 0;
    this->Lock();
    m_lCount++;
    nCount = m_lCount;
    this->UnLock();
    ::PosaPrintf(TRUE, FALSE, "[kdmposa] mutex 0x%x after add count is %d\n", this, nCount);
    return nCount;
}
long CPosaCountMutex::Release()
{
    int nCount = 0;
    this->Lock();
    m_lCount--;
    nCount = m_lCount;
    this->UnLock();
    ::PosaPrintf(TRUE, FALSE, "[kdmposa] mutex 0x%x after delete count is %d\n", this, nCount);
    if (0 == nCount) 
    {
        delete this;
    }
    return nCount;
}
//////////////////////////////////////////////////////////////////////////
IPosaNetHandler::IPosaNetHandler()
{
    this->m_hSocket = -1;
    this->m_pRequest = NULL;
    this->m_pMutex = new CPosaCountMutex;
    m_pMutex->Add();
    m_bCanISendData = TRUE;
    m_pNetProactor = NULL;
}

IPosaNetHandler::~IPosaNetHandler()
{
    ::PosaPrintf(TRUE, FALSE, "[kdmposa]PosaNetHandler destructor\n");
    this->Close();

    m_pMutex->Release();
    m_pMutex = NULL;
}

int IPosaNetHandler::Open(CPosaNetProactor* pProactor, int nHanderType, char* szServerIpAddr, int dwRemotePort, int dwLocalPort, int nSocket)
{
    CPosaAutoLock cAuto(m_pMutex);
    
    if (-1 != m_hSocket || NULL != m_pRequest)
    {
        ::PosaPrintf(TRUE, FALSE, "[kdmposa] please call IPosaNetHandler close()  m_hSocket %d, m_pRequest 0x%x\n",
            m_hSocket, m_pRequest);
        return SOCKET_ERROR;
    }
    m_pNetProactor = pProactor;
    do 
    {                        
        long dwEvent = EPOLLIN;
        long dwOperateType;
        
        if (POSA_NET_HANLDER_SERVER== nHanderType) 
        {
            dwEvent |= EPOLLET;
            dwOperateType = POSA_FD_ACCEPT;
            m_hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
            if (m_hSocket < 0) 
            {
                m_hSocket = -1;
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]unable to create stream socket: %d %s\n", errno, strerror(errno));
                return SOCKET_ERROR;
            }                
        }
        else if (POSA_NET_HANDLER_INSTANCE == nHanderType) 
        {
            if (nSocket < 0)
            {
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]HandlerInstance nSocket error %d\n", nSocket);
                return SOCKET_ERROR;
            }
            m_hSocket = nSocket;
            dwOperateType = POSA_FD_READ;
        }
        else if (POSA_NET_HANDLER_CLIENT == nHanderType)
        {
            m_hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
            if (m_hSocket < 0)
            {
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]unable to create client stream socket: %d %s\n", errno, strerror(errno));
                m_hSocket = -1;
                return SOCKET_ERROR;
            }
            
            dwEvent |= EPOLLOUT;
            dwOperateType = POSA_FD_CONNECT;
        }
        else if (POSA_NET_HANDLER_UDP == nHanderType)
        {
            //wait for adding
            dwOperateType = POSA_FD_UDPREAD;    
        }
        else
        {
            ::PosaPrintf(TRUE, FALSE, "[kdmposa]PosaHandler HandlerType error %d\n", nHanderType);
            return SOCKET_ERROR;
        }    
        
        {
            int curFlags = ::fcntl(m_hSocket, F_GETFL, 0);
            int nRet = ::fcntl(m_hSocket, F_SETFL, curFlags|O_NONBLOCK);
            if (nRet < 0)
            {
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]fcntl set O_NONBLOCK error %d %s\n", errno, strerror(errno));
                break;
            }
        }
        
        {                
            u32 keepAlive = 1;//�趨KeepAlive   
            u32 keepIdle = 10;//��ʼ�״�KeepAlive̽��ǰ��TCP�ձ�ʱ��   
            u32 keepInterval = 5;//����KeepAlive̽����ʱ����   
            u32 keepCount = 1;//�ж��Ͽ�ǰ��KeepAlive̽�����   
            if(setsockopt(m_hSocket,SOL_SOCKET,SO_KEEPALIVE,(void*)&keepAlive,sizeof(keepAlive)) == -1)   
            {
            }
            if(setsockopt(m_hSocket,SOL_TCP,TCP_KEEPIDLE,(void *)&keepIdle,sizeof(keepIdle)) == -1)   
            {   
                
            }   
            if(setsockopt(m_hSocket,SOL_TCP,TCP_KEEPINTVL,(void *)&keepInterval,sizeof(keepInterval)) == -1)   
            {   
                
            }   
            if(setsockopt(m_hSocket,SOL_TCP,TCP_KEEPCNT,(void *)&keepCount,sizeof(keepCount)) == -1)   
            {   
                
            }
        }

        {
            int err = -1;   
            int snd_size = 0;
            int rcv_size = 0;
            socklen_t optlen;
            optlen = sizeof(snd_size);
            err = getsockopt(m_hSocket, SOL_SOCKET, SO_SNDBUF,&snd_size, &optlen); 
            ::PosaPrintf(TRUE, FALSE, "so_sndbuffer %d bytes\r\n", snd_size);
        }
        //����sendbuf    
        {
            int err = -1;   
            int snd_size = 0;
            int rcv_size = 0;
            socklen_t optlen;
            optlen = sizeof(snd_size);
            err = getsockopt(m_hSocket, SOL_SOCKET, SO_RCVBUF,&snd_size, &optlen); 
            ::PosaPrintf(TRUE, FALSE, "so_recvbuffer %d bytes\r\n", snd_size);
        }
        //recvbuf
        /*{
            u32 nBufSize=0x400000;
            if (0 != ::setsockopt(m_hSocket, SOL_SOCKET, SO_RCVBUF,(s8 *)&nBufSize, sizeof(u32))) 
            {
                ::PosaPrintf(TRUE, FALSE, "setsockopt error recvbuffer %d\r\n", errno);
            }
        }
        //sendbuf
        {
            u32 nBufSize=0x400000;
            if (0 != ::setsockopt(m_hSocket, SOL_SOCKET, SO_SNDBUF,(s8 *)&nBufSize, sizeof(u32)))
            {
                ::PosaPrintf(TRUE, FALSE, "setsockopt error sndbuffer %d\r\n", errno);
            }
        }*/
        

        if (POSA_NET_HANLDER_SERVER == nHanderType)
        {
            int reuseFlag = 1;
            if ( ::setsockopt(m_hSocket, SOL_SOCKET, SO_REUSEADDR,
                (const char*)&reuseFlag, sizeof(reuseFlag) ) < 0) 
            {
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]setsockopt(SO_REUSEADDR) error: %d %s\n", errno, strerror(errno));
                break;
            }
        }
        if (POSA_NET_HANLDER_SERVER == nHanderType)
        {
            struct sockaddr_in addrLocal;
            memset(&addrLocal, 0, sizeof(addrLocal));
            addrLocal.sin_family = AF_INET;
            addrLocal.sin_addr.s_addr = INADDR_ANY;
            addrLocal.sin_port = htons(dwLocalPort);
            
            if (::bind(m_hSocket, (struct sockaddr*)&addrLocal, sizeof(addrLocal)) != 0) 
            {            
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]bind socket error %d %s, port: %d\n", 
                    errno, strerror(errno), dwLocalPort);
                break;
            }
        }
        if (POSA_NET_HANLDER_SERVER == nHanderType) 
        {
            int nRet = ::listen(m_hSocket, 20);
            if (nRet < 0)
            {
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]listen error %d %s\n", errno, strerror(errno));
                break;
            }
            ::PosaPrintf(TRUE, FALSE, "[kdmposa]IPosaHandler Listen Server Port %d ok socket %d\n", dwLocalPort, m_hSocket);
        }
        
        this->m_pRequest = m_pNetProactor->PosaCreateNetRequest();
        m_pMutex->Add();
        m_pRequest->m_pMutex = this->m_pMutex;
        m_pRequest->m_pPosaHandler = this;    
        m_pRequest->m_dwOpertateType = dwOperateType;    
        m_pRequest->m_hSocket = this->m_hSocket;
        if (SOCKET_ERROR == m_pRequest->RegisterEventToEpoll(m_hSocket, dwEvent))
        {
            break;
        }
        
        if (POSA_NET_HANDLER_CLIENT == nHanderType)
        {
            struct sockaddr_in addrLocal;
            memset(&addrLocal, 0, sizeof(addrLocal));
            addrLocal.sin_family = AF_INET;
            addrLocal.sin_addr.s_addr = inet_addr(szServerIpAddr);
            addrLocal.sin_port = htons(dwRemotePort);
            int nRet = ::connect(m_hSocket, (sockaddr*)&addrLocal, sizeof(addrLocal));
            if (nRet > 0)
            {
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]socket %dconnect to server %s:%d just ok\n", 
                    m_hSocket, szServerIpAddr, dwRemotePort);
            }
            else if (errno == EINPROGRESS || errno == EALREADY)
            {
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]socket %d connect to server %s:%d return EINPROGRESS or EALREADY\n",
                    m_hSocket, szServerIpAddr, dwRemotePort);
            } 
            else
            {
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]connect to server %s:%d error %d %s\n", 
                    szServerIpAddr, dwRemotePort, errno, strerror(errno));
                break;
            }            
        }
        
        return 0;
        
    } while(0);
    
    this->Close();
    return SOCKET_ERROR;
}

int IPosaNetHandler::Close()
{
    CPosaAutoLock cAuto(m_pMutex);
    
    if (-1 != m_hSocket)
    {        
        ::PosaPrintf(TRUE, FALSE, "IPosaHandler::Close close socket %d\n", m_hSocket);
        ::close(m_hSocket);
        m_hSocket = -1;
    }    
    
    if (NULL != m_pRequest) 
    {
        m_pRequest->m_hSocket = -1;
        m_pRequest->m_pPosaHandler = NULL;
        
        m_pNetProactor->PosaReleaseNetRequestDelayDel(m_pRequest);        
        m_pRequest = NULL;        
    }           
    return 0;
}

int IPosaNetHandler::HandleClose(int nOperateType)
{
	int err = 0;
	socklen_t len = sizeof(err);
	if (0 == ::getsockopt(m_hSocket, SOL_SOCKET, SO_ERROR, (char*) &err, &len))
	{
		::PosaPrintf(TRUE, FALSE, "[kdmposa]socket error is %d %s", err, strerror(err));
	}
    this->Close();
    return 0;
}

int IPosaNetHandler::HandleSend(int nOperateType)
{
    m_bCanISendData = TRUE;
    
    return 0;
}


int IPosaNetHandler::HandleRead(CPosaBuffer& cBuffer)
{
    return 0;
}
int IPosaNetHandler::HandleConnect()
{
    return 0;
}
int IPosaNetHandler::HandleAccept(int sclientfd)
{
    return 0;
}

int IPosaNetHandler::SendData(const char*const szBuffer, u32 dwLen, u32& dwSendBytes, BOOL32 bNeedBuf)
{
    dwSendBytes = 0;
    if (0 == dwLen)
    {
        return 0;
    }
    CPosaAutoLock cLock(this->m_pMutex);
    if (-1 != this->m_hSocket && m_pRequest) 
    {
        if (FALSE == m_bCanISendData)
        {
        	if (FALSE == bNeedBuf)
        	{
        		::PosaPrintf(TRUE, FALSE, "[kdmposa] pending u can not send any data \r\n");
        		            return POSA_NET_PENDING;
        	}
        	else
        	{
        		m_pRequest->m_bufWriteOut.ReadFromBuffer((u8*)(szBuffer), dwLen);
        		dwSendBytes = dwLen;
        		return 0;
        	}
        }
        int nRet = m_pRequest->SendData(szBuffer, dwLen, dwSendBytes, TRUE);
        if (POSA_NET_PENDING == nRet) 
        {
            m_bCanISendData = FALSE;
            return POSA_NET_PENDING;
        }
        else if (-1 == nRet) 
        {
            //::PosaPrintf(TRUE, FALSE, "[kdmposa]senddata error socket %d\n", m_hSocket);
            //this->HandleClose(POSA_FD_WRITE);
            return -1;
        }
        else
        {
            return 0;
        }
    }    
    return -1;   
}
