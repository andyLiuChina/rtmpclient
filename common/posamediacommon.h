/*
    this file is created by wxh wangxiaohui 2014-3-13
*/

#ifndef _KDM_MEDIA_COMMON_H_
#define _KDM_MEDIA_COMMON_H_

#ifndef KDM_MEDIA_H264_NAL_SPS
#define KDM_MEDIA_H264_NAL_SPS  0x7
#endif

#ifndef KDM_MEDIA_H264_NAL_PPS
#define KDM_MEDIA_H264_NAL_PPS  0x8
#endif

#ifndef KDM_MEDIA_H264_NAL_KEYFRAME
#define KDM_MEDIA_H264_NAL_KEYFRAME 0x5
#endif

typedef struct NaluUnit
{
	int type;
	int size;
	unsigned char *data;
}NaluUnit;

int H264_IsKeyFrame(unsigned char* pNalu, int nLen);
int H264_FindSpsPpsInfo(unsigned char* pNaldata, int size, int nType, NaluUnit& nal);
void H264_RestoreFrame(unsigned char* pNalu, int nLen);
void H264_ReEncodeFrame(unsigned char* pNaldata, int size);
int H264_Decode_Sps(unsigned char * buf,unsigned int nLen,int &width,int &height);


int H264_IsKeyFrame(unsigned char* pNalu, int nLen)
{
    unsigned char szType = pNalu[4] & 0x1F;
    if (KDM_MEDIA_H264_NAL_SPS == szType || 
        KDM_MEDIA_H264_NAL_PPS == szType || 
        KDM_MEDIA_H264_NAL_KEYFRAME == szType)
    {
        return TRUE;
    }
    return FALSE;
}

void H264_RestoreFrame(unsigned char* pNalu, int nLen)
{
    char szH264Header[4] = {0, 0, 0, 1};
    unsigned char* pNaluStart = pNalu;
    while (nLen > 0) 
    {
        int nNaluLen = htonl(*((unsigned int*)pNaluStart));
        memcpy(pNaluStart, szH264Header, 4);
        pNaluStart += nNaluLen + 4;
        nLen -= nNaluLen + 4;
    }
}

int H264_FindSpsPpsInfo(unsigned char* pNaldata, int size, int nType, NaluUnit& nal)
{
    unsigned char* pBuffer = pNaldata;
    unsigned char* pStart = pNaldata;
    for(int inx = 4; inx < size-4; inx++)
    {
        if (pBuffer[inx+0] == 0x00 &&
            pBuffer[inx+1] == 0x00 &&
            pBuffer[inx+2] == 0x00 &&
            pBuffer[inx+3] == 0x01)
        {
            int nLen = pBuffer + inx - pStart;            
            nal.size = nLen -4;
            nal.data = (pStart+4);
            nal.type = pStart[4] & 0x1f;
            if (nType == nal.type)
            {
                return TRUE;
            }

            pStart = &pBuffer[inx];
        }
    }
    int nLen = pNaldata + size - pStart;    
    nal.size = nLen -4;
    nal.data = (pStart+4);
    nal.type = pStart[4] & 0x1F;
    if (nType == nal.type)
    {
        return TRUE;
    }
    return FALSE;
}

void H264_ReEncodeFrame(unsigned char* pNaldata, int size)
{
    // find 00 00 00 01 and change to nal unit length
    unsigned char* pBuffer = pNaldata;
    unsigned char* pStart = pNaldata;
    for(int inx = 4; inx < size-4; inx++)
    {
        if (pBuffer[inx+0] == 0x00 &&
            pBuffer[inx+1] == 0x00 &&
            pBuffer[inx+2] == 0x00 &&
            pBuffer[inx+3] == 0x01)
        {
            int nLen = pBuffer + inx - pStart;            
            int nTemp = htonl(nLen-4);
            memcpy(pStart, &nTemp, 4);
            pStart = &pBuffer[inx];
        }
    }
    int nLen = pNaldata + size - pStart;    
    int nTemp = htonl(nLen-4);
    memcpy(pStart, &nTemp, 4);
}

#include <math.h>

static unsigned int Ue(unsigned char *pBuff, unsigned int nLen, unsigned int &nStartBit)
{
	//??0bit???
	unsigned int nZeroNum = 0;
	while (nStartBit < nLen * 8)
	{
		if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8))) //&:???,%??
		{
			break;
		}
		nZeroNum++;
		nStartBit++;
	}
	nStartBit ++;


	//????
	unsigned int dwRet = 0;
	for (unsigned int i=0; i<nZeroNum; i++)
	{
		dwRet <<= 1;
		if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
		{
			dwRet += 1;
		}
		nStartBit++;
	}
	return (1 << nZeroNum) - 1 + dwRet;
}


static int Se(unsigned char *pBuff, unsigned int nLen, unsigned int &nStartBit)
{
	int UeVal=Ue(pBuff,nLen,nStartBit);
	double k=UeVal;
	int nValue=ceil(k/2);//ceil??:ceil????????????????????ceil(2)=ceil(1.2)=cei(1.5)=2.00
	if (UeVal % 2==0)
		nValue=-nValue;
	return nValue;
}


static unsigned int u(unsigned int BitCount,unsigned char * buf,unsigned int &nStartBit)
{
	unsigned int dwRet = 0;
	for (unsigned int i=0; i<BitCount; i++)
	{
		dwRet <<= 1;
		if (buf[nStartBit / 8] & (0x80 >> (nStartBit % 8)))
		{
			dwRet += 1;
		}
		nStartBit++;
	}
	return dwRet;
}


int H264_Decode_Sps(unsigned char * buf,unsigned int nLen,int &width,int &height)
{
	unsigned int StartBit=0; 
	int forbidden_zero_bit=u(1,buf,StartBit);
	int nal_ref_idc=u(2,buf,StartBit);
	int nal_unit_type=u(5,buf,StartBit);
	if(nal_unit_type==7)
	{
		int profile_idc=u(8,buf,StartBit);
		int constraint_set0_flag=u(1,buf,StartBit);//(buf[1] & 0x80)>>7;
		int constraint_set1_flag=u(1,buf,StartBit);//(buf[1] & 0x40)>>6;
		int constraint_set2_flag=u(1,buf,StartBit);//(buf[1] & 0x20)>>5;
		int constraint_set3_flag=u(1,buf,StartBit);//(buf[1] & 0x10)>>4;
		int reserved_zero_4bits=u(4,buf,StartBit);
		int level_idc=u(8,buf,StartBit);

		int seq_parameter_set_id=Ue(buf,nLen,StartBit);

		if( profile_idc == 100 || profile_idc == 110 ||
			profile_idc == 122 || profile_idc == 144 )
		{
			int chroma_format_idc=Ue(buf,nLen,StartBit);
			if( chroma_format_idc == 3 )
				int residual_colour_transform_flag=u(1,buf,StartBit);
			int bit_depth_luma_minus8=Ue(buf,nLen,StartBit);
			int bit_depth_chroma_minus8=Ue(buf,nLen,StartBit);
			int qpprime_y_zero_transform_bypass_flag=u(1,buf,StartBit);
			int seq_scaling_matrix_present_flag=u(1,buf,StartBit);

			int seq_scaling_list_present_flag[8];
			if( seq_scaling_matrix_present_flag )
			{
				for( int i = 0; i < 8; i++ ) 
                {
					seq_scaling_list_present_flag[i]=u(1,buf,StartBit);
				}
			}
		}
		int log2_max_frame_num_minus4=Ue(buf,nLen,StartBit);
		int pic_order_cnt_type=Ue(buf,nLen,StartBit);
		if( pic_order_cnt_type == 0 )
			int log2_max_pic_order_cnt_lsb_minus4=Ue(buf,nLen,StartBit);
		else if( pic_order_cnt_type == 1 )
		{
			int delta_pic_order_always_zero_flag=u(1,buf,StartBit);
			int offset_for_non_ref_pic=Se(buf,nLen,StartBit);
			int offset_for_top_to_bottom_field=Se(buf,nLen,StartBit);
			int num_ref_frames_in_pic_order_cnt_cycle=Ue(buf,nLen,StartBit);

			int *offset_for_ref_frame=new int[num_ref_frames_in_pic_order_cnt_cycle];
			for( int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
				offset_for_ref_frame[i]=Se(buf,nLen,StartBit);
			delete [] offset_for_ref_frame;
		}
		int num_ref_frames=Ue(buf,nLen,StartBit);
		int gaps_in_frame_num_value_allowed_flag=u(1,buf,StartBit);
		int pic_width_in_mbs_minus1=Ue(buf,nLen,StartBit);
		int pic_height_in_map_units_minus1=Ue(buf,nLen,StartBit);

		width=(pic_width_in_mbs_minus1+1)*16;
		height=(pic_height_in_map_units_minus1+1)*16;

		return TRUE;
	}
	else
		return FALSE;
}
#endif
