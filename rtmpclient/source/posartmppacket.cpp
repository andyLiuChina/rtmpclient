#include "posartmppacket.h"

static const AMFObjectProperty AMFProp_Invalid = { {0, 0}, AMF_INVALID };
static const AVal AV_empty = { 0, 0 };


unsigned short AMF_DecodeInt16(const char *data)
{
  unsigned char *c = (unsigned char *) data;
  unsigned short val;
  val = (c[0] << 8) | c[1];
  return val;
}
unsigned int AMF_DecodeInt24(const char *data)
{
    unsigned char *c = (unsigned char *) data;
    unsigned int val;
    val = (c[0] << 16) | (c[1] << 8) | c[2];
    return val;
}

unsigned int AMF_DecodeInt32(const char *data)
{
    unsigned char *c = (unsigned char *)data;
    unsigned int val;
    val = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
    return val;
}

char * AMF_EncodeInt16(char *output, char *outend, short nVal)
{
  if (output+2 > outend)
    return NULL;

  output[1] = nVal & 0xff;
  output[0] = nVal >> 8;
  return output+2;
}

char * AMF_EncodeInt24(char *output, char *outend, int nVal)
{
  if (output+3 > outend)
    return NULL;

  output[2] = nVal & 0xff;
  output[1] = nVal >> 8;
  output[0] = nVal >> 16;
  return output+3;
}

char* AMF_EncodeInt32(char *output, char *outend, int nVal)
{
  if (output+4 > outend)
    return NULL;

  output[3] = nVal & 0xff;
  output[2] = nVal >> 8;
  output[1] = nVal >> 16;
  output[0] = nVal >> 24;
  return output+4;
}

char *AMF_EncodeStringEx(char *output, char *outend, const char* strValue)
{
    AVal temp;
    temp.av_val = (char*)strValue;
    temp.av_len = strlen(strValue);
    return AMF_EncodeString(output, outend, &temp);
}
char *AMF_EncodeString(char *output, char *outend, const AVal * bv)
{
    if ((bv->av_len < 65536 && output + 1 + 2 + bv->av_len > outend) ||
        output + 1 + 4 + bv->av_len > outend)
        return NULL;
    
    if (bv->av_len < 65536)
    {
        *output++ = AMF_STRING;
        
        output = AMF_EncodeInt16(output, outend, bv->av_len);
    }
    else
    {
        *output++ = AMF_LONG_STRING;
        
        output = AMF_EncodeInt32(output, outend, bv->av_len);
    }
    memcpy(output, bv->av_val, bv->av_len);
    output += bv->av_len;
    
    return output;
}

char *AMF_EncodeNumber(char *output, char *outend, double dVal)
{
    if (output+1+8 > outend)
        return NULL;
    
    *output++ = AMF_NUMBER;	/* type: Number */
    
#if __FLOAT_WORD_ORDER == __BYTE_ORDER
#if __BYTE_ORDER == __BIG_ENDIAN
    memcpy(output, &dVal, 8);
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    {
        unsigned char *ci, *co;
        ci = (unsigned char *)&dVal;
        co = (unsigned char *)output;
        co[0] = ci[7];
        co[1] = ci[6];
        co[2] = ci[5];
        co[3] = ci[4];
        co[4] = ci[3];
        co[5] = ci[2];
        co[6] = ci[1];
        co[7] = ci[0];
    }
#endif
#else
#if __BYTE_ORDER == __LITTLE_ENDIAN	/* __FLOAT_WORD_ORER == __BIG_ENDIAN */
    {
        unsigned char *ci, *co;
        ci = (unsigned char *)&dVal;
        co = (unsigned char *)output;
        co[0] = ci[3];
        co[1] = ci[2];
        co[2] = ci[1];
        co[3] = ci[0];
        co[4] = ci[7];
        co[5] = ci[6];
        co[6] = ci[5];
        co[7] = ci[4];
    }
#else /* __BYTE_ORDER == __BIG_ENDIAN && __FLOAT_WORD_ORER == __LITTLE_ENDIAN */
    {
        unsigned char *ci, *co;
        ci = (unsigned char *)&dVal;
        co = (unsigned char *)output;
        co[0] = ci[4];
        co[1] = ci[5];
        co[2] = ci[6];
        co[3] = ci[7];
        co[4] = ci[0];
        co[5] = ci[1];
        co[6] = ci[2];
        co[7] = ci[3];
    }
#endif
#endif
    
    return output+8;
}
char *AMF_EncodeBoolean(char *output, char *outend, int bVal)
{
    if (output+2 > outend)
        return NULL;
    
    *output++ = AMF_BOOLEAN;
    
    *output++ = bVal ? 0x01 : 0x00;
    
    return output;
}

char *AMF_EncodeNamedStringEx(char *output, char *outend, const AVal *strName, const char* strValue)
{
    AVal temp;
    temp.av_len = strlen(strValue);
    temp.av_val = (char*)strValue;
    return AMF_EncodeNamedString(output, outend, strName, &temp);
}
char *AMF_EncodeNamedString(char *output, char *outend, const AVal *strName, const AVal *strValue)
{
    if (output+2+strName->av_len > outend)
        return NULL;
    output = AMF_EncodeInt16(output, outend, strName->av_len);
    
    memcpy(output, strName->av_val, strName->av_len);
    output += strName->av_len;
    
    return AMF_EncodeString(output, outend, strValue);
}
char *AMF_EncodeNamedNumber(char *output, char *outend, const AVal *strName, double dVal)
{
    if (output+2+strName->av_len > outend)
        return NULL;
    output = AMF_EncodeInt16(output, outend, strName->av_len);
    
    memcpy(output, strName->av_val, strName->av_len);
    output += strName->av_len;
    
    return AMF_EncodeNumber(output, outend, dVal);
}

char *AMF_EncodeNamedBoolean(char *output, char *outend, const AVal *strName, int bVal)
{
    if (output+2+strName->av_len > outend)
        return NULL;
    output = AMF_EncodeInt16(output, outend, strName->av_len);
    
    memcpy(output, strName->av_val, strName->av_len);
    output += strName->av_len;
    
    return AMF_EncodeBoolean(output, outend, bVal);
}


int AM_DecodeInt32LE(const char *data)
{
    unsigned char *c = (unsigned char *)data;
    unsigned int val;
    
    val = (c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
    return val;
}

int AM_EncodeInt32LE(char *output, int nVal)
{
    output[0] = nVal;
    nVal >>= 8;
    output[1] = nVal;
    nVal >>= 8;
    output[2] = nVal;
    nVal >>= 8;
    output[3] = nVal;
    return 4;
}

void AMF_DecodeString(const char *data, AVal *bv)
{
  bv->av_len = AMF_DecodeInt16(data);
  bv->av_val = (bv->av_len > 0) ? (char *)data + 2 : NULL;
}

void AMF_DecodeLongString(const char *data, AVal *bv)
{
  bv->av_len = AMF_DecodeInt32(data);
  bv->av_val = (bv->av_len > 0) ? (char *)data + 4 : NULL;
}


double AMF_DecodeNumber(const char *data)
{
    double dVal;
#if __FLOAT_WORD_ORDER == __BYTE_ORDER
#if __BYTE_ORDER == __BIG_ENDIAN
    memcpy(&dVal, data, 8);
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned char *ci, *co;
    ci = (unsigned char *)data;
    co = (unsigned char *)&dVal;
    co[0] = ci[7];
    co[1] = ci[6];
    co[2] = ci[5];
    co[3] = ci[4];
    co[4] = ci[3];
    co[5] = ci[2];
    co[6] = ci[1];
    co[7] = ci[0];
#endif
#else
#if __BYTE_ORDER == __LITTLE_ENDIAN	/* __FLOAT_WORD_ORER == __BIG_ENDIAN */
    unsigned char *ci, *co;
    ci = (unsigned char *)data;
    co = (unsigned char *)&dVal;
    co[0] = ci[3];
    co[1] = ci[2];
    co[2] = ci[1];
    co[3] = ci[0];
    co[4] = ci[7];
    co[5] = ci[6];
    co[6] = ci[5];
    co[7] = ci[4];
#else /* __BYTE_ORDER == __BIG_ENDIAN && __FLOAT_WORD_ORER == __LITTLE_ENDIAN */
    unsigned char *ci, *co;
    ci = (unsigned char *)data;
    co = (unsigned char *)&dVal;
    co[0] = ci[4];
    co[1] = ci[5];
    co[2] = ci[6];
    co[3] = ci[7];
    co[4] = ci[0];
    co[5] = ci[1];
    co[6] = ci[2];
    co[7] = ci[3];
#endif
#endif
    return dVal;
}



int AMF_DecodeBoolean(const char *data)
{
    return *data != 0;
}

int AMF_DecodeArray(AMFObject *obj, const char *pBuffer, int nSize,int nArrayLen, int bDecodeName)
{
    int nOriginalSize = nSize;
    int bError = FALSE;
    
    obj->o_num = 0;
    obj->o_props = NULL;
    while (nArrayLen > 0)
    {
        AMFObjectProperty prop;
        int nRes;
        nArrayLen--;
        
        nRes = AMFProp_Decode(&prop, pBuffer, nSize, bDecodeName);
        if (nRes == -1)
            bError = TRUE;
        else
        {
            nSize -= nRes;
            pBuffer += nRes;
            AMF_AddProp(obj, &prop);
        }
    }
    if (bError)
        return -1;
    
    return nOriginalSize - nSize;
}



int AMF_Decode(AMFObject * obj, const char *pBuffer, int nSize, int bDecodeName)
{
    int nOriginalSize = nSize;
    int bError = FALSE; /* if there is an error while decoding - try to at least find the end mark AMF_OBJECT_END */
    
    obj->o_num = 0;
    obj->o_props = NULL;
    while (nSize > 0)
    {
        AMFObjectProperty prop;
        int nRes;
        
        if (nSize >=3 && AMF_DecodeInt24(pBuffer) == AMF_OBJECT_END)
        {
            nSize -= 3;
            bError = FALSE;
            break;
        }
        
        if (bError)
        {
            PosaPrintf(TRUE, FALSE, "DECODING ERROR, IGNORING BYTES UNTIL NEXT KNOWN PATTERN!\n");
            nSize--;
            pBuffer++;
            continue;
        }
        
        nRes = AMFProp_Decode(&prop, pBuffer, nSize, bDecodeName);
        if (nRes == -1)
        {
            bError = TRUE;
        }            
        else
        {
            nSize -= nRes;
            pBuffer += nRes;
            AMF_AddProp(obj, &prop);
        }
    }
    
    if (bError)
        return -1;
    
    return nOriginalSize - nSize;
}

void AMF_AddProp(AMFObject *obj, const AMFObjectProperty *prop)
{
    if (!(obj->o_num & 0x0f))
    {
        obj->o_props = (AMFObjectProperty*)realloc(obj->o_props, (obj->o_num + 16) * sizeof(AMFObjectProperty));
    }
    memcpy(&obj->o_props[obj->o_num++], prop, sizeof(AMFObjectProperty));
}

int AMFProp_Decode(AMFObjectProperty *prop, const char *pBuffer, int nSize,int bDecodeName)
{
    int nOriginalSize = nSize;
    int nRes;
    
    prop->p_name.av_len = 0;
    prop->p_name.av_val = NULL;
    
    if (nSize == 0 || !pBuffer)
    {
        PosaPrintf(TRUE, FALSE, "Empty buffer/no buffer pointer!");
        return -1;
    }
    
    if (bDecodeName && nSize < 4)
    {				/* at least name (length + at least 1 byte) and 1 byte of data */
        PosaPrintf(TRUE, FALSE, "Not enough data for decoding with name, less than 4 bytes!");
        return -1;
    }
    
    if (bDecodeName)
    {
        unsigned short nNameSize = AMF_DecodeInt16(pBuffer);
        if (nNameSize > nSize - 2)
        {
            PosaPrintf(TRUE, FALSE, "Name size out of range: namesize (%d) > len (%d) - 2",
                nNameSize, nSize);
            return -1;
        }
        
        AMF_DecodeString(pBuffer, &prop->p_name);
        nSize -= 2 + nNameSize;
        pBuffer += 2 + nNameSize;
    }
    
    if (nSize == 0)
    {
        return -1;
    }
    
    nSize--;    
    char szType = *pBuffer++;;
    prop->p_type = (AMFDataType)szType;
    
    switch (prop->p_type)
    {
    case AMF_NUMBER:
        if (nSize < 8)
            return -1;
        prop->p_vu.p_number = AMF_DecodeNumber(pBuffer);
        nSize -= 8;
        break;
    case AMF_BOOLEAN:
        if (nSize < 1)
            return -1;
        prop->p_vu.p_number = (double)AMF_DecodeBoolean(pBuffer);
        nSize--;
        break;
    case AMF_STRING:
        {
            unsigned short nStringSize = AMF_DecodeInt16(pBuffer);            
            if (nSize < (long)nStringSize + 2)
            {
                return -1;
            }
            AMF_DecodeString(pBuffer, &prop->p_vu.p_aval);
            nSize -= (2 + nStringSize);
            break;
        }
    case AMF_OBJECT:
        {
            int nRes = AMF_Decode(&prop->p_vu.p_object, pBuffer, nSize, TRUE);
            if (nRes == -1)
            {
                return -1;
            }                
            nSize -= nRes;
            break;
        }
    case AMF_MOVIECLIP:
        {
            PosaPrintf(TRUE, FALSE, "AMF_MOVIECLIP reserved!");
            return -1;
            break;
        }
    case AMF_NULL:
    case AMF_UNDEFINED:
    case AMF_UNSUPPORTED:
        prop->p_type = AMF_NULL;
        break;
    case AMF_REFERENCE:
        {
            PosaPrintf(TRUE, FALSE, "AMF_REFERENCE not supported!");
            return -1;
            break;
        }
    case AMF_ECMA_ARRAY:
        {
            nSize -= 4;
            
            /* next comes the rest, mixed array has a final 0x000009 mark and names, so its an object */
            nRes = AMF_Decode(&prop->p_vu.p_object, pBuffer + 4, nSize, TRUE);
            if (nRes == -1)
                return -1;
            nSize -= nRes;
            break;
        }
    case AMF_OBJECT_END:
        {
            return -1;
            break;
        }
    case AMF_STRICT_ARRAY:
        {
            unsigned int nArrayLen = AMF_DecodeInt32(pBuffer);
            nSize -= 4;
            
            nRes = AMF_DecodeArray(&prop->p_vu.p_object, pBuffer + 4, nSize,
                nArrayLen, FALSE);
            if (nRes == -1)
                return -1;
            nSize -= nRes;
            break;
        }
    case AMF_DATE:
        {
            PosaPrintf(TRUE, FALSE, "AMF_DATE");
            
            if (nSize < 10)
                return -1;
            
            prop->p_vu.p_number = AMF_DecodeNumber(pBuffer);
            prop->p_UTCoffset = AMF_DecodeInt16(pBuffer + 8);
            
            nSize -= 10;
            break;
        }
    case AMF_LONG_STRING:
    case AMF_XML_DOC:
        {
            unsigned int nStringSize = AMF_DecodeInt32(pBuffer);
            if (nSize < (long)nStringSize + 4)
                return -1;
            AMF_DecodeLongString(pBuffer, &prop->p_vu.p_aval);
            nSize -= (4 + nStringSize);
            if (prop->p_type == AMF_LONG_STRING)
                prop->p_type = AMF_STRING;
            break;
        }
    case AMF_RECORDSET:
        {
            PosaPrintf(TRUE, FALSE, "AMF_RECORDSET reserved!");
            return -1;
            break;
        }
    case AMF_TYPED_OBJECT:
        {
            PosaPrintf(TRUE, FALSE, "AMF_TYPED_OBJECT not supported!");
            return -1;
            break;
        }
    case AMF_AVMPLUS:
        {
            int nRes = AMF3_Decode(&prop->p_vu.p_object, pBuffer, nSize, TRUE);
            if (nRes == -1)
                return -1;
            nSize -= nRes;
            prop->p_type = AMF_OBJECT;
            break;
        }
    default:
        PosaPrintf(TRUE, FALSE, "unknown datatype 0x%02x, @%p",
            prop->p_type, pBuffer - 1);
        return -1;
    }
    
    return nOriginalSize - nSize;
}

void AMFProp_GetString(AMFObjectProperty *prop, AVal *str)
{
    *str = prop->p_vu.p_aval;
}

void AMFProp_SetName(AMFObjectProperty *prop, AVal *name)
{
  prop->p_name = *name;
}
void AMFProp_GetName(AMFObjectProperty *prop, AVal *name)
{
  *name = prop->p_name;
}

int AMF_FindFirstObjectProperty(AMFObject *obj, AMFObjectProperty * p)
{
    int n;
    /* this is a small object search to locate the "duration" property */
    for (n = 0; n < obj->o_num; n++)
    {
        AMFObjectProperty *prop = AMF_GetProp(obj, NULL, n);
        if (prop->p_type == AMF_OBJECT || prop->p_type == AMF_ECMA_ARRAY)
        {
            memcpy(p, prop, sizeof(*prop));
            return TRUE;   
        }
    }
    return FALSE;
}
int AMF_FindFirstMatchingProperty(AMFObject *obj, const AVal *name, AMFObjectProperty * p)
{
    int n;
    /* this is a small object search to locate the "duration" property */
    for (n = 0; n < obj->o_num; n++)
    {
        AMFObjectProperty *prop = AMF_GetProp(obj, NULL, n);
        
        if (AVMATCH(&prop->p_name, name))
        {
            memcpy(p, prop, sizeof(*prop));
            return TRUE;
        }
        
        if (prop->p_type == AMF_OBJECT || prop->p_type == AMF_ECMA_ARRAY)
        {
            if (AMF_FindFirstMatchingProperty(&prop->p_vu.p_object, name, p))
                return TRUE;
        }
    }
    return FALSE;
}

/* Like above, but only check if name is a prefix of property */
int AMF_FindPrefixProperty(AMFObject *obj, const AVal *name, AMFObjectProperty * p)
{
    int n;
    for (n = 0; n < obj->o_num; n++)
    {
        AMFObjectProperty *prop = AMF_GetProp(obj, NULL, n);
        
        if (prop->p_name.av_len > name->av_len &&
            !memcmp(prop->p_name.av_val, name->av_val, name->av_len))
        {
            memcpy(p, prop, sizeof(*prop));
            return TRUE;
        }
        
        if (prop->p_type == AMF_OBJECT)
        {
            if (AMF_FindPrefixProperty(&prop->p_vu.p_object, name, p))
                return TRUE;
        }
    }
    return FALSE;
}

AMFObjectProperty* AMF_GetProp(AMFObject *obj, const AVal *name, int nIndex)
{
    if (nIndex >= 0)
    {
        if (nIndex < obj->o_num)
            return &obj->o_props[nIndex];
    }
    else
    {
        int n;
        for (n = 0; n < obj->o_num; n++)
        {
            if (AVMATCH(&obj->o_props[n].p_name, name))
                return &obj->o_props[n];
        }
    }
    
    return (AMFObjectProperty *)&AMFProp_Invalid;
}

double AMFProp_GetNumber(AMFObjectProperty *prop)
{
    return prop->p_vu.p_number;
}

void AMFProp_GetObject(AMFObjectProperty *prop, AMFObject *obj)
{
    *obj = prop->p_vu.p_object;
}

int AMFProp_IsValid(AMFObjectProperty *prop)
{
    return prop->p_type != AMF_INVALID;
}

void AV_erase(RTMP_METHOD *vals, int *num, int i, int freeit)
{
    if (freeit)
    {
        free(vals[i].name.av_val);
    }        
    (*num)--;
    for (; i < *num; i++)
    {
        vals[i] = vals[i + 1];
    }
    vals[i].name.av_val = NULL;
    vals[i].name.av_len = 0;
    vals[i].num = 0;
}

void AV_queue(RTMP_METHOD **vals, int *num, AVal *av, int txn)
{
    char *tmp;
    if (!(*num & 0x0f))
    {
        *vals = (RTMP_METHOD*)realloc(*vals, (*num + 16) * sizeof(RTMP_METHOD));
    }
    tmp = (char*)malloc(av->av_len + 1);
    memcpy(tmp, av->av_val, av->av_len);
    tmp[av->av_len] = '\0';
    (*vals)[*num].num = txn;
    (*vals)[*num].name.av_len = av->av_len;
    (*vals)[(*num)++].name.av_val = tmp;
}

void AV_clear(RTMP_METHOD *vals, int num)
{
    int i;
    for (i = 0; i < num; i++)
        free(vals[i].name.av_val);
    free(vals);
}

void AMFProp_Reset(AMFObjectProperty *prop)
{
    if (prop->p_type == AMF_OBJECT || prop->p_type == AMF_ECMA_ARRAY ||
        prop->p_type == AMF_STRICT_ARRAY)
    {
        AMF_Reset(&prop->p_vu.p_object);
    }        
    else
    {
        prop->p_vu.p_aval.av_len = 0;
        prop->p_vu.p_aval.av_val = NULL;
    }
    prop->p_type = AMF_INVALID;
}

void AMF_Reset(AMFObject *obj)
{
    int n;
    for (n = 0; n < obj->o_num; n++)
    {
        AMFProp_Reset(&obj->o_props[n]);
    }
    free(obj->o_props);
    obj->o_props = NULL;
    obj->o_num = 0;
}




//////////////////////////////////////////////////////////////////////////
//     amf3
#define AMF3_INTEGER_MAX	268435455
#define AMF3_INTEGER_MIN	-268435456

int AMF3ReadInteger(const char *data, s32 *valp)
{
    int i = 0;
    s32 val = 0;
    
    while (i <= 2)
    {				/* handle first 3 bytes */
        if (data[i] & 0x80)
        {			/* byte used */
            val <<= 7;		/* shift up */
            val |= (data[i] & 0x7f);	/* add bits */
            i++;
        }
        else
        {
            break;
        }
    }
    
    if (i > 2)
    {				/* use 4th byte, all 8bits */
        val <<= 8;
        val |= data[3];
        
        /* range check */
        if (val > AMF3_INTEGER_MAX)
            val -= (1 << 29);
    }
    else
    {				/* use 7bits of last unparsed byte (0xxxxxxx) */
        val <<= 7;
        val |= data[i];
    }
    
    *valp = val;
    
    return i > 2 ? 4 : i + 1;
}

int AMF3ReadString(const char *data, AVal *str)
{
    s32 ref = 0;
    int len;
    assert(str != 0);
    
    len = AMF3ReadInteger(data, &ref);
    data += len;
    
    if ((ref & 0x1) == 0)
    {				/* reference: 0xxx */
        u32 refIndex = (ref >> 1);
        PosaPrintf(TRUE, FALSE, "%s, string reference, index: %d, not supported, ignoring!",
                   "AMF3ReadString", refIndex);
        return len;
    }
    else
    {
        u32 nSize = (ref >> 1);
        
        str->av_val = (char *)data;
        str->av_len = nSize;
        
        return len + nSize;
    }
    return len;
}

int AMF3Prop_Decode(AMFObjectProperty *prop, const char *pBuffer, int nSize,int bDecodeName)
{
    int nOriginalSize = nSize;
    AMF3DataType type;
    
    prop->p_name.av_len = 0;
    prop->p_name.av_val = NULL;
    
    if (nSize == 0 || !pBuffer)
    {
        PosaPrintf(TRUE, FALSE, "empty buffer/no buffer pointer!");
        return -1;
    }
    
    /* decode name */
    if (bDecodeName)
    {
        AVal name;
        int nRes = AMF3ReadString(pBuffer, &name);
        
        if (name.av_len <= 0)
            return nRes;
        
        prop->p_name = name;
        pBuffer += nRes;
        nSize -= nRes;
    }
    
    /* decode */
    type = (AMF3DataType)(*pBuffer++);
    nSize--;
    
    switch (type)
    {
    case AMF3_UNDEFINED:
    case AMF3_NULL:
        prop->p_type = AMF_NULL;
        break;
    case AMF3_FALSE:
        prop->p_type = AMF_BOOLEAN;
        prop->p_vu.p_number = 0.0;
        break;
    case AMF3_TRUE:
        prop->p_type = AMF_BOOLEAN;
        prop->p_vu.p_number = 1.0;
        break;
    case AMF3_INTEGER:
        {
            s32 res = 0;
            int len = AMF3ReadInteger(pBuffer, &res);
            prop->p_vu.p_number = (double)res;
            prop->p_type = AMF_NUMBER;
            nSize -= len;
            break;
        }
    case AMF3_DOUBLE:
        if (nSize < 8)
            return -1;
        prop->p_vu.p_number = AMF_DecodeNumber(pBuffer);
        prop->p_type = AMF_NUMBER;
        nSize -= 8;
        break;
    case AMF3_STRING:
    case AMF3_XML_DOC:
    case AMF3_XML:
        {
            int len = AMF3ReadString(pBuffer, &prop->p_vu.p_aval);
            prop->p_type = AMF_STRING;
            nSize -= len;
            break;
        }
    case AMF3_DATE:
        {
            s32 res = 0;
            int len = AMF3ReadInteger(pBuffer, &res);
            
            nSize -= len;
            pBuffer += len;
            
            if ((res & 0x1) == 0)
            {			/* reference */
                u32 nIndex = (res >> 1);
                PosaPrintf(TRUE, FALSE, "AMF3_DATE reference: %d, not supported!", nIndex);
            }
            else
            {
                if (nSize < 8)
                {
                    return -1;
                }
                
                prop->p_vu.p_number = AMF_DecodeNumber(pBuffer);
                nSize -= 8;
                prop->p_type = AMF_NUMBER;
            }
            break;
        }
    case AMF3_OBJECT:
        {
            int nRes = AMF3_Decode(&prop->p_vu.p_object, pBuffer, nSize, TRUE);
            if (nRes == -1)
                return -1;
            nSize -= nRes;
            prop->p_type = AMF_OBJECT;
            break;
        }
    case AMF3_ARRAY:
    case AMF3_BYTE_ARRAY:
    default:
        PosaPrintf(TRUE, FALSE, "AMF3 unknown/unsupported datatype 0x%02x, @%p",
                   (unsigned char)(*pBuffer), pBuffer);
        return -1;
    }    
    return nOriginalSize - nSize;
}

void AMF3CD_AddProp(AMF3ClassDef *cd, AVal *prop)
{
    if (!(cd->cd_num & 0x0f))
    {
        cd->cd_props = (AVal *)realloc(cd->cd_props, (cd->cd_num + 16) * sizeof(AVal));
    }
    
    cd->cd_props[cd->cd_num++] = *prop;
}

AVal* AMF3CD_GetProp(AMF3ClassDef *cd, int nIndex)
{
    if (nIndex >= cd->cd_num)
        return (AVal *)&AV_empty;
    return &cd->cd_props[nIndex];
}

int AMF3_Decode(AMFObject *obj, const char *pBuffer, int nSize, int bAMFData)
{
    int nOriginalSize = nSize;
    s32 ref;
    int len;
    
    obj->o_num = 0;
    obj->o_props = NULL;
    if (bAMFData)
    {
        if (*pBuffer != AMF3_OBJECT)
        {
            PosaPrintf(TRUE, FALSE, "AMF3 Object encapsulated in AMF stream does not start with AMF3_OBJECT!");
        }
        pBuffer++;
        nSize--;
    }
    
    ref = 0;
    len = AMF3ReadInteger(pBuffer, &ref);
    pBuffer += len;
    nSize -= len;
    
    if ((ref & 1) == 0)
    {				/* object reference, 0xxx */
        u32 objectIndex = (ref >> 1);
        
        PosaPrintf(TRUE, FALSE, "Object reference, index: %d", objectIndex);
    }
    else				/* object instance */
    {
        s32 classRef = (ref >> 1);
        
        AMF3ClassDef cd = { {0, 0} };
        AMFObjectProperty prop;
        
        if ((classRef & 0x1) == 0)
        {			/* class reference */
            u32 classIndex = (classRef >> 1);
            PosaPrintf(TRUE, FALSE, "Class reference: %d", classIndex);
        }
        else
        {
            s32 classExtRef = (classRef >> 1);
            int i;
            
            cd.cd_externalizable = (classExtRef & 0x1) == 1;
            cd.cd_dynamic = ((classExtRef >> 1) & 0x1) == 1;
            
            cd.cd_num = classExtRef >> 2;
            
            /* class name */
            
            len = AMF3ReadString(pBuffer, &cd.cd_name);
            nSize -= len;
            pBuffer += len;
            
            /*std::string str = className; */
            
            PosaPrintf(TRUE, FALSE, "Class name: %s, externalizable: %d, dynamic: %d, classMembers: %d",
                cd.cd_name.av_val, cd.cd_externalizable, cd.cd_dynamic, cd.cd_num);
            
            for (i = 0; i < cd.cd_num; i++)
            {
                AVal memberName;
                len = AMF3ReadString(pBuffer, &memberName);
                PosaPrintf(TRUE, FALSE, "Member: %s", memberName.av_val);
                AMF3CD_AddProp(&cd, &memberName);
                nSize -= len;
                pBuffer += len;
            }
        }
        
        /* add as referencable object */
        
        if (cd.cd_externalizable)
        {
            int nRes;
            AVal name = AVC("DEFAULT_ATTRIBUTE");
            
            PosaPrintf(TRUE, FALSE, "Externalizable, TODO check");
            
            nRes = AMF3Prop_Decode(&prop, pBuffer, nSize, FALSE);
            if (nRes == -1)
            {
                PosaPrintf(TRUE, FALSE, "%s, failed to decode AMF3 property!");
            }                
            else
            {
                nSize -= nRes;
                pBuffer += nRes;
            }
            
            AMFProp_SetName(&prop, &name);
            AMF_AddProp(obj, &prop);
        }
        else
        {
            int nRes, i;
            for (i = 0; i < cd.cd_num; i++)	/* non-dynamic */
            {
                nRes = AMF3Prop_Decode(&prop, pBuffer, nSize, FALSE);
                if (nRes == -1)
                {
                    PosaPrintf(TRUE, FALSE, "failed to decode AMF3 property!");
                }                    
                
                AMFProp_SetName(&prop, AMF3CD_GetProp(&cd, i));
                AMF_AddProp(obj, &prop);
                
                pBuffer += nRes;
                nSize -= nRes;
            }
            if (cd.cd_dynamic)
            {
                int len = 0;
                
                do
                {
                    nRes = AMF3Prop_Decode(&prop, pBuffer, nSize, TRUE);
                    AMF_AddProp(obj, &prop);
                    
                    pBuffer += nRes;
                    nSize -= nRes;
                    
                    len = prop.p_name.av_len;
                }
                while (len > 0);
            }
        }
        PosaPrintf(TRUE, FALSE, "class object!");
    }
    return nOriginalSize - nSize;
}


void AMF_Dump(AMFObject *obj)
{
    int n;
    PosaPrintf(TRUE, FALSE, "(object begin)");
    for (n = 0; n < obj->o_num; n++)
    {
        AMFProp_Dump(&obj->o_props[n]);
    }
    PosaPrintf(TRUE, FALSE, "(object end)");
}

void AMFProp_Dump(AMFObjectProperty *prop)
{
    char strRes[256];
    char str[256];
    AVal name;
    
    if (prop->p_type == AMF_INVALID)
    {
        PosaPrintf(TRUE, FALSE, "Property: INVALID");
        return;
    }
    
    if (prop->p_type == AMF_NULL)
    {
        PosaPrintf(TRUE, FALSE, "Property: NULL");
        return;
    }
    
    if (prop->p_name.av_len)
    {
        name = prop->p_name;
    }
    else
    {
        name.av_val = "no-name.";
        name.av_len = sizeof("no-name.") - 1;
    }
    if (name.av_len > 18)
        name.av_len = 18;
    
    snprintf(strRes, 255, "Name: %18.*s, ", name.av_len, name.av_val);
    
    if (prop->p_type == AMF_OBJECT)
    {
        PosaPrintf(TRUE, FALSE, "Property: <%sOBJECT>", strRes);
        AMF_Dump(&prop->p_vu.p_object);
        return;
    }
    else if (prop->p_type == AMF_ECMA_ARRAY)
    {
        PosaPrintf(TRUE, FALSE, "Property: <%sECMA_ARRAY>", strRes);
        AMF_Dump(&prop->p_vu.p_object);
        return;
    }
    else if (prop->p_type == AMF_STRICT_ARRAY)
    {
        PosaPrintf(TRUE, FALSE, "Property: <%sSTRICT_ARRAY>", strRes);
        AMF_Dump(&prop->p_vu.p_object);
        return;
    }
    
    switch (prop->p_type)
    {
    case AMF_NUMBER:
        snprintf(str, 255, "NUMBER:\t%.2f", prop->p_vu.p_number);
        break;
    case AMF_BOOLEAN:
        snprintf(str, 255, "BOOLEAN:\t%s",
            prop->p_vu.p_number != 0.0 ? "TRUE" : "FALSE");
        break;
    case AMF_STRING:
        snprintf(str, 255, "STRING:\t%.*s", prop->p_vu.p_aval.av_len,
            prop->p_vu.p_aval.av_val);
        break;
    case AMF_DATE:
        snprintf(str, 255, "DATE:\ttimestamp: %.2f, UTC offset: %d",
            prop->p_vu.p_number, prop->p_UTCoffset);
        break;
    default:
        snprintf(str, 255, "INVALID TYPE 0x%02x", (unsigned char)prop->p_type);
    }    
    PosaPrintf(TRUE, FALSE, "Property: <%s%s>", strRes, str);
}

//////////////////////////////////////////////////////////////////////////
void RTMPPacket_Reset(RTMPPacket *p)
{
    p->m_headerType = 0;
    p->m_packetType = 0;
    p->m_nChannel = 0;
    p->m_nTimeStamp = 0;
    p->m_nInfoField2 = 0;
    p->m_hasAbsTimestamp = FALSE;
    p->m_nBodySize = 0;
    p->m_nBytesRead = 0;
}

int RTMPPacket_Alloc(RTMPPacket *p, int nSize)
{
    char* ptr = (char*)calloc(1, nSize + RTMP_MAX_HEADER_SIZE);
    if (!ptr)
        return FALSE;
    p->m_body = ptr + RTMP_MAX_HEADER_SIZE;
    p->m_nBytesRead = 0;
    return TRUE;
}

void RTMPPacket_Free(RTMPPacket *p)
{
    if (p->m_body)
    {
        free(p->m_body - RTMP_MAX_HEADER_SIZE);
        p->m_body = NULL;
    }
}

//////////////////////////////////////////////////////////////////////////
