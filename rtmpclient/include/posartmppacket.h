#ifndef _KDM_POSA_RTMP_PACKET_H_
#define _KDM_POSA_RTMP_PACKET_H_

#include "posa_linux.h"

#define RTMP_SIG_SIZE 1536
#define RTMP_LARGE_HEADER_SIZE 12
#define RTMP_MAX_HEADER_SIZE 18

#define RTMP_PACKET_SIZE_LARGE    0
#define RTMP_PACKET_SIZE_MEDIUM   1
#define RTMP_PACKET_SIZE_SMALL    2
#define RTMP_PACKET_SIZE_MINIMUM  3


#define RTMP_LF_LIVE	0x0002	/* stream is live */


#define RTMP_FEATURE_HTTP	0x01
#define RTMP_FEATURE_ENC	0x02
#define RTMP_FEATURE_SSL	0x04
#define RTMP_FEATURE_MFP	0x08	/* not yet supported */
#define RTMP_FEATURE_WRITE	0x10	/* publish, not play */
#define RTMP_FEATURE_HTTP2	0x20	/* server-side rtmpt */

#define RTMP_PROTOCOL_UNDEFINED	-1
#define RTMP_PROTOCOL_RTMP      0
#define RTMP_PROTOCOL_RTMPE     RTMP_FEATURE_ENC
#define RTMP_PROTOCOL_RTMPT     RTMP_FEATURE_HTTP
#define RTMP_PROTOCOL_RTMPS     RTMP_FEATURE_SSL
#define RTMP_PROTOCOL_RTMPTE    (RTMP_FEATURE_HTTP|RTMP_FEATURE_ENC)
#define RTMP_PROTOCOL_RTMPTS    (RTMP_FEATURE_HTTP|RTMP_FEATURE_SSL)
#define RTMP_PROTOCOL_RTMFP     RTMP_FEATURE_MFP

#define RTMP_DEFAULT_CHUNKSIZE	128


#define RTMPPacket_IsReady(a) ((a)->m_nBytesRead == (a)->m_nBodySize)

#define RTMP_PACKET_TYPE_CHUNK_SIZE         0x01
#define RTMP_PACKET_TYPE_BYTES_READ_REPORT  0x03
#define RTMP_PACKET_TYPE_CONTROL            0x04
#define RTMP_PACKET_TYPE_SERVER_BW          0x05
#define RTMP_PACKET_TYPE_CLIENT_BW          0x06
#define RTMP_PACKET_TYPE_AUDIO              0x08
#define RTMP_PACKET_TYPE_VIDEO              0x09
#define RTMP_PACKET_TYPE_FLEX_STREAM_SEND   0x0F
#define RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT 0x10
#define RTMP_PACKET_TYPE_FLEX_MESSAGE       0x11
#define RTMP_PACKET_TYPE_INFO               0x12
#define RTMP_PACKET_TYPE_SHARED_OBJECT      0x13
#define RTMP_PACKET_TYPE_INVOKE             0x14
#define RTMP_PACKET_TYPE_FLASH_VIDEO        0x16

typedef struct RTMPPacket
{
    u8 m_headerType;
    u8 m_packetType;
    u8 m_hasAbsTimestamp;	/* timestamp absolute or relative? */
    int m_nChannel;
    u32 m_nTimeStamp;	/* timestamp */
    s32 m_nInfoField2;	/* last 4 bytes in a long header */
    u32 m_nBodySize;
    u32 m_nBytesRead;    
    char *m_body;
} RTMPPacket;

typedef struct RTMPMetadata
{
	// video, must be h264 type
	u32	nWidth;
	u32	nHeight;
	u32	nFrameRate;		// fps
	u32	nVideoDataRate;	// bps
	u32	nSpsLen;
	u8	Sps[1024];
	u32	nPpsLen;
	u8	Pps[1024];

	// audio, must be aac type
	bool bHasAudio;
	u32	nAudioSampleRate;
	u32	nAudioSampleSize;
	u32	nAudioChannels;
	char pAudioSpecCfg;
	u32	nAudioSpecCfgLen;

} RTMPMetadata;

typedef enum
{
        AMF_NUMBER = 0, 
        AMF_BOOLEAN, 
        AMF_STRING, 
        AMF_OBJECT,
        AMF_MOVIECLIP,		/* reserved, not used */
        AMF_NULL,
        AMF_UNDEFINED, 
        AMF_REFERENCE, 
        AMF_ECMA_ARRAY, 
        AMF_OBJECT_END,
        AMF_STRICT_ARRAY,
        AMF_DATE, 
        AMF_LONG_STRING, 
        AMF_UNSUPPORTED,
        AMF_RECORDSET,		/* reserved, not used */
        AMF_XML_DOC, 
        AMF_TYPED_OBJECT,
        AMF_AVMPLUS,		/* switch to AMF3 */
        AMF_INVALID = 0xff
} AMFDataType;

typedef enum
{ 
        AMF3_UNDEFINED = 0,
        AMF3_NULL, 
        AMF3_FALSE, 
        AMF3_TRUE,
        AMF3_INTEGER, 
        AMF3_DOUBLE, 
        AMF3_STRING, 
        AMF3_XML_DOC, 
        AMF3_DATE,
        AMF3_ARRAY, 
        AMF3_OBJECT, 
        AMF3_XML, 
        AMF3_BYTE_ARRAY
} AMF3DataType;

typedef struct AVal
{
    char *av_val;
    int av_len;
} AVal;

typedef struct RTMP_METHOD
{
    AVal name;
    int num;
} RTMP_METHOD;

struct AMFObjectProperty;

typedef struct AMFObject
{
    int o_num;
    struct AMFObjectProperty *o_props;
} AMFObject;

typedef struct AMFObjectProperty
{
    AVal p_name;
    AMFDataType p_type;
    union
    {
        double p_number;
        AVal p_aval;
        AMFObject p_object;
    } p_vu;
    s16 p_UTCoffset;
} AMFObjectProperty;

#define AVC(str)	{str,sizeof(str)-1}
#define AVMATCH(a1,a2)	((a1)->av_len == (a2)->av_len && \
                         !memcmp((a1)->av_val,(a2)->av_val,(a1)->av_len))

#define SAVC(x)	static const AVal av_##x = AVC(#x)

SAVC(app);
SAVC(connect);
SAVC(flashVer);
SAVC(swfUrl);
SAVC(pageUrl);
SAVC(tcUrl);
SAVC(fpad);
SAVC(capabilities);
SAVC(audioCodecs);
SAVC(videoCodecs);
SAVC(videoFunction);
SAVC(objectEncoding);
SAVC(secureToken);
SAVC(secureTokenResponse);
SAVC(type);
SAVC(nonprivate);

SAVC(onMetaData);
SAVC(duration);
SAVC(video);
SAVC(audio);
SAVC(copyright);

SAVC(onBWDone);
SAVC(onFCSubscribe);
SAVC(onFCUnsubscribe);
SAVC(_onbwcheck);
SAVC(_onbwdone);
SAVC(_error);
SAVC(close);
SAVC(code);
SAVC(level);
SAVC(description);
SAVC(onStatus);
SAVC(playlist_ready);
SAVC(set_playlist);

SAVC(createStream);
SAVC(releaseStream);
SAVC(deleteStream);
SAVC(closeStream);

SAVC(FCPublish);
SAVC(FCUnpublish);
SAVC(ping);
SAVC(pong);
SAVC(_result);
SAVC(_checkbw);


SAVC(play);
SAVC(publish);
SAVC(live);
SAVC(record);

SAVC(width);
SAVC(height);
SAVC(framerate);
SAVC(videocodecid);

SAVC(details);
SAVC(status);
SAVC(clientid);


static const AVal av_NetStream_Publish_BadName = AVC("NetStream.Publish.BadName");

static const AVal av_NetStream_Failed = AVC("NetStream.Failed");
static const AVal av_NetStream_Play_Failed = AVC("NetStream.Play.Failed");
static const AVal av_NetStream_Play_StreamNotFound = AVC("NetStream.Play.StreamNotFound");
static const AVal av_NetConnection_Connect_InvalidApp = AVC("NetConnection.Connect.InvalidApp");
static const AVal av_NetStream_Play_Start = AVC("NetStream.Play.Start");
static const AVal av_NetStream_Play_Complete = AVC("NetStream.Play.Complete");
static const AVal av_NetStream_Play_Stop = AVC("NetStream.Play.Stop");
static const AVal av_NetStream_Seek_Notify = AVC("NetStream.Seek.Notify");
static const AVal av_NetStream_Pause_Notify = AVC("NetStream.Pause.Notify");
static const AVal av_NetStream_Play_PublishNotify = AVC("NetStream.Play.PublishNotify");
static const AVal av_NetStream_Play_UnpublishNotify = AVC("NetStream.Play.UnpublishNotify");
static const AVal av_NetStream_Publish_Start = AVC("NetStream.Publish.Start");
static const AVal av_NetConnection_Connect_Rejected = AVC("NetConnection.Connect.Rejected");


int AM_DecodeInt32LE(const char *data);

unsigned int AMF_DecodeInt32(const char *data);
unsigned int AMF_DecodeInt24(const char *data);
unsigned short AMF_DecodeInt16(const char *data);
int AMF_DecodeBoolean(const char *data);


void AMF_AddProp(AMFObject *obj, const AMFObjectProperty *prop);
int AMFProp_Decode(AMFObjectProperty *prop, const char *pBuffer, int nSize,int bDecodeName);
int AMF_Decode(AMFObject * obj, const char *pBuffer, int nSize, int bDecodeName);

void AMF_DecodeString(const char *data, AVal *bv);
void AMF_DecodeLongString(const char *data, AVal *bv);

double AMFProp_GetNumber(AMFObjectProperty *prop);
void AMFProp_GetObject(AMFObjectProperty *prop, AMFObject *obj);
int AMFProp_IsValid(AMFObjectProperty *prop);

void AV_erase(RTMP_METHOD *vals, int *num, int i, int freeit);
void AV_queue(RTMP_METHOD **vals, int *num, AVal *av, int txn);
void AV_clear(RTMP_METHOD *vals, int num);

int AMF_DecodeArray(AMFObject *obj, const char *pBuffer, int nSize,int nArrayLen, int bDecodeName);


int AM_EncodeInt32LE(char *output, int nVal);
char* AMF_EncodeInt32(char *output, char *outend, int nVal);
char* AMF_EncodeInt24(char *output, char *outend, int nVal);
char* AMF_EncodeInt16(char *output, char *outend, short nVal);
char *AMF_EncodeString(char *output, char *outend, const AVal * bv);
char *AMF_EncodeNumber(char *output, char *outend, double dVal);
char *AMF_EncodeBoolean(char *output, char *outend, int bVal);
char *AMF_EncodeNamedString(char *output, char *outend, const AVal *strName, const AVal *strValue);
char *AMF_EncodeNamedNumber(char *output, char *outend, const AVal *strName, double dVal);
char *AMF_EncodeNamedBoolean(char *output, char *outend, const AVal *strName, int bVal);
char *AMF_EncodeNamedStringEx(char *output, char *outend, const AVal *strName, const char* strValue);
char *AMF_EncodeStringEx(char *output, char *outend, const char* strValue);
typedef struct AMF3ClassDef
{
    AVal cd_name;
    char cd_externalizable;
    char cd_dynamic;
    int cd_num;
    AVal *cd_props;
} AMF3ClassDef;

int AMF3_Decode(AMFObject *obj, const char *pBuffer, int nSize, int bAMFData);
int AMF3Prop_Decode(AMFObjectProperty *prop, const char *pBuffer, int nSize,int bDecodeName);
void AMF3CD_AddProp(AMF3ClassDef *cd, AVal *prop);
AVal* AMF3CD_GetProp(AMF3ClassDef *cd, int nIndex);


// 
void AMFProp_GetString(AMFObjectProperty *prop, AVal *str);
void AMFProp_GetName(AMFObjectProperty *prop, AVal *name);
void AMFProp_SetName(AMFObjectProperty *prop, AVal *name);

int AMF_FindFirstObjectProperty(AMFObject *obj, AMFObjectProperty * p);
int AMF_FindFirstMatchingProperty(AMFObject *obj, const AVal *name, AMFObjectProperty * p);
int AMF_FindPrefixProperty(AMFObject *obj, const AVal *name, AMFObjectProperty * p);
AMFObjectProperty* AMF_GetProp(AMFObject *obj, const AVal *name, int nIndex);
void AMFProp_Reset(AMFObjectProperty *prop);
void AMF_Reset(AMFObject *obj);

void AMF_Dump(AMFObject *obj);
void AMFProp_Dump(AMFObjectProperty *prop);


void RTMPPacket_Reset(RTMPPacket *p);
int RTMPPacket_Alloc(RTMPPacket *p, int nSize);
void RTMPPacket_Free(RTMPPacket *p);

//////////////////////////////////////////////////////////////////////////

#ifdef WIN32

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#define __FLOAT_WORD_ORDER __BYTE_ORDER

#else /* !_WIN32 */

#include <sys/param.h>

#if defined(BYTE_ORDER) && !defined(__BYTE_ORDER)
#define __BYTE_ORDER    BYTE_ORDER
#endif

#if defined(BIG_ENDIAN) && !defined(__BIG_ENDIAN)
#define __BIG_ENDIAN	BIG_ENDIAN
#endif

#if defined(LITTLE_ENDIAN) && !defined(__LITTLE_ENDIAN)
#define __LITTLE_ENDIAN	LITTLE_ENDIAN
#endif

#endif /* !_WIN32 */

/* define default endianness */
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN	1234
#endif

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN	4321
#endif

#ifndef __BYTE_ORDER
#warning "Byte order not defined on your system, assuming little endian!"
#define __BYTE_ORDER	__LITTLE_ENDIAN
#endif

/* ok, we assume to have the same float word order and byte order if float word order is not defined */
#ifndef __FLOAT_WORD_ORDER
#warning "Float word order not defined, assuming the same as byte order!"
#define __FLOAT_WORD_ORDER	__BYTE_ORDER
#endif

#if !defined(__BYTE_ORDER) || !defined(__FLOAT_WORD_ORDER)
#error "Undefined byte or float word order!"
#endif

#if __FLOAT_WORD_ORDER != __BIG_ENDIAN && __FLOAT_WORD_ORDER != __LITTLE_ENDIAN
#error "Unknown/unsupported float word order!"
#endif

#if __BYTE_ORDER != __BIG_ENDIAN && __BYTE_ORDER != __LITTLE_ENDIAN
#error "Unknown/unsupported byte order!"
#endif


double AMF_DecodeNumber(const char* data);

#endif
