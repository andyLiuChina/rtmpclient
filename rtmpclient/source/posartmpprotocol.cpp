#include "posartmpprotocol.h"
#include "posaopenssl.h"
#include "posartmppacket.h"
#include "posamediacommon.h"

static const u8 GenuineFMSKey[] = 
{
  0x47, 0x65, 0x6e, 0x75, 0x69, 0x6e, 0x65, 0x20,
  0x41, 0x64, 0x6f, 0x62, 0x65, 0x20, 0x46, 0x6c,
  0x61, 0x73, 0x68, 0x20, 0x4d, 0x65, 0x64, 0x69, 
  0x61, 0x20, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,
  0x20, 0x30, 0x30, 0x31,	/* Genuine Adobe Flash Media Server 001 */

  0xf0, 0xee, 0xc2, 0x4a, 0x80, 0x68, 0xbe, 0xe8, 
  0x2e, 0x00, 0xd0, 0xd1, 0x02, 0x9e, 0x7e, 0x57, 
  0x6e, 0xec, 0x5d, 0x2d, 0x29, 0x80, 0x6f, 0xab,
  0x93, 0xb8, 0xe6, 0x36, 0xcf, 0xeb, 0x31, 0xae
};				/* 68 */

static const u8 GenuineFPKey[] = 
{
  0x47, 0x65, 0x6E, 0x75, 0x69, 0x6E, 0x65, 0x20,
  0x41, 0x64, 0x6F, 0x62, 0x65, 0x20, 0x46, 0x6C,
  0x61, 0x73, 0x68, 0x20, 0x50, 0x6C, 0x61, 0x79,
  0x65, 0x72, 0x20, 0x30, 0x30, 0x31,			/* Genuine Adobe Flash Player 001 */
  0xF0, 0xEE, 0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8, 
  0x2E, 0x00, 0xD0, 0xD1, 0x02, 0x9E, 0x7E, 0x57,
  0x6E, 0xEC, 0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB, 
  0x93, 0xB8, 0xE6, 0x36, 0xCF, 0xEB, 0x31, 0xAE
};				/* 62 */

static getoff* digoff[] = {Client_GetDigestOffset1, Client_GetDigestOffset2};

static u32 RTMP_GetTime()
{
#ifdef WIN32
  return timeGetTime();
#else
  struct tms t;
  static int clk_tck = 0;
  if (!clk_tck) clk_tck = sysconf(_SC_CLK_TCK);
  return times(&t) * 1000 / clk_tck;
#endif
}

static const int packetSize[] = { 12, 8, 4, 1 };


CPosaRtmpProtocol::CPosaRtmpProtocol(IPosaNetHandler* pHandler, BOOL32 bServer)
{    
    m_pNetHandler = pHandler;
    m_bServer = bServer;

    m_channelTimestamp = NULL;
    m_vecChannelsIn = NULL;
    m_vecChannelsOut = NULL;
    m_channelsAllocatedOut = 0;
    m_channelsAllocatedIn = 0;
    
    m_methodCalls = NULL;
    m_numCalls = 0;
    m_pMetaData = new RTMPMetadata;
    this->Close();    

    m_funClientCB = NULL;
    m_dwClientUserData = 0;
}

CPosaRtmpProtocol::~CPosaRtmpProtocol()
{
    delete m_pMetaData;
    m_pMetaData = NULL;
}

int CPosaRtmpProtocol::ClientStartHandShake()
{              
    u32 uptime = htonl(RTMP_GetTime());
    memcpy(m_clientsig, &uptime, 4);

    m_clientsig[4] = 10;
    m_clientsig[6] = 45;	
    m_clientsig[5] = 0;
    m_clientsig[7] = 2;
    
    s32* ip = (s32*)(m_clientsig+8);
    for (int i = 2; i < RTMP_SIG_SIZE/4; i++)
    {
        *ip++ = rand();
    }
    
    //first
    if (m_bFp9HandShake)
    {        
        m_digestPosClient = m_pFunDig(m_clientsig, RTMP_SIG_SIZE);
        CalculateDigest(m_digestPosClient, m_clientsig, GenuineFPKey, 30,
		                &m_clientsig[m_digestPosClient]);
        
    }
    u32 dwSended;
    m_pNetHandler->SendData((char *)m_clientsig-1, RTMP_SIG_SIZE + 1, dwSended);
    m_dwHandCounter = 0;
    return 0;
}


int CPosaRtmpProtocol::DealData(CPosaBuffer& cBuffer)
{    
    //read buffer must is bigger than the chunk size
    cBuffer.SetMaxBufferSize(this->m_inChunkSize*2);

    int nLength = cBuffer.GetAvaliableBytesCount();
    u32 dwSended;
    if (FALSE == m_bServer && m_dwHandCounter < 2) 
    {
        if (0 == m_dwHandCounter)
        {
            if (cBuffer.GetAvaliableBytesCount() < RTMP_SIG_SIZE+1) 
            {
                return 0;
            }
            u8* pType = cBuffer.GetIBPointer();
            u8 type = pType[0];
            cBuffer.Ignore(1);
            
            u8* serversig = cBuffer.GetIBPointer();
            
            ::PosaPrintf(TRUE, FALSE, "FMS Version   : %d.%d.%d.%d\r\n", 
                serversig[4], serversig[5], serversig[6], serversig[7]);
            
            if (m_bFp9HandShake && type == 3 && !serversig[4])
            {
                m_bFp9HandShake = FALSE;
            }
            u8* reply = NULL;
            if (m_bFp9HandShake)
            {
                /* we have to use this signature now to find the correct algorithms for getting the digest and DH positions */
                int digestPosServer = m_pFunDig(serversig, RTMP_SIG_SIZE);            
                if (!VerifyDigest(digestPosServer, serversig, GenuineFMSKey, 36))
                {
                    PosaPrintf(TRUE, FALSE, "Trying different position for server digest!\n");
                    m_nDigFunInx ^= 1;
                    m_pFunDig = digoff[m_nDigFunInx];
                    
                    digestPosServer = m_pFunDig(serversig, RTMP_SIG_SIZE);
                    
                    if (!VerifyDigest(digestPosServer, serversig, GenuineFMSKey, 36))
                    {
                        PosaPrintf(TRUE, FALSE, "Couldn't verify the server digest");
                        return FALSE;
                    }
                }
                u8 client[RTMP_SIG_SIZE];
                reply = client;
                s32* ip = (s32*)reply;
                for (int i = 0; i < RTMP_SIG_SIZE/4; i++)
                {
                    *ip++ = rand();
                }
                u8 digestResp[SHA256_DIGEST_LENGTH];
                u8* signatureResp = reply+RTMP_SIG_SIZE-SHA256_DIGEST_LENGTH;
                
                HMACsha256(&serversig[digestPosServer], SHA256_DIGEST_LENGTH,
                    GenuineFPKey, sizeof(GenuineFPKey), digestResp);
                HMACsha256(reply, RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH, digestResp,
                    SHA256_DIGEST_LENGTH, signatureResp);
            }
            else
            {
                reply = serversig;
            }
            m_pNetHandler->SendData((s8*)reply, RTMP_SIG_SIZE, dwSended);
            m_dwHandCounter++;
            cBuffer.Ignore(RTMP_SIG_SIZE);
            
            if (cBuffer.GetAvaliableBytesCount())
            {
                this->DealData(cBuffer);
            }
        }
        else if (1 == m_dwHandCounter)
        {
            if (cBuffer.GetAvaliableBytesCount() < RTMP_SIG_SIZE) 
            {
                return 0;
            }
            
            cBuffer.Ignore(RTMP_SIG_SIZE);
            m_dwHandCounter++;
            
            SendConnectPacket();
            if (cBuffer.GetAvaliableBytesCount())
            {
                this->DealData(cBuffer);
            }
            //call back second handshake is ok
        }
    }
    else if (TRUE == m_bServer && FALSE == m_handshakeCompleted)
    {                
        this->PerformHandshake(cBuffer);
        if (m_handshakeCompleted)
        {
            if (cBuffer.GetAvaliableBytesCount())
            {
                this->DealData(cBuffer);
            }
        }
    }
    else
    {
        //receive packet 
        RTMPPacket packet = {0};
        int nLeftLength = cBuffer.GetAvaliableBytesCount();
        int nSrclength = nLeftLength;
        u8* pBuffer = cBuffer.GetIBPointer();        
        if (nLeftLength < 1) 
        {
            return TRUE;
        }
        u32 nSize;
        u8 headerType = pBuffer[0];
        packet.m_headerType = (headerType & 0xC0) >> 6;
        packet.m_nChannel = (headerType & 0x3F);
        pBuffer++;
        nLeftLength--;
        
        if (packet.m_nChannel == 0)
        {
            if (nLeftLength < 1)
            {
                return TRUE;
            }
            packet.m_nChannel = pBuffer[1];
            packet.m_nChannel += 64;
            pBuffer++;
            nLeftLength--;
        }
        else if (packet.m_nChannel == 1)
        {
            if (nLeftLength < 2)
            {
                return TRUE;
            }
            int tmp = (pBuffer[1] << 8) + pBuffer[0];
            packet.m_nChannel = tmp + 64;
            pBuffer += 2;
            nLeftLength -= 2;
        }        
        nSize = packetSize[packet.m_headerType];
        
        if (packet.m_nChannel >= m_channelsAllocatedIn)
        {
            int n = packet.m_nChannel + 10;
            int* timestamp = (int*)realloc(m_channelTimestamp, sizeof(int) * n);
            RTMPPacket** packets = (RTMPPacket**)realloc(m_vecChannelsIn, sizeof(RTMPPacket*) * n);
            if (!timestamp)
                free(m_channelTimestamp);
            if (!packets)
                free(m_vecChannelsIn);
            
            m_channelTimestamp = timestamp;
            m_vecChannelsIn = packets;
            
            if (!timestamp || !packets) 
            {
                m_channelsAllocatedIn = 0;
                // some exception happened
                return FALSE;
            }
            memset(m_channelTimestamp + m_channelsAllocatedIn, 0, sizeof(int) * (n - m_channelsAllocatedIn));
            memset(m_vecChannelsIn + m_channelsAllocatedIn, 0, sizeof(RTMPPacket*) * (n - m_channelsAllocatedIn));
            m_channelsAllocatedIn = n;
        }
        
        if (nSize == RTMP_LARGE_HEADER_SIZE)
        {
            packet.m_hasAbsTimestamp = TRUE;
        }
        else if (nSize < RTMP_LARGE_HEADER_SIZE)
        {				/* using values from the last message of this channel */
            if (m_vecChannelsIn[packet.m_nChannel])
            {
                memcpy(&packet, m_vecChannelsIn[packet.m_nChannel], sizeof(RTMPPacket));
            }
        }
        
        nSize--;        
        if (nLeftLength < nSize)
        {
            return FALSE;
        }
        
        if (nSize >= 3)
        {
            packet.m_nTimeStamp = AMF_DecodeInt24((char*)pBuffer);                        
            
            if (nSize >= 6)
            {
                packet.m_nBodySize = AMF_DecodeInt24((char*)(pBuffer + 3));
                packet.m_nBytesRead = 0;
                RTMPPacket_Free(&packet);
                
                if (nSize > 6)
                {
                    packet.m_packetType = pBuffer[6];
                    
                    if (nSize == 11)
                    {
                        packet.m_nInfoField2 = AM_DecodeInt32LE((char*)(pBuffer + 7));
                    }
                }
            }
            pBuffer += nSize;
            nLeftLength -= nSize;
            
            if (packet.m_nTimeStamp == 0xffffff)
            {
                if (nLeftLength < 4)
                {
                    return FALSE;
                }
                packet.m_nTimeStamp = AMF_DecodeInt32((char*)pBuffer);
                pBuffer += 4;
                nLeftLength -= 4;                
            }
        }
        
        if (packet.m_nBodySize > 0 && packet.m_body == NULL)
        {
            if (!RTMPPacket_Alloc(&packet, packet.m_nBodySize))
            {
                
            }
            //didAlloc = TRUE;
            //packet->m_headerType = (hbuf[0] & 0xc0) >> 6;
        }
        
        int nToRead = packet.m_nBodySize - packet.m_nBytesRead;
        int nChunk = m_inChunkSize;
        if (nToRead < nChunk)
        {
            nChunk = nToRead;
        }
        
        if (nLeftLength < nChunk)
        {
            return TRUE;
        }
        memcpy(packet.m_body + packet.m_nBytesRead, pBuffer, nChunk);
        pBuffer += nChunk;
        nLeftLength -= nChunk;                
        
        packet.m_nBytesRead += nChunk;
        
        /* keep the packet as ref for other packets on this channel */
        if (!m_vecChannelsIn[packet.m_nChannel])
        {
            m_vecChannelsIn[packet.m_nChannel] = (RTMPPacket*)malloc(sizeof(RTMPPacket));
        }
        
        memcpy(m_vecChannelsIn[packet.m_nChannel], &packet, sizeof(RTMPPacket));
        
        if (RTMPPacket_IsReady(&packet))
        {
            /* make packet's timestamp absolute */
            if (!packet.m_hasAbsTimestamp)
            {   
                /* timestamps seem to be always relative!! */
                packet.m_nTimeStamp += m_channelTimestamp[packet.m_nChannel];
            }
            
            m_channelTimestamp[packet.m_nChannel] = packet.m_nTimeStamp;
            
            /* reset the data from the stored packet. we keep the header since we may use it later if a new packet for this channel */
            /* arrives and requests to re-use some info (small packet header) */
            m_vecChannelsIn[packet.m_nChannel]->m_body = NULL;
            m_vecChannelsIn[packet.m_nChannel]->m_nBytesRead = 0;
            m_vecChannelsIn[packet.m_nChannel]->m_hasAbsTimestamp = FALSE;	/* can only be false if we reuse header */                        

            if (FALSE)
            {
            	PosaPrintf(TRUE, FALSE, "packet is ready size is %d type is %d channel is %d time is %d ",
            	                       packet.m_nBodySize, packet.m_packetType, packet.m_nChannel, packet.m_nTimeStamp);
            }

            //deal with the ready packet
            this->HandleRtmpPacket(&packet);
            
            m_dwReadedLastInterval += packet.m_nBodySize + 11;
            if (m_dwReadedLastInterval > (m_nServerBW/2) ) 
            {
                m_dwReadedData += m_dwReadedLastInterval; 
                m_dwReadedLastInterval = 0;
                this->SendAcknowledgement();
            }

            RTMPPacket_Free(&packet);
        }
        else
        {
            packet.m_body = NULL;	/* so it won't be erased on free */
        }
        
        cBuffer.Ignore(nSrclength-nLeftLength);        

        if (cBuffer.GetAvaliableBytesCount())
        {
            this->DealData(cBuffer);
        }        
    }
    return 0;
}

int CPosaRtmpProtocol::ClearStatesWhenDonotCloseSocket()
{
	m_strUrl.erase();

	m_nBWCheckCounter = 0;
	m_stream_id = 0;
	m_dwReadedLastInterval = 0;
	m_dwReadedData = 0;

	m_dwFlags = 0;
	m_dwProtocol = 0;
	m_bPlaying = FALSE;

	m_bSpsSended = FALSE;
	m_bPpsSended = FALSE;
	m_bH264MetaSended = FALSE;
	memset(m_pMetaData, 0, sizeof(RTMPMetadata));

	m_bAmrMetaSended = FALSE;
	////////////////////////////////



	return 0;
}

int CPosaRtmpProtocol::Close()
{
	PosaPrintf(TRUE, FALSE, "call CPosaRtmpProtocol::Close");

	//states with the handshake
    m_handshakeCompleted = FALSE;
    _rtmpState = RTMP_STATE_NOT_INITIALIZED;
    _pOutputBuffer = NULL;
    m_dwHandCounter = 0;
	m_nDigFunInx = 0;
	m_pFunDig = digoff[m_nDigFunInx];

	m_clientsig = m_clientbuf + 4;
	m_clientsig[-1] = 0x03;
	m_bFp9HandShake = TRUE;

	m_bClientNegotiating = FALSE;

	m_inChunkSize = 128;
	m_outChunkSize = 128;

	m_nClientBW = 2500000;
	m_nClientBW2 = 2;
	m_nServerBW = 2500000;

	m_nBufferMS = 30000;
	m_fAudioCodecs = 3191.0;
	m_fVideoCodecs = 252.0;


	ClearStatesWhenDonotCloseSocket();

	if (m_channelTimestamp)
	{
		free(m_channelTimestamp);
		m_channelTimestamp = NULL;
	}
	if (m_vecChannelsIn)
	{
		for (int inx = 0; inx < m_channelsAllocatedIn; inx++)
		{
			if (m_vecChannelsIn[inx])
			{
				free(m_vecChannelsIn[inx]);
				m_vecChannelsIn[inx] = NULL;
			}
		}
		free(m_vecChannelsIn);
		m_vecChannelsIn = NULL;
	}
	m_channelsAllocatedIn = 0;

	if (m_vecChannelsOut)
	{
		for (int i = 0; i < m_channelsAllocatedOut; i++)
		{
			if (m_vecChannelsOut[i])
			{
				free(m_vecChannelsOut[i]);
				m_vecChannelsOut[i] = NULL;
			}
		}
		free(m_vecChannelsOut);
		m_vecChannelsOut = NULL;
	}
	m_channelsAllocatedOut = 0;

	AV_clear(m_methodCalls, m_numCalls);
	m_methodCalls = NULL;
	m_numCalls = 0;
	m_numInvokes = 0;
    return 0;
}

static void RTMP_ParsePlaypath(AVal *in, string& strPath) 
{
    int addMP4 = 0;
    int addMP3 = 0;
    int subExt = 0;
    const char *playpath = in->av_val;
    const char *temp, *q, *ext = NULL;
    const char *ppstart = playpath;
    char *streamname, *destptr, *p;
    
    int pplen = in->av_len;        
    
    if ((*ppstart == '?') &&
        (temp=strstr(ppstart, "slist=")) != 0) 
    {
        ppstart = temp+6;
        pplen = strlen(ppstart);
        
        temp = strchr(ppstart, '&');
        if (temp) 
        {
            pplen = temp-ppstart;
        }
    }
    
    q = strchr(ppstart, '?');
    if (pplen >= 4) 
    {
        if (q)
            ext = q-4;
        else
            ext = &ppstart[pplen-4];
        if ((strncmp(ext, ".f4v", 4) == 0) ||
            (strncmp(ext, ".mp4", 4) == 0)) 
        {
            addMP4 = 1;
            subExt = 1;
            /* Only remove .flv from rtmp URL, not slist params */
        } else if ((ppstart == playpath) &&
            (strncmp(ext, ".flv", 4) == 0)) 
        {
            subExt = 1;
        } else if (strncmp(ext, ".mp3", 4) == 0) 
        {
            addMP3 = 1;
            subExt = 1;
        }
    }
    
    streamname = (char *)malloc((pplen+4+1)*sizeof(char));
    if (!streamname)
        return;
    
    destptr = streamname;
    if (addMP4) 
    {
        if (strncmp(ppstart, "mp4:", 4)) 
        {
            strcpy(destptr, "mp4:");
            destptr += 4;
        } else {
            subExt = 0;
        }
    }
    else if (addMP3) 
    {
        if (strncmp(ppstart, "mp3:", 4)) 
        {
            strcpy(destptr, "mp3:");
            destptr += 4;
        } else 
        {
            subExt = 0;
        }
    }
    
    for (p=(char *)ppstart; pplen >0;) 
    {
        /* skip extension */
        if (subExt && p == ext) 
        {
            p += 4;
            pplen -= 4;
            continue;
        }
        if (*p == '%') 
        {
            unsigned int c;
            sscanf(p+1, "%02x", &c);
            *destptr++ = c;
            pplen -= 3;
            p += 3;
        } 
        else 
        {
            *destptr++ = *p++;
            pplen--;
        }
    }
    *destptr = '\0';
    strPath = string(streamname, (destptr - streamname));
    free(streamname);
}

static BOOL32 RTMP_ParseURL(const char* url, u32* protocol, string& host, u32* port,
	string& playpath, string& app)

{
	char *p, *end, *col, *ques, *slash;
    char szBuffer[256];
	*protocol = RTMP_PROTOCOL_RTMP;
	*port = 0;	

	/* look for usual :// pattern */
    char strTag[] = "://";
	p = strstr((char*)url, strTag);
	if(!p) 
    {
		PosaPrintf(TRUE, FALSE, "RTMP URL: No :// in url!");
		return FALSE;
	}
    
    int len = (int)(p-url);
    
    if(len == 4 && strncasecmp(url, "rtmp", 4)==0)
    {    
        *protocol = RTMP_PROTOCOL_RTMP;
    }
    else if(len == 5 && strncasecmp(url, "rtmpt", 5)==0)
    {
        *protocol = RTMP_PROTOCOL_RTMPT;
    }
    else if(len == 5 && strncasecmp(url, "rtmps", 5)==0)
    {
        *protocol = RTMP_PROTOCOL_RTMPS;
    }
    else if(len == 5 && strncasecmp(url, "rtmpe", 5)==0)
    {
        *protocol = RTMP_PROTOCOL_RTMPE;
    }
    else if(len == 5 && strncasecmp(url, "rtmfp", 5)==0)
    {
        *protocol = RTMP_PROTOCOL_RTMFP;
    }
    else if(len == 6 && strncasecmp(url, "rtmpte", 6)==0)
    {
        *protocol = RTMP_PROTOCOL_RTMPTE;
    }
    else if(len == 6 && strncasecmp(url, "rtmpts", 6)==0)
    {
        *protocol = RTMP_PROTOCOL_RTMPTS;
    }
    else 
    {
        PosaPrintf(TRUE, FALSE, "Unknown protocol!\n");        
    }
    
    
    /* let's get the hostname */
    p+=3;
    
    /* check for sudden death */
    if(*p==0) 
    {
        ::PosaPrintf(TRUE, FALSE, "No hostname in URL!");
        return FALSE;
    }

	end   = p + strlen(p);
	col   = strchr(p, ':');
	ques  = strchr(p, '?');
	slash = strchr(p, '/');

    {
        int hostlen;
        if(slash)
            hostlen = slash - p;
        else
            hostlen = end - p;
        if(col && col -p < hostlen)
            hostlen = col - p;
        
        if(hostlen < 256) 
        {
        /*host->av_val = p;
            host->av_len = hostlen;*/
            strncpy(szBuffer, p, hostlen);
            szBuffer[hostlen] = 0;
            host = szBuffer;
        }
        else 
        {
            PosaPrintf(TRUE, FALSE, "Hostname exceeds 255 characters!");
        }
        
        p+=hostlen;
    }

	/* get the port number if available */
	if(*p == ':') 
    {
		u32 p2;
		p++;
		p2 = atoi(p);
		if(p2 > 65535) 
        {
			PosaPrintf(TRUE, FALSE, "Invalid port number!");
		} 
        else 
        {
			*port = p2;
		}
	}

	if(!slash) 
    {
		PosaPrintf(TRUE, FALSE, "No application or playpath in URL!");
		return TRUE;
	}
	p = slash+1;

    {
    /* parse application
    *
    * rtmp://host[:port]/app[/appinstance][/...]
    * application = app[/appinstance]
        */
        
        char *slash2, *slash3 = NULL, *slash4 = NULL;
        int applen, appnamelen;
        
        slash2 = strchr(p, '/');
        if(slash2)
            slash3 = strchr(slash2+1, '/');
        if(slash3)
            slash4 = strchr(slash3+1, '/');
        
        applen = end-p; /* ondemand, pass all parameters as app */
        appnamelen = applen; /* ondemand length */
        
        if(ques && strstr(p, "slist=")) 
        { /* whatever it is, the '?' and slist= means we need to use everything as app and parse plapath from slist= */
            appnamelen = ques-p;
        }
        else if(strncmp(p, "ondemand/", 9)==0) 
        {
            /* app = ondemand/foobar, only pass app=ondemand */
            applen = 8;
            appnamelen = 8;
        }
        else 
        { /* app!=ondemand, so app is app[/appinstance] */
            if(slash4)
                appnamelen = slash4-p;
            else if(slash3)
                appnamelen = slash3-p;
            else if(slash2)
                appnamelen = slash2-p;
            
            applen = appnamelen;
        }
        strncpy(szBuffer, p, applen);
        szBuffer[applen] = 0;
        app = szBuffer;
        /*app->av_val = p;
        app->av_len = applen;*/	
        
        p += appnamelen;
    }

	if (*p == '/')
		p++;

	if (end-p) 
    {
		AVal av = {p, end-p};
        
		RTMP_ParsePlaypath(&av, playpath);        
	}
	return TRUE;
}
int CPosaRtmpProtocol::SetupURL(string strRtmpUrl, BOOL32 bPublish, string& strHost, u32& dwPort)
{
	PosaPrintf(TRUE, FALSE, "[rtmpclient] url %s  bpublish %d", strRtmpUrl.c_str(), bPublish);

    BOOL32 bParseOk = FALSE;
    m_strUrl = strRtmpUrl;
        
    BOOL32 bRet = RTMP_ParseURL(strRtmpUrl.c_str(), &m_dwProtocol, 
                                strHost, &dwPort, m_strPlayPath, m_strApp);    
    if (FALSE  == bRet)
    {
        return -1;
    }
    if (0 == strcmp(m_strApp.c_str(), "live"))
    {
        m_dwFlags |= RTMP_LF_LIVE;
    }
    if (bPublish)
    {
        m_dwProtocol |= RTMP_FEATURE_WRITE;
    }
    if (0 == dwPort) 
    {
        dwPort = 1935;
    }

    return 0;
}

int CPosaRtmpProtocol::HandleRtmpPacket(RTMPPacket *packet)
{
    //PosaPrintf(TRUE, FALSE, "deal with packet type is %d", packet->m_packetType);
    int bHasMediaPacket = 0;
    switch (packet->m_packetType)
    {
    case RTMP_PACKET_TYPE_CHUNK_SIZE:
        /* chunk size */
        HandleChangeChunkSize(packet);
        break;
        
    case RTMP_PACKET_TYPE_BYTES_READ_REPORT:
        /* bytes read report */
        {            
            u32 dwReaded = AMF_DecodeInt32(packet->m_body);
            ::PosaPrintf(TRUE, FALSE, "received: %d bytes read report\r\n", dwReaded);        
        }
        break;
        
    case RTMP_PACKET_TYPE_CONTROL:
        /* ctrl */
        HandleCtrl(packet);
        break;
        
    case RTMP_PACKET_TYPE_SERVER_BW:
        /* server bw */
        HandleServerBW(packet);
        break;
        
    case RTMP_PACKET_TYPE_CLIENT_BW:
        /* client bw */
        HandleClientBW(packet);
        break;
        
    case RTMP_PACKET_TYPE_AUDIO:
        /* audio data */
        /*RTMP_Log(RTMP_LOGDEBUG, "%s, received: audio %lu bytes", __FUNCTION__, packet.m_nBodySize); */
        HandleAudio((u8*)packet->m_body, packet->m_nBodySize, packet->m_nTimeStamp);
        bHasMediaPacket = 1;

        /*if (! m_mediaChannel)
             m_mediaChannel = packet->m_nChannel;
        if (! m_pausing)
             m_mediaStamp = packet->m_nTimeStamp;*/
        break;
        
    case RTMP_PACKET_TYPE_VIDEO:
        /* video data */
        /*RTMP_Log(RTMP_LOGDEBUG, "%s, received: video %lu bytes", __FUNCTION__, packet.m_nBodySize); */
        HandleVideo((u8*)packet->m_body, packet->m_nBodySize, packet->m_nTimeStamp);
        bHasMediaPacket = 1;
        /*if (! m_mediaChannel)
             m_mediaChannel = packet->m_nChannel;
        if (! m_pausing)
             m_mediaStamp = packet->m_nTimeStamp;*/
        break;
        
    case RTMP_PACKET_TYPE_FLEX_STREAM_SEND:
        /* flex stream send */
        ::PosaPrintf(TRUE, FALSE, "flex stream send, size %u bytes, not supported, ignoring\n",
            packet->m_nBodySize);
        break;
        
    case RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT:
        /* flex shared object */
        ::PosaPrintf(TRUE, FALSE, "flex shared object, size %u bytes, not supported, ignoring\n",
            packet->m_nBodySize);
        break;
        
    case RTMP_PACKET_TYPE_FLEX_MESSAGE:      
        {            
            ::PosaPrintf(TRUE, FALSE, "flex message, size %u bytes, not fully supported\n"
                , packet->m_nBodySize);
            
            if (HandleInvoke(packet->m_body + 1, packet->m_nBodySize - 1) == 1)
                bHasMediaPacket = 2;
            break;
        }
    case RTMP_PACKET_TYPE_INFO:
        /* metadata (notify) */
        PosaPrintf(TRUE, FALSE, "received: notify %u bytes\n", packet->m_nBodySize);
        if (HandleMetadata(packet->m_body, packet->m_nBodySize, packet->m_nTimeStamp))
            bHasMediaPacket = 1;
        break;
        
    case RTMP_PACKET_TYPE_SHARED_OBJECT:
        PosaPrintf(TRUE, FALSE, "shared object, not supported, ignoring\n");
        break;
        
    case RTMP_PACKET_TYPE_INVOKE:
        /* invoke */
        ::PosaPrintf(TRUE, FALSE, "received: invoke %u bytes", packet->m_nBodySize);
        if (HandleInvoke(packet->m_body, packet->m_nBodySize) == 1)
            bHasMediaPacket = 2;
        break;
        
    case RTMP_PACKET_TYPE_FLASH_VIDEO:
        {
            /* go through FLV packets and handle metadata packets */
            u32 pos = 0;
            u32 nTimeStamp = packet->m_nTimeStamp;
            
            while (pos + 11 < packet->m_nBodySize)
            {
                u32 dataSize = AMF_DecodeInt24(packet->m_body + pos + 1);	/* size without header (11) and prevTagSize (4) */
                
                if (pos + 11 + dataSize + 4 > packet->m_nBodySize)
                {
                    PosaPrintf(TRUE, FALSE, "Stream corrupt?!\n");
                    break;
                }
                if (packet->m_body[pos] == 0x12)
                {
                    HandleMetadata(packet->m_body + pos + 11, dataSize, packet->m_nTimeStamp);
                }
                else if (9 == packet->m_body[pos])
                {
                    nTimeStamp = AMF_DecodeInt24(packet->m_body + pos + 4);
                    nTimeStamp |= (packet->m_body[pos + 7] << 24);

                    // read video frame and call back
                    this->HandleVideo((u8*)&packet->m_body[pos+11], dataSize, nTimeStamp);
                }
                else if (8 == packet->m_body[pos])
                {
                	::PosaPrintf(TRUE, FALSE, "[rtmp]get audio data from the 0x16 msg");
                	nTimeStamp = AMF_DecodeInt24(packet->m_body + pos + 4);
                	nTimeStamp |= (packet->m_body[pos + 7] << 24);
                	this->HandleAudio((u8*)&packet->m_body[pos+11], dataSize, nTimeStamp);
                }
                pos += (11 + dataSize + 4);
            }
            /*if (! m_pausing)
                 m_mediaStamp = nTimeStamp;*/
            
            /* FLV tag(s) */
            /*RTMP_Log(RTMP_LOGDEBUG, "%s, received: FLV tag(s) %lu bytes", __FUNCTION__, packet.m_nBodySize); */
            bHasMediaPacket = 1;
            break;
        }
    default:
        PosaPrintf(TRUE, FALSE, "unknown packet type received: 0x%02x\n", packet->m_packetType);
    }
    
    return bHasMediaPacket;
}

int CPosaRtmpProtocol::HandleMetadata(char *body, u32 len, u32 dwTimeStamp)
{
    /* allright we get some info here, so parse it and print it */
    /* also keep duration or filesize to make a nice progress bar */
	if (this->m_funClientCB)
	{
		m_funClientCB(em_RtmpClient_Msg_Data, dwTimeStamp, (u8*) body, len, m_dwClientUserData);
	}
    return 0;

    AMFObject obj;
    AVal metastring;
    int ret = FALSE;
    
    int nRes = AMF_Decode(&obj, body, len, FALSE);
    if (nRes < 0)
    {
        PosaPrintf(TRUE, FALSE, "error decoding meta data packet\n");
        return FALSE;
    }
    
    AMF_Dump(&obj);
    AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &metastring);
    
    if (AVMATCH(&metastring, &av_onMetaData))
    {
        AMFObjectProperty prop;
        
        PosaPrintf(TRUE, FALSE, "Metadata:");
        //DumpMetaData(&obj);
        if (AMF_FindFirstMatchingProperty(&obj, &av_duration, &prop))
        {
            m_fDuration = prop.p_vu.p_number;
            
        }
        // Search for audio or video tags
        if (AMF_FindPrefixProperty(&obj, &av_video, &prop))
        {
            //m_read.dataType |= 1;
        }            
        if (AMF_FindPrefixProperty(&obj, &av_audio, &prop))
        {
            //m_read.dataType |= 4;
        }   
        ret = TRUE;
    }
    AMF_Reset(&obj);
    return ret;
}

void CPosaRtmpProtocol::HandleChangeChunkSize(const RTMPPacket *packet)
{
    if (packet->m_nBodySize >= 4)
    {
      m_inChunkSize = AMF_DecodeInt32(packet->m_body);
      PosaPrintf(TRUE, FALSE, "received: chunk size change to %d\n", m_inChunkSize);
    }
}

void CPosaRtmpProtocol::HandleAudio(u8* pBody, u32 nBodySize, u32 dwTimeStamp)
{
	if (FALSE)
	{
		::PosaPrintf(TRUE, FALSE, "[rtmp] get audio data  time %d  len %d\r\n",
					      dwTimeStamp, nBodySize);
	}

	if (m_funClientCB)
	{
		m_funClientCB(em_RtmpClient_Audio_Data, dwTimeStamp, (u8*)&(pBody[1]),
				      nBodySize-1, m_dwClientUserData);
	}
}

void CPosaRtmpProtocol::HandleVideo(u8* pBody, u32 nBodySize, u32 dwTimeStamp)
{
    //u8* pBody = (u8*)packet->m_body;
    BOOL32 bH264 = ((pBody[0] & 0x0F) == 7);
    if (FALSE == bH264) 
    {
        PosaPrintf(TRUE, FALSE, "get video is not h264 0x[%x], what u can do just is die", pBody[0]);
        return; 
    }
    
    BOOL32 bKeyFrame = ((pBody[0] & 0xF0) == 0x10);
    
    if (0x00 == pBody[1]) 
    {
        char szBuffer[128];
        PosaPrintf(TRUE, FALSE, "AVC sequence header get sps pps data");
        int i = 10;
        if (0xE1 != pBody[i++]) 
        {
            PosaPrintf(TRUE, FALSE, "sps num calc error!");
            return ;
        }
        int x = pBody[i++];
        int y = pBody[i++];
        int nSpsLen = (x<<8) | y;
        memcpy(szBuffer, &pBody[i], nSpsLen);
        i+= nSpsLen;
        if (0x1 != pBody[i++]) 
        {
            PosaPrintf(TRUE, FALSE, "pps num calc error!");
        }
        x = pBody[i++];
        y = pBody[i++];
	    int nPPsLen = (x<<8) | y;
        memcpy(szBuffer, &pBody[i], nPPsLen);
    }
    else if (0x01 == pBody[1])
    {
        // call back to data
        H264_RestoreFrame((u8*)&pBody[5], nBodySize-5);
        FILE* pFile = fopen("c:\\test.dat", "ab+");
        if (pFile)
        {
            fwrite(&pBody[5], 1, nBodySize-5, pFile);
            fclose(pFile);
        }
    }
}
void CPosaRtmpProtocol::HandleCtrl(const RTMPPacket *packet)
{
    s16 nType = -1;
    u32 tmp;
    if (packet->m_body && packet->m_nBodySize >= 2)
    {
        nType = AMF_DecodeInt16(packet->m_body);
    }
    ::PosaPrintf(TRUE, FALSE, "received ctrl. type: %d, len: %d", 
                 nType, packet->m_nBodySize);    
    
    if (packet->m_nBodySize >= 6)
    {
        switch (nType)
        {
        case 0:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            PosaPrintf(TRUE, FALSE, "Stream Begin %d", tmp);
            break;
            
        case 1:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            PosaPrintf(TRUE, FALSE, "Stream EOF %d", tmp);
            if (m_pausing == 1)
                m_pausing = 2;
            break;
            
        case 2:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            PosaPrintf(TRUE, FALSE, "Stream Dry %d", tmp);
            break;
        case 3:
            {
                int streamid = AMF_DecodeInt32(packet->m_body + 2);
                tmp = AMF_DecodeInt32(packet->m_body + 2 + 4);
                PosaPrintf(TRUE, FALSE, "server get Stream %d buffer %d", streamid, tmp);
            }
            break;
        case 4:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            PosaPrintf(TRUE, FALSE, "Stream IsRecorded %d", tmp);
            break;
            
        case 6:		/* server ping. reply with pong. */
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            PosaPrintf(TRUE, FALSE, "Ping %d", tmp);
            SendCtrl(0x07, tmp, 0);
            break;
            
            /* FMS 3.5 servers send the following two controls to let the client
            * know when the server has sent a complete buffer. I.e., when the
            * server has sent an amount of data equal to m_nBufferMS in duration.
            * The server meters its output so that data arrives at the client
            * in realtime and no faster.
            *
            * The rtmpdump program tries to set m_nBufferMS as large as
            * possible, to force the server to send data as fast as possible.
            * In practice, the server appears to cap this at about 1 hour's
            * worth of data. After the server has sent a complete buffer, and
            * sends this BufferEmpty message, it will wait until the play
            * duration of that buffer has passed before sending a new buffer.
            * The BufferReady message will be sent when the new buffer starts.
            * (There is no BufferReady message for the very first buffer;
            * presumably the Stream Begin message is sufficient for that
            * purpose.)
            *
            * If the network speed is much faster than the data bitrate, then
            * there may be long delays between the end of one buffer and the
            * start of the next.
            *
            * Since usually the network allows data to be sent at
            * faster than realtime, and rtmpdump wants to download the data
            * as fast as possible, we use this RTMP_LF_BUFX hack: when we
            * get the BufferEmpty message, we send a Pause followed by an
            * Unpause. This causes the server to send the next buffer immediately
            * instead of waiting for the full duration to elapse. (That's
            * also the purpose of the ToggleStream function, which rtmpdump
            * calls if we get a read timeout.)
            *
            * Media player apps don't need this hack since they are just
            * going to play the data in realtime anyway. It also doesn't work
            * for live streams since they obviously can only be sent in
            * realtime. And it's all moot if the network speed is actually
            * slower than the media bitrate.
            */
        case 31:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            PosaPrintf(TRUE, FALSE, "Stream BufferEmpty %d", tmp);
            /*if (!( Link.lFlags & RTMP_LF_BUFX))
                break;
            if (! m_pausing)
            {
                 m_pauseStamp =  m_mediaChannel <  m_channelsAllocatedIn ?
                     m_channelTimestamp[ m_mediaChannel] : 0;
                RTMP_SendPause(r, TRUE,  m_pauseStamp);
                 m_pausing = 1;
            }
            else if ( m_pausing == 2)
            {
                RTMP_SendPause(r, FALSE,  m_pauseStamp);
                 m_pausing = 3;
            }*/
            break;
            
        case 32:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            PosaPrintf(TRUE, FALSE, "Stream BufferReady %d", tmp);
            break;
            
        default:
            tmp = AMF_DecodeInt32(packet->m_body + 2);
            PosaPrintf(TRUE, FALSE, "Stream xx %d\n", tmp);
            break;
        }
        
    }
    
    if (nType == 0x1A)
    {
        PosaPrintf(TRUE, FALSE, "SWFVerification ping received: ");
        if (packet->m_nBodySize > 2 && packet->m_body[2] > 0x01)
        {
            PosaPrintf(TRUE, FALSE, "SWFVerification Type %d request not supported! Patches welcome...",
                       packet->m_body[2]);
        }                
        /* respond with HMAC SHA256 of decompressed SWF, key is the 30byte player key, also the last 30 bytes of the server handshake are applied */
        /*else if ( Link.SWFSize)
        {
            SendCtrl(r, 0x1B, 0, 0);
        }*/
        else
        {
            PosaPrintf(TRUE, FALSE, "Ignoring SWFVerification request, use --swfVfy!");
        }

    }
}
void CPosaRtmpProtocol::HandleServerBW(const RTMPPacket *packet)
{    
    m_nServerBW = AMF_DecodeInt32(packet->m_body);
    ::PosaPrintf(TRUE, FALSE, "server BW = %d\n", m_nServerBW);    
}
void CPosaRtmpProtocol::HandleClientBW(const RTMPPacket *packet)
{
    m_nClientBW = AMF_DecodeInt32(packet->m_body);
    if (packet->m_nBodySize > 4)
    {
        m_nClientBW2 = packet->m_body[4];
    }
    else
    {
        m_nClientBW2 = -1;
    }        
    PosaPrintf(TRUE, FALSE, "client BW = %d %d\n", m_nClientBW, m_nClientBW2);
}

int CPosaRtmpProtocol::SendCtrl(s16 nType, u32 nObject, u32 nTime)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    int nSize;
    char *buf;
    
    PosaPrintf(TRUE, FALSE, "sending ctrl. type: 0x%04x", (u16)nType);
    
    packet.m_nChannel = 0x02;	/* control channel (ping) */
    //packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_CONTROL;
    packet.m_nTimeStamp = 0;	/* RTMP_GetTime(); */
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    switch(nType) 
    {
    case 0x00: nSize = 6; break;
    case 0x03: nSize = 10; break;	/* buffer time */
    case 0x1A: nSize = 3; break;	/* SWF verify request */
    case 0x1B: nSize = 44; break;	/* SWF verify response */
    case 0x1F: nSize = 6; break; // 31 
    case 0x20: nSize = 6; break; // 32 buff is ready
    default: nSize = 6; break;
    }
    
    packet.m_nBodySize = nSize;
    
    buf = packet.m_body;
    buf = AMF_EncodeInt16(buf, pend, nType);
    
    if (nType == 0x1B)
    {
    /*
    memcpy(buf,  Link.SWFVerificationResponse, 42);
    RTMP_Log(RTMP_LOGDEBUG, "Sending SWFVerification response: ");
    RTMP_LogHex(RTMP_LOGDEBUG, (u8 *)packet.m_body, packet.m_nBodySize);
        */
    }
    else if (nType == 0x1A)
    {
        *buf = nObject & 0xff;
    }
    else
    {
        if (nSize > 2)
            buf = AMF_EncodeInt32(buf, pend, nObject);
        
        if (nSize > 6)
            buf = AMF_EncodeInt32(buf, pend, nTime);
    }
    
    return SendPacket(&packet, FALSE);
}

void CPosaRtmpProtocol::ClientSendServerCreateStream()
{
	// may be not needed
	/*if (r->Link.token.av_len)
	 {
	 AMFObjectProperty p;
	 if (RTMP_FindFirstMatchingProperty(&obj, &av_secureToken, &p))
	 {
	 DecodeTEA(&r->Link.token, &p.p_vu.p_aval);
	 SendSecureTokenResponse(r, &p.p_vu.p_aval);
	 }
	 }*/
	if (m_dwProtocol & RTMP_FEATURE_WRITE)
	{
		SendReleaseStream();
		SendFCPublish();
	}
	else
	{
		SendServerBW();
		SendCtrl(3, 0, 300);
	}
	SendCreateStream();
	if (!(m_dwProtocol & RTMP_FEATURE_WRITE))
	{
		/*
		 // Authenticate on Justin.tv legacy servers before sending FCSubscribe
		 if (r->Link.usherToken.av_len)
		 SendUsherToken(r, &r->Link.usherToken);

		 // Send the FCSubscribe if live stream or if subscribepath is set
		 if (r->Link.subscribepath.av_len)
		 SendFCSubscribe(r, &r->Link.subscribepath);
		 else if (r->Link.lFlags & RTMP_LF_LIVE)
		 SendFCSubscribe(r, &r->Link.playpath); */
	}
}

int CPosaRtmpProtocol::HandleInvoke(const char *body, u32 nBodySize)
{
    AMFObject obj;
    AVal method;
    double txn;
    int ret = 0, nRes;
    if (body[0] != 0x02)		/* make sure it is a string method name we start with */
    {
        ::PosaPrintf(TRUE, FALSE, "Sanity failed. no string method in invoke packet");
        return 0;
    }
    
    nRes = AMF_Decode(&obj, body, nBodySize, FALSE);
    if (nRes < 0)
    {
        ::PosaPrintf(TRUE, FALSE, "error decoding invoke packet");
        return 0;
    }
    
    AMF_Dump(&obj);
    AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &method);
    txn = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));
    ::PosaPrintf(TRUE, FALSE, "peer invoking <%s>", method.av_val);
    
    if (AVMATCH(&method, &av__result))
    {
        AVal methodInvoked = {0};
        int i;
        
        for (i = 0; i < m_numCalls; i++) 
        {
            if (m_methodCalls[i].num == (int)txn)
            {
                methodInvoked = m_methodCalls[i].name;
                AV_erase(m_methodCalls, &m_numCalls, i, FALSE);
                break;
            }
        }
        if (!methodInvoked.av_val) 
        {
            ::PosaPrintf(TRUE, FALSE, "received result id %f without matching request",txn);
            goto leave;
        }
        
        ::PosaPrintf(TRUE, FALSE, "received result for method call <%s>", methodInvoked.av_val);
        
        if (AVMATCH(&methodInvoked, &av_connect))
        {
			ClientSendServerCreateStream();
        }
        else if (AVMATCH(&methodInvoked, &av_createStream))
        {
            m_stream_id = (int)AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 3));
            
            if (m_dwProtocol & RTMP_FEATURE_WRITE)
            {
                SendPublish();
            }
            else
            {
                /*if (r->Link.lFlags & RTMP_LF_PLST)
                    SendPlaylist(r);*/
                SendPlay();
                SendCtrl(3, m_stream_id, m_nBufferMS);
            }
        }
        else if (AVMATCH(&methodInvoked, &av_play))
        {
        }
        else if (AVMATCH(&methodInvoked, &av_publish))
		{

        }
        free(methodInvoked.av_val);
    }
    else if (AVMATCH(&method, &av_onBWDone))
    {
        if (!m_nBWCheckCounter)
        {
            SendCheckBW();
        }            
    }
    else if (AVMATCH(&method, &av_onFCSubscribe))
    {
        /* SendOnFCSubscribe(); */
    }
    else if (AVMATCH(&method, &av_onFCUnsubscribe))
    {
        //RTMP_Close(r);
        // send callback error
        ret = 1;
    }
    else if (AVMATCH(&method, &av_ping))
    {
        SendPong(txn);
    }
    else if (AVMATCH(&method, &av__onbwcheck))
    {
        SendCheckBWResult(txn);
    }
    else if (AVMATCH(&method, &av__onbwdone))
    {
        int i;
        for (i = 0; i < m_numCalls; i++)
        {        
            if (AVMATCH(&m_methodCalls[i].name, &av__checkbw))
            {
                AV_erase(m_methodCalls, &m_numCalls, i, TRUE);
                break;
            }
        }
    }
    else if (AVMATCH(&method, &av__error))
    {
        AVal methodInvoked = {0};
        int i;
        
        if (m_dwProtocol & RTMP_FEATURE_WRITE)
        {
            for (i = 0; i < m_numCalls; i++)
            {
                if (m_methodCalls[i].num == txn)
                {
                    methodInvoked = m_methodCalls[i].name;
                    AV_erase(m_methodCalls, &m_numCalls, i, FALSE);
                    break;
                }
            }
            if (!methodInvoked.av_val)
            {
                PosaPrintf(TRUE, FALSE, "received result id %f without matching request", txn);
                goto leave;
            }
            
            PosaPrintf(TRUE, FALSE, "received error for method call <%s>", methodInvoked.av_val);
            
            if (AVMATCH(&methodInvoked, &av_connect))
            {
                AMFObject obj2;
                AVal code, level, description;
                AMFProp_GetObject(AMF_GetProp(&obj, NULL, 3), &obj2);
                AMFProp_GetString(AMF_GetProp(&obj2, &av_code, -1), &code);
                AMFProp_GetString(AMF_GetProp(&obj2, &av_level, -1), &level);
                AMFProp_GetString(AMF_GetProp(&obj2, &av_description, -1), &description);
                PosaPrintf(TRUE, FALSE, "error description: %s", description.av_val);
                /* if PublisherAuth returns 1, then reconnect */
                //PublisherAuth(&description);
            }
        }
        else
        {
            PosaPrintf(TRUE, FALSE, "rtmp server sent error");
        }
        free(methodInvoked.av_val);
    }
    else if (AVMATCH(&method, &av_close))
    {
        PosaPrintf(TRUE, FALSE, "rtmp server requested close");
        //RTMP_Close(r);        
    }
    else if (AVMATCH(&method, &av_onStatus))
    {
        AMFObject obj2;
        AVal code, level;
        AMFProp_GetObject(AMF_GetProp(&obj, NULL, 3), &obj2);
        AMFProp_GetString(AMF_GetProp(&obj2, &av_code, -1), &code);
        AMFProp_GetString(AMF_GetProp(&obj2, &av_level, -1), &level);
        
        ::PosaPrintf(TRUE, FALSE, "onStatus: %s", code.av_val);
        if (AVMATCH(&code, &av_NetStream_Failed)
            || AVMATCH(&code, &av_NetStream_Play_Failed)
            || AVMATCH(&code, &av_NetStream_Play_StreamNotFound)
            || AVMATCH(&code, &av_NetConnection_Connect_InvalidApp))
        {
        	//PosaPrintf(TRUE, FALSE, "***************************play error: %s", code.av_val);
            m_stream_id = 0;
            // call back play stream error

            m_bClientNegotiating = FALSE;
            if (m_funClientCB)
            {
            	m_funClientCB(em_RtmpClient_Play_Error, 0, NULL, 0, m_dwClientUserData);
            }
        }        
        else if (AVMATCH(&code, &av_NetStream_Play_Start)
            || AVMATCH(&code, &av_NetStream_Play_PublishNotify))
        {
            int i;
            m_bPlaying = TRUE;
            for (i = 0; i < m_numCalls; i++)
            {
                if (AVMATCH(&m_methodCalls[i].name, &av_play))
                {
                    AV_erase(m_methodCalls, &m_numCalls, i, TRUE);
                    break;
                }
            }
            m_bClientNegotiating = FALSE;
            //here call back  em_RtmpClient_Play_Ok
			if (m_funClientCB)
			{
				m_funClientCB(em_RtmpClient_Play_Ok, 0, NULL, 0, m_dwClientUserData);
			}
        }        
        else if (AVMATCH(&code, &av_NetStream_Publish_Start))
        {
            int i;
            m_bPlaying = TRUE;
            for (i = 0; i < m_numCalls; i++)
            {
                if (AVMATCH(&m_methodCalls[i].name, &av_publish))
                {
                    AV_erase(m_methodCalls, &m_numCalls, i, TRUE);
                    break;
                }
            }
            // here call back  em_RtmpClient_Publish_Ok
            m_bClientNegotiating = FALSE;
			if (m_funClientCB)
			{
				m_funClientCB(em_RtmpClient_Publish_Ok, 0, NULL, 0, m_dwClientUserData);
			}

        }
        else if (AVMATCH(&code, &av_NetStream_Publish_BadName))
        {
        	// here call back  em_RtmpClient_Publish_Error
        	m_bClientNegotiating = FALSE;
			if (m_funClientCB)
			{
				m_funClientCB(em_RtmpClient_Publish_Error, 0, NULL, 0, m_dwClientUserData);
			}
        }
        
        /* Return 1 if this is a Play.Complete or Play.Stop */
        else if (AVMATCH(&code, &av_NetStream_Play_Complete)
            || AVMATCH(&code, &av_NetStream_Play_Stop))
        {
        	m_bClientNegotiating = FALSE;
			if (m_funClientCB)
			{
				m_funClientCB(em_RtmpClient_Play_Over, 0, NULL, 0, m_dwClientUserData);
			}

            ret = 1;
        }
        else if (AVMATCH(&code, &av_NetStream_Play_UnpublishNotify))
        {
             // the peer do not publish any more, after this will get the play stop msg
        }
        
        else if (AVMATCH(&code, &av_NetStream_Seek_Notify))
        {
            //r->m_read.flags &= ~RTMP_READ_SEEKING;
        }
        
        else if (AVMATCH(&code, &av_NetStream_Pause_Notify))
        {
            if (m_pausing == 1 || m_pausing == 2)
            {
                //RTMP_SendPause(FALSE, m_pauseStamp);
                m_pausing = 3;
            }
        }
    }
    else if (AVMATCH(&method, &av_playlist_ready))
    {
        int i;
        for (i = 0; i < m_numCalls; i++)
        {
            if (AVMATCH(&m_methodCalls[i].name, &av_set_playlist))
            {
                AV_erase(m_methodCalls, &m_numCalls, i, TRUE);
                break;
            }
        }
    }
    else if (AVMATCH(&method, &av_connect))
    {
        AMFObjectProperty prop;
        if (AMF_FindFirstMatchingProperty(&obj, &av_app, &prop))
        {
            if (0 == strncmp(prop.p_vu.p_aval.av_val, "live", 4))
            {
                this->m_dwFlags |= RTMP_LF_LIVE;
            }
            else if (0 == strncmp(prop.p_vu.p_aval.av_val, "vod", 3))
            {
                
            }
            else
            {
                PosaPrintf(TRUE, FALSE, "call close is not support %s", 
                    prop.p_vu.p_aval.av_val);
                return 0;
            }
        }
        if (AMF_FindFirstMatchingProperty(&obj, &av_tcUrl, &prop))
        {
            m_strUrl = prop.p_vu.p_aval.av_val;
            PosaPrintf(TRUE, FALSE, "get connect %s", m_strUrl.c_str());
        }
        int nInvokeNum = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));
                
        //send connect ok back
        this->SendServerBW();
        this->SendClientBW();
        this->SendCtrl(0, 0, 0); //sream begin off
        
        this->ServerSendClientConnectOk(nInvokeNum);

        this->SendOnBWDone();
    }
    else if (AVMATCH(&method, &av_createStream))
    {                
        int nInvokeNum = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));        
        
        RTMPPacket packet;
        char pbuf[4096], *pend = pbuf + sizeof(pbuf);
        char *enc;
        
        packet.m_nChannel = 0x03;	/* control channel (invoke) */
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
        packet.m_nTimeStamp = 0;
        packet.m_nInfoField2 = 0;
        packet.m_hasAbsTimestamp = 0;
        packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
        
        enc = packet.m_body;                
        enc = AMF_EncodeString(enc, pend, &av__result);
        enc = AMF_EncodeNumber(enc, pend, nInvokeNum);
        *enc++ = AMF_NULL;

        m_stream_id = 1;
        enc = AMF_EncodeNumber(enc, pend, m_stream_id);

        packet.m_nBodySize = enc - packet.m_body;            
        SendPacket(&packet, FALSE);
    }
    else if (AVMATCH(&method, &av_play))
    {
        AVal playpath;        
        AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &playpath);
        string szPlayPath(playpath.av_val, playpath.av_len);
        m_strPlayPath = szPlayPath;
        // channel is 4
        //call back the play path
        this->ServerSendClientPlayOk();
    }
leave:
    AMF_Reset(&obj);
    return ret;
}

int CPosaRtmpProtocol::ServerSendClientConnectOk(int nInvokeNum)
{
    RTMPPacket packet = {0};
    char pbuf[4096], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x03;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;                
    enc = AMF_EncodeString(enc, pend, &av__result);
    enc = AMF_EncodeNumber(enc, pend, nInvokeNum);
    
    // one object
    *enc++ = AMF_OBJECT;
    
    SAVC(fmsVer);
    AVal tSuccess;
    tSuccess.av_val = "FMS/3,5,2,654";
    tSuccess.av_len = strlen(tSuccess.av_val);        
    enc = AMF_EncodeNamedString(enc, pend, &av_fmsVer, &tSuccess);
    if (!enc)
        return FALSE;
    
    enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 31);
    if (!enc)
        return FALSE;        
    
    if (enc + 3 >= pend)
        return FALSE;
    *enc++ = 0;
    *enc++ = 0;			/* end of object - 0x00 0x00 0x09 */
    *enc++ = AMF_OBJECT_END;
    
    
    // one object
    *enc++ = AMF_OBJECT;
    
    SAVC(status);
    enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
    if (!enc)
        return FALSE;
    
    //AVal tSuccess;
    tSuccess.av_val = "NetConnection.Connect.Success";
    tSuccess.av_len = strlen(tSuccess.av_val);
    enc = AMF_EncodeNamedString(enc, pend, &av_code, &tSuccess);
    if (!enc)
        return FALSE;
    
    tSuccess.av_val = "Connection succeeded.";
    tSuccess.av_len = strlen(tSuccess.av_val);
    enc = AMF_EncodeNamedString(enc, pend, &av_description, &tSuccess);
    if (!enc)
        return FALSE;

    SAVC(objectEncoding);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, 0);
    if (!enc)
        return FALSE;
    
    if (enc + 3 >= pend)
        return FALSE;
    *enc++ = 0;
    *enc++ = 0;			/* end of object - 0x00 0x00 0x09 */
    *enc++ = AMF_OBJECT_END;
    
    packet.m_nBodySize = enc - packet.m_body;            
    SendPacket(&packet, FALSE);
    return 0;
}

int CPosaRtmpProtocol::ServerSendClientPlayOk()
{
    // ���� ok
    
    int x = 0;
    
    this->SendCtrl(0x0, 1, 0); // stream begin        
    
    // send reset
    {
        RTMPPacket packet;
        char pbuf[4096], *pend = pbuf + sizeof(pbuf);
        char *enc;
        // 0x04
        packet.m_nChannel = 4;	// control channel (invoke) 
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
        packet.m_nTimeStamp = 0;
        packet.m_nInfoField2 = m_stream_id;
        packet.m_hasAbsTimestamp = 1;
        packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
        
        enc = packet.m_body;                
        enc = AMF_EncodeString(enc, pend, &av_onStatus);
        enc = AMF_EncodeNumber(enc, pend, 0);

        *enc++ = AMF_NULL;

        // one object
        *enc++ = AMF_OBJECT;
                   
        enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
        if (!enc)
            return FALSE;
        
        enc = AMF_EncodeNamedStringEx(enc, pend, &av_code, "NetStream.Play.Reset");
        if (!enc)
            return FALSE;

        enc = AMF_EncodeNamedStringEx(enc, pend, &av_description, 
                                      "Playing and resetting testme.");
        if (!enc)
            return FALSE;
        
        enc = AMF_EncodeNamedStringEx(enc, pend, &av_details, (char*)m_strPlayPath.c_str());
        if (!enc)
            return FALSE;
        
        enc = AMF_EncodeNamedStringEx(enc, pend, &av_clientid, "DCAIE83B");
        if (!enc)
            return FALSE;
                
        if (enc + 3 >= pend)
            return FALSE;
        *enc++ = 0;
        *enc++ = 0;			/* end of object - 0x00 0x00 0x09 */
        *enc++ = AMF_OBJECT_END;

        packet.m_nBodySize = enc - packet.m_body;            
        SendPacket(&packet, FALSE);
    }    
    
    // data start
    /*{
        RTMPPacket packet;
        char pbuf[4096], *pend = pbuf + sizeof(pbuf);
        char *enc;
        
        packet.m_nChannel = 0x04;	// control channel (invoke) 
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
        packet.m_nTimeStamp = 0;
        packet.m_nInfoField2 = m_stream_id;
        packet.m_hasAbsTimestamp = 0;
        packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
        
        enc = packet.m_body;                
        enc = AMF_EncodeString(enc, pend, &av_onStatus);

        // one object
        *enc++ = AMF_OBJECT;           

        AVal temp;
        temp.av_val = "NetStream.Data.Start";
        temp.av_len = strlen(temp.av_val);
        enc = AMF_EncodeNamedString(enc, pend, &av_code, &temp);
        if (!enc)
            return FALSE;
       
        if (enc + 3 >= pend)
            return FALSE;
        *enc++ = 0;
        *enc++ = 0;			// end of object - 0x00 0x00 0x09 
        *enc++ = AMF_OBJECT_END;

        packet.m_nBodySize = enc - packet.m_body;            
        SendPacket(&packet, FALSE);
    }*/

    // send start
    {
        RTMPPacket packet;
        char pbuf[4096], *pend = pbuf + sizeof(pbuf);
        char *enc;
        
        packet.m_nChannel = 4;	// control channel (invoke)
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
        packet.m_nTimeStamp = 0;
        packet.m_nInfoField2 = m_stream_id;
        packet.m_hasAbsTimestamp = 1;
        packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
        
        enc = packet.m_body;                
        enc = AMF_EncodeString(enc, pend, &av_onStatus);
        enc = AMF_EncodeNumber(enc, pend, 0);

        *enc++ = AMF_NULL;

        // one object
        *enc++ = AMF_OBJECT;
           
        enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
        if (!enc)
            return FALSE;

        enc = AMF_EncodeNamedStringEx(enc, pend, &av_code, "NetStream.Play.Start");
        if (!enc)
            return FALSE;

        enc = AMF_EncodeNamedStringEx(enc, pend, &av_description, "Started playing testme.");
        if (!enc)
            return FALSE;

        SAVC(details);        
        enc = AMF_EncodeNamedStringEx(enc, pend, &av_details, (char*)m_strPlayPath.c_str());
        if (!enc)
            return FALSE;

        SAVC(clientid);
        enc = AMF_EncodeNamedStringEx(enc, pend, &av_clientid, "DCAIE83B");
        if (!enc)
            return FALSE;
                
        if (enc + 3 >= pend)
            return FALSE;
        *enc++ = 0;
        *enc++ = 0;			/* end of object - 0x00 0x00 0x09 */
        *enc++ = AMF_OBJECT_END;

        packet.m_nBodySize = enc - packet.m_body;            
        SendPacket(&packet, FALSE);
    }
    
    // send RtmpSampleAccess
    {
        RTMPPacket packet;
        char pbuf[4096], *pend = pbuf + sizeof(pbuf);
        char *enc;
        
        packet.m_nChannel =  4;	// control channel (invoke)
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = RTMP_PACKET_TYPE_INFO;
        packet.m_nTimeStamp = 0;
        packet.m_nInfoField2 = m_stream_id;
        packet.m_hasAbsTimestamp = 1;
        packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
        
        enc = packet.m_body;
        enc = AMF_EncodeStringEx(enc, pend, "|RtmpSampleAccess");
        enc = AMF_EncodeBoolean(enc, pend, FALSE);
        enc = AMF_EncodeBoolean(enc, pend, FALSE);

        packet.m_nBodySize = enc - packet.m_body;            
        SendPacket(&packet, FALSE);
    }
    return 0;
}

int CPosaRtmpProtocol::ClientSendServerCloseStream()
{
	if (RTMP_FEATURE_WRITE == (RTMP_FEATURE_WRITE & m_dwProtocol))
	{
		RTMPPacket packet;
		char pbuf[1024], *pend = pbuf + sizeof(pbuf);
		char *enc;

		packet.m_nChannel = 0x04; // source channel (invoke)
		packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
		packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
		packet.m_nTimeStamp = 0;
		packet.m_nInfoField2 = m_stream_id;
		packet.m_hasAbsTimestamp = 0;
		packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

		enc = packet.m_body;
		enc = AMF_EncodeString(enc, pend, &av_FCUnpublish);
		enc = AMF_EncodeNumber(enc, pend, ++m_numInvokes);
		*enc++ = AMF_NULL;
		AVal avTemp;
		avTemp.av_val = (char*) m_strPlayPath.c_str();
		avTemp.av_len = m_strPlayPath.length();
		enc = AMF_EncodeString(enc, pend, &avTemp);
		packet.m_nBodySize = enc - packet.m_body;

		int nRet = SendPacket(&packet, FALSE);
	}
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;

    packet.m_nChannel = 0x04;	/* source channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = m_stream_id;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_closeStream);
    enc = AMF_EncodeNumber(enc, pend, ++m_numInvokes);
    *enc++ = AMF_NULL;
    //enc = AMF_EncodeNumber(enc, pend, m_stream_id);

    packet.m_nBodySize = enc - packet.m_body;

    int nRet = SendPacket(&packet, FALSE);


    {
        RTMPPacket packet;
        char pbuf[1024], *pend = pbuf + sizeof(pbuf);
        char *enc;

        packet.m_nChannel = 0x04;	// source channel (invoke)
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
        packet.m_nTimeStamp = 0;
        packet.m_nInfoField2 = m_stream_id;
        packet.m_hasAbsTimestamp = 0;
        packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

        enc = packet.m_body;
        enc = AMF_EncodeString(enc, pend, &av_deleteStream);
        enc = AMF_EncodeNumber(enc, pend, ++m_numInvokes);
        *enc++ = AMF_NULL;
        enc = AMF_EncodeNumber(enc, pend, m_stream_id);

        packet.m_nBodySize = enc - packet.m_body;

        int nRet = SendPacket(&packet, FALSE);
    }

    // some states need to be clear, some do not need
    ClearStatesWhenDonotCloseSocket();

    return nRet;
}

int CPosaRtmpProtocol::SendPublish()
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x04;	/* source channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = m_stream_id;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_publish);
    enc = AMF_EncodeNumber(enc, pend, ++m_numInvokes);
    *enc++ = AMF_NULL;

    AVal avTemp;
    avTemp.av_val = (char*)m_strPlayPath.c_str();
    avTemp.av_len = m_strPlayPath.length();
    enc = AMF_EncodeString(enc, pend, &avTemp);
    if (!enc)
    {
        return FALSE;
    }
            
    /* FIXME: should we choose live based on Link.lFlags & RTMP_LF_LIVE? */
    enc = AMF_EncodeString(enc, pend, &av_live);
    if (!enc)
        return FALSE;
    
    packet.m_nBodySize = enc - packet.m_body;
    
    return SendPacket(&packet, TRUE);
}

int CPosaRtmpProtocol::SendFCPublish()
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x03;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_FCPublish);
    enc = AMF_EncodeNumber(enc, pend, ++m_numInvokes);
    *enc++ = AMF_NULL;
    AVal avTemp;
    avTemp.av_val = (char*)m_strPlayPath.c_str();
    avTemp.av_len = m_strPlayPath.length();
    enc = AMF_EncodeString(enc, pend, &avTemp);
    if (!enc)
        return FALSE;
    
    packet.m_nBodySize = enc - packet.m_body;
    
    return SendPacket(&packet, FALSE);
}

int CPosaRtmpProtocol::SendReleaseStream()
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x03;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_releaseStream);
    enc = AMF_EncodeNumber(enc, pend, ++m_numInvokes);
    *enc++ = AMF_NULL;
    AVal avTemp;
    avTemp.av_val = (char*)m_strPlayPath.c_str();
    avTemp.av_len = m_strPlayPath.length();
    enc = AMF_EncodeString(enc, pend, &avTemp);
    if (!enc)
        return FALSE;
    
    packet.m_nBodySize = enc - packet.m_body;
    
    return SendPacket(&packet, FALSE);
}

int CPosaRtmpProtocol::SendPlay()
{
    RTMPPacket packet;
    char pbuf[1024], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x08;	/* we make 8 our stream channel */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = m_stream_id;	/*0x01000000; */
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_play);
    enc = AMF_EncodeNumber(enc, pend, ++m_numInvokes);
    *enc++ = AMF_NULL;
    
    /*PosaPrintf(TRUE, FALSE, "seekTime=%d, stopTime=%d, sending play: %s",
               seekTime, stopTime,
        playpath.av_val);*/
    AVal playpath;
    playpath.av_val = (char*)m_strPlayPath.c_str();
    playpath.av_len = m_strPlayPath.length();
    enc = AMF_EncodeString(enc, pend, &playpath);
    if (!enc)
        return FALSE;
    
        /* Optional parameters start and len.
        *
        * start: -2, -1, 0, positive number
        *  -2: looks for a live stream, then a recorded stream,
        *      if not found any open a live stream
        *  -1: plays a live stream
        * >=0: plays a recorded streams from 'start' milliseconds
    */
    if (m_dwFlags & RTMP_LF_LIVE)
    {
        enc = AMF_EncodeNumber(enc, pend, -1000.0);
    }        
    else
    {
        /*if (r->Link.seekTime > 0.0)
            enc = AMF_EncodeNumber(enc, pend, r->Link.seekTime);	// resume from here 
        else*/

        enc = AMF_EncodeNumber(enc, pend, 0.0);	// -2000.0);  recorded as default, -2000.0 is not reliable since that freezes the player if the stream is not found 
    }
    
    if (!enc)
        return FALSE;
    
        /* len: -1, 0, positive number
        *  -1: plays live or recorded stream to the end (default)
        *   0: plays a frame 'start' ms away from the beginning
        *  >0: plays a live or recoded stream for 'len' milliseconds
    */
    /*enc += EncodeNumber(enc, -1.0); */ /* len */
    /* if (stopTime)
    {
        enc = AMF_EncodeNumber(enc, pend, 40000);
        if (!enc)
            return FALSE;
    }*/
    
    packet.m_nBodySize = enc - packet.m_body;
    
    return SendPacket(&packet, TRUE);
}

int CPosaRtmpProtocol::SendPong(double txn)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x03;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0x16 * m_nBWCheckCounter;	/* temp inc value. till we figure it out. */
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_pong);
    enc = AMF_EncodeNumber(enc, pend, txn);
    *enc++ = AMF_NULL;
    
    packet.m_nBodySize = enc - packet.m_body;
    
    return SendPacket(&packet, FALSE);
}

int CPosaRtmpProtocol::SendCheckBW()
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x03;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;	/* RTMP_GetTime(); */
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av__checkbw);
    enc = AMF_EncodeNumber(enc, pend, ++m_numInvokes);
    *enc++ = AMF_NULL;
    
    packet.m_nBodySize = enc - packet.m_body;
    
    /* triggers _onbwcheck and eventually results in _onbwdone */
    return SendPacket(&packet, FALSE);
}

int CPosaRtmpProtocol::SendOnBWDone()
{
    RTMPPacket packet = {0};
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x03;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;	/* temp inc value. till we figure it out. */
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_onBWDone);
    enc = AMF_EncodeNumber(enc, pend, 0);

    *enc++ = AMF_NULL;
    enc = AMF_EncodeNumber(enc, pend, 8192);
    
    packet.m_nBodySize = enc - packet.m_body;
    
    return SendPacket(&packet, FALSE);
}

int CPosaRtmpProtocol::SendCheckBWResult(double txn)
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x03;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0x16 * m_nBWCheckCounter;	/* temp inc value. till we figure it out. */
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av__result);
    enc = AMF_EncodeNumber(enc, pend, txn);
    *enc++ = AMF_NULL;
    enc = AMF_EncodeNumber(enc, pend, (double)m_nBWCheckCounter++);
    
    packet.m_nBodySize = enc - packet.m_body;
    
    return SendPacket(&packet, FALSE);
}

int CPosaRtmpProtocol::SendServerBW()
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    
    packet.m_nChannel = 0x02;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_SERVER_BW;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    packet.m_nBodySize = 4;
    
    AMF_EncodeInt32(packet.m_body, pend, m_nServerBW);
    ::PosaPrintf(TRUE, FALSE, "Send server bw");
    return SendPacket(&packet, FALSE);
}

int CPosaRtmpProtocol::SendClientBW()
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    
    packet.m_nChannel = 0x02;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_CLIENT_BW;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    packet.m_nBodySize = 4+1;
    
    AMF_EncodeInt32(packet.m_body, pend, m_nClientBW);
    packet.m_body[4] = 2;

    ::PosaPrintf(TRUE, FALSE, "Send client bw");

    return SendPacket(&packet, FALSE);
}

int CPosaRtmpProtocol::SendCreateStream()
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x03;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_createStream);
    enc = AMF_EncodeNumber(enc, pend, ++m_numInvokes);
    *enc++ = AMF_NULL;		/* NULL */
    
    packet.m_nBodySize = enc - packet.m_body;
    
    return SendPacket(&packet, TRUE);
}

int CPosaRtmpProtocol::SendPacket(RTMPPacket *packet, int queue, BOOL32 bNeedBuf)
{
	int nRet = 0;
    const RTMPPacket *prevPacket;
    u32 last = 0;
    int nSize;
    int hSize, cSize;
    char *header, *hptr, *hend, hbuf[RTMP_MAX_HEADER_SIZE], c;
    u32 t;
    char *buffer, *tbuf = NULL, *toff = NULL;
    int nChunkSize;
    int tlen;
    
    if (packet->m_nChannel >= m_channelsAllocatedOut)
    {
        int n = packet->m_nChannel + 10;
        RTMPPacket **packets = (RTMPPacket **)realloc(m_vecChannelsOut, sizeof(RTMPPacket*) * n);
        if (!packets) 
        {
            free(m_vecChannelsOut);
            m_vecChannelsOut = NULL;
            m_channelsAllocatedOut = 0;
            return FALSE;
        }
        m_vecChannelsOut = packets;
        memset(m_vecChannelsOut + m_channelsAllocatedOut, 0, sizeof(RTMPPacket*) * (n - m_channelsAllocatedOut));
        m_channelsAllocatedOut = n;
    }
    
    prevPacket = m_vecChannelsOut[packet->m_nChannel];
    if (prevPacket && packet->m_headerType != RTMP_PACKET_SIZE_LARGE)
    {
        /* compress a bit by using the prev packet's attributes */
        if (prevPacket->m_nBodySize == packet->m_nBodySize
            && prevPacket->m_packetType == packet->m_packetType
            && packet->m_headerType == RTMP_PACKET_SIZE_MEDIUM)
            packet->m_headerType = RTMP_PACKET_SIZE_SMALL;
        
        if (prevPacket->m_nTimeStamp == packet->m_nTimeStamp
            && packet->m_headerType == RTMP_PACKET_SIZE_SMALL)
            packet->m_headerType = RTMP_PACKET_SIZE_MINIMUM;
        last = prevPacket->m_nTimeStamp;
    }
    
    if (packet->m_headerType > 3)	/* sanity */
    {
        PosaPrintf(TRUE, FALSE, "sanity failed!! trying to send header of type: 0x%02x.",
                   (unsigned char)packet->m_headerType);
        return -1;
    }
    
    nSize = packetSize[packet->m_headerType];
    hSize = nSize; cSize = 0;
    t = packet->m_nTimeStamp - last;
    
    if (packet->m_body)
    {
        header = packet->m_body - nSize;
        hend = packet->m_body;
    }
    else
    {
        header = hbuf + 6;
        hend = hbuf + sizeof(hbuf);
    }
    
    if (packet->m_nChannel > 319)
    {
        cSize = 2;
    }        
    else if (packet->m_nChannel > 63)
    {
        cSize = 1;
    }
    
    if (cSize)
    {
        header -= cSize;
        hSize += cSize;
    }
    
    if (nSize > 1 && t >= 0xffffff)
    {
        header -= 4;
        hSize += 4;
    }
    
    hptr = header;
    c = packet->m_headerType << 6;
    switch (cSize)
    {
    case 0:
        c |= packet->m_nChannel;
        break;
    case 1:
        break;
    case 2:
        c |= 1;
        break;
    }
    *hptr++ = c;
    if (cSize)
    {
        int tmp = packet->m_nChannel - 64;
        *hptr++ = tmp & 0xff;
        if (cSize == 2)
            *hptr++ = tmp >> 8;
    }
    
    if (nSize > 1)
    {
        hptr = AMF_EncodeInt24(hptr, hend, t > 0xffffff ? 0xffffff : t);
    }
    
    if (nSize > 4)
    {
        hptr = AMF_EncodeInt24(hptr, hend, packet->m_nBodySize);
        *hptr++ = packet->m_packetType;
    }
    
    if (nSize > 8)
        hptr += AM_EncodeInt32LE(hptr, packet->m_nInfoField2);
    
    if (nSize > 1 && t >= 0xffffff)
        hptr = AMF_EncodeInt32(hptr, hend, t);
    
    nSize = packet->m_nBodySize;
    buffer = packet->m_body;
    nChunkSize = m_outChunkSize;    
    
    /* send all chunks in one HTTP request */
    //if (m_dwProtocol & RTMP_FEATURE_HTTP)
    {
        int chunks = (nSize+nChunkSize-1) / nChunkSize;
        if (chunks > 1)
        {
            tlen = chunks * (cSize + 1) + nSize + hSize;
            tbuf = (char*)malloc(tlen);
            if (!tbuf)
                return -1;
            toff = tbuf;
        }
    }
    int nSendCount = 0;
    while (nSize + hSize)
    {
        u32 wrote;
        
        if (nSize < nChunkSize)
            nChunkSize = nSize;
        
        //RTMP_LogHexString(RTMP_LOGDEBUG2, (u8 *)header, hSize);
        //RTMP_LogHexString(RTMP_LOGDEBUG2, (u8 *)buffer, nChunkSize);
        if (tbuf)
        {
            memcpy(toff, header, nChunkSize + hSize);
            toff += nChunkSize + hSize;
        }
        else
        {
            //wrote = WriteN(r, header, nChunkSize + hSize);
        	nSendCount++;
        	if (2 == nSendCount)
        	{
        		::PosaPrintf(TRUE, FALSE, "XXXXXXXXXXXXXXXXXXXXXXX[rtmp]Send Packet some big thing happened");
        	}
        	nRet = m_pNetHandler->SendData(header, nChunkSize + hSize, wrote, bNeedBuf);
        }
        nSize -= nChunkSize;
        buffer += nChunkSize;
        hSize = 0;
        
        if (nSize > 0)
        {
            header = buffer - 1;
            hSize = 1;
            if (cSize)
            {
                header -= cSize;
                hSize += cSize;
            }
            *header = (0xc0 | c);
            if (cSize)
            {
                int tmp = packet->m_nChannel - 64;
                header[1] = tmp & 0xff;
                if (cSize == 2)
                    header[2] = tmp >> 8;
            }
        }
    }
    if (tbuf) // http  one post
    {
        u32 wrote;
        nRet = m_pNetHandler->SendData(tbuf, toff-tbuf, wrote, bNeedBuf);
        free(tbuf);
        tbuf = NULL;
    }
    
    /* we invoked a remote method */
    if (packet->m_packetType == RTMP_PACKET_TYPE_INVOKE)
    {
        AVal method;
        char *ptr;
        ptr = packet->m_body + 1;
        AMF_DecodeString(ptr, &method);
        PosaPrintf(TRUE, FALSE, "Invoking %s", method.av_val);
        /* keep it in call queue till result arrives */
        if (queue) 
        {
            int txn;
            ptr += 3 + method.av_len;
            txn = (int)AMF_DecodeNumber(ptr);
            AV_queue(&m_methodCalls, &m_numCalls, &method, txn);
        }
    }
    
    if (!m_vecChannelsOut[packet->m_nChannel])
        m_vecChannelsOut[packet->m_nChannel] = (RTMPPacket*)malloc(sizeof(RTMPPacket));
    memcpy(m_vecChannelsOut[packet->m_nChannel], packet, sizeof(RTMPPacket));
    return nRet;
}

int CPosaRtmpProtocol::SendConnectPacket()
{
    RTMPPacket packet;
    char pbuf[4096], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    
    packet.m_nChannel = 0x03;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_INVOKE;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeString(enc, pend, &av_connect);
    enc = AMF_EncodeNumber(enc, pend, ++m_numInvokes);
    *enc++ = AMF_OBJECT;
    
    AVal avApp;
    avApp.av_val = (char*)m_strApp.c_str();
    avApp.av_len = m_strApp.length();
    enc = AMF_EncodeNamedString(enc, pend, &av_app, &avApp);
    if (!enc)
        return FALSE;
    
    if (m_dwProtocol & RTMP_FEATURE_WRITE)
    {
        enc = AMF_EncodeNamedString(enc, pend, &av_type, &av_nonprivate);
        if (!enc)
            return FALSE;
    }
    /*if (r->Link.flashVer.av_len)
    {
    enc = AMF_EncodeNamedString(enc, pend, &av_flashVer, &r->Link.flashVer);
    if (!enc)
    return FALSE;
    }
    if (r->Link.swfUrl.av_len)
    {
    enc = AMF_EncodeNamedString(enc, pend, &av_swfUrl, &r->Link.swfUrl);
    if (!enc)
    return FALSE;
}*/    
    AVal avTemp;
    avTemp.av_val = (char*)m_strUrl.c_str();
    avTemp.av_len = m_strUrl.length();
    enc = AMF_EncodeNamedString(enc, pend, &av_tcUrl, &avTemp);
    if (!enc)
        return FALSE;
    
    if (!(m_dwProtocol & RTMP_FEATURE_WRITE))
    {
        enc = AMF_EncodeNamedBoolean(enc, pend, &av_fpad, FALSE);
        if (!enc)
            return FALSE;
        enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 40.0);//15.0);
        if (!enc)
            return FALSE;
        enc = AMF_EncodeNamedNumber(enc, pend, &av_audioCodecs, m_fAudioCodecs);
        if (!enc)
            return FALSE;
        enc = AMF_EncodeNamedNumber(enc, pend, &av_videoCodecs, m_fVideoCodecs);
        if (!enc)
            return FALSE;
        enc = AMF_EncodeNamedNumber(enc, pend, &av_videoFunction, 1.0);
        if (!enc)
            return FALSE;
            /*if (r->Link.pageUrl.av_len)
            {
            enc = AMF_EncodeNamedString(enc, pend, &av_pageUrl, &r->Link.pageUrl);
            if (!enc)
            return FALSE;
    }*/
    }
    /*if (m_fEncoding != 0.0 || m_bSendEncoding)
    {	// AMF0, AMF3 not fully supported yet 
        enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, r->m_fEncoding);
        if (!enc)
            return FALSE;
    }*/
    if (enc + 3 >= pend)
        return FALSE;
    *enc++ = 0;
    *enc++ = 0;			/* end of object - 0x00 0x00 0x09 */
    *enc++ = AMF_OBJECT_END;
    
    /* add auth string */
    /*if (r->Link.auth.av_len)
    {
    enc = AMF_EncodeBoolean(enc, pend, r->Link.lFlags & RTMP_LF_AUTH);
    if (!enc)
    return FALSE;
    enc = AMF_EncodeString(enc, pend, &r->Link.auth);
    if (!enc)
    return FALSE;
    }*/
    
    packet.m_nBodySize = enc - packet.m_body;
    
    return SendPacket(&packet, TRUE);
}

int CPosaRtmpProtocol::SendH264StreamMetadata()
{       
    int width = 0,height = 0;
    H264_Decode_Sps(m_pMetaData->Sps, m_pMetaData->nSpsLen, width, height);
	m_pMetaData->nWidth = width;
    m_pMetaData->nHeight = height;
	m_pMetaData->nFrameRate = 25;    
    
    {
        RTMPPacket packet = {0};
        char pbuf[4096], *pend = pbuf + sizeof(pbuf);
        char *enc;
        
        packet.m_nChannel = 4;
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = RTMP_PACKET_TYPE_INFO;
        packet.m_nTimeStamp = 0;
        packet.m_nInfoField2 = m_stream_id;
        packet.m_hasAbsTimestamp = 0;
        packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
        
        enc = packet.m_body;      
        
        
        enc = AMF_EncodeString(enc, pend, &av_onMetaData);    
        
        *enc++ = AMF_OBJECT;        
        
        enc = AMF_EncodeNamedNumber(enc, pend, &av_width, m_pMetaData->nWidth);
        if (!enc)
            return FALSE;
        
        enc = AMF_EncodeNamedNumber(enc, pend, &av_height, m_pMetaData->nHeight);
        if (!enc)
            return FALSE;
        
        enc = AMF_EncodeNamedNumber(enc, pend, &av_framerate, m_pMetaData->nFrameRate);
        if (!enc)
            return FALSE;
        
        enc = AMF_EncodeNamedNumber(enc, pend, &av_videocodecid, 7);    
        if (!enc)
            return FALSE;
        
            /*tTemp.av_len = strlen("wangxiaohui");
        tTemp.av_val = ;*/
        enc = AMF_EncodeNamedStringEx(enc, pend, &av_copyright, "wangxiaohui");
        if (!enc)
            return FALSE;
        
        if (enc + 3 >= pend)
            return FALSE;
        
        *enc++ = 0;
        *enc++ = 0;			/* end of object - 0x00 0x00 0x09 */
        *enc++ = AMF_OBJECT_END;       
        
        
        packet.m_nBodySize = enc - packet.m_body;            
        SendPacket(&packet, FALSE);                        
    }
    //    return 0;
    
    //////////////////////////////////////////////////////////////////////////    
    {
        RTMPPacket packet = {0};
        char pbuf[4096], *pend = pbuf + sizeof(pbuf);
        char *enc;
        
        packet.m_nChannel = 0x04;	/* control channel (invoke) */
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;
        packet.m_nTimeStamp = 0;
        packet.m_nInfoField2 = m_stream_id;
        packet.m_hasAbsTimestamp = 0;
        packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
        
        enc = packet.m_body;      
        
        *enc++ = 0x17; // 1:keyframe  7:AVC
        *enc++ = 0x00; // AVC sequence header
        
        *enc++ = 0x00;
        *enc++ = 0x00;
        *enc++ = 0x00; // fill in 0;
        
        // AVCDecoderConfigurationRecord.
        *enc++ = 0x01; // configurationVersion
        *enc++ = m_pMetaData->Sps[1]; // AVCProfileIndication
        *enc++ = m_pMetaData->Sps[2]; // profile_compatibility
        *enc++ = m_pMetaData->Sps[3]; // AVCLevelIndication 
        *enc++ = (char)0xff; // lengthSizeMinusOne  
        
        // sps nums
        *enc++ = (char)0xE1; //&0x1f
        // sps data length
        *enc++ = m_pMetaData->nSpsLen>>8;
        *enc++ = m_pMetaData->nSpsLen&0xff;
        // sps data
        memcpy(enc,m_pMetaData->Sps,m_pMetaData->nSpsLen);
        enc += m_pMetaData->nSpsLen;
        
        // pps nums
        *enc++ = 0x01; //&0x1f
        // pps data length 
        *enc++ = m_pMetaData->nPpsLen>>8;
        *enc++ = m_pMetaData->nPpsLen&0xff;
        // sps data
        memcpy(enc,m_pMetaData->Pps,m_pMetaData->nPpsLen);
        enc += m_pMetaData->nPpsLen;
        
        packet.m_nBodySize = enc - packet.m_body;            
        SendPacket(&packet, FALSE);                        
    }
    return 0;
}


int CPosaRtmpProtocol::SendAmrFrame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp)
{
	if (FALSE == m_bPlaying)
	{
		::PosaPrintf(TRUE, FALSE, "what r u doing, the rtmp connect is not finished!");
		return -1;
	}

	if (RTMP_FEATURE_WRITE != (RTMP_FEATURE_WRITE & m_dwProtocol))
	{
		::PosaPrintf(TRUE, FALSE, "cannot send amr, because u r playing not publish!");
		return -1;
	}

	if (FALSE == this->m_bAmrMetaSended)
	{
		//sended amr metadata
		m_bAmrMetaSended = TRUE;
	}


	u8* body = NULL;

	RTMPPacket packet = { 0 };
	RTMPPacket_Reset(&packet);
	RTMPPacket_Alloc(&packet, dwLen + 1);


	packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
	packet.m_nChannel = 0x04;
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_nTimeStamp = dwTimeStamp;
	packet.m_nInfoField2 = m_stream_id;
	packet.m_hasAbsTimestamp = 1;
	packet.m_nBodySize = dwLen + 1;

	body = (u8*) packet.m_body;

	int i = 0;
	body[i++] = 0x00;
	memcpy(&body[i], pBuffer, dwLen);

	int nRet = SendPacket(&packet, FALSE, FALSE);
	RTMPPacket_Free(&packet);

	return nRet;
}

int CPosaRtmpProtocol::SendWbFrame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp)
{
	if (FALSE == m_bPlaying)
	{
		::PosaPrintf(TRUE, FALSE, "what r u doing, the rtmp connect is not finished!");
		return -1;
	}
	if (RTMP_FEATURE_WRITE != (RTMP_FEATURE_WRITE & m_dwProtocol))
	{
		::PosaPrintf(TRUE, FALSE, "cannot send data, because u r playing not publish!");
		return -1;
	}
	RTMPPacket packet = {0};
	RTMPPacket_Reset(&packet);
	RTMPPacket_Alloc(&packet, dwLen);

	packet.m_packetType = 18;
	packet.m_nChannel = 0x04;
	packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet.m_nTimeStamp = dwTimeStamp;
	packet.m_nInfoField2 = m_stream_id;
	packet.m_hasAbsTimestamp = 1;
	packet.m_nBodySize = dwLen;
	memcpy(packet.m_body, pBuffer, dwLen);

	int nRet = SendPacket(&packet, FALSE, FALSE);
	RTMPPacket_Free(&packet);
	return nRet;
}

int CPosaRtmpProtocol::SendH264Frame(u8* pBuffer, u32 dwLen, u32 dwTimeStamp)
{       
	if (FALSE == m_bPlaying)
	{
		::PosaPrintf(TRUE, FALSE, "what r u doing, the rtmp connect is not finished!");
		return -1;
	}
    int nRet = -1;
    if (FALSE == m_bSpsSended) 
    {
        NaluUnit nal;
        if (H264_FindSpsPpsInfo(pBuffer, dwLen, KDM_MEDIA_H264_NAL_SPS, nal))
        {
            m_bSpsSended = TRUE;
            m_pMetaData->nSpsLen = nal.size;
            memcpy(m_pMetaData->Sps, nal.data, nal.size);
        }
    }
    
    if (FALSE == m_bPpsSended)
    {
        NaluUnit nal;
        if (H264_FindSpsPpsInfo(pBuffer, dwLen, KDM_MEDIA_H264_NAL_PPS, nal))
        {
            m_bPpsSended = TRUE;
            m_pMetaData->nPpsLen = nal.size;
            memcpy(m_pMetaData->Pps, nal.data, nal.size);
        }
    }
    
    if (FALSE == m_bH264MetaSended && TRUE == m_bSpsSended && TRUE == m_bPpsSended)
    {
        
        SendChangeChunkSize(4096);
        this->m_outChunkSize = 4096;
        

        // send meta data        
        nRet = this->SendH264StreamMetadata();
        m_bH264MetaSended = TRUE;
        
    }    
    //return 0;
    if (m_bSpsSended && m_bPpsSended) 
    {         
        BOOL32 bKeyFrame = H264_IsKeyFrame(pBuffer, dwLen);
        
        u8* body = NULL;//new u8[dwLen+5];                

        RTMPPacket packet = {0};	
        RTMPPacket_Reset(&packet);
        RTMPPacket_Alloc(&packet, dwLen+5);
        
        packet.m_packetType = RTMP_PACKET_TYPE_VIDEO;
        packet.m_nChannel = 0x04;  
        packet.m_headerType = RTMP_PACKET_SIZE_LARGE;  
        packet.m_nTimeStamp = dwTimeStamp;
        packet.m_nInfoField2 = m_stream_id;	
        packet.m_hasAbsTimestamp = 1;
        packet.m_nBodySize = dwLen+5;
        
        body = (u8*)packet.m_body;

        //memcpy(packet.m_body, pBuffer, dwLen);
        int i = 0;
        if(bKeyFrame)
        {
            body[i++] = 0x17;// 1:Iframe  7:AVC
        }
        else
        {
            body[i++] = 0x27;// 2:Pframe  7:AVC
        }
        body[i++] = 0x01;// AVC NALU
        body[i++] = 0x00;
        body[i++] = 0x00;
        body[i++] = 0x00;
        memcpy(&body[i], pBuffer, dwLen);
        
        H264_ReEncodeFrame(&body[i], dwLen);
        
        int nRet = SendPacket(&packet, FALSE, FALSE);
        RTMPPacket_Free(&packet);     
        
        //nRet = SendCommonPacket(RTMP_PACKET_TYPE_VIDEO, (char*)body, dwLen+5, dwTimeStamp);
        //delete [] body;
        //body = NULL;
    }
	if (FALSE == m_bSpsSended || FALSE == m_bPpsSended)
	{
		printf("fa ge dan a!!! m_bsps %d m_bpps %d\r\n", m_bSpsSended, m_bPpsSended);
	}	
    return 0;
    return nRet;    
}

int CPosaRtmpProtocol::SendAcknowledgement()
{
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    
    packet.m_nChannel = 0x02;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_BYTES_READ_REPORT;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    packet.m_nBodySize = 4;
    
    AMF_EncodeInt32(packet.m_body, pend, m_dwReadedData);
    return SendPacket(&packet, FALSE);
}

int CPosaRtmpProtocol::SendChangeChunkSize(int nChunkSize)
{
    // send change chunk size
    RTMPPacket packet;
    char pbuf[256], *pend = pbuf + sizeof(pbuf);
    char *enc;
    
    packet.m_nChannel = 0x02;	/* control channel (invoke) */
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_CHUNK_SIZE;
    packet.m_nTimeStamp = 0;	/* RTMP_GetTime(); */
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;
    
    enc = packet.m_body;
    enc = AMF_EncodeInt32(enc, pend, nChunkSize);
    //*enc++ = AMF_NULL;        
    packet.m_nBodySize = enc - packet.m_body;        
    return SendPacket(&packet, FALSE);
}


BOOL32 CPosaRtmpProtocol::PerformHandshake(CPosaBuffer& buffer)
{
	PosaPrintf(TRUE, FALSE, "[CRTMPProtocol][PerformHandshake] state:%d\n", _rtmpState);
	switch (_rtmpState) {
	case RTMP_STATE_NOT_INITIALIZED:
		{
			if (buffer.GetAvaliableBytesCount() < 1537) {
				return TRUE;
			}
			u8 handshakeType = buffer.GetIBPointer()[0];
			if (!buffer.Ignore(1)) 
            {
				printf("Unable to ignore one byte");
				return FALSE;
			}
			
			_currentFPVersion = ntohl((*buffer.GetIBPointer() + 4) );
			
			switch (handshakeType) 
            {
			case 3: //plain
				{
					return PerformHandshake(buffer, FALSE);
				}
			case 6: //encrypted
				{
					return PerformHandshake(buffer, TRUE);
				}
			default:
				{
					printf("Handshake type not implemented: %hhu", handshakeType);
					return FALSE;
				}
			}
		}
	case RTMP_STATE_SERVER_RESPONSE_SENT:
		{
			if (buffer.GetAvaliableBytesCount() < 1536) 
            {
				return FALSE;
			} 
            else 
            {
				//ignore the client's last handshake part
				if (!buffer.Ignore(1536)) 
                {
					printf("Unable to ignore inbound data");
					return FALSE;
				}
				m_handshakeCompleted = true;
				_rtmpState = RTMP_STATE_DONE;	
				
				/*if (_pKeyIn != NULL && _pKeyOut != NULL) {
					//insert the RTMPE protocol in the current protocol stack
					BaseProtocol *pFarProtocol = GetFarProtocol();
					RTMPEProtocol *pRTMPE = new RTMPEProtocol(_pKeyIn, _pKeyOut);
					ResetFarProtocol();
					pFarProtocol->SetNearProtocol(pRTMPE);
					pRTMPE->SetNearProtocol(this);
					DEBUGLOG("New protocol chain: %s", STR(*pFarProtocol));
					
					
					//decrypt the leftovers
					RC4(_pKeyIn, buffer.Length(), (u8*)GETIBPOINTER(buffer), (u8*)GETIBPOINTER(buffer));
				}*/
				return TRUE;
			}
		}
	default:
		{
			printf("Invalid RTMP state: %hhu", _rtmpState);
			return FALSE;
		}
	}
}

BOOL32 CPosaRtmpProtocol::PerformHandshake(CPosaBuffer& buffer, BOOL32 encrypted) 
{
	printf("[CRTMPProtocol][PerformHandshake] encrypted:%d\n", encrypted);
	if (!ValidateClient(buffer)) 
	{
		if (encrypted) 
		{
			printf("Unable to validate client");
			return FALSE;
		} else {
			printf("Client not validated");
			_validationScheme = 0;
		}
	}
	
	//get the buffers
	u8 *pInputBuffer = buffer.GetIBPointer();
	if (_pOutputBuffer == NULL) 
    {
		_pOutputBuffer = new u8[3072];
	}
    else 
    {
		delete[] _pOutputBuffer;
		_pOutputBuffer = new u8[3072];
	}

	//timestamp
	//EHTONLP(_pOutputBuffer, (u32) time(NULL));
    u32 uTime = (u32) time(NULL);
    u32 u1Time = htonl(uTime);
    memcpy(_pOutputBuffer, &u1Time, 4);

	//version
	memset(_pOutputBuffer + 4, 4, 0);

	//generate random data
	for (u32 i = 8; i < 3072; i++) 
    {
		_pOutputBuffer[i] = rand() % 256;
	}
	/*for (u32 i = 0; i < 10; i++) {
		u32 index = rand() % (3072 - HTTP_HEADERS_SERVER_US_LEN);
		memcpy(_pOutputBuffer + index, HTTP_HEADERS_SERVER_US, HTTP_HEADERS_SERVER_US_LEN);
	}*/
	

	// FIRST 1536 bytes from server response
	//compute DH key position
	// u32 serverDHOffset = GetDHOffset(_pOutputBuffer, _validationScheme);
	// u32 clientDHOffset = GetDHOffset(pInputBuffer, _validationScheme);

	//generate DH key
	/*DHWrapper dhWrapper(1024);

	if (!dhWrapper.Initialize()) {
		FATALLOG("Unable to initialize DH wrapper");
		return FALSE;
	}

	if (!dhWrapper.CreateSharedKey(pInputBuffer + clientDHOffset, 128)) {
		FATALLOG("Unable to create shared key");
		return FALSE;
	}

	if (!dhWrapper.CopyPublicKey(_pOutputBuffer + serverDHOffset, 128)) {
		FATALLOG("Couldn't write public key!");
		return FALSE;
	}

	if (encrypted) {
		u8 secretKey[128];
		if (!dhWrapper.CopySharedKey(secretKey, sizeof (secretKey))) {
			FATALLOG("Unable to copy shared key");
			return FALSE;
		}

		_pKeyIn = new RC4_KEY;
		_pKeyOut = new RC4_KEY;
		InitRC4Encryption(
				secretKey,
				(u8*) & pInputBuffer[clientDHOffset],
				(u8*) & _pOutputBuffer[serverDHOffset],
				_pKeyIn,
				_pKeyOut);

		//bring the keys to correct cursor
		u8 data[1536];
		RC4(_pKeyIn, 1536, data, data);
		RC4(_pKeyOut, 1536, data, data);
	}*/

	//generate the digest
	u32 serverDigestOffset = GetDigestOffset(_pOutputBuffer, _validationScheme);

	u8 *pTempBuffer = new u8[1536 - 32];
	memcpy(pTempBuffer, _pOutputBuffer, serverDigestOffset);
	memcpy(pTempBuffer + serverDigestOffset, _pOutputBuffer + serverDigestOffset + 32,
			1536 - serverDigestOffset - 32);

	u8 *pTempHash = new u8[512];
	HMACsha256(pTempBuffer, 1536 - 32, GenuineFMSKey, 36, pTempHash);

	//put the digest in place
	memcpy(_pOutputBuffer + serverDigestOffset, pTempHash, 32);

	//cleanup
	delete[] pTempBuffer;
	delete[] pTempHash;

	// SECOND 1536 bytes from server response 
	//Compute the chalange index from the initial client request
	u32 keyChallengeIndex = GetDigestOffset(pInputBuffer, _validationScheme);

	//compute the key
	pTempHash = new u8[512];
	HMACsha256(pInputBuffer + keyChallengeIndex, //pData
			32, //dataLength
			GenuineFMSKey, //key
			68, //keyLength
			pTempHash //pResult
			);

	
	//generate the hash
	u8 *pLastHash = new u8[512];
	HMACsha256(_pOutputBuffer + 1536, //pData
			1536 - 32, //dataLength
			pTempHash, //key
			32, //keyLength
			pLastHash //pResult
			);

	//put the hash where it belongs
	memcpy(_pOutputBuffer + 1536 * 2 - 32, pLastHash, 32);


	//cleanup
	delete[] pTempHash;
	delete[] pLastHash;
	// DONE BUILDING THE RESPONSE 

	//wire the response
	/*if (encrypted)
		_outputBuffer.Write(6);
	else
		_outputBuffer.Write(3);
	_outputBuffer.Write(_pOutputBuffer, 3072);*/

	//final cleanup
	
	
	//SendOutMessage();		
    u32 dwSended;
    char sztype = 3;
    m_pNetHandler->SendData(&sztype, 1, dwSended);
    m_pNetHandler->SendData((char*)_pOutputBuffer, 3072, dwSended);

    delete[] _pOutputBuffer;
	_pOutputBuffer = NULL;	

	//move to the next stage in the handshake
	_rtmpState = RTMP_STATE_SERVER_RESPONSE_SENT;
    if (!buffer.IgnoreAll()) 
    {
		printf("Unable to ignore input buffer");
		return FALSE;
	}
	return TRUE;
}

BOOL32 CPosaRtmpProtocol::ValidateClient(CPosaBuffer& inputBuffer) 
{
	if (_currentFPVersion == 0) 
    {
		printf("This version of player doesn't support validation");
		return true;
	}
	if (ValidateClientScheme(inputBuffer, 0)) {
		_validationScheme = 0;
		return true;
	}
	if (ValidateClientScheme(inputBuffer, 1)) {
		_validationScheme = 1;
		return true;
	}
	printf("Unable to validate client");
	return false;
}

BOOL32 CPosaRtmpProtocol::ValidateClientScheme(CPosaBuffer&inputBuffer, u8 scheme)
{
	u8*pBuffer = inputBuffer.GetIBPointer();
	
	u32 clientDigestOffset = GetDigestOffset(pBuffer, scheme);
	
	u8 *pTempBuffer = new u8[1536 - 32];
	memcpy(pTempBuffer, pBuffer, clientDigestOffset);
	memcpy(pTempBuffer + clientDigestOffset, pBuffer + clientDigestOffset + 32,
		1536 - clientDigestOffset - 32);
	
	u8 *pTempHash = new u8[512];
	HMACsha256(pTempBuffer, 1536 - 32, GenuineFPKey, 30, pTempHash);
	
	bool result = true;
	for (u32 i = 0; i < 32; i++) {
		if (pBuffer[clientDigestOffset + i] != pTempHash[i]) {
			result = false;
			break;
		}
	}
	
	delete[] pTempBuffer;
	delete[] pTempHash;	
	return result;
}

u32 CPosaRtmpProtocol::GetDigestOffset(u8 *pBuffer, u8 schemeNumber) 
{
	switch (schemeNumber) {
	case 0:
		{
			return GetDigestOffset0(pBuffer);
		}
	case 1:
		{
			return GetDigestOffset1(pBuffer);
		}
	default:
		{
			printf("Invalid scheme number: %hhu. Defaulting to 0", schemeNumber);
			return GetDigestOffset0(pBuffer);
		}
	}
}

u32 CPosaRtmpProtocol::GetDigestOffset0(u8 *pBuffer) 
{
	u32 offset = pBuffer[8] + pBuffer[9] + pBuffer[10] + pBuffer[11];
	offset = offset % 728;
	offset = offset + 12;
	if (offset + 32 >= 1536) 
    {
		printf("Invalid digest offset");
	}
	return offset;
}

u32 CPosaRtmpProtocol::GetDigestOffset1(u8 *pBuffer) 
{
	u32 offset = pBuffer[772] + pBuffer[773] + pBuffer[774] + pBuffer[775];
	offset = offset % 728;
	offset = offset + 776;
	if (offset + 32 >= 1536) 
    {
		printf("Invalid digest offset");
	}
	return offset;
}
