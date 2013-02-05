/*
 * R820T tuner driver, taken from Realteks RTL2832U Linux Kernel Driver
 *
 * This driver is a mess, and should be cleaned up/rewritten.
 *
 */

#include <stdint.h>
#include <stdio.h>

#include "rtlsdr_i2c.h"
#include "tuner_r820t.h"

static inline uint8_t reverse(uint8_t value)
{
    uint8_t result = 0;
    int cnt = 8;
    while(cnt)
    {
	result <<= 1;
	result |= value & 1;
	value >>= 1;
	cnt--;
    }
    return result;
}

int r820t_SetRfFreqHz(void *pTuner, uint64_t freq)
{
    if(R828_SetFrequency(pTuner, freq, DVB_T_6M, false) != 0)
	return -1;

    return 0;
}

int r820t_SetStandardMode(void *pTuner, int StandardMode)
{
    if(R828_SetStandard(pTuner, (R828_Standard_Type)StandardMode) != 0)
	return -1;

    return 0;
}

int r820t_SetStandby(void *pTuner, char loop_through)
{

    if(R828_Standby(pTuner, loop_through) != 0)
	return -1;

    return 0;
}

// The following context is implemented for R820T source code.

int I2C_Write_Len(void *pTuner, R828_I2C_LEN_TYPE *I2C_Info)
{
    uint8_t DeviceAddr;

    unsigned int i, j;

    uint8_t ByteNum = I2C_Info->Len;

    uint8_t WritingBuffer[128];
    unsigned long WritingByteNum, WritingByteNumMax, WritingByteNumRem;

    // Calculate maximum writing byte number.
    //	WritingByteNumMax = pBaseInterface->I2cWritingByteNumMax - LEN_1_BYTE;
    WritingByteNumMax = 2 - 1; //9 orig

    for(i = 0; i < ByteNum; i += WritingByteNumMax)
    {

	WritingByteNumRem = ByteNum - i;
	WritingByteNum = (WritingByteNumRem > WritingByteNumMax) ? WritingByteNumMax : WritingByteNumRem;
	WritingBuffer[0] = I2C_Info->RegAddr + i;

	for(j = 0; j < WritingByteNum; j++)
	    WritingBuffer[j+1] = I2C_Info->Data[i + j];

	if (rtlsdr_i2c_write_fn(pTuner, R820T_I2C_ADDR, WritingBuffer, WritingByteNum + 1) < 0)
	    return -1;
    }
    return 0;
}

    int
I2C_Read_Len(void *pTuner, R828_I2C_LEN_TYPE *I2C_Info)
{
    uint8_t DeviceAddr;

    unsigned int i;

    uint8_t RegStartAddr = 0x00;
    uint8_t ReadingBytes[128];
    unsigned long ByteNum = (unsigned long)I2C_Info->Len;

    // Set tuner register reading address.
    // Note: The I2C format of tuner register reading address setting is as follows:
    //       start_bit + (DeviceAddr | writing_bit) + RegReadingAddr + stop_bit

    if (rtlsdr_i2c_write_fn(pTuner, R820T_I2C_ADDR, &RegStartAddr, 1) < 0)
	return -1;

    // Get tuner register bytes.
    // Note: The I2C format of tuner register byte getting is as follows:
    //       start_bit + (DeviceAddr | reading_bit) + reading_bytes (ReadingByteNum bytes) + stop_bit

    if (rtlsdr_i2c_read_fn(pTuner, R820T_I2C_ADDR, ReadingBytes, ByteNum) < 0)
	return -1;

    for(i = 0; i<ByteNum; i++)
    {
	I2C_Info->Data[i] = reverse(ReadingBytes[i]);
    }

    return 0;
}

    int
I2C_Write(void *pTuner, R828_I2C_TYPE *I2C_Info)
{
    uint8_t WritingBuffer[2] = {
	I2C_Info->RegAddr,
	I2C_Info->Data
    };

    if (rtlsdr_i2c_write_fn(pTuner, R820T_I2C_ADDR, WritingBuffer, 2) < 0)
	return -1;
    return 0;
}

void R828_Delay_MS(
	void *pTuner,
	unsigned long WaitTimeMs
	)
{
    /* simply don't wait for now */
    return;
}

//-----------------------------------------------------
//  
// Filename: R820T.c   
//
// This file is R820T tuner driver
// Copyright 2011 by Rafaelmicro., Inc.
//
//-----------------------------------------------------


//#include "stdafx.h"
//#include "R828.h"
//#include "..\I2C_Sys.h"


uint8_t R828_iniArry[27] = {
    0x83, 0x32, 0x75, 0xC0, 0x40, 0xD6, 0x6C, 0xF5, 0x63,
    /* 0x05  0x06  0x07  0x08  0x09  0x0A  0x0B  0x0C  0x0D */

#if(TUNER_CLK_OUT==true)  //enable tuner clk output for share Xtal application
    0x75, 0x68, 0x6C, 0x83, 0x80, 0x00, 0x0F, 0x00, 0xC0, //xtal_check
#else
    0x75, 0x78, 0x6C, 0x83, 0x80, 0x00, 0x0F, 0x00, 0xC0, //xtal_check
#endif
    /* 0x0E  0x0F  0x10  0x11  0x12  0x13  0x14  0x15  0x16 */
    0x30, 0x48, 0xCC, 0x60, 0x00, 0x54, 0xAE, 0x4A, 0xC0};
/*  0x17  0x18  0x19  0x1A  0x1B  0x1C  0x1D  0x1E  0x1F */

//----------------------------------------------------------//
//                   Internal Structs                       //
//----------------------------------------------------------//
typedef struct _R828_SectType
{
    uint8_t Phase_Y;
    uint8_t Gain_X;
    uint16_t Value;
}R828_SectType;

typedef enum _BW_Type
{
    BW_6M = 0,
    BW_7M,
    BW_8M,
    BW_1_7M,
    BW_10M,
    BW_200K
}BW_Type;

typedef struct _Sys_Info_Type
{
    uint16_t		IF_KHz;
    BW_Type		BW;
    uint32_t		FILT_CAL_LO;
    uint8_t		FILT_GAIN;
    uint8_t		IMG_R;
    uint8_t		FILT_Q;
    uint8_t		HP_COR;
    uint8_t       EXT_ENABLE;
    uint8_t       LOOP_THROUGH;
    uint8_t       LT_ATT;
    uint8_t       FLT_EXT_WIDEST;
    uint8_t       POLYFIL_CUR;
}Sys_Info_Type;

typedef struct _Freq_Info_Type
{
    uint8_t		OPEN_D;
    uint8_t		RF_MUX_PLOY;
    uint8_t		TF_C;
    uint8_t		XTAL_CAP20P;
    uint8_t		XTAL_CAP10P;
    uint8_t		XTAL_CAP0P;
    uint8_t		IMR_MEM;
}Freq_Info_Type;

typedef struct _SysFreq_Info_Type
{
    uint8_t		LNA_TOP;
    uint8_t		LNA_VTH_L;
    uint8_t		MIXER_TOP;
    uint8_t		MIXER_VTH_L;
    uint8_t		AIR_CABLE1_IN;
    uint8_t		CABLE2_IN;
    uint8_t		PRE_DECT;
    uint8_t		LNA_DISCHARGE;
    uint8_t		CP_CUR;
    uint8_t		DIV_BUF_CUR;
    uint8_t		FILTER_CUR;
}SysFreq_Info_Type;

//----------------------------------------------------------//
//                   Internal Parameters                    //
//----------------------------------------------------------//
enum XTAL_CAP_VALUE
{
    XTAL_LOW_CAP_30P = 0,
    XTAL_LOW_CAP_20P,
    XTAL_LOW_CAP_10P,
    XTAL_LOW_CAP_0P,
    XTAL_HIGH_CAP_0P
};

uint8_t R828_Arry[27];

R828_SectType IMR_Data[5] = {
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0}
};  //Please keep this array data for standby mode.

R828_I2C_LEN_TYPE R828_I2C_Len;

uint32_t R828_IF_khz;
uint32_t R828_CAL_LO_khz;
uint8_t  R828_IMR_point_num;
uint8_t  R828_IMR_done_flag = false;
uint8_t  R828_Fil_Cal_flag[STD_SIZE];
static uint8_t R828_Fil_Cal_code[STD_SIZE];

static uint8_t Xtal_cap_sel = XTAL_LOW_CAP_0P;
static uint8_t Xtal_cap_sel_tmp = XTAL_LOW_CAP_0P;
//----------------------------------------------------------//
//                   Internal static struct                 //
//----------------------------------------------------------//
static SysFreq_Info_Type SysFreq_Info1;
static Sys_Info_Type Sys_Info1;
//static Freq_Info_Type R828_Freq_Info;
static Freq_Info_Type Freq_Info1;
//----------------------------------------------------------//
//                   Internal Functions                     //
//----------------------------------------------------------//
int R828_Xtal_Check(void *pTuner);
int R828_InitReg(void *pTuner);
int R828_IMR_Prepare(void *pTuner);
int R828_IMR(void *pTuner, uint8_t IMR_MEM, int IM_Flag);
int R828_PLL(void *pTuner, uint64_t LO_freq, R828_Standard_Type R828_Standard);
int R828_MUX(void *pTuner, uint64_t RF_freq);
int R828_IQ(void *pTuner, R828_SectType* IQ_Pont);
int R828_IQ_Tree(void *pTuner, uint8_t FixPot, uint8_t FlucPot, uint8_t PotReg, R828_SectType* CompareTree);
int R828_CompreCor(R828_SectType* CorArry);
int R828_CompreStep(void *pTuner, R828_SectType* StepArry, uint8_t Pace);
int R828_Muti_Read(void *pTuner, uint8_t IMR_Reg, uint16_t* IMR_Result_Data);
int R828_Section(void *pTuner, R828_SectType* SectionArry);
int R828_F_IMR(void *pTuner, R828_SectType* IQ_Pont);
int R828_IMR_Cross(void *pTuner, R828_SectType* IQ_Pont, uint8_t* X_Direct);

Sys_Info_Type R828_Sys_Sel(R828_Standard_Type R828_Standard);
Freq_Info_Type R828_Freq_Sel(uint32_t RF_freq);
SysFreq_Info_Type R828_SysFreq_Sel(R828_Standard_Type R828_Standard,uint32_t RF_freq);

int R828_Filt_Cal(void *pTuner, uint32_t Cal_Freq,BW_Type R828_BW);

Sys_Info_Type R828_Sys_Sel(R828_Standard_Type R828_Standard)
{
    Sys_Info_Type R828_Sys_Info;

    switch (R828_Standard)
    {

	case DVB_T_6M: 
	case DVB_T2_6M: 
	    R828_Sys_Info.IF_KHz=3570;
	    R828_Sys_Info.BW=BW_6M;
	    R828_Sys_Info.FILT_CAL_LO=56000; //52000->56000
	    R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
	    R828_Sys_Info.IMG_R=0x00;		//image negative
	    R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
	    R828_Sys_Info.HP_COR=0x6B;		// 1.7M disable, +2cap, 1.0MHz		
	    R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1 
	    R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
	    R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
	    R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
	    R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
	    break;

	case DVB_T_7M: 
	case DVB_T2_7M: 
	    R828_Sys_Info.IF_KHz=4070;
	    R828_Sys_Info.BW=BW_7M;
	    R828_Sys_Info.FILT_CAL_LO=60000;
	    R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
	    R828_Sys_Info.IMG_R=0x00;		//image negative
	    R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
	    R828_Sys_Info.HP_COR=0x2B;		// 1.7M disable, +1cap, 1.0MHz		
	    R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1 
	    R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
	    R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
	    R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
	    R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
	    break;

	case DVB_T_7M_2:  
	case DVB_T2_7M_2:  
	    R828_Sys_Info.IF_KHz=4570;
	    R828_Sys_Info.BW=BW_7M;
	    R828_Sys_Info.FILT_CAL_LO=63000;
	    R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
	    R828_Sys_Info.IMG_R=0x00;		//image negative
	    R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
	    R828_Sys_Info.HP_COR=0x2A;		// 1.7M disable, +1cap, 1.25MHz		
	    R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1 
	    R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
	    R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
	    R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
	    R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
	    break;

	case DVB_T_8M: 
	case DVB_T2_8M: 
	    R828_Sys_Info.IF_KHz=4570;
	    R828_Sys_Info.BW=BW_8M;
	    R828_Sys_Info.FILT_CAL_LO=68500;
	    R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
	    R828_Sys_Info.IMG_R=0x00;		//image negative
	    R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
	    R828_Sys_Info.HP_COR=0x0B;		// 1.7M disable, +0cap, 1.0MHz		
	    R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1 
	    R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
	    R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
	    R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
	    R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
	    break;

	case ISDB_T: 
	    R828_Sys_Info.IF_KHz=4063;
	    R828_Sys_Info.BW=BW_6M;
	    R828_Sys_Info.FILT_CAL_LO=59000;
	    R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
	    R828_Sys_Info.IMG_R=0x00;		//image negative
	    R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
	    R828_Sys_Info.HP_COR=0x6A;		// 1.7M disable, +2cap, 1.25MHz		
	    R828_Sys_Info.EXT_ENABLE=0x40;  //R30[6], ext enable; R30[5]:0 ext at LNA max 
	    R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
	    R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
	    R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
	    R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
	    break;

	default:  //DVB_T_8M
	    R828_Sys_Info.IF_KHz=4570;
	    R828_Sys_Info.BW=BW_8M;
	    R828_Sys_Info.FILT_CAL_LO=68500;
	    R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
	    R828_Sys_Info.IMG_R=0x00;		//image negative
	    R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
	    R828_Sys_Info.HP_COR=0x0D;		// 1.7M disable, +0cap, 0.7MHz		
	    R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1 
	    R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
	    R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
	    R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
	    R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min
	    break;

    }

    return R828_Sys_Info;
}

Freq_Info_Type R828_Freq_Sel(uint32_t LO_freq)
{
    Freq_Info_Type R828_Freq_Info;

    if(LO_freq<50000)
    {
	R828_Freq_Info.OPEN_D=0x08; // low
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0xDF;     //R27[7:0]  band2,band0
	R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 0;
    }

    else if(LO_freq>=50000 && LO_freq<55000)
    {
	R828_Freq_Info.OPEN_D=0x08; // low
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0xBE;     //R27[7:0]  band4,band1 
	R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 0;
    }
    else if( LO_freq>=55000 && LO_freq<60000)
    {
	R828_Freq_Info.OPEN_D=0x08; // low
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x8B;     //R27[7:0]  band7,band4
	R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 0;
    }	
    else if( LO_freq>=60000 && LO_freq<65000)
    {
	R828_Freq_Info.OPEN_D=0x08; // low
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x7B;     //R27[7:0]  band8,band4
	R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 0;
    }
    else if( LO_freq>=65000 && LO_freq<70000)
    {
	R828_Freq_Info.OPEN_D=0x08; // low
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x69;     //R27[7:0]  band9,band6
	R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 0;
    }	
    else if( LO_freq>=70000 && LO_freq<75000)
    {
	R828_Freq_Info.OPEN_D=0x08; // low
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x58;     //R27[7:0]  band10,band7
	R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 0;
    }
    else if( LO_freq>=75000 && LO_freq<80000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x44;     //R27[7:0]  band11,band11
	R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 0;
    }
    else if( LO_freq>=80000 && LO_freq<90000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x44;     //R27[7:0]  band11,band11
	R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 0;
    }
    else if( LO_freq>=90000 && LO_freq<100000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x34;     //R27[7:0]  band12,band11
	R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 0;
    }
    else if( LO_freq>=100000 && LO_freq<110000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x34;     //R27[7:0]  band12,band11
	R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)   
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 0;
    }
    else if( LO_freq>=110000 && LO_freq<120000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x24;     //R27[7:0]  band13,band11
	R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 1;
    }
    else if( LO_freq>=120000 && LO_freq<140000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x24;     //R27[7:0]  band13,band11
	R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 1;
    }
    else if( LO_freq>=140000 && LO_freq<180000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x14;     //R27[7:0]  band14,band11
	R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)  
	R828_Freq_Info.XTAL_CAP10P=0x01; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 1;
    }
    else if( LO_freq>=180000 && LO_freq<220000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x13;     //R27[7:0]  band14,band12
	R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)  
	R828_Freq_Info.XTAL_CAP10P=0x00; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 1;
    }
    else if( LO_freq>=220000 && LO_freq<250000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x13;     //R27[7:0]  band14,band12
	R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)  
	R828_Freq_Info.XTAL_CAP10P=0x00; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 2;
    }
    else if( LO_freq>=250000 && LO_freq<280000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x11;     //R27[7:0]  highest,highest
	R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)  
	R828_Freq_Info.XTAL_CAP10P=0x00; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 2;
    }
    else if( LO_freq>=280000 && LO_freq<310000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
	R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
	R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)  
	R828_Freq_Info.XTAL_CAP10P=0x00; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 2;
    }
    else if( LO_freq>=310000 && LO_freq<450000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x41;  //R26[7:6]=1 (bypass)  R26[1:0]=1 (middle)
	R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
	R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)  
	R828_Freq_Info.XTAL_CAP10P=0x00; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 2;
    }
    else if( LO_freq>=450000 && LO_freq<588000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x41;  //R26[7:6]=1 (bypass)  R26[1:0]=1 (middle)
	R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
	R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)  
	R828_Freq_Info.XTAL_CAP10P=0x00; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 3;
    }
    else if( LO_freq>=588000 && LO_freq<650000)
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x40;  //R26[7:6]=1 (bypass)  R26[1:0]=0 (highest)
	R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
	R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)  
	R828_Freq_Info.XTAL_CAP10P=0x00; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 3;
    }
    else
    {
	R828_Freq_Info.OPEN_D=0x00; // high
	R828_Freq_Info.RF_MUX_PLOY = 0x40;  //R26[7:6]=1 (bypass)  R26[1:0]=0 (highest)
	R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
	R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)  
	R828_Freq_Info.XTAL_CAP10P=0x00; 
	R828_Freq_Info.XTAL_CAP0P=0x00;
	R828_Freq_Info.IMR_MEM = 4;
    }

    return R828_Freq_Info;
}

SysFreq_Info_Type R828_SysFreq_Sel(R828_Standard_Type R828_Standard, uint32_t RF_freq)
{
    SysFreq_Info_Type R828_SysFreq_Info;

    R828_SysFreq_Info.LNA_TOP=0xE5;		// Detect BW 3, LNA TOP:4, PreDet Top:2
    R828_SysFreq_Info.LNA_VTH_L=0x53;		// LNA VTH 0.84	,  VTL 0.64
    R828_SysFreq_Info.MIXER_TOP=0x24;	    // MIXER TOP:13 , TOP-1, low-discharge
    R828_SysFreq_Info.MIXER_VTH_L=0x75;	// MIXER VTH 1.04, VTL 0.84
    R828_SysFreq_Info.AIR_CABLE1_IN=0x00;
    R828_SysFreq_Info.CABLE2_IN=0x00;
    R828_SysFreq_Info.PRE_DECT=0x40;
    R828_SysFreq_Info.LNA_DISCHARGE=14;
    R828_SysFreq_Info.CP_CUR=0x38;            // 111, auto
    R828_SysFreq_Info.DIV_BUF_CUR=0x30; // 11, 150u
    R828_SysFreq_Info.FILTER_CUR=0x40;    // 10, low

    switch(R828_Standard)
    {

	case DVB_T_6M:
	case DVB_T_7M:
	case DVB_T_7M_2:
	case DVB_T_8M:
	    if( (RF_freq==506000) || (RF_freq==666000) || (RF_freq==818000) )
	    {
		R828_SysFreq_Info.MIXER_TOP=0x14;	    // MIXER TOP:14 , TOP-1, low-discharge
		R828_SysFreq_Info.CP_CUR=0x28;            //101, 0.2
		R828_SysFreq_Info.DIV_BUF_CUR=0x20; // 10, 200u
	    }
	    break;

	case ISDB_T:
	    R828_SysFreq_Info.LNA_VTH_L=0x75;		// LNA VTH 1.04	,  VTL 0.84
	    break;

	case DVB_T2_6M:
	case DVB_T2_7M: 
	case DVB_T2_7M_2:
	case DVB_T2_8M:
	default:
	    break;
    }

#if(USE_DIPLEXER==true)
    if ((CHIP_VARIANT==R820C) || (CHIP_VARIANT==R820T) || (CHIP_VARIANT==R828S))
    {
	if(RF_freq < DIP_FREQ)
	    R828_SysFreq_Info.AIR_CABLE1_IN = 0x60; //cable-1 in, air off
    }
#endif
    return R828_SysFreq_Info;

}

int R828_Xtal_Check(void *pTuner)
{
    uint8_t ArrayNum;
    R828_I2C_TYPE  R828_I2C;

    ArrayNum = 27;
    for(ArrayNum=0;ArrayNum<27;ArrayNum++)
    {
	R828_Arry[ArrayNum] = R828_iniArry[ArrayNum];
    }

    //cap 30pF & Drive Low
    R828_I2C.RegAddr = 0x10;
    R828_Arry[11]    = (R828_Arry[11] & 0xF4) | 0x0B ;
    R828_I2C.Data    = R828_Arry[11];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //set pll autotune = 128kHz
    R828_I2C.RegAddr = 0x1A;
    R828_Arry[21]    = R828_Arry[21] & 0xF3;
    R828_I2C.Data    = R828_Arry[21];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //set manual initial reg = 111111; 
    R828_I2C.RegAddr = 0x13;
    R828_Arry[14]    = (R828_Arry[14] & 0x80) | 0x7F;
    R828_I2C.Data    = R828_Arry[14];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //set auto
    R828_I2C.RegAddr = 0x13;
    R828_Arry[14]    = (R828_Arry[14] & 0xBF);
    R828_I2C.Data    = R828_Arry[14];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_Delay_MS(pTuner, 5);

    R828_I2C_Len.RegAddr = 0x00;
    R828_I2C_Len.Len     = 3;
    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
	return -1;

    // if 30pF unlock, set to cap 20pF
#if (USE_16M_XTAL==true)
    //VCO=2360MHz for 16M Xtal. VCO band 26
    if(((R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((R828_I2C_Len.Data[2] & 0x3F) > 29) || ((R828_I2C_Len.Data[2] & 0x3F) < 23)) 
#else
	if(((R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((R828_I2C_Len.Data[2] & 0x3F) == 0x3F)) 
#endif
	{
	    //cap 20pF 
	    R828_I2C.RegAddr = 0x10;
	    R828_Arry[11]    = (R828_Arry[11] & 0xFC) | 0x02;
	    R828_I2C.Data    = R828_Arry[11];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    R828_Delay_MS(pTuner, 5);

	    R828_I2C_Len.RegAddr = 0x00;
	    R828_I2C_Len.Len     = 3;
	    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
		return -1;

	    // if 20pF unlock, set to cap 10pF
#if (USE_16M_XTAL==true)
	    if(((R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((R828_I2C_Len.Data[2] & 0x3F) > 29) || ((R828_I2C_Len.Data[2] & 0x3F) < 23)) 
#else
		if(((R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((R828_I2C_Len.Data[2] & 0x3F) == 0x3F)) 
#endif
		{
		    //cap 10pF 
		    R828_I2C.RegAddr = 0x10;
		    R828_Arry[11]    = (R828_Arry[11] & 0xFC) | 0x01;
		    R828_I2C.Data    = R828_Arry[11];
		    if(I2C_Write(pTuner, &R828_I2C) != 0)
			return -1;

		    R828_Delay_MS(pTuner, 5);

		    R828_I2C_Len.RegAddr = 0x00;
		    R828_I2C_Len.Len     = 3;
		    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
			return -1;

		    // if 10pF unlock, set to cap 0pF
#if (USE_16M_XTAL==true)
		    if(((R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((R828_I2C_Len.Data[2] & 0x3F) > 29) || ((R828_I2C_Len.Data[2] & 0x3F) < 23)) 
#else
			if(((R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((R828_I2C_Len.Data[2] & 0x3F) == 0x3F)) 
#endif 
			{
			    //cap 0pF 
			    R828_I2C.RegAddr = 0x10;
			    R828_Arry[11]    = (R828_Arry[11] & 0xFC) | 0x00;
			    R828_I2C.Data    = R828_Arry[11];
			    if(I2C_Write(pTuner, &R828_I2C) != 0)
				return -1;

			    R828_Delay_MS(pTuner, 5);

			    R828_I2C_Len.RegAddr = 0x00;
			    R828_I2C_Len.Len     = 3;
			    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
				return -1;

			    // if unlock, set to high drive
#if (USE_16M_XTAL==true)
			    if(((R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((R828_I2C_Len.Data[2] & 0x3F) > 29) || ((R828_I2C_Len.Data[2] & 0x3F) < 23)) 
#else
				if(((R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((R828_I2C_Len.Data[2] & 0x3F) == 0x3F)) 
#endif 
				{
				    //X'tal drive high
				    R828_I2C.RegAddr = 0x10;
				    R828_Arry[11]    = (R828_Arry[11] & 0xF7) ;
				    R828_I2C.Data    = R828_Arry[11];
				    if(I2C_Write(pTuner, &R828_I2C) != 0)
					return -1;

				    //R828_Delay_MS(15);
				    R828_Delay_MS(pTuner, 20);

				    R828_I2C_Len.RegAddr = 0x00;
				    R828_I2C_Len.Len     = 3;
				    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
					return -1;

#if (USE_16M_XTAL==true)
				    if(((R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((R828_I2C_Len.Data[2] & 0x3F) > 29) || ((R828_I2C_Len.Data[2] & 0x3F) < 23)) 
#else
					if(((R828_I2C_Len.Data[2] & 0x40) == 0x00) || ((R828_I2C_Len.Data[2] & 0x3F) == 0x3F)) 
#endif
					{
					    return -1;
					}
					else //0p+high drive lock
					{
					    Xtal_cap_sel_tmp = XTAL_HIGH_CAP_0P;
					}
				}
				else //0p lock
				{
				    Xtal_cap_sel_tmp = XTAL_LOW_CAP_0P;
				}
			}
			else //10p lock
			{   
			    Xtal_cap_sel_tmp = XTAL_LOW_CAP_10P;
			}
		}
		else //20p lock
		{
		    Xtal_cap_sel_tmp = XTAL_LOW_CAP_20P;
		}
	}
	else // 30p lock
	{
	    Xtal_cap_sel_tmp = XTAL_LOW_CAP_30P;
	}

    return 0;
}	

int R828_Init(void *pTuner)
{
    uint8_t i;

    if(R828_IMR_done_flag==false)
    {

	//Do Xtal check
	if(
		(CHIP_VARIANT==R820T) || 
		(CHIP_VARIANT==R828S) || 
		(CHIP_VARIANT==R820C))
	{
	    Xtal_cap_sel = XTAL_HIGH_CAP_0P;
	}
	else
	{
	    if(R828_Xtal_Check(pTuner) != 0)        //1st
		return -1;

	    Xtal_cap_sel = Xtal_cap_sel_tmp;

	    if(R828_Xtal_Check(pTuner) != 0)        //2nd
		return -1;

	    if(Xtal_cap_sel_tmp > Xtal_cap_sel)
	    {
		Xtal_cap_sel = Xtal_cap_sel_tmp;
	    }

	    if(R828_Xtal_Check(pTuner) != 0)        //3rd
		return -1;

	    if(Xtal_cap_sel_tmp > Xtal_cap_sel)
	    {
		Xtal_cap_sel = Xtal_cap_sel_tmp;
	    }

	}

	//reset filter cal.
	for (i=0; i<STD_SIZE; i++)
	{	  
	    R828_Fil_Cal_flag[i] = false;
	    R828_Fil_Cal_code[i] = 0;
	}

#if 0
	//start imr cal.
	if(R828_InitReg(pTuner) != 0)        //write initial reg before doing cal
	    return -1;

	if(R828_IMR_Prepare(pTuner) != 0)
	    return -1;

	if(R828_IMR(pTuner, 3, true) != 0)       //Full K node 3
	    return -1;

	if(R828_IMR(pTuner, 1, false) != 0)
	    return -1;

	if(R828_IMR(pTuner, 0, false) != 0)
	    return -1;

	if(R828_IMR(pTuner, 2, false) != 0)
	    return -1;

	if(R828_IMR(pTuner, 4, false) != 0)
	    return -1;

	R828_IMR_done_flag = true;
#endif
    }

    //write initial reg
    if(R828_InitReg(pTuner) != 0)        
	return -1;

    return 0;
}



int R828_InitReg(void *pTuner)
{
    uint8_t InitArryCount;
    uint8_t InitArryNum;

    InitArryCount = 0;
    InitArryNum  = 27;

    //uint32_t LO_KHz      = 0;

    //Write Full Table
    R828_I2C_Len.RegAddr = 0x05;
    R828_I2C_Len.Len     = InitArryNum;
    for(InitArryCount = 0;InitArryCount < InitArryNum;InitArryCount ++)
    {
	R828_I2C_Len.Data[InitArryCount] = R828_iniArry[InitArryCount];
    }
    if(I2C_Write_Len(pTuner, &R828_I2C_Len) != 0)
	return -1;

    return 0;
}


int R828_IMR_Prepare(void *pTuner)

{
    uint8_t ArrayNum;
    R828_I2C_TYPE  R828_I2C;

    ArrayNum=27;

    for(ArrayNum=0;ArrayNum<27;ArrayNum++)
    {
	R828_Arry[ArrayNum] = R828_iniArry[ArrayNum];
    }
    //IMR Preparation    
    //lna off (air-in off)
    R828_I2C.RegAddr = 0x05;
    R828_Arry[0]     = R828_Arry[0]  | 0x20;
    R828_I2C.Data    = R828_Arry[0];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1; 
    //mixer gain mode = manual
    R828_I2C.RegAddr = 0x07;
    R828_Arry[2]     = (R828_Arry[2] & 0xEF);
    R828_I2C.Data    = R828_Arry[2];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    //filter corner = lowest
    R828_I2C.RegAddr = 0x0A;
    R828_Arry[5]     = R828_Arry[5] | 0x0F;
    R828_I2C.Data    = R828_Arry[5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    //filter bw=+2cap, hp=5M
    R828_I2C.RegAddr = 0x0B;
    R828_Arry[6]    = (R828_Arry[6] & 0x90) | 0x60;
    R828_I2C.Data    = R828_Arry[6];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    //adc=on, vga code mode, gain = 26.5dB  
    R828_I2C.RegAddr = 0x0C;
    R828_Arry[7]    = (R828_Arry[7] & 0x60) | 0x0B;
    R828_I2C.Data    = R828_Arry[7];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    //ring clk = on
    R828_I2C.RegAddr = 0x0F;
    R828_Arry[10]   &= 0xF7;
    R828_I2C.Data    = R828_Arry[10];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    //ring power = on
    R828_I2C.RegAddr = 0x18;
    R828_Arry[19]    = R828_Arry[19] | 0x10;
    R828_I2C.Data    = R828_Arry[19];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    //from ring = ring pll in
    R828_I2C.RegAddr = 0x1C;
    R828_Arry[23]    = R828_Arry[23] | 0x02;
    R828_I2C.Data    = R828_Arry[23];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    //sw_pdect = det3
    R828_I2C.RegAddr = 0x1E;
    R828_Arry[25]    = R828_Arry[25] | 0x80;
    R828_I2C.Data    = R828_Arry[25];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    // Set filt_3dB
    R828_Arry[1]  = R828_Arry[1] | 0x20;  
    R828_I2C.RegAddr  = 0x06;
    R828_I2C.Data     = R828_Arry[1];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;
}

int R828_IMR(void *pTuner, uint8_t IMR_MEM, int IM_Flag)
{

    uint32_t RingVCO = 0;
    uint32_t RingFreq = 0;
    uint32_t RingRef = 0;
    uint8_t n_ring = 0;
    uint8_t n;

    R828_I2C_TYPE  R828_I2C;
    R828_SectType IMR_POINT;

    if (R828_Xtal>24000)
	RingRef = R828_Xtal /2;
    else
	RingRef = R828_Xtal;

    for(n=0; n<16; n++)
    {
	if((16+n)* 8 * RingRef >= 3100000) 
	{
	    n_ring=n;
	    break;
	}

	if(n==15)   //n_ring not found
	{
	    //return -1;
	    n_ring=n;
	}
    }

    R828_Arry[19] &= 0xF0;      //set ring[3:0]
    R828_Arry[19] |= n_ring;
    RingVCO = (16+n_ring)* 8 * RingRef;
    R828_Arry[19]&=0xDF;   //clear ring_se23
    R828_Arry[20]&=0xFC;   //clear ring_seldiv
    R828_Arry[26]&=0xFC;   //clear ring_att

    switch(IMR_MEM)
    {
	case 0:
	    RingFreq = RingVCO/48;
	    R828_Arry[19]|=0x20;  // ring_se23 = 1
	    R828_Arry[20]|=0x03;  // ring_seldiv = 3
	    R828_Arry[26]|=0x02;  // ring_att 10
	    break;
	case 1:
	    RingFreq = RingVCO/16;
	    R828_Arry[19]|=0x00;  // ring_se23 = 0
	    R828_Arry[20]|=0x02;  // ring_seldiv = 2
	    R828_Arry[26]|=0x00;  // pw_ring 00
	    break;
	case 2:
	    RingFreq = RingVCO/8;
	    R828_Arry[19]|=0x00;  // ring_se23 = 0
	    R828_Arry[20]|=0x01;  // ring_seldiv = 1
	    R828_Arry[26]|=0x03;  // pw_ring 11
	    break;
	case 3:
	    RingFreq = RingVCO/6;
	    R828_Arry[19]|=0x20;  // ring_se23 = 1
	    R828_Arry[20]|=0x00;  // ring_seldiv = 0
	    R828_Arry[26]|=0x03;  // pw_ring 11
	    break;
	case 4:
	default:
	    RingFreq = RingVCO/4;
	    R828_Arry[19]|=0x00;  // ring_se23 = 0
	    R828_Arry[20]|=0x00;  // ring_seldiv = 0
	    R828_Arry[26]|=0x01;  // pw_ring 01
	    break;
    }


    //write pw_ring,n_ring,ringdiv2 to I2C

    //------------n_ring,ring_se23----------//
    R828_I2C.RegAddr = 0x18;
    R828_I2C.Data    = R828_Arry[19];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    //------------ring_sediv----------------//
    R828_I2C.RegAddr = 0x19;
    R828_I2C.Data    = R828_Arry[20];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    //------------pw_ring-------------------//
    R828_I2C.RegAddr = 0x1f;
    R828_I2C.Data    = R828_Arry[26];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //Must do before PLL() 
    if(R828_MUX(pTuner, RingFreq - 5300) != 0) 	//MUX input freq ~ RF_in Freq
	return -1;

    if(R828_PLL(pTuner, (RingFreq - 5300) * 1000, STD_SIZE) != 0)	//set pll freq = ring freq - 6M
	return -1;

    if(IM_Flag == true)
    {
	if(R828_IQ(pTuner, &IMR_POINT) != 0)
	    return -1;
    }
    else
    {
	IMR_POINT.Gain_X = IMR_Data[3].Gain_X;
	IMR_POINT.Phase_Y = IMR_Data[3].Phase_Y;
	IMR_POINT.Value = IMR_Data[3].Value;
	if(R828_F_IMR(pTuner, &IMR_POINT) != 0)
	    return -1;
    }

    //Save IMR Value
    if(IMR_MEM > 4){
	IMR_Data[4].Gain_X  = IMR_POINT.Gain_X;
	IMR_Data[4].Phase_Y = IMR_POINT.Phase_Y;
	IMR_Data[4].Value   = IMR_POINT.Value;
    }else{
	IMR_Data[IMR_MEM].Gain_X  = IMR_POINT.Gain_X;
	IMR_Data[IMR_MEM].Phase_Y = IMR_POINT.Phase_Y;
	IMR_Data[IMR_MEM].Value   = IMR_POINT.Value;
    }
    return 0;
}

int R828_PLL(void *pTuner, uint64_t LO_freq, R828_Standard_Type R828_Standard)
{

    //	R820T_EXTRA_MODULE *pExtra;

    uint8_t  MixDiv;
    uint8_t  DivBuf;
    uint8_t  Ni;
    uint8_t  Si;
    uint8_t  DivNum;
    uint8_t  Nint;
    uint32_t VCO_Min_kHz;
    uint32_t VCO_Max_kHz;
    uint64_t VCO_Freq;
    uint32_t PLL_Ref;		//Max 24000 (kHz)
    uint32_t VCO_Fra;		//VCO contribution by SDM (kHz)
    uint16_t Nsdm;
    uint16_t SDM;
    uint16_t SDM16to9;
    uint16_t SDM8to1;
    //uint8_t  Judge    = 0;
    uint8_t  VCO_fine_tune;

    R828_I2C_TYPE  R828_I2C;

    MixDiv   = 2;
    DivBuf   = 0;
    Ni       = 0;
    Si       = 0;
    DivNum   = 0;
    Nint     = 0;
    VCO_Min_kHz  = 1770000;
    VCO_Max_kHz  = VCO_Min_kHz*2;
    VCO_Freq = 0;
    PLL_Ref	= 0;		//Max 24000 (kHz)
    VCO_Fra	= 0;		//VCO contribution by SDM (kHz)
    Nsdm		= 2;
    SDM		= 0;
    SDM16to9	= 0;
    SDM8to1  = 0;
    //uint8_t  Judge    = 0;
    VCO_fine_tune = 0;

#if 0
    if ((CHIP_VARIANT==R620D) || (CHIP_VARIANT==R828D) || (CHIP_VARIANT==R828))  //X'tal can't not exceed 20MHz for ATV
    {
	if(R828_Standard <= SECAM_L1)	  //ref set refdiv2, reffreq = Xtal/2 on ATV application
	{
	    R828_Arry[11] |= 0x10; //b4=1
	    PLL_Ref = R828_Xtal /2;
	}
	else //DTV, FilCal, IMR
	{
	    R828_Arry[11] &= 0xEF;
	    PLL_Ref = R828_Xtal;
	}
    }
    else
    {
	if(R828_Xtal > 24000)
	{
	    R828_Arry[11] |= 0x10; //b4=1
	    PLL_Ref = R828_Xtal /2;
	}
	else
	{
	    R828_Arry[11] &= 0xEF;
	    PLL_Ref = R828_Xtal;
	}
    }
#endif
    //FIXME hack
    R828_Arry[11] &= 0xEF;
    PLL_Ref = rtlsdr_get_tuner_clock(pTuner);

    R828_I2C.RegAddr = 0x10;
    R828_I2C.Data = R828_Arry[11];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //set pll autotune = 128kHz
    R828_I2C.RegAddr = 0x1A;
    R828_Arry[21]    = R828_Arry[21] & 0xF3;
    R828_I2C.Data    = R828_Arry[21];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //Set VCO current = 100
    R828_I2C.RegAddr = 0x12;
    R828_Arry[13]    = (R828_Arry[13] & 0x1F) | 0x80; 
    R828_I2C.Data    = R828_Arry[13];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //Divider
    while(MixDiv <= 64)
    {
	if((((LO_freq/1000) * MixDiv) >= VCO_Min_kHz) && (((LO_freq/1000) * MixDiv) < VCO_Max_kHz))
	{
	    DivBuf = MixDiv;
	    while(DivBuf > 2)
	    {
		DivBuf = DivBuf >> 1;
		DivNum ++;
	    }
	    break;
	}
	MixDiv = MixDiv << 1;
    }

    R828_I2C_Len.RegAddr = 0x00;
    R828_I2C_Len.Len     = 5;
    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
	return -1;	

    VCO_fine_tune = (R828_I2C_Len.Data[4] & 0x30)>>4;

    if(VCO_fine_tune > VCO_pwr_ref)
	DivNum = DivNum - 1;
    else if(VCO_fine_tune < VCO_pwr_ref)
	DivNum = DivNum + 1; 

    R828_I2C.RegAddr = 0x10;
    R828_Arry[11] &= 0x1F;
    R828_Arry[11] |= (DivNum << 5);
    R828_I2C.Data = R828_Arry[11];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    VCO_Freq = LO_freq * (uint64_t)MixDiv;
    Nint     = (uint8_t) (VCO_Freq / 2 / PLL_Ref);
    VCO_Fra  = (uint16_t) ((VCO_Freq - 2 * PLL_Ref * Nint) / 1000);

    //FIXME hack
    PLL_Ref /= 1000;

    //	printf("VCO_Freq = %lu, Nint= %u, VCO_Fra= %lu, LO_freq= %u, MixDiv= %u\n", VCO_Freq, Nint, VCO_Fra, LO_freq, MixDiv);

    //boundary spur prevention
    if (VCO_Fra < PLL_Ref/64)           //2*PLL_Ref/128
	VCO_Fra = 0;
    else if (VCO_Fra > PLL_Ref*127/64)  //2*PLL_Ref*127/128
    {
	VCO_Fra = 0;
	Nint ++;
    }
    else if((VCO_Fra > PLL_Ref*127/128) && (VCO_Fra < PLL_Ref)) //> 2*PLL_Ref*127/256,  < 2*PLL_Ref*128/256
	VCO_Fra = PLL_Ref*127/128;      // VCO_Fra = 2*PLL_Ref*127/256
    else if((VCO_Fra > PLL_Ref) && (VCO_Fra < PLL_Ref*129/128)) //> 2*PLL_Ref*128/256,  < 2*PLL_Ref*129/256
	VCO_Fra = PLL_Ref*129/128;      // VCO_Fra = 2*PLL_Ref*129/256
    else
	VCO_Fra = VCO_Fra;

    if (Nint > 63) {
	fprintf(stderr, "[R820T] No valid PLL values for %u kHz!\n", (uint32_t)(LO_freq/1000));
	return -1;
    }

    //N & S
    Ni       = (Nint - 13) / 4;
    Si       = Nint - 4 *Ni - 13;
    R828_I2C.RegAddr = 0x14;
    R828_Arry[15]  = 0x00;
    R828_Arry[15] |= (Ni + (Si << 6));
    R828_I2C.Data = R828_Arry[15];

    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //pw_sdm
    R828_I2C.RegAddr = 0x12;
    R828_Arry[13] &= 0xF7;
    if(VCO_Fra == 0)
	R828_Arry[13] |= 0x08;
    R828_I2C.Data = R828_Arry[13];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //SDM calculator
    while(VCO_Fra > 1)
    {			
	if (VCO_Fra > (2*PLL_Ref / Nsdm))
	{		
	    SDM = SDM + 32768 / (Nsdm/2);
	    VCO_Fra = VCO_Fra - 2*PLL_Ref / Nsdm;
	    if (Nsdm >= 0x8000)
		break;
	}
	Nsdm = Nsdm << 1;
    }

    SDM16to9 = SDM >> 8;
    SDM8to1 =  SDM - (SDM16to9 << 8);

    R828_I2C.RegAddr = 0x16;
    R828_Arry[17]    = (uint8_t) SDM16to9;
    R828_I2C.Data    = R828_Arry[17];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
    R828_I2C.RegAddr = 0x15;
    R828_Arry[16]    = (uint8_t) SDM8to1;
    R828_I2C.Data    = R828_Arry[16];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //	R828_Delay_MS(10);

    if (
	    (CHIP_VARIANT==R620D) ||
	    (CHIP_VARIANT==R828D) || 
	    (CHIP_VARIANT==R828))
    {
	if(R828_Standard <= SECAM_L1)
	    R828_Delay_MS(pTuner, 20);
	else
	    R828_Delay_MS(pTuner, 10);
    }
    else
    {
	R828_Delay_MS(pTuner, 10);
    }

    //check PLL lock status
    R828_I2C_Len.RegAddr = 0x00;
    R828_I2C_Len.Len     = 3;
    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
	return -1;

    if( (R828_I2C_Len.Data[2] & 0x40) == 0x00 )
    {
	fprintf(stderr, "[R820T] PLL not locked for %u kHz!\n", (uint32_t)(LO_freq/1000));
	R828_I2C.RegAddr = 0x12;
	R828_Arry[13]    = (R828_Arry[13] & 0x1F) | 0x60;  //increase VCO current
	R828_I2C.Data    = R828_Arry[13];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	return -1;
    }

    //set pll autotune = 8kHz
    R828_I2C.RegAddr = 0x1A;
    R828_Arry[21]    = R828_Arry[21] | 0x08;
    R828_I2C.Data    = R828_Arry[21];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;
}

int R828_MUX(void *pTuner, uint64_t freq)
{	
    uint8_t RT_Reg08 = 0;
    uint8_t RT_Reg09 = 0;
    R828_I2C_TYPE  R828_I2C;

    //Freq_Info_Type Freq_Info1;
    Freq_Info1 = R828_Freq_Sel(freq / 1000);

    // Open Drain
    R828_I2C.RegAddr = 0x17;
    R828_Arry[18] = (R828_Arry[18] & 0xF7) | Freq_Info1.OPEN_D;
    R828_I2C.Data = R828_Arry[18];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // RF_MUX,Polymux 
    R828_I2C.RegAddr = 0x1A;
    R828_Arry[21] = (R828_Arry[21] & 0x3C) | Freq_Info1.RF_MUX_PLOY;
    R828_I2C.Data = R828_Arry[21];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // TF BAND
    R828_I2C.RegAddr = 0x1B;
    R828_Arry[22] &= 0x00;
    R828_Arry[22] |= Freq_Info1.TF_C;	
    R828_I2C.Data = R828_Arry[22];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // XTAL CAP & Drive
    R828_I2C.RegAddr = 0x10;
    R828_Arry[11] &= 0xF4;
    switch(Xtal_cap_sel)
    {
	case XTAL_LOW_CAP_30P:
	case XTAL_LOW_CAP_20P:
	    R828_Arry[11] = R828_Arry[11] | Freq_Info1.XTAL_CAP20P | 0x08;
	    break;

	case XTAL_LOW_CAP_10P:
	    R828_Arry[11] = R828_Arry[11] | Freq_Info1.XTAL_CAP10P | 0x08;
	    break;

	case XTAL_LOW_CAP_0P:
	    R828_Arry[11] = R828_Arry[11] | Freq_Info1.XTAL_CAP0P | 0x08;
	    break;

	case XTAL_HIGH_CAP_0P:
	    R828_Arry[11] = R828_Arry[11] | Freq_Info1.XTAL_CAP0P | 0x00;
	    break;

	default:
	    R828_Arry[11] = R828_Arry[11] | Freq_Info1.XTAL_CAP0P | 0x08;
	    break;
    }
    R828_I2C.Data    = R828_Arry[11];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //Set_IMR
    if(R828_IMR_done_flag == true)
    {
	RT_Reg08 = IMR_Data[Freq_Info1.IMR_MEM].Gain_X & 0x3F;
	RT_Reg09 = IMR_Data[Freq_Info1.IMR_MEM].Phase_Y & 0x3F;
    }
    else
    {
	RT_Reg08 = 0;
	RT_Reg09 = 0;
    }

    R828_I2C.RegAddr = 0x08;
    R828_Arry[3] = R828_iniArry[3] & 0xC0;
    R828_Arry[3] = R828_Arry[3] | RT_Reg08;
    R828_I2C.Data = R828_Arry[3];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x09;
    R828_Arry[4] = R828_iniArry[4] & 0xC0;
    R828_Arry[4] = R828_Arry[4] | RT_Reg09;
    R828_I2C.Data =R828_Arry[4]  ;
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;
}

int R828_IQ(void *pTuner, R828_SectType* IQ_Pont)
{
    R828_SectType Compare_IQ[3];
    //	R828_SectType CompareTemp;
    //	uint8_t IQ_Count  = 0;
    uint8_t VGA_Count;
    uint16_t VGA_Read;
    uint8_t  X_Direction;  // 1:X, 0:Y
    R828_I2C_TYPE  R828_I2C;

    VGA_Count = 0;
    VGA_Read = 0;

    // increase VGA power to let image significant
    for(VGA_Count = 12;VGA_Count < 16;VGA_Count ++)
    {
	R828_I2C.RegAddr = 0x0C;
	R828_I2C.Data    = (R828_Arry[7] & 0xF0) + VGA_Count;  
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	R828_Delay_MS(pTuner, 10); //

	if(R828_Muti_Read(pTuner, 0x01, &VGA_Read) != 0)
	    return -1;

	if(VGA_Read > 40*4)
	    break;
    }

    //initial 0x08, 0x09
    //Compare_IQ[0].Gain_X  = 0x40; //should be 0xC0 in R828, Jason
    //Compare_IQ[0].Phase_Y = 0x40; //should be 0x40 in R828
    Compare_IQ[0].Gain_X  = R828_iniArry[3] & 0xC0; // Jason modified, clear b[5], b[4:0]
    Compare_IQ[0].Phase_Y = R828_iniArry[4] & 0xC0; //

    //while(IQ_Count < 3)
    //{
    // Determine X or Y
    if(R828_IMR_Cross(pTuner, &Compare_IQ[0], &X_Direction) != 0)
	return -1;

    //if(X_Direction==1)
    //{
    //    if(R828_IQ_Tree(Compare_IQ[0].Phase_Y, Compare_IQ[0].Gain_X, 0x09, &Compare_IQ[0]) != 0) //X
    //	  return -1;
    //}
    //else
    //{
    //   if(R828_IQ_Tree(Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != 0) //Y
    //	return -1;
    //}

    /*
    //--- X direction ---//
    //X: 3 points
    if(R828_IQ_Tree(Compare_IQ[0].Phase_Y, Compare_IQ[0].Gain_X, 0x09, &Compare_IQ[0]) != 0) //
    return -1;

    //compare and find min of 3 points. determine I/Q direction
    if(R828_CompreCor(&Compare_IQ[0]) != 0)
    return -1;

    //increase step to find min value of this direction
    if(R828_CompreStep(&Compare_IQ[0], 0x08) != 0)
    return -1;
    */

    if(X_Direction==1)
    {
	//compare and find min of 3 points. determine I/Q direction
	if(R828_CompreCor(&Compare_IQ[0]) != 0)
	    return -1;

	//increase step to find min value of this direction
	if(R828_CompreStep(pTuner, &Compare_IQ[0], 0x08) != 0)  //X
	    return -1;
    }
    else
    {
	//compare and find min of 3 points. determine I/Q direction
	if(R828_CompreCor(&Compare_IQ[0]) != 0)
	    return -1;

	//increase step to find min value of this direction
	if(R828_CompreStep(pTuner, &Compare_IQ[0], 0x09) != 0)  //Y
	    return -1;
    }
    /*
    //--- Y direction ---//
    //Y: 3 points
    if(R828_IQ_Tree(Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != 0) //
    return -1;

    //compare and find min of 3 points. determine I/Q direction
    if(R828_CompreCor(&Compare_IQ[0]) != 0)
    return -1;

    //increase step to find min value of this direction
    if(R828_CompreStep(&Compare_IQ[0], 0x09) != 0)
    return -1;
    */

    //Another direction
    if(X_Direction==1)
    {	    
	if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != 0) //Y
	    return -1;

	//compare and find min of 3 points. determine I/Q direction
	if(R828_CompreCor(&Compare_IQ[0]) != 0)
	    return -1;

	//increase step to find min value of this direction
	if(R828_CompreStep(pTuner, &Compare_IQ[0], 0x09) != 0)  //Y
	    return -1;
    }
    else
    {
	if(R828_IQ_Tree(pTuner, Compare_IQ[0].Phase_Y, Compare_IQ[0].Gain_X, 0x09, &Compare_IQ[0]) != 0) //X
	    return -1;

	//compare and find min of 3 points. determine I/Q direction
	if(R828_CompreCor(&Compare_IQ[0]) != 0)
	    return -1;

	//increase step to find min value of this direction
	if(R828_CompreStep(pTuner, &Compare_IQ[0], 0x08) != 0) //X
	    return -1;
    }
    //CompareTemp = Compare_IQ[0];

    //--- Check 3 points again---//
    if(X_Direction==1)
    {
	if(R828_IQ_Tree(pTuner, Compare_IQ[0].Phase_Y, Compare_IQ[0].Gain_X, 0x09, &Compare_IQ[0]) != 0) //X
	    return -1;
    }
    else
    {
	if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != 0) //Y
	    return -1;
    }

    //if(R828_IQ_Tree(Compare_IQ[0].Phase_Y, Compare_IQ[0].Gain_X, 0x09, &Compare_IQ[0]) != 0) //
    //	return -1;

    if(R828_CompreCor(&Compare_IQ[0]) != 0)
	return -1;

    //if((CompareTemp.Gain_X == Compare_IQ[0].Gain_X) && (CompareTemp.Phase_Y == Compare_IQ[0].Phase_Y))//Ben Check
    //	break;

    //IQ_Count ++;
    //}
    //if(IQ_Count ==  3)
    //	return -1;

    //Section-4 Check 
    /*
       CompareTemp = Compare_IQ[0];
       for(IQ_Count = 0;IQ_Count < 5;IQ_Count ++)
       {
       if(R828_Section(&Compare_IQ[0]) != 0)
       return -1;

       if((CompareTemp.Gain_X == Compare_IQ[0].Gain_X) && (CompareTemp.Phase_Y == Compare_IQ[0].Phase_Y))
       break;
       }
       */

    //Section-9 check
    //if(R828_F_IMR(&Compare_IQ[0]) != 0)
    if(R828_Section(pTuner, &Compare_IQ[0]) != 0)
	return -1;

    *IQ_Pont = Compare_IQ[0];

    //reset gain/phase control setting
    R828_I2C.RegAddr = 0x08;
    R828_I2C.Data    = R828_iniArry[3] & 0xC0; //Jason
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x09;
    R828_I2C.Data    = R828_iniArry[4] & 0xC0;
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;
}

//--------------------------------------------------------------------------------------------
// Purpose: record IMC results by input gain/phase location
//          then adjust gain or phase positive 1 step and negtive 1 step, both record results
// input: FixPot: phase or gain
//        FlucPot phase or gain
//        PotReg: 0x08 or 0x09
//        CompareTree: 3 IMR trace and results
// output: TREU or false
//--------------------------------------------------------------------------------------------
int R828_IQ_Tree(void *pTuner, uint8_t FixPot, uint8_t FlucPot, uint8_t PotReg, R828_SectType* CompareTree)
{
    uint8_t TreeCount;
    uint8_t TempPot = 0;
    uint8_t PntReg;
    R828_I2C_TYPE  R828_I2C;

    if(PotReg == 0x08)
	PntReg = 0x09; //phase control
    else
	PntReg = 0x08; //gain control

    for(TreeCount = 0; TreeCount < 3; TreeCount ++)
    {
	R828_I2C.RegAddr = PotReg;
	R828_I2C.Data    = FixPot;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	R828_I2C.RegAddr = PntReg;
	R828_I2C.Data    = FlucPot;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	if(R828_Muti_Read(pTuner, 0x01, &CompareTree[TreeCount].Value) != 0)
	    return -1;

	if(PotReg == 0x08)
	{
	    CompareTree[TreeCount].Gain_X  = FixPot;
	    CompareTree[TreeCount].Phase_Y = FlucPot;
	}
	else
	{
	    CompareTree[TreeCount].Phase_Y  = FixPot;
	    CompareTree[TreeCount].Gain_X = FlucPot;
	}

	if(TreeCount == 0)   //try right-side point
	    FlucPot ++; 
	else if(TreeCount == 1) //try left-side point
	{
	    if((FlucPot & 0x1F) < 0x02) //if absolute location is 1, change I/Q direction
	    {
		TempPot = 2 - (FlucPot & 0x1F);
		if(FlucPot & 0x20) //b[5]:I/Q selection. 0:Q-path, 1:I-path
		{
		    FlucPot &= 0xC0;
		    FlucPot |= TempPot;
		}
		else
		{
		    FlucPot |= (0x20 | TempPot);
		}
	    }
	    else
		FlucPot -= 2;  
	}
    }

    return 0;
}

//-----------------------------------------------------------------------------------/ 
// Purpose: compare IMC result aray [0][1][2], find min value and store to CorArry[0]
// input: CorArry: three IMR data array
// output: true or false
//-----------------------------------------------------------------------------------/
int R828_CompreCor(R828_SectType* CorArry)
{
    uint8_t CompCount;
    R828_SectType CorTemp;

    CompCount = 0;

    for(CompCount = 3;CompCount > 0;CompCount --)
    {
	if(CorArry[0].Value > CorArry[CompCount - 1].Value) //compare IMC result [0][1][2], find min value
	{
	    CorTemp = CorArry[0];
	    CorArry[0] = CorArry[CompCount - 1];
	    CorArry[CompCount - 1] = CorTemp;
	}
    }

    return 0;
}

//-------------------------------------------------------------------------------------//
// Purpose: if (Gain<9 or Phase<9), Gain+1 or Phase+1 and compare with min value
//          new < min => update to min and continue
//          new > min => Exit
// input: StepArry: three IMR data array
//        Pace: gain or phase register
// output: true or false 
//-------------------------------------------------------------------------------------//
int R828_CompreStep(void *pTuner, R828_SectType* StepArry, uint8_t Pace)
{
    R828_SectType StepTemp;
    R828_I2C_TYPE  R828_I2C;

    //min value already saved in StepArry[0]
    StepTemp.Phase_Y = StepArry[0].Phase_Y;
    StepTemp.Gain_X  = StepArry[0].Gain_X;

    while(((StepTemp.Gain_X & 0x1F) < IMR_TRIAL) && ((StepTemp.Phase_Y & 0x1F) < IMR_TRIAL))  //5->10
    {
	if(Pace == 0x08)
	    StepTemp.Gain_X ++;
	else
	    StepTemp.Phase_Y ++;

	R828_I2C.RegAddr = 0x08;
	R828_I2C.Data    = StepTemp.Gain_X ;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	R828_I2C.RegAddr = 0x09;
	R828_I2C.Data    = StepTemp.Phase_Y;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	if(R828_Muti_Read(pTuner, 0x01, &StepTemp.Value) != 0)
	    return -1;

	if(StepTemp.Value <= StepArry[0].Value)
	{
	    StepArry[0].Gain_X  = StepTemp.Gain_X;
	    StepArry[0].Phase_Y = StepTemp.Phase_Y;
	    StepArry[0].Value   = StepTemp.Value;
	}
	else
	{
	    break;		
	}

    } //end of while()

    return 0;
}

//-----------------------------------------------------------------------------------/ 
// Purpose: read multiple IMC results for stability
// input: IMR_Reg: IMC result address
//        IMR_Result_Data: result 
// output: true or false
//-----------------------------------------------------------------------------------/
int R828_Muti_Read(void *pTuner, uint8_t IMR_Reg, uint16_t* IMR_Result_Data)  //jason modified
{
    uint8_t ReadCount;
    uint16_t ReadAmount;
    uint8_t ReadMax;
    uint8_t ReadMin;
    uint8_t ReadData;

    ReadCount     = 0;
    ReadAmount  = 0;
    ReadMax = 0;
    ReadMin = 255;
    ReadData = 0;

    R828_Delay_MS(pTuner, 5);

    for(ReadCount = 0;ReadCount < 6;ReadCount ++)
    {
	R828_I2C_Len.RegAddr = 0x00;
	R828_I2C_Len.Len     = IMR_Reg + 1;  //IMR_Reg = 0x01
	if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
	    return -1;

	ReadData = R828_I2C_Len.Data[1];

	ReadAmount = ReadAmount + (uint16_t)ReadData;

	if(ReadData < ReadMin)
	    ReadMin = ReadData;

	if(ReadData > ReadMax)
	    ReadMax = ReadData;
    }
    *IMR_Result_Data = ReadAmount - (uint16_t)ReadMax - (uint16_t)ReadMin;

    return 0;
}

int R828_Section(void *pTuner, R828_SectType* IQ_Pont)
{
    R828_SectType Compare_IQ[3];
    R828_SectType Compare_Bet[3];

    //Try X-1 column and save min result to Compare_Bet[0]
    if((IQ_Pont->Gain_X & 0x1F) == 0x00)
    {
	/*
	   if((IQ_Pont->Gain_X & 0xE0) == 0x40) //bug => only compare b[5],     
	   Compare_IQ[0].Gain_X = 0x61; // Gain=1, I-path //Jason
	   else
	   Compare_IQ[0].Gain_X = 0x41; // Gain=1, Q-path
	   */
	Compare_IQ[0].Gain_X = ((IQ_Pont->Gain_X) & 0xDF) + 1;  //Q-path, Gain=1
    }
    else
	Compare_IQ[0].Gain_X  = IQ_Pont->Gain_X - 1;  //left point
    Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;

    if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != 0)  // y-direction
	return -1;

    if(R828_CompreCor(&Compare_IQ[0]) != 0)
	return -1;

    Compare_Bet[0].Gain_X = Compare_IQ[0].Gain_X;
    Compare_Bet[0].Phase_Y = Compare_IQ[0].Phase_Y;
    Compare_Bet[0].Value = Compare_IQ[0].Value;

    //Try X column and save min result to Compare_Bet[1]
    Compare_IQ[0].Gain_X = IQ_Pont->Gain_X;
    Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;

    if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != 0)
	return -1;

    if(R828_CompreCor(&Compare_IQ[0]) != 0)
	return -1;

    Compare_Bet[1].Gain_X = Compare_IQ[0].Gain_X;
    Compare_Bet[1].Phase_Y = Compare_IQ[0].Phase_Y;
    Compare_Bet[1].Value = Compare_IQ[0].Value;

    //Try X+1 column and save min result to Compare_Bet[2]
    if((IQ_Pont->Gain_X & 0x1F) == 0x00)		
	Compare_IQ[0].Gain_X = ((IQ_Pont->Gain_X) | 0x20) + 1;  //I-path, Gain=1
    else
	Compare_IQ[0].Gain_X = IQ_Pont->Gain_X + 1;
    Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;

    if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != 0)
	return -1;

    if(R828_CompreCor(&Compare_IQ[0]) != 0)
	return -1;

    Compare_Bet[2].Gain_X = Compare_IQ[0].Gain_X;
    Compare_Bet[2].Phase_Y = Compare_IQ[0].Phase_Y;
    Compare_Bet[2].Value = Compare_IQ[0].Value;

    if(R828_CompreCor(&Compare_Bet[0]) != 0)
	return -1;

    *IQ_Pont = Compare_Bet[0];

    return 0;
}

int R828_IMR_Cross(void *pTuner, R828_SectType* IQ_Pont, uint8_t* X_Direct)
{

    R828_SectType Compare_Cross[5]; //(0,0)(0,Q-1)(0,I-1)(Q-1,0)(I-1,0)
    R828_SectType Compare_Temp;
    uint8_t CrossCount = 0;
    uint8_t Reg08 = R828_iniArry[3] & 0xC0;
    uint8_t Reg09 = R828_iniArry[4] & 0xC0;	
    R828_I2C_TYPE  R828_I2C;

    Compare_Temp.Gain_X = 0;
    Compare_Temp.Phase_Y = 0;
    Compare_Temp.Value = 255;

    for(CrossCount=0; CrossCount<5; CrossCount++)
    {
	if(CrossCount==0)
	{
	    Compare_Cross[CrossCount].Gain_X = Reg08;
	    Compare_Cross[CrossCount].Phase_Y = Reg09;
	}
	else if(CrossCount==1)
	{
	    Compare_Cross[CrossCount].Gain_X = Reg08;       //0
	    Compare_Cross[CrossCount].Phase_Y = Reg09 + 1;  //Q-1
	}
	else if(CrossCount==2)
	{
	    Compare_Cross[CrossCount].Gain_X = Reg08;               //0
	    Compare_Cross[CrossCount].Phase_Y = (Reg09 | 0x20) + 1; //I-1
	}
	else if(CrossCount==3)
	{
	    Compare_Cross[CrossCount].Gain_X = Reg08 + 1; //Q-1
	    Compare_Cross[CrossCount].Phase_Y = Reg09;
	}
	else
	{
	    Compare_Cross[CrossCount].Gain_X = (Reg08 | 0x20) + 1; //I-1
	    Compare_Cross[CrossCount].Phase_Y = Reg09;
	}

	R828_I2C.RegAddr = 0x08;
	R828_I2C.Data    = Compare_Cross[CrossCount].Gain_X;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	R828_I2C.RegAddr = 0x09;
	R828_I2C.Data    = Compare_Cross[CrossCount].Phase_Y;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	if(R828_Muti_Read(pTuner, 0x01, &Compare_Cross[CrossCount].Value) != 0)
	    return -1;

	if( Compare_Cross[CrossCount].Value < Compare_Temp.Value)
	{
	    Compare_Temp.Value = Compare_Cross[CrossCount].Value;
	    Compare_Temp.Gain_X = Compare_Cross[CrossCount].Gain_X;
	    Compare_Temp.Phase_Y = Compare_Cross[CrossCount].Phase_Y;		
	}
    } //end for loop


    if((Compare_Temp.Phase_Y & 0x1F)==1)  //y-direction
    {
	*X_Direct = (uint8_t) 0;
	IQ_Pont[0].Gain_X = Compare_Cross[0].Gain_X;
	IQ_Pont[0].Phase_Y = Compare_Cross[0].Phase_Y;
	IQ_Pont[0].Value = Compare_Cross[0].Value;

	IQ_Pont[1].Gain_X = Compare_Cross[1].Gain_X;
	IQ_Pont[1].Phase_Y = Compare_Cross[1].Phase_Y;
	IQ_Pont[1].Value = Compare_Cross[1].Value;

	IQ_Pont[2].Gain_X = Compare_Cross[2].Gain_X;
	IQ_Pont[2].Phase_Y = Compare_Cross[2].Phase_Y;
	IQ_Pont[2].Value = Compare_Cross[2].Value;
    }
    else //(0,0) or x-direction
    {	
	*X_Direct = (uint8_t) 1;
	IQ_Pont[0].Gain_X = Compare_Cross[0].Gain_X;
	IQ_Pont[0].Phase_Y = Compare_Cross[0].Phase_Y;
	IQ_Pont[0].Value = Compare_Cross[0].Value;

	IQ_Pont[1].Gain_X = Compare_Cross[3].Gain_X;
	IQ_Pont[1].Phase_Y = Compare_Cross[3].Phase_Y;
	IQ_Pont[1].Value = Compare_Cross[3].Value;

	IQ_Pont[2].Gain_X = Compare_Cross[4].Gain_X;
	IQ_Pont[2].Phase_Y = Compare_Cross[4].Phase_Y;
	IQ_Pont[2].Value = Compare_Cross[4].Value;
    }
    return 0;
}

//----------------------------------------------------------------------------------------//
// purpose: search surrounding points from previous point 
//          try (x-1), (x), (x+1) columns, and find min IMR result point
// input: IQ_Pont: previous point data(IMR Gain, Phase, ADC Result, RefRreq)
//                 will be updated to final best point                 
// output: true or false
//----------------------------------------------------------------------------------------//
int R828_F_IMR(void *pTuner, R828_SectType* IQ_Pont)
{
    R828_SectType Compare_IQ[3];
    R828_SectType Compare_Bet[3];
    uint8_t VGA_Count;
    uint16_t VGA_Read = 0;
    R828_I2C_TYPE  R828_I2C;

    //VGA
    for(VGA_Count = 12;VGA_Count < 16;VGA_Count ++)
    {
	R828_I2C.RegAddr = 0x0C;
	R828_I2C.Data    = (R828_Arry[7] & 0xF0) + VGA_Count;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	R828_Delay_MS(pTuner, 10);

	if(R828_Muti_Read(pTuner, 0x01, &VGA_Read) != 0)
	    return -1;

	if(VGA_Read > 40*4)
	    break;
    }

    //Try X-1 column and save min result to Compare_Bet[0]
    if((IQ_Pont->Gain_X & 0x1F) == 0x00)
    {
	Compare_IQ[0].Gain_X = ((IQ_Pont->Gain_X) & 0xDF) + 1;  //Q-path, Gain=1
    }
    else
	Compare_IQ[0].Gain_X  = IQ_Pont->Gain_X - 1;  //left point
    Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;

    if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != 0)  // y-direction
	return -1;

    if(R828_CompreCor(&Compare_IQ[0]) != 0)
	return -1;

    Compare_Bet[0].Gain_X = Compare_IQ[0].Gain_X;
    Compare_Bet[0].Phase_Y = Compare_IQ[0].Phase_Y;
    Compare_Bet[0].Value = Compare_IQ[0].Value;

    //Try X column and save min result to Compare_Bet[1]
    Compare_IQ[0].Gain_X = IQ_Pont->Gain_X;
    Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;

    if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != 0)
	return -1;

    if(R828_CompreCor(&Compare_IQ[0]) != 0)
	return -1;

    Compare_Bet[1].Gain_X = Compare_IQ[0].Gain_X;
    Compare_Bet[1].Phase_Y = Compare_IQ[0].Phase_Y;
    Compare_Bet[1].Value = Compare_IQ[0].Value;

    //Try X+1 column and save min result to Compare_Bet[2]
    if((IQ_Pont->Gain_X & 0x1F) == 0x00)		
	Compare_IQ[0].Gain_X = ((IQ_Pont->Gain_X) | 0x20) + 1;  //I-path, Gain=1
    else
	Compare_IQ[0].Gain_X = IQ_Pont->Gain_X + 1;
    Compare_IQ[0].Phase_Y = IQ_Pont->Phase_Y;

    if(R828_IQ_Tree(pTuner, Compare_IQ[0].Gain_X, Compare_IQ[0].Phase_Y, 0x08, &Compare_IQ[0]) != 0)
	return -1;

    if(R828_CompreCor(&Compare_IQ[0]) != 0)
	return -1;

    Compare_Bet[2].Gain_X = Compare_IQ[0].Gain_X;
    Compare_Bet[2].Phase_Y = Compare_IQ[0].Phase_Y;
    Compare_Bet[2].Value = Compare_IQ[0].Value;

    if(R828_CompreCor(&Compare_Bet[0]) != 0)
	return -1;

    *IQ_Pont = Compare_Bet[0];

    return 0;
}

int R828_GPIO(void *pTuner, char value)
{
    R828_I2C_TYPE  R828_I2C;
    if(value)
	R828_Arry[10] |= 0x01;
    else
	R828_Arry[10] &= 0xFE;

    R828_I2C.RegAddr = 0x0F;
    R828_I2C.Data    = R828_Arry[10];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;
}

int R828_SetStandard(void *pTuner, R828_Standard_Type RT_Standard)
{

    // Used Normal Arry to Modify
    uint8_t ArrayNum;
    R828_I2C_TYPE  R828_I2C;

    for(ArrayNum=0;ArrayNum<27;ArrayNum++)
    {
	R828_Arry[ArrayNum] = R828_iniArry[ArrayNum];
    }

    // Record Init Flag & Xtal_check Result
    if(R828_IMR_done_flag == true)
	R828_Arry[7]    = (R828_Arry[7] & 0xF0) | 0x01 | (Xtal_cap_sel<<1);
    else
	R828_Arry[7]    = (R828_Arry[7] & 0xF0) | 0x00;

    R828_I2C.RegAddr = 0x0C;
    R828_I2C.Data    = R828_Arry[7];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // Record version
    R828_I2C.RegAddr = 0x13;
    R828_Arry[14]    = (R828_Arry[14] & 0xC0) | VER_NUM;
    R828_I2C.Data    = R828_Arry[14];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;


    //for LT Gain test
    if(RT_Standard > SECAM_L1)
    {
	R828_I2C.RegAddr = 0x1D;  //[5:3] LNA TOP
	R828_I2C.Data = (R828_Arry[24] & 0xC7) | 0x00;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	//R828_Delay_MS(1);
    }

    // Look Up System Dependent Table
    Sys_Info1 = R828_Sys_Sel(RT_Standard);
    R828_IF_khz = Sys_Info1.IF_KHz;
    R828_CAL_LO_khz = Sys_Info1.FILT_CAL_LO;

    // Filter Calibration
    if(R828_Fil_Cal_flag[RT_Standard] == false)
    {
	// do filter calibration 
	if(R828_Filt_Cal(pTuner, Sys_Info1.FILT_CAL_LO,Sys_Info1.BW) != 0)
	    return -1;


	// read and set filter code
	R828_I2C_Len.RegAddr = 0x00;
	R828_I2C_Len.Len     = 5;
	if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
	    return -1;

	R828_Fil_Cal_code[RT_Standard] = R828_I2C_Len.Data[4] & 0x0F;

	//Filter Cali. Protection
	if(R828_Fil_Cal_code[RT_Standard]==0 || R828_Fil_Cal_code[RT_Standard]==15)
	{
	    if(R828_Filt_Cal(pTuner, Sys_Info1.FILT_CAL_LO,Sys_Info1.BW) != 0)
		return -1;

	    R828_I2C_Len.RegAddr = 0x00;
	    R828_I2C_Len.Len     = 5;
	    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
		return -1;

	    R828_Fil_Cal_code[RT_Standard] = R828_I2C_Len.Data[4] & 0x0F;

	    if(R828_Fil_Cal_code[RT_Standard]==15) //narrowest
		R828_Fil_Cal_code[RT_Standard] = 0;

	}
	R828_Fil_Cal_flag[RT_Standard] = true;
    }

    // Set Filter Q
    R828_Arry[5]  = (R828_Arry[5] & 0xE0) | Sys_Info1.FILT_Q | R828_Fil_Cal_code[RT_Standard];  
    R828_I2C.RegAddr  = 0x0A;
    R828_I2C.Data     = R828_Arry[5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // Set BW, Filter_gain, & HP corner
    R828_Arry[6]= (R828_Arry[6] & 0x10) | Sys_Info1.HP_COR;
    R828_I2C.RegAddr  = 0x0B;
    R828_I2C.Data     = R828_Arry[6];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // Set Img_R
    R828_Arry[2]  = (R828_Arry[2] & 0x7F) | Sys_Info1.IMG_R;  
    R828_I2C.RegAddr  = 0x07;
    R828_I2C.Data     = R828_Arry[2];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;


    // Set filt_3dB, V6MHz
    R828_Arry[1]  = (R828_Arry[1] & 0xCF) | Sys_Info1.FILT_GAIN;  
    R828_I2C.RegAddr  = 0x06;
    R828_I2C.Data     = R828_Arry[1];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //channel filter extension
    R828_Arry[25]  = (R828_Arry[25] & 0x9F) | Sys_Info1.EXT_ENABLE;  
    R828_I2C.RegAddr  = 0x1E;
    R828_I2C.Data     = R828_Arry[25];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;


    //Loop through
    R828_Arry[0]  = (R828_Arry[0] & 0x7F) | Sys_Info1.LOOP_THROUGH;  
    R828_I2C.RegAddr  = 0x05;
    R828_I2C.Data     = R828_Arry[0];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //Loop through attenuation
    R828_Arry[26]  = (R828_Arry[26] & 0x7F) | Sys_Info1.LT_ATT;  
    R828_I2C.RegAddr  = 0x1F;
    R828_I2C.Data     = R828_Arry[26];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //filter extention widest
    R828_Arry[10]  = (R828_Arry[10] & 0x7F) | Sys_Info1.FLT_EXT_WIDEST;  
    R828_I2C.RegAddr  = 0x0F;
    R828_I2C.Data     = R828_Arry[10];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //RF poly filter current
    R828_Arry[20]  = (R828_Arry[20] & 0x9F) | Sys_Info1.POLYFIL_CUR;  
    R828_I2C.RegAddr  = 0x19;
    R828_I2C.Data     = R828_Arry[20];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;
}

int R828_Filt_Cal(void *pTuner, uint32_t Cal_Freq,BW_Type R828_BW)
{
    R828_I2C_TYPE  R828_I2C;
    //set in Sys_sel()
    /*
       if(R828_BW == BW_8M)
       {
    //set filt_cap = no cap
    R828_I2C.RegAddr = 0x0B;  //reg11
    R828_Arry[6]   &= 0x9F;  //filt_cap = no cap
    R828_I2C.Data    = R828_Arry[6];		
    }
    else if(R828_BW == BW_7M)
    {
    //set filt_cap = +1 cap
    R828_I2C.RegAddr = 0x0B;  //reg11
    R828_Arry[6]   &= 0x9F;  //filt_cap = no cap
    R828_Arry[6]   |= 0x20;  //filt_cap = +1 cap
    R828_I2C.Data    = R828_Arry[6];		
    }
    else if(R828_BW == BW_6M)
    {
    //set filt_cap = +2 cap
    R828_I2C.RegAddr = 0x0B;  //reg11
    R828_Arry[6]   &= 0x9F;  //filt_cap = no cap
    R828_Arry[6]   |= 0x60;  //filt_cap = +2 cap
    R828_I2C.Data    = R828_Arry[6];		
    }


    if(I2C_Write(pTuner, &R828_I2C) != 0)
    return -1;	
    */

    // Set filt_cap
    R828_I2C.RegAddr  = 0x0B;
    R828_Arry[6]= (R828_Arry[6] & 0x9F) | (Sys_Info1.HP_COR & 0x60);
    R828_I2C.Data     = R828_Arry[6];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;


    //set cali clk =on
    R828_I2C.RegAddr = 0x0F;  //reg15
    R828_Arry[10]   |= 0x04;  //calibration clk=on
    R828_I2C.Data    = R828_Arry[10];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //X'tal cap 0pF for PLL
    R828_I2C.RegAddr = 0x10;
    R828_Arry[11]    = (R828_Arry[11] & 0xFC) | 0x00;
    R828_I2C.Data    = R828_Arry[11];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //Set PLL Freq = Filter Cali Freq
    if(R828_PLL(pTuner, Cal_Freq * 1000, STD_SIZE) != 0)
	return -1;

    //Start Trigger
    R828_I2C.RegAddr = 0x0B;	//reg11
    R828_Arry[6]   |= 0x10;	    //vstart=1	
    R828_I2C.Data    = R828_Arry[6];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //delay 0.5ms
    R828_Delay_MS(pTuner, 1);  

    //Stop Trigger
    R828_I2C.RegAddr = 0x0B;
    R828_Arry[6]   &= 0xEF;     //vstart=0
    R828_I2C.Data    = R828_Arry[6];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;


    //set cali clk =off
    R828_I2C.RegAddr  = 0x0F;	//reg15
    R828_Arry[10]    &= 0xFB;	//calibration clk=off
    R828_I2C.Data     = R828_Arry[10];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;

}

int R828_SetFrequency(void *pTuner, uint64_t freq, R828_Standard_Type R828_Standard, char fast_mode)
{
    uint64_t LO_freq = freq;
    R828_I2C_TYPE  R828_I2C;

    /*
       assert(freq < 900000000);
       assert(freq > 40000000);
       */

    if(R828_Standard==SECAM_L1)
	LO_freq -= Sys_Info1.IF_KHz * 1000;
    else
	LO_freq += Sys_Info1.IF_KHz * 1000;

    //Set MUX dependent var. Must do before PLL( ) 
    if(R828_MUX(pTuner, LO_freq) != 0)
	return -1;

    //Set PLL
    if(R828_PLL(pTuner, LO_freq, R828_Standard) != 0)
	return -1;

    R828_IMR_point_num = Freq_Info1.IMR_MEM;


    //Set TOP,VTH,VTL
    SysFreq_Info1 = R828_SysFreq_Sel(R828_Standard, freq/1000);


    // write DectBW, pre_dect_TOP
    R828_Arry[24] = (R828_Arry[24] & 0x38) | (SysFreq_Info1.LNA_TOP & 0xC7);
    R828_I2C.RegAddr = 0x1D;
    R828_I2C.Data = R828_Arry[24];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // write MIXER TOP, TOP+-1
    R828_Arry[23] = (R828_Arry[23] & 0x07) | (SysFreq_Info1.MIXER_TOP & 0xF8); 
    R828_I2C.RegAddr = 0x1C;
    R828_I2C.Data = R828_Arry[23];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;


    // write LNA VTHL
    R828_Arry[8] = (R828_Arry[8] & 0x00) | SysFreq_Info1.LNA_VTH_L;
    R828_I2C.RegAddr = 0x0D;
    R828_I2C.Data = R828_Arry[8];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // write MIXER VTHL
    R828_Arry[9] = (R828_Arry[9] & 0x00) | SysFreq_Info1.MIXER_VTH_L;
    R828_I2C.RegAddr = 0x0E;
    R828_I2C.Data = R828_Arry[9];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // Cable-1/Air in 
    R828_I2C.RegAddr = 0x05;
    R828_Arry[0] &= 0x9F;
    R828_Arry[0] |= SysFreq_Info1.AIR_CABLE1_IN;
    R828_I2C.Data = R828_Arry[0];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // Cable-2 in 
    R828_I2C.RegAddr = 0x06;
    R828_Arry[1] &= 0xF7;
    R828_Arry[1] |= SysFreq_Info1.CABLE2_IN;
    R828_I2C.Data = R828_Arry[1];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //CP current
    R828_I2C.RegAddr = 0x11;
    R828_Arry[12] &= 0xC7;
    R828_Arry[12] |= SysFreq_Info1.CP_CUR;	
    R828_I2C.Data = R828_Arry[12];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;	

    //div buffer current
    R828_I2C.RegAddr = 0x17;
    R828_Arry[18] &= 0xCF;
    R828_Arry[18] |= SysFreq_Info1.DIV_BUF_CUR;
    R828_I2C.Data = R828_Arry[18];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;	

    // Set channel filter current 
    R828_I2C.RegAddr  = 0x0A;
    R828_Arry[5]  = (R828_Arry[5] & 0x9F) | SysFreq_Info1.FILTER_CUR;  
    R828_I2C.Data     = R828_Arry[5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //Air-In only for Astrometa
    R828_Arry[0] =  (R828_Arry[0] & 0x9F) | 0x00;
    R828_Arry[1] =  (R828_Arry[1] & 0xF7) | 0x00;

    R828_I2C.RegAddr = 0x05;
    R828_I2C.Data = R828_Arry[0];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x06;
    R828_I2C.Data = R828_Arry[1];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //Set LNA
    if(R828_Standard > SECAM_L1)
    {

	if(fast_mode)
	{
	    //R828_Arry[24] = (R828_Arry[24] & 0xC7) | 0x20; //LNA TOP:4
	    R828_Arry[24] = (R828_Arry[24] & 0xC7) | 0x00; //LNA TOP:lowest
	    R828_I2C.RegAddr = 0x1D;
	    R828_I2C.Data = R828_Arry[24];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    R828_Arry[23] = (R828_Arry[23] & 0xFB);  // 0: normal mode
	    R828_I2C.RegAddr = 0x1C;
	    R828_I2C.Data = R828_Arry[23];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    R828_Arry[1]  = (R828_Arry[1] & 0xBF);   //0: PRE_DECT off
	    R828_I2C.RegAddr  = 0x06;
	    R828_I2C.Data     = R828_Arry[1];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    //agc clk 250hz
	    R828_Arry[21]  = (R828_Arry[21] & 0xCF) | 0x30;
	    R828_I2C.RegAddr  = 0x1A;
	    R828_I2C.Data     = R828_Arry[21];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;
	}
	else  //NORMAL mode
	{

	    R828_Arry[24] = (R828_Arry[24] & 0xC7) | 0x00; //LNA TOP:lowest
	    R828_I2C.RegAddr = 0x1D;
	    R828_I2C.Data = R828_Arry[24];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    R828_Arry[23] = (R828_Arry[23] & 0xFB);  // 0: normal mode
	    R828_I2C.RegAddr = 0x1C;
	    R828_I2C.Data = R828_Arry[23];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    R828_Arry[1]  = (R828_Arry[1] & 0xBF);   //0: PRE_DECT off
	    R828_I2C.RegAddr  = 0x06;
	    R828_I2C.Data     = R828_Arry[1];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    //agc clk 250hz
	    R828_Arry[21]  = (R828_Arry[21] & 0xCF) | 0x30;   //250hz
	    R828_I2C.RegAddr  = 0x1A;
	    R828_I2C.Data     = R828_Arry[21];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    R828_Delay_MS(pTuner, 250);

	    // PRE_DECT on
	    /*
	       R828_Arry[1]  = (R828_Arry[1] & 0xBF) | SysFreq_Info1.PRE_DECT;
	       R828_I2C.RegAddr  = 0x06;
	       R828_I2C.Data     = R828_Arry[1];
	       if(I2C_Write(pTuner, &R828_I2C) != 0)
	       return -1;			 
	       */
	    // write LNA TOP = 3
	    //R828_Arry[24] = (R828_Arry[24] & 0xC7) | (SysFreq_Info1.LNA_TOP & 0x38);
	    R828_Arry[24] = (R828_Arry[24] & 0xC7) | 0x18;  //TOP=3
	    R828_I2C.RegAddr = 0x1D;
	    R828_I2C.Data = R828_Arry[24];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    // write discharge mode
	    R828_Arry[23] = (R828_Arry[23] & 0xFB) | (SysFreq_Info1.MIXER_TOP & 0x04);
	    R828_I2C.RegAddr = 0x1C;
	    R828_I2C.Data = R828_Arry[23];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    // LNA discharge current
	    R828_Arry[25]  = (R828_Arry[25] & 0xE0) | SysFreq_Info1.LNA_DISCHARGE;
	    R828_I2C.RegAddr  = 0x1E;
	    R828_I2C.Data     = R828_Arry[25];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;

	    //agc clk 60hz 
	    R828_Arry[21]  = (R828_Arry[21] & 0xCF) | 0x20;
	    R828_I2C.RegAddr  = 0x1A;
	    R828_I2C.Data     = R828_Arry[21];
	    if(I2C_Write(pTuner, &R828_I2C) != 0)
		return -1;
	}
    }
    else 
    {
	/*
	// PRE_DECT on
	R828_Arry[1]  = (R828_Arry[1] & 0xBF) | SysFreq_Info1.PRE_DECT;
	R828_I2C.RegAddr  = 0x06;
	R828_I2C.Data     = R828_Arry[1];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;
	*/
	// PRE_DECT off
	R828_Arry[1]  = (R828_Arry[1] & 0xBF);   //0: PRE_DECT off
	R828_I2C.RegAddr  = 0x06;
	R828_I2C.Data     = R828_Arry[1];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	// write LNA TOP
	R828_Arry[24] = (R828_Arry[24] & 0xC7) | (SysFreq_Info1.LNA_TOP & 0x38);
	R828_I2C.RegAddr = 0x1D;
	R828_I2C.Data = R828_Arry[24];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	// write discharge mode
	R828_Arry[23] = (R828_Arry[23] & 0xFB) | (SysFreq_Info1.MIXER_TOP & 0x04); 
	R828_I2C.RegAddr = 0x1C;
	R828_I2C.Data = R828_Arry[23];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	// LNA discharge current
	R828_Arry[25]  = (R828_Arry[25] & 0xE0) | SysFreq_Info1.LNA_DISCHARGE;  
	R828_I2C.RegAddr  = 0x1E;
	R828_I2C.Data     = R828_Arry[25];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	// agc clk 1Khz, external det1 cap 1u
	R828_Arry[21]  = (R828_Arry[21] & 0xCF) | 0x00;   			
	R828_I2C.RegAddr  = 0x1A;
	R828_I2C.Data     = R828_Arry[21];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	R828_Arry[11]  = (R828_Arry[11] & 0xFB) | 0x00;   			
	R828_I2C.RegAddr  = 0x10;
	R828_I2C.Data     = R828_Arry[11];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;
    }

    return 0;

}

int R828_Standby(void *pTuner, char loop_through)
{
    R828_I2C_TYPE  R828_I2C;
    if(loop_through)
    {
	R828_I2C.RegAddr = 0x06;
	R828_I2C.Data    = 0xB1;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;
	R828_I2C.RegAddr = 0x05;
	R828_I2C.Data = 0x03;


	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;
    }
    else
    {
	R828_I2C.RegAddr = 0x05;
	R828_I2C.Data    = 0xA3;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	R828_I2C.RegAddr = 0x06;
	R828_I2C.Data    = 0xB1;
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;
    }

    R828_I2C.RegAddr = 0x07;
    R828_I2C.Data    = 0x3A;
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x08;
    R828_I2C.Data    = 0x40;
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x09;
    R828_I2C.Data    = 0xC0;   //polyfilter off
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x0A;
    R828_I2C.Data    = 0x36;
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x0C;
    R828_I2C.Data    = 0x35;
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x0F;
    R828_I2C.Data    = 0x68; /* was 0x78, which turns off CLK_Out */
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x11;
    R828_I2C.Data    = 0x03;
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x17;
    R828_I2C.Data    = 0xF4;
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x19;
    R828_I2C.Data    = 0x0C;
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;


    return 0;
}

int R828_GetRfGain(void *pTuner, R828_RF_Gain_Info *pR828_rf_gain)
{

    R828_I2C_Len.RegAddr = 0x00;
    R828_I2C_Len.Len     = 4;
    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
	return -1;

    pR828_rf_gain->RF_gain1 = (R828_I2C_Len.Data[3] & 0x0F);
    pR828_rf_gain->RF_gain2 = ((R828_I2C_Len.Data[3] & 0xF0) >> 4);
    pR828_rf_gain->RF_gain_comb = pR828_rf_gain->RF_gain1*2 + pR828_rf_gain->RF_gain2;

    return 0;
}


/* measured with a Racal 6103E GSM test set at 928 MHz with -60 dBm
 * input power, for raw results see:
 * http://steve-m.de/projects/rtl-sdr/gain_measurement/r820t/
 */

#define VGA_BASE_GAIN	-47
static const int r820t_vga_gain_steps[]  = {
    0, 26, 26, 30, 42, 35, 24, 13, 14, 32, 36, 34, 35, 37, 35, 36
};

static const int r820t_lna_gain_steps[]  = {
    0, 9, 13, 40, 38, 13, 31, 22, 26, 31, 26, 14, 19, 5, 35, 13
};

static const int r820t_mixer_gain_steps[]  = {
    0, 5, 10, 10, 19, 9, 10, 25, 17, 10, 8, 16, 13, 6, 3, -8
};

int R828_SetRfGain(void *pTuner, int gain)
{
    R828_I2C_TYPE  R828_I2C;
    int i, total_gain = 0;
    uint8_t mix_index = 0, lna_index = 0;

    for (i = 0; i < 15; i++) {
	if (total_gain >= gain)
	    break;

	total_gain += r820t_lna_gain_steps[++lna_index];

	if (total_gain >= gain)
	    break;

	total_gain += r820t_mixer_gain_steps[++mix_index];
    }

    /* set LNA gain */
    R828_I2C.RegAddr = 0x05;
    R828_Arry[0] = (R828_Arry[0] & 0xF0) | lna_index;
    R828_I2C.Data = R828_Arry[0];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    /* set Mixer gain */
    R828_I2C.RegAddr = 0x07;
    R828_Arry[2] = (R828_Arry[2] & 0xF0) | mix_index;
    R828_I2C.Data = R828_Arry[2];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;
}

int R828_RfGainMode(void *pTuner, int manual)
{
    uint8_t MixerGain = 0;
    uint8_t LnaGain = 0;
    R828_I2C_TYPE  R828_I2C;

    if (manual) {
	//LNA auto off
	R828_I2C.RegAddr = 0x05;
	R828_Arry[0] = R828_Arry[0] | 0x10;
	R828_I2C.Data = R828_Arry[0];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	//Mixer auto off
	R828_I2C.RegAddr = 0x07;
	R828_Arry[2] = R828_Arry[2] & 0xEF;
	R828_I2C.Data = R828_Arry[2];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	R828_I2C_Len.RegAddr = 0x00;
	R828_I2C_Len.Len     = 4; 
	if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
	    return -1;

	/* set fixed VGA gain for now (16.3 dB) */
	R828_I2C.RegAddr = 0x0C;
	R828_Arry[7]    = (R828_Arry[7] & 0x60) | 0x08;
	R828_I2C.Data    = R828_Arry[7];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;


    } else {
	//LNA
	R828_I2C.RegAddr = 0x05;
	R828_Arry[0] = R828_Arry[0] & 0xEF;
	R828_I2C.Data = R828_Arry[0];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	//Mixer
	R828_I2C.RegAddr = 0x07;
	R828_Arry[2] = R828_Arry[2] | 0x10;
	R828_I2C.Data = R828_Arry[2];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;

	/* set fixed VGA gain for now (26.5 dB) */
	R828_I2C.RegAddr = 0x0C;
	R828_Arry[7]    = (R828_Arry[7] & 0x60) | 0x0B;
	R828_I2C.Data    = R828_Arry[7];
	if(I2C_Write(pTuner, &R828_I2C) != 0)
	    return -1;
    }

    return 0;
}
