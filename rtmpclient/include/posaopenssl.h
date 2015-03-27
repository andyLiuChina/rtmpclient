#ifndef _KDM_POSA_OPEN_SSL_H_
#define _KDM_POSA_OPEN_SSL_H_

#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH	32
#endif

#ifndef RTMP_SIG_SIZE
#define RTMP_SIG_SIZE 1536
#endif


#define _POSA_USE_OPENSSL_

#ifdef _POSA_USE_OPENSSL_
#include <openssl/hmac.h>
#if OPENSSL_VERSION_NUMBER < 0x0090800 || !defined(SHA256_DIGEST_LENGTH)
#error Your OpenSSL is too old, need 0.9.8 or newer with SHA256
#endif
#define HMAC_setup(ctx, key, len)	HMAC_CTX_init(&ctx); HMAC_Init_ex(&ctx, key, len, EVP_sha256(), 0)
#define HMAC_crunch(ctx, buf, len)	HMAC_Update(&ctx, buf, len)
#define HMAC_finish(ctx, dig, dlen)	HMAC_Final(&ctx, dig, &dlen); HMAC_CTX_cleanup(&ctx)
#else

#endif

static u32 Client_GetDigestOffset1(u8* handshake, u32 len)
{
    u32 offset = 0;
    u8 *ptr = handshake + 8;
    u32 res;
    assert(12 <= len);
    
    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    
    res = (offset % 728) + 12;
    
    if (res + 32 > 771)
    {
        PosaPrintf(TRUE, FALSE, "GetDigestOffset1: Couldn't calculate digest offset (got %d), exiting!\r\n", 
            res);
        exit(1);
    }
    
    return res;
}
static u32 Client_GetDigestOffset2(u8* handshake, u32 len)
{
    u32 offset = 0;
    u8 *ptr = handshake + 772;
    u32 res;
    
    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    ptr++;
    offset += (*ptr);
    
    res = (offset % 728) + 776;
    
    if (res + 32 > 1535)
    {
        PosaPrintf(TRUE, FALSE, "Couldn't calculate correct digest offset (got %d), exiting",
            res);
        exit(1);
    }
    return res;
}

static void HMACsha256(const u8 *message, size_t messageLen, const u8 *key,
	                   size_t keylen, u8 *digest)
{
    unsigned int digestLen;
    HMAC_CTX ctx;
    
    HMAC_setup(ctx, key, keylen);
    HMAC_crunch(ctx, message, messageLen);
    HMAC_finish(ctx, digest, digestLen);
    
    assert(digestLen == 32);
}

static void CalculateDigest(u32 digestPos, u8 *handshakeMessage,
		                    const u8 *key, size_t keyLen, u8 *digest)
{
    const int messageLen = RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH;
    u8 message[RTMP_SIG_SIZE - SHA256_DIGEST_LENGTH];
    
    memcpy(message, handshakeMessage, digestPos);
    memcpy(message + digestPos,
        &handshakeMessage[digestPos + SHA256_DIGEST_LENGTH],
        messageLen - digestPos);
    
    HMACsha256(message, messageLen, key, keyLen, digest);
}

static int VerifyDigest(u32 digestPos, u8 *handshakeMessage, const u8 *key,
	                    size_t keyLen)
{
    u8 calcDigest[SHA256_DIGEST_LENGTH];
    
    CalculateDigest(digestPos, handshakeMessage, key, keyLen, calcDigest);
    
    return (memcmp(&handshakeMessage[digestPos], calcDigest,  SHA256_DIGEST_LENGTH) == 0);
}



#endif
