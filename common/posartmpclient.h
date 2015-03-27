/*
    this lib is created by wxh for rtmp client and publish. 2014-3-3
*/

#ifndef _KDM_POSA_RTMP_CLIENT_H_
#define _KDM_POSA_RTMP_CLIENT_H_

// return value is int , the 0 is ok, -1 or other is error
// return value is BOOL, the 0 is error, 1 or other is ok

#include "posa_linux.h"
#include <string>
using namespace std;

typedef enum
{
	em_RtmpClient_Connecct_Ok = 0,
	em_RtmpClient_Connecct_Error,   // connect server ip error
	em_RtmpClient_Socket_Error,

	em_RtmpClient_Play_Ok,
	em_RtmpClient_Publish_Ok,

	em_RtmpClient_Play_Over,   // when error happened just close the client
	em_RtmpClient_Play_Error,
	em_RtmpClient_Publish_Error,

	em_RtmpClient_Video_Data,
	em_RtmpClient_Audio_Data,
	em_RtmpClient_Msg_Data,

	em_RtmpClient_Net_Pending, // network pending u can not send any data
	em_RtmpClient_Net_Can_Send, // now u can send

}emPosaRtmpClientMsgType;

typedef int (*funRtmpClientCallBack)(u32 nMsgType, u32 dwTimeStamp, const u8* szBuffer, u32 dwBufLen, void* dwUserData);


class CPosaRtmpProtocol;
class CPosaRtmpClient : public IPosaNetHandler
{
public:
    CPosaRtmpClient();
    virtual ~CPosaRtmpClient();
    int SetCallBack(funRtmpClientCallBack fun, void* dwUserData);
    int SetupURL(CPosaNetProactor* pProactor, string strRtmpUrl, BOOL32 bPublish);

    virtual int Close();
    int SendH264Frame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp);
    int SendAmrFrame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp);
    int SendWbFrame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp);

protected:
    virtual int HandleConnect();

    virtual int HandleClose(int nOperateType);

    virtual int HandleRead(CPosaBuffer& cBuffer);
        
    virtual int HandleSend(int nOperateType);
    
private:
    CPosaRtmpProtocol* m_pProtocol;
public:
    funRtmpClientCallBack m_funCB;
    void* m_dwUserData;
};

#endif

