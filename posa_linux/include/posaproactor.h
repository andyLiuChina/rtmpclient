#ifndef _KDM_POSA_PROACTOR_H_
#define _KDM_POSA_PROACTOR_H_

#include "posa_linux.h"

#include <vector>
#include <map>
using namespace std;

class IPosaNetHandler;
class CPosaCountMutex;

#define POSA_DATA_BUFFER_LEN 11*1024
class CPosaNetRequest
{
public:
    CPosaNetRequest();
    ~CPosaNetRequest();
public:
    long HandleEpollWaitEvent(int nEvent);    

    int RegisterEventToEpoll(int sfd, int nEvent);

    int ModifyEventToEpoll(int nEvent);    
        
    int RemoveEventFromEpoll();

    int SendData(const char*const szBuffer, u32 dwLen, u32& dwSendBytes, BOOL32 bStartEvent);
public:
    CPosaBuffer m_bufReadIn;
    CPosaBuffer m_bufWriteOut;

    IPosaNetHandler* m_pPosaHandler;    

    long m_dwOpertateType;
    
    int m_hSocket;  

    CPosaCountMutex* m_pMutex;
    
    CPosaNetProactor* m_pProactor;
};

class CPosaRequestPool
{
public:
    CPosaRequestPool()
    {
        m_dwNumber = 0;
    }
    ~CPosaRequestPool()
    {
        this->Close();
    }
    void Open()
    {
        CPosaAutoLock cLock(&m_tMutex);
        if (m_vecUnUsed.size() || m_mapWaitedDelay.size())
        {
            ::PosaPrintf(TRUE, FALSE, "[kdmposa] CPosaRequestPool not closing\n");
        }
        else
        {
            m_dwNumber = 0;
        }        
    }

    void Close()
    {
        CPosaAutoLock cLock(&m_tMutex);

        // now all the socket is closed
        ReleaseNetRequestFromListDel();

        //delete all the unusedlist
        int y = m_vecUnUsed.size();
        assert(m_dwNumber == m_vecUnUsed.size());
        if (m_dwNumber != m_vecUnUsed.size())
        {
            PosaPrintf(TRUE, FALSE, "%d %d!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!nima\r\n",
                       m_dwNumber, m_vecUnUsed.size());
        }
        for(int x = 0; x < m_vecUnUsed.size(); x++)
        {
            delete m_vecUnUsed[x];
        }
        m_vecUnUsed.clear();
        m_dwNumber = 0;
    }
    CPosaNetRequest* CreateNetRequest()
    {
        CPosaNetRequest* pRequest = NULL;
        CPosaAutoLock cLock(&m_tMutex);
        if (0 == m_vecUnUsed.size())         
        {
            pRequest = new CPosaNetRequest;    
            m_dwNumber++;
            ::PosaPrintf(TRUE, FALSE, "[kdmposa] new request %d\r\n", m_dwNumber);            
        }
        else
        {
            pRequest = m_vecUnUsed[0];
            vector < CPosaNetRequest* >::iterator iterBegin = m_vecUnUsed.begin();
            m_vecUnUsed.erase(iterBegin);
        }                              
        return pRequest;
    }
    void ReleaseNetRequestFromListDel()
    {
        CPosaAutoLock cLock(&m_tMutex);
        map < CPosaNetRequest*, CPosaNetRequest* >::iterator iter;
        for (iter = m_mapWaitedDelay.begin(); iter != m_mapWaitedDelay.end(); iter++)
        {       
            CPosaNetRequest* pTemp = iter->first;            
            if (pTemp->m_pMutex)
            {
                pTemp->m_pMutex->Release();
                pTemp->m_pMutex = NULL;
            }
            m_vecUnUsed.push_back(pTemp);
        }
        m_mapWaitedDelay.clear();
    }
    void ReleaseNetRequestToDelList(CPosaNetRequest* pDeleteMe)
    {
        CPosaAutoLock cLock(&m_tMutex);
        m_mapWaitedDelay[pDeleteMe] = pDeleteMe;
    }
    
private:
    //posa�̹߳رգ�����ɲ�������δ�ͷţ�����������Ҫ����
    vector < CPosaNetRequest* > m_vecUnUsed;
    map < CPosaNetRequest*, CPosaNetRequest* > m_mapWaitedDelay;
    CPosaCountMutex m_tMutex;
    u32 m_dwNumber;
};

//extern CPosaRequestPool g_cRequestPool;


CPosaNetRequest* PosaCreateNetRequest();
void PosaClearDelList();
void PosaReleaseNetRequestDelayDel(CPosaNetRequest* pDeleteMe);




//////////////////////////////////////////////////////////////////////////
void* PosaScoketThread(void *pParam);

class CPosaNetProactor
{
public:
    CPosaNetProactor();
    ~CPosaNetProactor();
    
    friend void* PosaScoketThread(void *pParam);
public:
    //����epoll����
    long Open();
    
    //�ر�epoll��ѯ�߳�
    long Close();
    
    //��epoll���в���
    long OperateToEpoll(int nOperateType, int nFavorEvent, CPosaNetRequest* pNetRequest);

    //��Epoll������Ϣ
    long SendMsgToEpoll(u32 dwMessage);

    CPosaNetRequest* PosaCreateNetRequest();
    void PosaClearDelList();
    void PosaReleaseNetRequestDelayDel(CPosaNetRequest* pDeleteMe);
private:
    //���Դ������epoll�������epoll_wait�̳߳�
    SOCKHANDLE m_hEpoll;

    TASKHANDLE m_hThread;
    int m_nCloseThread;

    SOCKHANDLE m_hControlSocket;
    SOCKHANDLE m_hClientSocket;
    u16 m_wControlPort;
    
    CPosaCountMutex* m_pMutex;
    CPosaRequestPool m_cRequestPool;
};


extern CPosaNetProactor g_cPosaactor;

#endif

