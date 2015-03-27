#include "posatelnetserver.h"

#define TELCMD_WILL             (u8)251
#define TELCMD_WONT             (u8)252
#define TELCMD_DO               (u8)253
#define TELCMD_DONT             (u8)254
#define TELCMD_IAC              (u8)255

#define TELOPT_ECHO             (u8)1
#define TELOPT_SGA              (u8)3
#define TELOPT_LFLOW            (u8)33
#define TELOPT_NAWS             (u8)34

CPosaTelnetServer::CPosaTelnetServer()
{
    m_pInstance = new CPosaTelnetInstance;
}
CPosaTelnetServer::~CPosaTelnetServer()
{
    this->Close();
    delete m_pInstance;
    m_pInstance = NULL;
}

int CPosaTelnetServer::Open(CPosaNetProactor* pProactor, int dwLocalPort)
{
    return IPosaNetHandler::Open(pProactor, IPosaNetHandler::POSA_NET_HANLDER_SERVER, 
        NULL, NULL, dwLocalPort, 0);
}
int CPosaTelnetServer::Close()
{
    IPosaNetHandler::Close();    
    m_pInstance->Close();    
    return 0;
}
int CPosaTelnetServer::HandleAccept(int sclientfd)
{
    m_pInstance->Close();        
    m_pInstance->Open(m_pNetProactor, IPosaNetHandler::POSA_NET_HANDLER_INSTANCE, 
        NULL, NULL, NULL, sclientfd);
    
    s8 abyBuf[] = {TELCMD_IAC, TELCMD_DO, TELOPT_ECHO,
        TELCMD_IAC, TELCMD_DO, TELOPT_NAWS,
        TELCMD_IAC, TELCMD_DO, TELOPT_LFLOW,
        TELCMD_IAC, TELCMD_WILL, TELOPT_ECHO,
        TELCMD_IAC, TELCMD_WILL, TELOPT_SGA};
    u32 dwSendBytes = 0;
    m_pInstance->SendData(abyBuf, sizeof(abyBuf), dwSendBytes);        
    m_pInstance->TelePrint("****************************************************************\n");
    m_pInstance->TelePrint("welcome to posa telnet server. this is implemented by wxh.\n");
    m_pInstance->TelePrint("****************************************************************\n");
    m_pInstance->PromptShow();
    return 0;
}

int CPosaTelnetServer::Print(char* strInfo)
{    
    return 0;
    if (m_pInstance)
    {
        u32 dwSended;
        m_pInstance->SendData(strInfo, strlen(strInfo), dwSended);
    }
    return 0;
}
