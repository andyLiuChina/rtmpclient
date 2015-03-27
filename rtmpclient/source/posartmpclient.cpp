#include "posa_linux.h"
#include "posartmpclient.h"
#include "posartmpprotocol.h"

CPosaRtmpClient::CPosaRtmpClient()
{        
    m_pProtocol = new CPosaRtmpProtocol(this, FALSE);
    this->Close();
    m_funCB = NULL;
    m_dwUserData = 0;
}

CPosaRtmpClient::~CPosaRtmpClient()
{    
    delete m_pProtocol;
    m_pProtocol = NULL;
}


int CPosaRtmpClient::SetCallBack(funRtmpClientCallBack fun, void* dwUserData)
{
	CPosaAutoLock cLock(m_pMutex);

	m_funCB = fun;
	m_dwUserData = dwUserData;
	m_pProtocol->m_funClientCB = fun;
	m_pProtocol->m_dwClientUserData = dwUserData;
	return 0;
}

int CPosaRtmpClient::HandleConnect()
{    
    IPosaNetHandler::HandleConnect();
    if (m_funCB)
    {
    	m_funCB(em_RtmpClient_Connecct_Ok, 0, NULL, 0, m_dwUserData);
    }
    m_pProtocol->ClientStartHandShake();

    return 0;
}

int CPosaRtmpClient::HandleClose(int nOperateType)
{    
	IPosaNetHandler::HandleClose(nOperateType);

	if (POSA_FD_CONNECT == nOperateType)
	{
		if (m_funCB)
		{
			m_funCB(em_RtmpClient_Connecct_Error, 0, NULL, 0, m_dwUserData);
		}
	}
	else
	{
		if (m_funCB)
		{
			m_funCB(em_RtmpClient_Socket_Error, 0, NULL, 0, m_dwUserData);
		}
	}
    return 0;
}


int CPosaRtmpClient::HandleRead(CPosaBuffer& cBuffer)
{    
    return m_pProtocol->DealData(cBuffer);;
}

int CPosaRtmpClient::HandleSend(int nOperateType)
{
    IPosaNetHandler::HandleSend(nOperateType);
	if (m_funCB)
	{
		m_funCB(em_RtmpClient_Net_Can_Send, 0, NULL, 0, m_dwUserData);
	}
    return 0;
}

int CPosaRtmpClient::Close()
{
    CPosaAutoLock cLock(m_pMutex);

    IPosaNetHandler::Close();
    
    m_pProtocol->Close();
    return 0;
}


int CPosaRtmpClient::SendH264Frame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp)
{
    CPosaAutoLock cLock(m_pMutex);
    m_pProtocol->SendH264Frame(pBuffer, dwLen, dwTimeStamp);
    return 0;
}

int CPosaRtmpClient::SendAmrFrame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp)
{
	CPosaAutoLock cLock(m_pMutex);
	int nRet = m_pProtocol->SendAmrFrame(pBuffer, dwLen, dwTimeStamp);
	if (POSA_NET_PENDING == nRet)
	{
		return em_RtmpClient_Net_Pending;
	}
	return nRet;
}

int CPosaRtmpClient::SendWbFrame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp)
{
	CPosaAutoLock cLock(m_pMutex);
	int nRet = m_pProtocol->SendWbFrame(pBuffer, dwLen, dwTimeStamp);
	if (POSA_NET_PENDING == nRet)
	{
		return em_RtmpClient_Net_Pending;
	}
	return nRet;
}

int CPosaRtmpClient::SetupURL(CPosaNetProactor* pProactor, string strRtmpUrl, BOOL32 bPublish)
{
	CPosaAutoLock cLock(m_pMutex);
	string strHost;
	u32 dwPort = 0;

	if (INVALID_SOCKET != this->m_hSocket)
	{
		if (TRUE == m_pProtocol->m_bClientNegotiating)
		{
			::PosaPrintf(TRUE, FALSE, "[rtmp] client is Negotiating, the call is too frequently! ");
			return em_RtmpClient_Net_Pending;
		}

		int nRet = m_pProtocol->ClientSendServerCloseStream();
		int dwRet = m_pProtocol->SetupURL(strRtmpUrl, bPublish, strHost, dwPort);
		if (0 == dwRet)
		{
			m_pProtocol->m_bClientNegotiating = TRUE;
			//m_pProtocol->SendConnectPacket();
			m_pProtocol->ClientSendServerCreateStream();
			return 0;
		}
	}
	else
	{
	    int dwRet = m_pProtocol->SetupURL(strRtmpUrl, bPublish, strHost, dwPort);

	    if (0 == dwRet)
	    {
	    	m_pProtocol->m_bClientNegotiating = TRUE;
	    	return this->Open(pProactor, IPosaNetHandler::POSA_NET_HANDLER_CLIENT, (char*)strHost.c_str(),
	                              dwPort, 0, 0);
	    }
	}

    return -1;
}
