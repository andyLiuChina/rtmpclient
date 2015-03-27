#include "posartmpclient.h"
#include "posamediacommon.h"
#include "posartmpserver.h"

#define DISCONNECT 100

bool setMaxFdCount(u32 &current, u32 &max) {
	struct rlimit limits;

	//2. get the current value
	if (getrlimit(RLIMIT_NOFILE, &limits) != 0) {
		int err = errno;
		printf("getrlimit failed: (%d) %s", err, strerror(err));
		return false;
	}

	//3. Set the current value to max value
	limits.rlim_cur = limits.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &limits) != 0) {
		int err = errno;
		printf("setrlimit failed: (%d) %s", err, strerror(err));
		return false;
	}

	//4. Try to get it back
	if (getrlimit(RLIMIT_NOFILE, &limits) != 0) {
		int err = errno;
		printf("getrlimit failed: (%d) %s", err, strerror(err));
		return false;
	}
	current = (u32) limits.rlim_cur;
	max = (u32) limits.rlim_max;

	return true;
}

int myfunRtmpClientCallBack(u32 nMsgType, u32 dwTimeStamp, const u8* szBuffer, u32 dwBufLen, void* dwUserData)
{
	CPosaRtmpClient* pClient = (CPosaRtmpClient*)dwUserData;

	if (em_RtmpClient_Connecct_Ok == nMsgType)
	{
		printf("********************get em_RtmpClient_Connecct_Ok\r\n");
	}
	else if (em_RtmpClient_Connecct_Error == nMsgType)
	{
		printf("********************get em_RtmpClient_Connecct_Error\r\n");
		pClient->Close();
	}
	else if (em_RtmpClient_Socket_Error == nMsgType)
	{
		printf("********************get em_RtmpClient_Socket_Error\r\n");
		pClient->Close();
	}
	else if (em_RtmpClient_Play_Ok == nMsgType)
	{
		printf("********************get em_RtmpClient_Play_Ok\r\n");
	}
	else if (em_RtmpClient_Publish_Ok == nMsgType)
	{
		printf("********************get em_RtmpClient_Publish_Ok\r\n");
	}
	else if (em_RtmpClient_Play_Over == nMsgType)
	{
		printf("********************get em_RtmpClient_Play_Over\r\n");
		//pClient->Close();
		//pClient->SetupURL(g_pProactor, "rtmp://192.168.0.77/live/tcr_teacher", FALSE);
	}
	else if (em_RtmpClient_Play_Error == nMsgType)
	{
		printf("********************get em_RtmpClient_Play_Error\r\n");
		pClient->Close();
	}
	else if (em_RtmpClient_Publish_Error == nMsgType)
	{
		printf("********************get em_RtmpClient_Publish_Error\r\n");
		//pClient->Close();
	}
	else if (em_RtmpClient_Msg_Data == nMsgType)
	{
		printf("********************msg data\r\n");
		FILE* pFile = fopen("./test.txt", "ab+");
		if (pFile)
		{
			fwrite(szBuffer, 1, dwBufLen, pFile);
			fclose(pFile);
		}
	}
	else if (em_RtmpClient_Audio_Data == nMsgType)
	{
		printf("********************audio data\r\n");
	}
	return 0;
}

CPosaNetProactor* g_pProactor;
int main( int argc, char* argv[])
{
    if (argc < 5)
    {
        ::printf("hi man, u should start like : \r\n"
                  "*.exe  rtmp://127.0.0.1/vod/1080.mp4 play | publish numofconnection audioSourceFile");
        return 0;
    }
    KdmPosaStartup();    
    KdmPosaCreateProactor(&g_pProactor);

	const int PLAYNUM = atoi(argv[3]);
    CPosaRtmpClient* cClient = new CPosaRtmpClient[PLAYNUM];
    size_t i;
	for(i = 0; i < PLAYNUM; i++)
    	cClient[i].SetCallBack((funRtmpClientCallBack)myfunRtmpClientCallBack, &cClient[i]);
    BOOL32 bPublish = TRUE;
    if (0 == strcasecmp(argv[2], "play"))
    {
        bPublish = FALSE;
    }

	int res;
	for(i = 0; i < PLAYNUM; i++) {
		res = cClient[i].SetupURL(g_pProactor, argv[1], bPublish);
		printf("Set %s URL return value: %d\n", argv[2], res);
	}

    if (bPublish)
    {
    	sleep(5);
        printf("go go go , we start send stream\r\n");

        //send h264 stream
        unsigned char* pBuffer = new  unsigned char[1*1024*1024];

        FILE* pAmr = ::fopen(argv[4], "rb");
        if (NULL == pAmr)
        {
        	perror("sb le , file can not open");//
        }
        fseek(pAmr, 6, SEEK_SET);

        int x = 0;
        
        while (1)
		{
			int nRead = fread(pBuffer, 1, 32, pAmr);
            printf("read %d bytes \r\n", nRead);
			if (nRead != 32)
			{
				printf("nima read error break  %d times\r\n", x);
				fseek(pAmr, 6, SEEK_SET);
				continue;
			}
			//send audio data
			x += 20;
			cClient[0].SendAmrFrame(pBuffer, 32, x);

			usleep(20000);
		}
		printf("ok it is over\r\n");
        if(pAmr)
        {
        	fclose(pAmr);
        	pAmr = NULL;
        }

        delete [] pBuffer;
    }
#if 0    
	int discon[DISCONNECT] = {0};
	while(1) {
		for(i = 0; i < DISCONNECT; i++) {
			res = rand() % PLAYNUM;
			discon[i] = res;
			cClient[res].Close();
			printf("The index of %d client closed\n", res);
			sleep(1);
		}

		sleep(60);
		for(i = 0; i < DISCONNECT; i++) {
			res = cClient[i].SetupURL(g_pProactor, argv[1], bPublish);
			printf("Reset %s URL return value: %d\n", argv[2], res);
        }
		sleep(60);
	}
#endif
    sleep(99999999);
    KdmPosaProactorClose(&g_pProactor);
    KdmPosaCleanup();

	delete [] cClient;
    return 0;
}
