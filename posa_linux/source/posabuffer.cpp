#include "posa_linux.h"
//////////////////////////////////////////////////////////////////////////
// IObuffer


#ifdef WITH_SANITY_INPUT_BUFFER
#define SANITY_INPUT_BUFFER \
assert(m_consumed<=m_published); \
assert(m_published<=m_size);
#else
#define SANITY_INPUT_BUFFER
#endif

CPosaBuffer::CPosaBuffer() 
{
	m_pBuffer = NULL;
	m_size = 0;
	m_published = 0;
	m_consumed = 0;
	m_minChunkSize = 4096;	
	SANITY_INPUT_BUFFER;    
}

CPosaBuffer::~CPosaBuffer() 
{
	SANITY_INPUT_BUFFER;
	Cleanup();
	SANITY_INPUT_BUFFER;
}
void CPosaBuffer::Initialize(u32 expected)
{
    if ((m_pBuffer != NULL) ||
        (m_size != 0) ||
        (m_published != 0) ||
        (m_consumed != 0))
        
    {
        printf("This buffer was used before. Please initialize it before using");
    }
    EnsureSize(expected);
}
BOOL32 CPosaBuffer::ReadFromBuffer(const u8 *pBuffer, const u32 size) 
{
	::PosaPrintf(TRUE, FALSE, "[posa] buf the sending data size is %d, the buf size is %d  left is %d", size,
	        				  m_size, m_size-m_published);
	SANITY_INPUT_BUFFER;
	if (!EnsureSize(size)) 
    {
		::PosaPrintf(TRUE, FALSE, "[posa] super error buf ensuresize error");
		SANITY_INPUT_BUFFER;
		return false;
	}
	memcpy(m_pBuffer + m_published, pBuffer, size);
	m_published += size;
	SANITY_INPUT_BUFFER;
	return true;
}


u32 CPosaBuffer::GetMinChunkSize() 
{

	return m_minChunkSize;
}

void CPosaBuffer::SetMinChunkSize(u32 minChunkSize) 
{
	assert(minChunkSize > 0 && minChunkSize < 16 * 1024 * 1024);
	m_minChunkSize = minChunkSize;
}

u32 CPosaBuffer::GetAvaliableBytesCount()
{
	return (m_published - m_consumed);
}

u8* CPosaBuffer::GetIBPointer() 
{
    return ((u8*) (m_pBuffer + m_consumed));
}

BOOL32 CPosaBuffer::Ignore(u32 size) 
{
	SANITY_INPUT_BUFFER;
	m_consumed += size;
	Recycle();
	SANITY_INPUT_BUFFER;
	return true;
}

BOOL32 CPosaBuffer::IgnoreAll() 
{
	SANITY_INPUT_BUFFER;
	m_consumed = m_published;
	Recycle();
	SANITY_INPUT_BUFFER;
	return true;
}

BOOL32 CPosaBuffer::MoveDataToHead()
{
	SANITY_INPUT_BUFFER;	
    if (m_consumed > 0)
    {
        memmove(m_pBuffer, m_pBuffer + m_consumed, m_published - m_consumed);    
        m_published = m_published - m_consumed;
	    m_consumed = 0;
    }
    
	SANITY_INPUT_BUFFER;

	return true;
}

BOOL32 CPosaBuffer::SetMaxBufferSize(u32 expected)
{
    if (expected <= m_size) 
    {
        return TRUE;
    }
    u32 nMore = expected - this->m_published;
    return this->EnsureSize(nMore);
}
BOOL32 CPosaBuffer::EnsureSize(u32 expected) 
{
	SANITY_INPUT_BUFFER;
	//1. Do we have enough space?
	if (m_published + expected <= m_size) 
    {
		SANITY_INPUT_BUFFER;
		return true;
	}

	//2. Apparently we don't! Try to move some data
	MoveDataToHead();

	//3. Again, do we have enough space?
	if (m_published + expected <= m_size) 
    {
		SANITY_INPUT_BUFFER;
		return true;
	}

	//4. Nope! So, let's get busy with making a brand new buffer...
	//First, we allocate at least 1/3 of what we have and no less than m_minChunkSize
	if ((m_published + expected - m_size)<(m_size / 3)) 
    {
		expected = m_size + m_size / 3 - m_published;
	}

	if (expected < m_minChunkSize) 
    {
		expected = m_minChunkSize;
	}

	//5. Allocate
	u8 *pTempBuffer = new u8[m_published + expected];

	//6. Copy existing data if necessary and switch buffers
	if (m_pBuffer != NULL) 
    {
		memcpy(pTempBuffer, m_pBuffer, m_published);
		delete[] m_pBuffer;
	}
	m_pBuffer = pTempBuffer;

	//7. Update the size
	m_size = m_published + expected;
	SANITY_INPUT_BUFFER;

	return true;
}


void CPosaBuffer::Cleanup() 
{
	if (m_pBuffer != NULL) 
    {
		delete[] m_pBuffer;
		m_pBuffer = NULL;
	}
	m_size = 0;
	m_published = 0;
	m_consumed = 0;
}

void CPosaBuffer::Recycle() 
{
	if (m_consumed != m_published)
		return;
	SANITY_INPUT_BUFFER;
	m_consumed = 0;
	m_published = 0;
	SANITY_INPUT_BUFFER;
}
