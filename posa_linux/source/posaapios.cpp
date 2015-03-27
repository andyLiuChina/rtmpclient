#include "posa_linux.h"

//////////////////////////////////////////////////////////////////////////
// API layer
API TASKHANDLE PosaCreateThread(PosaLINUXFUNC pvTaskEntry, u8 byPriority, u32 dwStacksize, u32 dwParam, u32* pdwTaskID)
{
    TASKHANDLE  hTask = NULL;
    u32 dwTaskID = 0;    
    s32 nRet = 0;
    sched_param tSchParam;
    pthread_attr_t tThreadAttr;
    s32 nSchPolicy, nGetSchPolicy;

    pthread_attr_init(&tThreadAttr);

    byPriority = 0;
    nSchPolicy = SCHED_OTHER;

    pthread_attr_getschedpolicy(&tThreadAttr, &nGetSchPolicy);
    pthread_attr_setschedpolicy(&tThreadAttr, nSchPolicy);

    pthread_attr_getschedparam(&tThreadAttr, &tSchParam);
    tSchParam.sched_priority = byPriority;
    pthread_attr_setschedparam(&tThreadAttr, &tSchParam);

    pthread_attr_setstacksize(&tThreadAttr, dwStacksize);
    pthread_attr_setdetachstate(&tThreadAttr, PTHREAD_CREATE_JOINABLE);

    nRet = pthread_create(&hTask, &tThreadAttr, pvTaskEntry, (void*)dwParam);
    if (0 == nRet)
    {
        if (NULL != pdwTaskID)
        {
            *pdwTaskID = (u32)hTask;
        }
        return hTask;
    }
    return 0;
}

/*CPosaMailbox::CPosaMailbox()
{
    m_dwReadID = -1;
    m_dwWriteID = -1;
    m_achMQPath[0] = 0;
}
CPosaMailbox::~CPosaMailbox()
{
    if (-1 != m_dwReadID)
    {
        this->CloseMailbox();
    }    
}
u32 g_dwMailBoxIndex = 1;
BOOL32 CPosaMailbox::CreateMailbox(LPSTR szName, u32 dwMsgNumber, u32 dwMsgLength)
{
    if (-1 != m_dwReadID) 
    {
        this->CloseMailbox();
    }
    sprintf(m_achMQPath, "/%s.ospmq.%d", "posa",  g_dwMailBoxIndex);
    mq_unlink(m_achMQPath);
    struct mq_attr tAttr;
    tAttr.mq_maxmsg = dwMsgNumber;
    tAttr.mq_msgsize = dwMsgLength;
    mqd_t tMQHandle = mq_open(m_achMQPath, O_CREAT|O_EXCL|O_RDWR, S_IRUSR | S_IWUSR | S_IXUSR |
                              S_IRGRP | S_IWGRP | S_IXGRP, &tAttr);
    if (-1 == tMQHandle)
    {        
        printf("create msgqueue (%d)(%s) failed! errno(%d)(%s)\n", g_dwMailBoxIndex, m_achMQPath, errno, strerror(errno));
        return FALSE;
    }
    g_dwMailBoxIndex++;
    m_dwReadID = (u32)tMQHandle;
    m_dwWriteID = (u32)tMQHandle;    
    return TRUE;
}

void CPosaMailbox::CloseMailbox()
{
    mq_close((mqd_t)m_dwReadID);
    mq_unlink(m_achMQPath);
    m_dwReadID = -1;
    m_dwWriteID = -1;
    m_achMQPath[0] = 0;
}

BOOL32 CPosaMailbox::SndMsg(LPCSTR szMsgBuf, u32 dwLen)
{
    s32 nRet = mq_send((mqd_t)m_dwWriteID, szMsgBuf, dwLen, 0);
    if (-1 == nRet)
    {
        printf("msgqueue(%d) send failed! errno(%d)(%s)\n", m_dwWriteID, errno, strerror(errno));     
        return FALSE;
    }
    return TRUE;
}

BOOL32 CPosaMailbox::RcvMsg(u32 dwTimeout, LPSTR szMsgBuf, u32 dwLen, u32* pdwLenRcved)
{
    u32 dwRecvLen;
    ssize_t tRecvLen = mq_receive((mqd_t)m_dwReadID, szMsgBuf, dwLen, NULL);

    if (-1 == tRecvLen)
    {
        printf("msgqueue(%d) recv failed! errno(%d)(%s)\n", m_dwReadID, errno, strerror(errno));     
        return FALSE;
    }
    if (NULL != pdwLenRcved)
    {
        *pdwLenRcved = (u32)tRecvLen;;
    }
    return TRUE;
}*/


