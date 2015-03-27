#include "posaproactor.h"
#include "posa_linux.h"
#include <sys/ioctl.h>



//CPosaNetProactor g_cPosaactor;
//CPosaRequestPool g_cRequestPool;
#define POSA_MAX_EVENT  5000

//////////////////////////////////////////////////////////////////////////
CPosaNetProactor::CPosaNetProactor()
{
    m_hEpoll = -1;
    m_hThread = NULL;
    m_pMutex = new CPosaCountMutex;
    m_pMutex->Add();        
}
CPosaNetProactor::~CPosaNetProactor()
{
    this->Close();
    m_pMutex->Release();
}


long CPosaNetProactor::Open()
{
    CPosaAutoLock cLock(m_pMutex);
    do 
    {               
        if (-1 != m_hEpoll) 
        {
            ::PosaPrintf(TRUE, FALSE, "[kdmposa]m_hEpoll 0x%x is not -1 please close first\r\n", m_hEpoll);
            break;
        }
        
        m_nCloseThread = FALSE;

        m_hControlSocket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_hControlSocket < 0)
        {
            PosaPrintf(TRUE, FALSE, "[kdmposa]proactor create control socket error ! %d %s\n", errno, strerror(errno));
            break;
        }
        u32 on = TRUE;
        if (ioctl(m_hControlSocket, FIONBIO,  (unsigned long*)&on) < 0)
        {
            PosaPrintf(1,0,"[kdmposa]control socket option set error %d  %s\n", errno, strerror(errno));
            break;
        }
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family      = AF_INET;
        sin.sin_port        = 0;
        sin.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        if (::bind(m_hControlSocket, (sockaddr*)&sin , sizeof(sin)) < 0)
        {
            PosaPrintf(1,0,"[kdmposa]control socket bind error %d  %s\n", errno, strerror(errno));
            break;
        }
        
        struct sockaddr_in fsin;    
        s32 nLength = sizeof(fsin);
        if (::getsockname (m_hControlSocket, (sockaddr*)&fsin, (socklen_t*)&nLength) <0)
        {
            PosaPrintf(1,0,"[kdmposa]control socket getsockname error %d  %s\n", errno, strerror(errno));
            break;
        }
        
        m_wControlPort = ntohs(fsin.sin_port);
        ::PosaPrintf(TRUE, FALSE, "[kdmposa]Proactor open socket %d, port %d\n", m_hControlSocket, m_wControlPort);
        
        m_hClientSocket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_hClientSocket < 0)
        {
            PosaPrintf(1,0,"[kdmposa]create proactor client socket error %d %s\n", errno, strerror(errno));
            break;
        }
        if (ioctl(m_hClientSocket, FIONBIO,  (unsigned long*)&on) < 0)
        {
            PosaPrintf(1,0,"[kdmposa]proactor client socket option set error %d  %s\n", errno, strerror(errno));
            break;
        }
        
        m_hEpoll = ::epoll_create(POSA_MAX_EVENT);
        if (-1 == m_hEpoll)
        {
            ::PosaPrintf(TRUE, FALSE, "[kdmposa]proactor epoll_create error %d %s\n", errno, strerror(errno));
            break;
        }
        
        epoll_event tEpollEvent = { 0 };
        tEpollEvent.events = EPOLLIN;
        tEpollEvent.data.u64 = (u64)this;
        
        if (0 != epoll_ctl(m_hEpoll, EPOLL_CTL_ADD, m_hControlSocket, &tEpollEvent))
        {
            ::PosaPrintf(TRUE, FALSE, "[kdmposa]epoll_ctl add control socket socket %d error %d %s\n", 
                m_hControlSocket, errno, strerror(errno));
            break;
        }
        
        m_nCloseThread = FALSE;
        u32 dwThreadID = 0;
        m_hThread = ::PosaCreateThread(PosaScoketThread, 100, 4*1024*1024, 
                      (u32)this, &dwThreadID);
        if (0 == m_hThread)
        {
            ::PosaPrintf(TRUE, FALSE, "[kdmposa] OspTaskCreate PosaEpollThread error %d %s\n", errno, strerror(errno));
            break;
        }            
        
        m_cRequestPool.Open();
        return 0;

    } while(0);

    //error
    this->Close();
    return SOCKET_ERROR;
}

long CPosaNetProactor::Close()
{
    CPosaAutoLock cLock(m_pMutex);
            
    if (m_hThread)
    {
        m_nCloseThread = TRUE;
        this->SendMsgToEpoll(-1);

        ::PosaPrintf(TRUE, FALSE, "[kdmposa]start call pthread_join wait for the epoll thread exit\n");

        ::pthread_join(m_hThread, NULL);

        ::PosaPrintf(TRUE, FALSE, "[kdmposa]end call pthread_join wait for the epoll thread end\n");
        m_hThread = 0;
    }    

    if (-1 != m_hControlSocket)
    {
        ::close(m_hControlSocket);
        m_hControlSocket = -1;
    }

    if (-1 != m_hClientSocket) 
    {
        ::close(m_hClientSocket);
        m_hClientSocket = -1;
    }

    if (-1 != m_hEpoll) 
    {
        ::close(m_hEpoll);
        m_hEpoll = -1;
    }

    m_cRequestPool.Close();
    
    return 0;
}


long CPosaNetProactor::OperateToEpoll(int nOperateType, int nFavorEvent, CPosaNetRequest* pNetRequest)
{
    if (NULL == pNetRequest || -1 == pNetRequest->m_hSocket)
    {        
        return SOCKET_ERROR;
    }

    CPosaAutoLock cLock(m_pMutex);

    if (-1 == m_hEpoll)
    {
        ::PosaPrintf(TRUE, FALSE, "[kdmposa]what have u done? m_hEpoll is -1, please call init fun!!\n");
        return SOCKET_ERROR;
    }
    
    epoll_event tEpollEvent = { 0 };    
    tEpollEvent.events = nFavorEvent;
    tEpollEvent.data.u64 = (u64)pNetRequest;
    
    if (0 != epoll_ctl(m_hEpoll, nOperateType, pNetRequest->m_hSocket, &tEpollEvent))
    {
        ::PosaPrintf(TRUE, FALSE, "[kdmposa]epoll_ctl Type %d, event %d socket %d errno %d  %s", 
                                  nOperateType, nFavorEvent, pNetRequest->m_hSocket, 
                                  errno, strerror(errno));
        return SOCKET_ERROR;
    }
    
    return 0;       
}

long CPosaNetProactor::SendMsgToEpoll(u32 dwMessage)
{    
    CPosaAutoLock cLock(m_pMutex);        

    if (-1 == m_hClientSocket)
    {
        ::PosaPrintf(TRUE, FALSE, "[kdmposa] what happened?  SendMsgToEpoll  m_hClientSocket  is -1!!!\n");
        return SOCKET_ERROR;
    }
    else
    {                
        struct sockaddr_in fsin;    
        memset(&fsin, 0, sizeof(fsin));
        fsin.sin_family = AF_INET;
        fsin.sin_addr.s_addr = inet_addr("127.0.0.1");
        fsin.sin_port= htons(m_wControlPort);        
        int nSend = ::sendto(m_hClientSocket, (char*)&dwMessage, sizeof(dwMessage), 0, (sockaddr*)&fsin, sizeof(fsin));
        ::PosaPrintf(TRUE, FALSE, "[kdmposa]send close message to epoll %d  bytes %d\n", dwMessage, nSend);
    }    
    return 0;
}

CPosaNetRequest* CPosaNetProactor::PosaCreateNetRequest()
{
    CPosaNetRequest* pTemp = m_cRequestPool.CreateNetRequest();    
    if (pTemp)
    {
        pTemp->m_pProactor = this;
    }
    return pTemp;
}
void CPosaNetProactor::PosaClearDelList()
{
    m_cRequestPool.ReleaseNetRequestFromListDel();
}
void CPosaNetProactor::PosaReleaseNetRequestDelayDel(CPosaNetRequest* pDeleteMe)
{
    m_cRequestPool.ReleaseNetRequestToDelList(pDeleteMe);
}

//////////////////////////////////////////////////////////////////////////

void* PosaScoketThread(void *pParam)
{
    CPosaNetProactor* pProactor = (CPosaNetProactor*)pParam;
    struct epoll_event tEvents[POSA_MAX_EVENT] = { 0 };
    while (1)
    {
        int nfds = 0;
        
        pProactor->PosaClearDelList();
        nfds = ::epoll_wait(pProactor->m_hEpoll, tEvents, POSA_MAX_EVENT, -1);            
        
        for (int nInx = 0; nInx < nfds; nInx++)
        {
            if (TRUE == pProactor->m_nCloseThread) 
            {
                ::PosaPrintf(TRUE, FALSE, "[kdmposa] posa socket thread exit !!!nCloseThread %d\n", pProactor->m_nCloseThread);
                return 0;
            }
            else
            {
                CPosaNetRequest* pRequest = (CPosaNetRequest*)(tEvents[nInx].data.fd);
                if (pRequest)
                {
                    pRequest->HandleEpollWaitEvent(tEvents[nInx].events);
                }
                else
                {
                    ::PosaPrintf(TRUE, FALSE, "[kdmposa] error from epoll_wait the pRequest is NULL \n");
                }
            }
            
        }
    }
}


static void PrintfEpollEvent(int nEvent, int nOperateType);
//////////////////////////////////////////////////////////////////////////
CPosaNetRequest::CPosaNetRequest()
{    
    m_pPosaHandler = NULL;    
    m_dwOpertateType = POSA_FD_READ;
    m_hSocket = -1;    
    m_pMutex = NULL;
    m_bufReadIn.Initialize(4096);
    m_bufWriteOut.Initialize(4096);
}
CPosaNetRequest::~CPosaNetRequest()
{    
    m_pMutex->Release();
    m_pMutex = NULL;
}
int CPosaNetRequest::SendData(const char*const szBuffer, u32 dwLen, u32& dwSendBytes, BOOL32 bStartEvent)
{    
    dwSendBytes = 0;
    if (-1 != this->m_hSocket) 
    {
        BOOL32 bEventOut = FALSE;
        int nRet = ::send(m_hSocket, szBuffer, dwLen, 0);
        if (nRet != -1)
        {
            dwSendBytes = nRet;
            if (FALSE)
            {
            	::PosaPrintf(TRUE, FALSE, "[kdmposa] request SendData %d bytes, %d sended start event %d\r\n",
                         dwLen, nRet, bStartEvent);
            }
            if (nRet == dwLen)
            {
                return 0;
            }
            else
            {
                bEventOut = TRUE;
            }
        }        
        else
        {
            if (EAGAIN == errno || EWOULDBLOCK == errno)
            {
				if (FALSE == bStartEvent)
				{
					::PosaPrintf(TRUE, FALSE, "[kdmposa] u notify me and u play me!!!!!! %d\n", errno);
				}                
                bEventOut = TRUE;
            }
            else
            {            
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]SendData errno %d\n", errno);
                return -1;
            }
        }                
        if (bEventOut && bStartEvent)
        {
            m_bufWriteOut.ReadFromBuffer((u8*)(szBuffer+dwSendBytes), dwLen-dwSendBytes);
            this->ModifyEventToEpoll(EPOLLIN | EPOLLOUT);
            dwSendBytes = dwLen;
            return POSA_NET_PENDING;
        }
		return 0;
    }   
    return -1;
}
int CPosaNetRequest::RegisterEventToEpoll(int sfd, int nEvent)
{
    if (-1 == sfd) 
    {
        ::PosaPrintf(TRUE, FALSE, "[kdmposa]error RegisterEventToEpoll sfd is -1\n", sfd);
        return SOCKET_ERROR;
    }
    m_hSocket = sfd;
    this->m_bufReadIn.IgnoreAll();
    this->m_bufWriteOut.IgnoreAll();
    return m_pProactor->OperateToEpoll(EPOLL_CTL_ADD, nEvent, this);
}

//�޸��¼���epoll����
int CPosaNetRequest::ModifyEventToEpoll(int nEvent)
{    
    if (-1 == m_hSocket) 
    {
        ::PosaPrintf(TRUE, FALSE, "[kdmposa]error ModifyEventToEpoll m_hsocket is  -1\n");
        return SOCKET_ERROR;
    }
    return m_pProactor->OperateToEpoll(EPOLL_CTL_MOD, nEvent, this);
}

int CPosaNetRequest::RemoveEventFromEpoll()
{
    if (-1 == m_hSocket) 
    {
        ::PosaPrintf(TRUE, FALSE, "[kdmposa]error RemoveEventFromEpoll m_hsocket is  -1\n");
        return SOCKET_ERROR;
    }
    return m_pProactor->OperateToEpoll(EPOLL_CTL_DEL, 0, this);
}




long CPosaNetRequest::HandleEpollWaitEvent(int nEvent)
{
    PrintfEpollEvent(nEvent, this->m_dwOpertateType);
    
    CPosaAutoLock cAuto(m_pMutex);    
    if (-1 == m_hSocket || NULL == m_pPosaHandler)
    {
        return 0;
    }
    
     
    if (EPOLLERR == (nEvent & EPOLLERR))
    {
        ::PosaPrintf(TRUE, FALSE, "[kdmposa]HandleEpollWaitEvent socket %d EPOLLERR  call HandleClose errno %d %s\n",
                    m_hSocket, errno, strerror(errno));
        m_pPosaHandler->HandleClose(this->m_dwOpertateType);
        return 0;
    }
    if (EPOLLHUP == (nEvent & EPOLLHUP))
    {
        ::PosaPrintf(TRUE, FALSE, "[kdmposa]HandleEpollWaitEvent socket %d EPOLLHUP\n",
                     m_hSocket);
        return 0;
    }

    if (EPOLLIN == (nEvent & EPOLLIN))
    {
        //�յ�EPOLLIN ����recv���socket�Ƿ�Ͽ�
        if (POSA_FD_ACCEPT == this->m_dwOpertateType) 
        {
            int nNewFd = -1;
            struct sockaddr_in tClientAddr = { 0 };
            socklen_t nLength = sizeof(tClientAddr);
            
            do
            {   
                nNewFd = ::accept(this->m_hSocket, (struct sockaddr *)&tClientAddr, &nLength);
                if (nNewFd <= 0)
                {
                    ::PosaPrintf(TRUE, FALSE, "[kdmposa]!!!HandleEpollWaitEvent accept error %d %s\n", errno, strerror(errno));
                    break;
                }
                else
                {
                    ::PosaPrintf(TRUE, FALSE, "HandleEpollWaitEvent call HandleAccept socket %d\n", nNewFd);                    
                    m_pPosaHandler->HandleAccept(nNewFd);                    
                }
            }while(nNewFd>0);
            return 0;
        }    
        else if (POSA_FD_READ == this->m_dwOpertateType)
        {
            int nLen = 0;            
            int bRecv = TRUE;                
            
			m_bufReadIn.MoveDataToHead();
            char* pBuffer = (char*)(m_bufReadIn.m_pBuffer+m_bufReadIn.m_published);
            u32 dwBufLen = m_bufReadIn.m_size - m_bufReadIn.m_published;
			if (0 == dwBufLen) 
			{
				::PosaPrintf(TRUE, FALSE, "[kdmposa]error error recv buffer is 0 !socket %d error!!!!\n", m_hSocket);
				m_pPosaHandler->HandleClose(m_dwOpertateType);
				return 0;
			}
			else
			{				
				nLen = ::recv(m_hSocket, pBuffer, dwBufLen, 0);
				//�Զ˹ر���
				if (nLen == 0)
				{
					::PosaPrintf(TRUE, FALSE, "[kdmposa]socket %d recv return 0, endpoint close correct\n", m_hSocket);
					bRecv = FALSE;
				}        
				else if (nLen < 0)
				{
					if (EAGAIN == errno || EINTR == errno)
					{
						::PosaPrintf(TRUE, FALSE, "[kdmposa]recv error %d %s,fd : %d, lenth :%d\n",
							errno, strerror(errno), m_hSocket, dwBufLen);
						
						struct sockaddr_in tAddrClient = { 0 };
						int nAddrClientLen = sizeof(tAddrClient);
						getpeername(m_hSocket, (struct sockaddr *)&tAddrClient, (socklen_t*)&nAddrClientLen);
						
						PosaPrintf(TRUE, FALSE, "port : %d \n", tAddrClient.sin_port);
						return 0;
					}
					else
					{
						bRecv = FALSE;
					}
				}
				else
				{
					bRecv = TRUE;                
				}
				
				if (bRecv)
				{
					if (FALSE)
					{
						::PosaPrintf(TRUE, FALSE, "[kdmposa]HandleEpollWaitEvent call HandleRead socket %d bufsize %d  handler %d\n",
												m_hSocket, nLen, m_pPosaHandler);
					}
					m_bufReadIn.m_published += nLen;
					m_pPosaHandler->HandleRead(m_bufReadIn);
				}
				else
				{
					::PosaPrintf(TRUE, FALSE, "[kdmposa]HandleEpollWaitEvent socket %d recv error %d %s call HandleClose\n",
						m_hSocket, errno, strerror(errno));                
					m_pPosaHandler->HandleClose(m_dwOpertateType);
					return 0;
				}
			}
        }
        else if (POSA_FD_UDPREAD == m_dwOpertateType)
        {
            //recv from
            if (m_pPosaHandler)
            {
                //call 
            }
            else
            {
                //��������
                return 0;
            }
            
        }
        else
        {
            //error operate type
            ::PosaPrintf(TRUE, FALSE, "[kdmposa]HandleEpollWaitEvent socket %d errortype %d\n",
                m_hSocket, m_dwOpertateType);
        }
    }
    
    if (EPOLLOUT == (nEvent & EPOLLOUT))
    {
        if (POSA_FD_CONNECT == this->m_dwOpertateType)
        {
            int err = 0;            
            socklen_t len = sizeof(err);
            if (::getsockopt(m_hSocket, SOL_SOCKET, SO_ERROR, (char*)&err, &len) < 0 || err != 0) 
            {
                //error
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]HandleEpollWaitEvent call HandleClose connect error %d %s socket %d\n",
                    errno, strerror(errno), m_hSocket);
                m_pPosaHandler->HandleClose(m_dwOpertateType);                    
                return 0;
            }
            else
            {
                this->m_dwOpertateType = POSA_FD_READ;
                this->ModifyEventToEpoll(EPOLLIN);
                
                ::PosaPrintf(TRUE, FALSE, "[kdmposa]HandleEpollWaitEvent call HandleConnect accept ok socket %d\n",m_hSocket);                     
                m_pPosaHandler->HandleConnect();
            }                
        }
        else if (POSA_FD_READ == this->m_dwOpertateType)
        {
            u32 dwWaitedLen = m_bufWriteOut.GetAvaliableBytesCount();
            u32 dwSended = 0;
            printf("hhsend ignor %d  waited %d\r\n", dwSended, dwWaitedLen);
            if (dwWaitedLen) 
            {
                int nRet = this->SendData((char*)m_bufWriteOut.GetIBPointer(), dwWaitedLen, dwSended, FALSE);
                if (-1 == nRet) 
                {
                    m_pPosaHandler->HandleClose(m_dwOpertateType);
                    return 0;
                }
                printf("hhsend ignor %d  waited %d\r\n", dwSended, dwWaitedLen);
                m_bufWriteOut.Ignore(dwSended);
            }

            // more sockets, the write out is more effect
            if (0 == m_bufWriteOut.GetAvaliableBytesCount())
            {
                this->ModifyEventToEpoll(EPOLLIN);
                m_pPosaHandler->m_bCanISendData = TRUE;
                m_pPosaHandler->HandleSend(POSA_FD_WRITE);
            }            
        }
    }
    return 0;    
}

#include <string>
void PrintfEpollEvent(int nEvent, int nOperateType)
{
    std::string str;
    if (EPOLLIN == (nEvent & EPOLLIN))
    {        
        str.append("EPOLLIN ");
    }
    if (EPOLLOUT == (nEvent & EPOLLOUT))
    {
        str.append("EPOLLOUT ");
    }
    if (EPOLLET == (nEvent & EPOLLET))
    {
        str.append("EPOLLET ");
    }
    if (EPOLLPRI == (nEvent & EPOLLPRI))
    {
        str.append("EPOLLPRI ");
    }
    if (EPOLLHUP == (nEvent & EPOLLHUP))
    {
        str.append("EPOLLHUP ");
    }
    if (EPOLLERR == (nEvent & EPOLLERR))
    {
        str.append("EPOLLERR ");
    }
      
    std::string strOperate;
    if (POSA_FD_CONNECT == nOperateType) 
    {
        strOperate.append("FD_CONNECT ");
    }
    else if (POSA_FD_ACCEPT == nOperateType)
    {
        strOperate.append("FD_ACCEPT ");
    }
    else if (POSA_FD_READ == nOperateType)
    {
        strOperate.append("FD_READ ");
    }
    else if (POSA_FD_WRITE == nOperateType)
    {
        strOperate.append("FD_WRITE ");
    }
    else if (POSA_FD_UDPREAD == nOperateType)
    {
        strOperate.append("FD_UDPREAD ");
    }
    else 
    {
        strOperate.append("Err Type ");
    }

    if (FALSE)
    {
    	::PosaPrintf(TRUE, FALSE, "[kdmposa]HandleEpollWaitEvent : EpollEvent :%s OperateType %s\n",
    	                              str.c_str(), strOperate.c_str());
    }
    
}


