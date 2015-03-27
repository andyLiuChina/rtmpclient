#ifndef _KDM_POSA_TELNET_SERVER_H_
#define _KDM_POSA_TELNET_SERVER_H_

#include "posa_linux.h"

const u8 MAX_COMMAND_LENGTH = 100;
const s8 NEWLINE_CHAR =         '\n';
const s8 BACKSPACE_CHAR =       8;
const s8 BLANK_CHAR =           ' ';
const s8 RETURN_CHAR =          13;
const s8 TAB_CHAR =             9;
const s8 DEL_CHAR =             127;
const s8 CTRL_S =               19;
const s8 CTRL_R =               18;
const s8 UP_ARROW =             27;
const s8 DOWN_ARROW =           28;
const s8 LEFT_ARROW =           29;
const s8 RIGHT_ARROW =          30;


enum tel_state
{
    tel_normal = 0,
    tel_nego,
    tel_sub
};
static BOOL32 s_bSeenIac = 0;
static enum tel_state s_emState;
static s32 s_nCountAfterSb = 0;

static s8 remove_iac(u8 byC)
{
    s8 chRet = 0;
    if ((0xFF == byC) && (FALSE == s_bSeenIac))    /* IAC*/
    {
        s_bSeenIac = TRUE;
        return chRet;
    }

    if (TRUE == s_bSeenIac)
    {
        switch(byC)
        {
        case 0xFB:
        case 0xFC:
        case 0xFD:
        case 0xFE:
            {
                if (tel_normal != s_emState)
                {
                    printf(" illegal negotiation.\n");
                }
                s_emState = tel_nego;
            }
            break;
        case 0xFA:
            {
                if (tel_normal != s_emState)
                {
                    printf(" illegal sub negotiation.\n");
                }
                s_emState = tel_sub;
                s_nCountAfterSb = 0;
            }
            break;
        case 0xF0:
            {
                if (tel_sub != s_emState)
                {
                    printf(" illegal sub end.\n");
                }
                s_emState = tel_normal;
            }
            break;
        default:
            {
                if (!((byC > 0xF0) && (byC < 0xFA) && (tel_normal == s_emState)))
                {
                    printf("illegal command.\n");
                }
                s_emState = tel_normal;
            }
        }
        s_bSeenIac = FALSE;
        return 0;
    }

    switch (s_emState)
    {
    case tel_nego:
        s_emState = tel_normal;
        break;
    case tel_sub:
        {
            s_nCountAfterSb++; /* set maximize sub negotiation length*/
            if (s_nCountAfterSb >= 100)
            {
                s_emState = tel_normal;
            }
        }
        break;
    default:
        chRet = byC;
    }
    return chRet;
}

typedef s32 (*UniformFunc)(int, int, int, int, int, int, int, int, int, int);
#define CMD_NAME_SIZE           32
#define CMD_USAGE_SIZE          200
struct TCmdTable {
    s8 name[CMD_NAME_SIZE];    /* Command Name; less than 30 bytes */
    UniformFunc cmd;            /* Implementation function */
    s8 usage[CMD_USAGE_SIZE];    /* Usage message */
};

typedef struct
{
    LPSTR szPara;
    BOOL32 bInQuote;
    BOOL32 bIsChar;
}TRawPara;


static s32 WordParse(LPCSTR szWord)
{
    s32 nTemp;

    if (NULL == szWord)
    {
        return 0;
    }

    nTemp = atoi(szWord);
    if ((0 == nTemp) && ('0' != szWord[0]))
    {
        return (u32)(LPSTR)szWord;
    }
    return nTemp;
}

class CPosaTelnetInstance : public IPosaNetHandler
{
public: 
    CPosaTelnetInstance()
    {
        m_byCmdLen = 0;
    }
    virtual ~CPosaTelnetInstance()
    {
    }
    virtual int HandleRead(CPosaBuffer& cBuffer)
    {        
        char* pBuffer = (char*)cBuffer.GetIBPointer();
        int dwBufferSize = cBuffer.GetAvaliableBytesCount();
        
        u32 dwSendNum;
        for(int inx = 0; inx < dwBufferSize; inx++)
        {
            s8 chCmdChar = pBuffer[inx];
            chCmdChar = remove_iac(chCmdChar);            
            switch(chCmdChar)
            {            
            case RETURN_CHAR:         // �س���
                {
                    this->SendData("\r\n", 2, dwSendNum);
                    CmdParse(m_achCommand, m_byCmdLen);
                    m_byCmdLen = 0;
                    memset(m_achCommand, 0, MAX_COMMAND_LENGTH);
                    PromptShow();         // ��ʾ��ʾ��
                }
                break;
            case NEWLINE_CHAR:        /* ���з� */
            case UP_ARROW:            // �ϼ�ͷ
            case DOWN_ARROW:          // �¼�ͷ
            case LEFT_ARROW:          // ���ͷ
            case RIGHT_ARROW:         // �Ҽ�ͷ
                break;
                
            case BACKSPACE_CHAR:         // �˸��
                {
                    if (m_byCmdLen <= 0)
                    {
                        continue;
                    }
                    
                    m_byCmdLen--;
                    if (m_byCmdLen > 0 && m_byCmdLen < MAX_COMMAND_LENGTH)
                    {
                        m_achCommand[m_byCmdLen] = '\0';
                    }                    

                    s8 tmpChar[3];
                    tmpChar[0] = BACKSPACE_CHAR;
                    tmpChar[1] = BLANK_CHAR;
                    tmpChar[2] = BACKSPACE_CHAR;
                    this->SendData(tmpChar, 3, dwSendNum);
                }
                break;
            default:
                {                    
                    this->SendData(&chCmdChar, 1, dwSendNum);
                    if (m_byCmdLen < MAX_COMMAND_LENGTH)
                    {
                        m_achCommand[m_byCmdLen++] = chCmdChar;
                    }
                    else
                    {
                        this->SendData("\r\n", 2, dwSendNum);
                        CmdParse(m_achCommand, m_byCmdLen);                        
                        PromptShow();
                        m_byCmdLen = 0;
                        memset(m_achCommand, 0, MAX_COMMAND_LENGTH);
                    }
                }
                break;
            }
        }

        cBuffer.IgnoreAll();
        return 0;
    }
    void PromptShow()
    {
        u32 dwSend;        
        this->SendData("posatelnet->", strlen("posatelnet->"), dwSend);
    }
    BOOL32 TelePrint(LPCSTR szMsg)
    {
        if (FALSE == this->m_bCanISendData) 
        {
            return FALSE;
        }
        s8 chCur;
        u32 dwStart = 0;
        u32 dwCount = 0;
        LPCSTR szRetStr = "\n\r";
        int nSendOK = 0;
        u32 dwSendNum = 0;
        
        if ((NULL == szMsg))
        {
            return FALSE;
        }
        
        while (1)
        {
            chCur = szMsg[dwCount];

            if (('\0' == chCur) || ('\n' == chCur))
            {
                nSendOK = this->SendData(&szMsg[dwStart], dwCount - dwStart, dwSendNum);
                if (-1 == nSendOK)
                {
                    return FALSE;
                }

                if ('\n' == chCur)
                {
                    u32 dwSendNum;
                    nSendOK = this->SendData(szRetStr, 2, dwSendNum);
                    if (-1 == nSendOK)
                    {
                        return FALSE;
                    }
                }
                if ('\0' == chCur)
                {
                    break;
                }

                dwStart = dwCount + 1;
            }
            dwCount++;
        }
        return TRUE;
    }
    UniformFunc FindCommand(LPCSTR szName)
    {        
        return NULL;
    }

    void RunCmd(LPSTR szCmd)
    {
        s32 anPara[10];
        TRawPara atRawPara[10];
        s32 nParaNum = 0;
        u8 byCount = 0;
        u8 byStartCnt = 0;
        BOOL32 bStrStart = FALSE;
        BOOL32 bCharStart = FALSE;
        u32 dwCmdLen = strlen(szCmd) + 1;
        
        memset(anPara, 0, sizeof(anPara));
        memset(atRawPara, 0, sizeof(TRawPara) * 10);
        
        
        while (byCount < dwCmdLen)
        {
            switch(szCmd[byCount])
            {
            case '\'':
                {
                    szCmd[byCount] = '\0';
                    if (FALSE == bCharStart)
                    {
                        byStartCnt = byCount;
                    }
                    else
                    {
                        if (byCount > byStartCnt + 2)
                        {
                            this->TelePrint("input error.\n");
                            return;
                        }
                    }
                    bCharStart = (TRUE == bCharStart)? FALSE: TRUE;
                }
                break;
            case '\"':
                {
                    szCmd[byCount] = '\0';
                    bStrStart = !bStrStart;
                }
                break;
            case ',':
            case ' ':
            case '\t':
            case '\n':
            case '(':
            case ')':
                {
                    if (FALSE == bStrStart)
                    {
                        szCmd[byCount] = '\0';
                    }
                }
                break;
            default:
                {
                    if ((byCount > 0) && ('\0' == szCmd[byCount - 1]) && ('\0' != szCmd[byCount])) //*by lxx
                    {
                        atRawPara[nParaNum].szPara = &szCmd[byCount];
                        if (TRUE == bStrStart)
                        {
                            atRawPara[nParaNum].bInQuote = TRUE;
                        }
                        if (TRUE == bCharStart)
                        {
                            atRawPara[nParaNum].bIsChar = TRUE;
                        }
                        if (++nParaNum >= 10)
                        {
                            break;
                        }
                    }
                }
            }
            byCount++;
        }
        
        if ((TRUE == bStrStart) || (TRUE == bCharStart))
        {
            this->TelePrint("input error.\n");
            return;
        }
        
        for (byCount = 0; byCount < 10; byCount++)
        {
            if (NULL == atRawPara[byCount].szPara)
            {
                anPara[byCount] = 0;
                continue;
            }
            if (TRUE == atRawPara[byCount].bInQuote)
            {
                anPara[byCount] = (u32)atRawPara[byCount].szPara;
                continue;
            }
            if (TRUE == atRawPara[byCount].bIsChar)
            {
                anPara[byCount] = (s8)atRawPara[byCount].szPara[0];
                continue;
            }
            anPara[byCount] = WordParse(atRawPara[byCount].szPara);
        }

        if (0 == strcmp("bye", szCmd))
        {
            u32 dwSendNum = 0;
            this->SendData("\n  bye......\n", strlen("\n  bye......\n"), dwSendNum);
            this->Close();
            return;
        }
        
        if (0 == strcmp("posahelp", szCmd))
        {
            this->posahelp();
            return;
        }
        
        UniformFunc pfFunc = FindCommand(szCmd);
        char szTemp[255] = {0};
        if (NULL != pfFunc)
        {
            s32 nRet = (*pfFunc)(anPara[0], anPara[1], anPara[2], anPara[3], anPara[4], anPara[5], anPara[6], anPara[7], anPara[8], anPara[9]);
            sprintf(szTemp, "Return value: %d\n", nRet);
            TelePrint(szTemp);
        }
        else
        {
            sprintf(szTemp, "function '%s' doesn't exist!\n", szCmd);
            TelePrint(szTemp);            
        }     
        
        return;
    }
    void CmdParse(LPCSTR szCmd, u8 byCmdLen)
    {
        u8 byCount = 0;
        s32 nCpyLen = 0;
        s8 achCommand[MAX_COMMAND_LENGTH];
        
        if (byCmdLen > 0)
        {
            for (byCount = 0; byCount < byCmdLen; byCount++)
            {
                s8 chTmp;
                chTmp = szCmd[byCount];
                if (isdigit(chTmp) || islower(chTmp) || isupper(chTmp))
                {
                    break;
                }
            }
            nCpyLen = byCmdLen-byCount;
        }
        if (nCpyLen <= 0)
        {        
            return;
        }
        if (nCpyLen > MAX_COMMAND_LENGTH)
        {
            PosaPrintf(TRUE, FALSE, "osp bug in cmdparse\n");
            return;
        }
        
        memcpy(achCommand, szCmd + byCount, nCpyLen);
        if (byCmdLen < MAX_COMMAND_LENGTH)
        {
            achCommand[nCpyLen] = '\0';
        }
        else
        {
            achCommand[MAX_COMMAND_LENGTH - 1] = '\0';
        }
        
        RunCmd(achCommand);
    }
    void PosaRegCommand(LPCSTR name, void* func, LPCSTR usage)
    {
        
    }
    void posahelp()
    {
        char szTemp[260] = {0};
        sprintf(szTemp, "Posa Telenet Version: %s. ", "1.0");
        TelePrint(szTemp);
        sprintf(szTemp, "Compile Time: %s  %s\n", __TIME__, __DATE__);
        TelePrint(szTemp);          
    }
private:
    s8 m_achCommand[MAX_COMMAND_LENGTH];
    u8 m_byCmdLen;
};


#endif
