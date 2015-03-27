/*
 *	this file is created by wangxiaohui 
 */

#ifndef _KDM_POSA_RTMP_PROTOCOL_H_ 
#define _KDM_POSA_RTMP_PROTOCOL_H_
#include "posa_linux.h"
#include "posartmpclient.h"

#include <string>
using namespace std;

typedef u32 getoff (u8*buf, u32 len);
struct RTMPPacket;
struct RTMP_METHOD;
struct RTMPMetadata;

typedef enum _RTMPState 
{
	RTMP_STATE_NOT_INITIALIZED,
	RTMP_STATE_CLIENT_REQUEST_RECEIVED,
	RTMP_STATE_CLIENT_REQUEST_SENT,
	RTMP_STATE_SERVER_RESPONSE_SENT,
	RTMP_STATE_DONE
} RTMPState;


class CPosaRtmpProtocol
{
public:
    CPosaRtmpProtocol(IPosaNetHandler* pHandler, BOOL32 bServer);
    ~CPosaRtmpProtocol();
    
    int DealData(CPosaBuffer& cBuffer);    
    int Close();        

    //client
    int SetupURL(string strRtmpUrl, BOOL32 bPublish, string& strHost, u32& dwPort);
    int ClientStartHandShake();

    // client and server
    int SendH264Frame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp);
    int SendAmrFrame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp);
    int SendWbFrame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp);

    int ClientSendServerCloseStream();

    int ServerSendClientConnectOk(int nInvokeNum);
    int ServerSendClientPlayOk();

    int SendConnectPacket();
	void ClientSendServerCreateStream();

    //i am Negotiating  follow the callback fun to change
    BOOL32 m_bClientNegotiating;
private:
    int HandleRtmpPacket(RTMPPacket *packet);
    int SendCtrl(s16 nType, u32 nObject, u32 nTime);    

    //as default the rtmp protocol is needbuf for intergrity
    // but the media stream data should not use the buf
    int SendPacket(RTMPPacket *packet, int queue, BOOL32 bNeedBuf = TRUE);

    int HandleInvoke(const char *body, u32 nBodySize);
    int HandleMetadata(char *body, u32 len, u32 dwTimeStamp);
    void HandleChangeChunkSize(const RTMPPacket *packet);
    void HandleAudio(u8* pBody, u32 nBodySize, u32 dwTimeStamp);
    void HandleVideo(u8* pBody, u32 nBodySize, u32 dwTimeStamp);
    void HandleCtrl(const RTMPPacket *packet);
    void HandleServerBW(const RTMPPacket *packet);
    void HandleClientBW(const RTMPPacket *packet);

    
    int SendPublish();
    int SendFCPublish();
    int SendReleaseStream();
    int SendPlay();
    int SendPong(double txn);
    int SendCheckBW();
    int SendCheckBWResult(double txn);
    int SendClientBW();
    int SendServerBW();
    int SendCreateStream();
    int SendH264StreamMetadata();    
    int SendAcknowledgement();
    int SendChangeChunkSize(int nChunkSize);
    int SendOnBWDone();

    int ClearStatesWhenDonotCloseSocket();
private: // server
    BOOL32 PerformHandshake(CPosaBuffer& buffer);
    BOOL32 PerformHandshake(CPosaBuffer& buffer, BOOL32 encrypted);
    BOOL32 ValidateClient(CPosaBuffer&inputBuffer);
    BOOL32 ValidateClientScheme(CPosaBuffer&inputBuffer, u8 scheme);
    u32 GetDigestOffset(u8 *pBuffer, u8 schemeNumber);
    u32 GetDigestOffset0(u8 *pBuffer);
    u32 GetDigestOffset1(u8 *pBuffer);

private:
    BOOL32 m_bServer;

    string m_strUrl;
    string m_strPlayPath;
    string m_strApp;
    u32 m_dwProtocol;
    u32 m_dwFlags;
    u32 m_dwHandCounter;
    BOOL32 m_bFp9HandShake;

    s32 m_nClientBW;
    s32 m_nClientBW2;
    s32 m_nServerBW;
    u32 m_dwReadedData;
    u32 m_dwReadedLastInterval;

    u32 m_inChunkSize;
    u32 m_outChunkSize;
    BOOL32 m_bPlaying;
    u32 m_pausing;
    double m_fDuration;		/* duration of stream in seconds */

    int* m_channelTimestamp;
    RTMPPacket** m_vecChannelsIn;
    u32 m_channelsAllocatedIn;

    RTMPPacket** m_vecChannelsOut;
    int m_channelsAllocatedOut;

    // sshl
    getoff* m_pFunDig;
    int m_nDigFunInx;
    u8 m_clientbuf[1536 + 4];
    u8* m_clientsig;
    u32 m_digestPosClient;

    int m_stream_id;
    int m_numInvokes;
    int m_numCalls;
    RTMP_METHOD* m_methodCalls;	/* remote method calls queue */
    int m_nBWCheckCounter;
    int m_nBufferMS;
    
    double m_fAudioCodecs;
    double m_fVideoCodecs;

    BOOL32 m_bSpsSended;
    BOOL32 m_bPpsSended;
    BOOL32 m_bH264MetaSended;
    RTMPMetadata* m_pMetaData;    

    BOOL32 m_bAmrMetaSended;

    IPosaNetHandler* m_pNetHandler;
    u32 _currentFPVersion;
    BOOL32 m_handshakeCompleted;
    RTMPState _rtmpState;
    int _validationScheme;
    u8* _pOutputBuffer;

public:
    funRtmpClientCallBack m_funClientCB;
    void* m_dwClientUserData;
};

#endif
