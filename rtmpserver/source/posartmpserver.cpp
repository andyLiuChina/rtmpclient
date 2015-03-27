#include "posartmpserver.h"
#include "posartmpprotocol.h"

CPosaRtmpServer::CPosaRtmpServer()
{
    m_pFun = NULL;
}

CPosaRtmpServer::~CPosaRtmpServer()
{
}

int CPosaRtmpServer::Open(CPosaNetProactor* pProactor, int dwLocalPort)
{
    return IPosaNetHandler::Open(pProactor, IPosaNetHandler::POSA_NET_HANLDER_SERVER, 
        NULL, NULL, dwLocalPort, 0);
}
int CPosaRtmpServer::Close()
{
    IPosaNetHandler::Close();
    return 0;
}
int CPosaRtmpServer::HandleAccept(int sclientfd)
{
    IPosaNetHandler* pInstance = new CPosaRtmpServerInstance;
    pInstance->Open(m_pNetProactor, IPosaNetHandler::POSA_NET_HANDLER_INSTANCE, 
        NULL, NULL, NULL, sclientfd);
    // call back instance
    if (m_pFun)
    {
        m_pFun((CPosaRtmpServerInstance*)pInstance);
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//  instance 

CPosaRtmpServerInstance::CPosaRtmpServerInstance()
{
    m_pRtmpProtocol = new CPosaRtmpProtocol(this, TRUE);
}

CPosaRtmpServerInstance::~CPosaRtmpServerInstance()
{
    delete m_pRtmpProtocol;
    m_pRtmpProtocol = NULL;
}

void CPosaRtmpServerInstance::SetCallback(RtmpNewInstance pfun)
{
    //m_pRtmpProtocol->m_pFun = (MyRtmpNewInstance)pfun;
}

int CPosaRtmpServerInstance::SendH264Frame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp)
{
    return m_pRtmpProtocol->SendH264Frame(pBuffer, dwLen, dwTimeStamp);
}
int CPosaRtmpServerInstance::HandleClose(int nOperateType)
{
    return IPosaNetHandler::HandleClose(nOperateType);    
}

int CPosaRtmpServerInstance::HandleRead(CPosaBuffer& cBuffer)
{
    IPosaNetHandler::HandleRead(cBuffer);
    return m_pRtmpProtocol->DealData(cBuffer);
}
int CPosaRtmpServerInstance::HandleSend(int nOperateType)
{
    IPosaNetHandler::HandleSend(nOperateType);
    return 0;
}
