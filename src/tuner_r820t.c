/*
 * R820T tuner driver, taken from Realteks RTL2832U Linux Kernel Driver
 *
 * This driver is a mess, and should be cleaned up/rewritten.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "rtlsdr_i2c.h"
#include "tuner_r820t.h"

static const uint8_t init_state[32] = {
    0,
    0,
    0, //0x02 (1<<14) XTAL_status (ok = 1) ;  VCO_band? 0x3F ( (XTAL > 16M) => bad=0x3F, (XTAL == 16M) => ok = [23:29] ) 
    0,
    0,
    0x83, 
    0x32, 
    0x75, 

/*0x08*/
    0xC0, 
    0x40, 
    0xD6, 
    0x6C, 
    0xF5, 
    0x63,
    0x75,
    0x68, //0x0F (1<<8) XTAL_out (on=0, off=1)
    
/*0x10*/
    0x6C, //0x10 
    // (1<<8) XTAL_to_PLL_devider (1=0, 2=1) PLL_freq < 24 MHz
    // (1<<7) XTAL_drive (low =1, high=0) ; 
    // (1<<1)|(1<<0) XTAL_cap = value * 10pF
    0x83, 
    0x80, 
    0x00, //0x13 initial=0x7F; (1 <<14) (manual=0, auto=1)
    0x0F, 
    0x00, 
    0xC0,
    0x30, 
    
/*0x18*/
    0x48, 
    0xCC, 
    0x60, //0x1A (1<<2)|(1<<2) PLL_autotune (128kHz = 0)
    0x00, 
    0x54, 
    0xAE, 
    0x4A, 
    0xC0
};

static uint8_t register_state[32];

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

static int i2c_write_reg(void *pTuner, uint8_t addr, uint8_t value){
    const uint8_t data[2] = {addr, value};
    assert(addr < 32);
    int ret = rtlsdr_i2c_write_fn(pTuner, R820T_I2C_ADDR, data, 2);
    if(ret < 0) return ret;
    if(ret < 2) return -1;
    register_state[addr] = value;
    return 0;
}

#if 0
/* write a register array consisting addr/value pairs */
static int i2c_write_reg_seq(void *pTuner, const uint8_t *seq, int len){
    int ret = 0;
    int i;
    for(i = 0; i < len; i++){
	assert(seq[i*2] < 32);
    	ret = rtlsdr_i2c_write_fn(pTuner, R820T_I2C_ADDR, &seq[i*2], 2);
	if(ret < 0) return ret;
	if(ret < 2) return -1;
	register_state[seq[i*2]] = seq[i*2 +1];
    }
    return ret;
}
#endif

/* 
 * write a register array consisting (addr, mask, value) 
 * reg[addr] = (reg[addr] & mask) | value;
 * */
static int i2c_write_reg_seq_mask(void *pTuner, const uint8_t *seq, int len){
    int ret = 0;
    int i;
    for(i = 0; i < len; i++){
	uint8_t addr = seq[i*3];
	uint8_t data[2];
	assert(addr < 32);
	data[0] = addr;
	data[1] = (register_state[addr] & seq[i*3 +1]) | seq[i*3 +2];
    	ret = rtlsdr_i2c_write_fn(pTuner, R820T_I2C_ADDR, data, 2);
	if(ret < 0) return ret;
	if(ret < 2) return -1;
	register_state[addr] = data[1];
    }
    return ret;
}

static int R828_SetFrequency(void *pTuner, uint64_t freq, R828_Standard_Type R828_Standard, char fast_mode);
int r820t_SetRfFreqHz(void *pTuner, uint64_t freq)
{
    if(R828_SetFrequency(pTuner, freq, DVB_T_6M, false) != 0)
	return -1;

    return 0;
}

int r820t_SetStandby(void *pTuner, char loop_through)
{

    int ret;
    const uint8_t seq[] = {
	// addr, mask, value
	0x05, 0x00, loop_through ? 0x03 : 0xA3, // LNA
	0x06, 0x00, 0xB1,
	0x07, 0x00, 0x3A, // Mixer
	0x08, 0x00, 0x40,
	0x09, 0x00, 0xC0, //polyfilter off
	0x0A, 0x00, 0x36,
	0x0C, 0x00, 0x35,
	// don't turn it off - the rtl2832 uses it
	// 0x0F, !(1<<8), (1<<8), // XTAL clk off
	0x11, 0x00, 0x03,
	0x17, 0x00, 0xF4,
	0x19, 0x00, 0x0C,
    };

    ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
    if(ret < 0)
	return ret;

    return 0;
}

// The following context is implemented for R820T source code.

int I2C_Write_Len(void *pTuner, R828_I2C_LEN_TYPE *I2C_Info)
{
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
    unsigned int i;

    uint8_t RegStartAddr = 0x00;
    uint8_t ReadingBytes[128];
    unsigned long ByteNum = (unsigned long)I2C_Info->Len;

    if (rtlsdr_i2c_write_fn(pTuner, R820T_I2C_ADDR, &RegStartAddr, 1) < 0)
	return -1;

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

static R828_SectType IMR_Data[5] = {
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0}
};  //Please keep this array data for standby mode.

static R828_I2C_LEN_TYPE R828_I2C_Len;

uint32_t R828_IF_khz;
uint32_t R828_CAL_LO_khz;
uint8_t  R828_IMR_point_num;
uint8_t  R828_IMR_done_flag = false;
uint8_t  R828_Fil_Cal_flag[STD_SIZE];
static uint8_t R828_Fil_Cal_code[STD_SIZE];

static uint8_t Xtal_cap_sel = XTAL_LOW_CAP_0P;
//----------------------------------------------------------//
//                   Internal static struct                 //
//----------------------------------------------------------//
static Sys_Info_Type Sys_Info1;
static Freq_Info_Type Freq_Info1;
//----------------------------------------------------------//
//                   Internal Functions                     //
//----------------------------------------------------------//
static int R828_IMR(void *pTuner, uint8_t IMR_MEM, int IM_Flag);
static int R828_PLL(void *pTuner, uint64_t LO_freq, R828_Standard_Type R828_Standard);
static int R828_MUX(void *pTuner, uint64_t RF_freq);
static int R828_IQ(void *pTuner, R828_SectType* IQ_Pont);
static int R828_IQ_Tree(void *pTuner, uint8_t FixPot, uint8_t FlucPot, uint8_t PotReg, R828_SectType* CompareTree);
static int R828_CompreCor(R828_SectType* CorArry);
static int R828_CompreStep(void *pTuner, R828_SectType* StepArry, uint8_t Pace);
static int R828_Muti_Read(void *pTuner, uint8_t IMR_Reg, uint16_t* IMR_Result_Data);
static int R828_Section(void *pTuner, R828_SectType* SectionArry);
static int R828_F_IMR(void *pTuner, R828_SectType* IQ_Pont);
static int R828_IMR_Cross(void *pTuner, R828_SectType* IQ_Pont, uint8_t* X_Direct);

static Sys_Info_Type R828_Sys_Sel(R828_Standard_Type R828_Standard);
static Freq_Info_Type R828_Freq_Sel(uint32_t RF_freq);

static int R828_Filt_Cal(void *pTuner, uint32_t Cal_Freq,BW_Type R828_BW);

static Sys_Info_Type R828_Sys_Sel(R828_Standard_Type R828_Standard)
{
    Sys_Info_Type R828_Sys_Info;

    R828_Sys_Info.FILT_GAIN=0x10;  //+3dB, 6MHz on
    R828_Sys_Info.IMG_R=0x00;		//image negative
    R828_Sys_Info.FILT_Q=0x10;		//R10[4]:low Q(1'b1)
    R828_Sys_Info.EXT_ENABLE=0x60;  //R30[6]=1 ext enable; R30[5]:1 ext at LNA max-1 
    R828_Sys_Info.LOOP_THROUGH=0x00; //R5[7], LT ON
    R828_Sys_Info.LT_ATT=0x00;       //R31[7], LT ATT enable
    R828_Sys_Info.FLT_EXT_WIDEST=0x00;//R15[7]: FLT_EXT_WIDE OFF
    R828_Sys_Info.POLYFIL_CUR=0x60;  //R25[6:5]:Min

    switch (R828_Standard)
    {

	case DVB_T_6M: 
	case DVB_T2_6M: 
	    R828_Sys_Info.IF_KHz=3570;
	    R828_Sys_Info.BW=BW_6M;
	    R828_Sys_Info.FILT_CAL_LO=56000; //52000->56000
	    R828_Sys_Info.HP_COR=0x6B;		// 1.7M disable, +2cap, 1.0MHz		
	    break;

	case DVB_T_7M: 
	case DVB_T2_7M: 
	    R828_Sys_Info.IF_KHz=4070;
	    R828_Sys_Info.BW=BW_7M;
	    R828_Sys_Info.FILT_CAL_LO=60000;
	    R828_Sys_Info.HP_COR=0x2B;		// 1.7M disable, +1cap, 1.0MHz		
	    break;

	case DVB_T_7M_2:  
	case DVB_T2_7M_2:  
	    R828_Sys_Info.IF_KHz=4570;
	    R828_Sys_Info.BW=BW_7M;
	    R828_Sys_Info.FILT_CAL_LO=63000;
	    R828_Sys_Info.HP_COR=0x2A;		// 1.7M disable, +1cap, 1.25MHz		
	    break;

	case DVB_T_8M: 
	case DVB_T2_8M: 
	    R828_Sys_Info.IF_KHz=4570;
	    R828_Sys_Info.BW=BW_8M;
	    R828_Sys_Info.FILT_CAL_LO=68500;
	    R828_Sys_Info.HP_COR=0x0B;		// 1.7M disable, +0cap, 1.0MHz		
	    break;

	case ISDB_T: 
	    R828_Sys_Info.IF_KHz=4063;
	    R828_Sys_Info.BW=BW_6M;
	    R828_Sys_Info.FILT_CAL_LO=59000;
	    R828_Sys_Info.HP_COR=0x6A;		// 1.7M disable, +2cap, 1.25MHz		
	    R828_Sys_Info.EXT_ENABLE=0x40;  //R30[6], ext enable; R30[5]:0 ext at LNA max 
	    break;

	default:  //DVB_T_8M
	    R828_Sys_Info.IF_KHz=4570;
	    R828_Sys_Info.BW=BW_8M;
	    R828_Sys_Info.FILT_CAL_LO=68500;
	    R828_Sys_Info.HP_COR=0x0D;		// 1.7M disable, +0cap, 0.7MHz		
	    break;
    }

    return R828_Sys_Info;
}

static Freq_Info_Type R828_Freq_Sel(uint32_t LO_freq)
{
    Freq_Info_Type R828_Freq_Info;

    R828_Freq_Info.XTAL_CAP10P=0x01; 
    R828_Freq_Info.XTAL_CAP0P=0x00;
    if( LO_freq < 90000){
    	R828_Freq_Info.XTAL_CAP20P=0x02;  //R16[1:0]  20pF (10)
    }else if( LO_freq>=140000 && LO_freq<180000){
	R828_Freq_Info.XTAL_CAP20P=0x01;  //R16[1:0]  10pF (01)
    }else{
	R828_Freq_Info.XTAL_CAP20P=0x00;  //R16[1:0]  0pF (00)  
	R828_Freq_Info.XTAL_CAP10P=0x00; 
    }

    if(LO_freq < 75000){
	R828_Freq_Info.OPEN_D=0x08; // low
    }else{
	R828_Freq_Info.OPEN_D=0x00; // high
    }
    
    if(LO_freq < 110000){
	R828_Freq_Info.IMR_MEM = 0;
    }else if(LO_freq<220000){
	R828_Freq_Info.IMR_MEM = 1;
    }else if(LO_freq<450000){
	R828_Freq_Info.IMR_MEM = 2;
    }else if(LO_freq<650000){
	R828_Freq_Info.IMR_MEM = 3;
    }else{
	R828_Freq_Info.IMR_MEM = 4;
    }

    if(LO_freq < 310000)
    {
	R828_Freq_Info.RF_MUX_PLOY = 0x02;  //R26[7:6]=0 (LPF)  R26[1:0]=2 (low)
    }else if(LO_freq < 588000){
	R828_Freq_Info.RF_MUX_PLOY = 0x41;  //R26[7:6]=1 (bypass)  R26[1:0]=1 (middle)
    }else{
	R828_Freq_Info.RF_MUX_PLOY = 0x40;  //R26[7:6]=1 (bypass)  R26[1:0]=0 (highest)
    }

    if(LO_freq < 50000)
    {
	R828_Freq_Info.TF_C=0xDF;     //R27[7:0]  band2,band0
    }
    else if(LO_freq>=50000 && LO_freq<55000)
    {
	R828_Freq_Info.TF_C=0xBE;     //R27[7:0]  band4,band1 
    }
    else if( LO_freq>=55000 && LO_freq<60000)
    {
	R828_Freq_Info.TF_C=0x8B;     //R27[7:0]  band7,band4
    }	
    else if( LO_freq>=60000 && LO_freq<65000)
    {
	R828_Freq_Info.TF_C=0x7B;     //R27[7:0]  band8,band4
    }
    else if( LO_freq>=65000 && LO_freq<70000)
    {
	R828_Freq_Info.TF_C=0x69;     //R27[7:0]  band9,band6
    }	
    else if( LO_freq>=70000 && LO_freq<75000)
    {
	R828_Freq_Info.TF_C=0x58;     //R27[7:0]  band10,band7
    }
    else if( LO_freq>=75000 && LO_freq<80000)
    {
	R828_Freq_Info.TF_C=0x44;     //R27[7:0]  band11,band11
    }
    else if( LO_freq>=80000 && LO_freq<90000)
    {
	R828_Freq_Info.TF_C=0x44;     //R27[7:0]  band11,band11
    }
    else if( LO_freq>=90000 && LO_freq<100000)
    {
	R828_Freq_Info.TF_C=0x34;     //R27[7:0]  band12,band11
    }
    else if( LO_freq>=100000 && LO_freq<110000)
    {
	R828_Freq_Info.TF_C=0x34;     //R27[7:0]  band12,band11
    }
    else if( LO_freq>=110000 && LO_freq<120000)
    {
	R828_Freq_Info.TF_C=0x24;     //R27[7:0]  band13,band11
    }
    else if( LO_freq>=120000 && LO_freq<140000)
    {
	R828_Freq_Info.TF_C=0x24;     //R27[7:0]  band13,band11
    }
    else if( LO_freq>=140000 && LO_freq<180000)
    {
	R828_Freq_Info.TF_C=0x14;     //R27[7:0]  band14,band11
    }
    else if( LO_freq>=180000 && LO_freq<220000)
    {
	R828_Freq_Info.TF_C=0x13;     //R27[7:0]  band14,band12
    }
    else if( LO_freq>=220000 && LO_freq<250000)
    {
	R828_Freq_Info.TF_C=0x13;     //R27[7:0]  band14,band12
    }
    else if( LO_freq>=250000 && LO_freq<280000)
    {
	R828_Freq_Info.TF_C=0x11;     //R27[7:0]  highest,highest
    }
    else
    {
	R828_Freq_Info.TF_C=0x00;     //R27[7:0]  highest,highest
    }

    return R828_Freq_Info;
}

int R828_Init(void *pTuner)
{
    uint8_t i;
    uint8_t addr;

    if(R828_IMR_done_flag==false)
    {
	Xtal_cap_sel = XTAL_HIGH_CAP_0P;
	//reset filter cal.
	for (i=0; i<STD_SIZE; i++)
	{	  
	    R828_Fil_Cal_flag[i] = false;
	    R828_Fil_Cal_code[i] = 0;
	}

#if 0
	//start imr cal.
	for(addr = 0x5; addr < sizeof(init_state); addr++){
	    int ret = i2c_write_reg(pTuner, addr, init_state[addr]);
	    if(ret < 0)
		return ret;
	}

	{// R828_IMR_Prepare
	    const uint8_t seq[] = {
		// addr, mask, value
		0x05, 0xFF, 0x20,	//lna off (air-in off)
		0x07, 0xEF, 0x00, //mixer gain mode = manual	
		0x0A, 0xFF, 0x0F,	//filter corner = lowest
		0x0B, 0x90, 0x60, //filter bw=+2cap, hp=5M	
		0x0C, 0x60, 0x0B,	//adc=on, vga code mode, gain = 26.5dB
		0x0F, 0xF7, 0x00,	//ring clk = on
		0x18, 0xFF, 0x10,	//ring power = on
		0x1C, 0xFF, 0x02,	//from ring = ring pll in
		0x1E, 0xFF, 0x80,	//sw_pdect = det3
		0x06, 0xFF, 0x10, // Set filt_3dB	
	    };

	    ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	    if(ret < 0)
		return ret;
	}

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
    for(addr = 0x5; addr < sizeof(init_state); addr++){
	int ret = i2c_write_reg(pTuner, addr, init_state[addr]);
	if(ret < 0)
	    return ret;
    }

    return 0;
}

static int R828_IMR(void *pTuner, uint8_t IMR_MEM, int IM_Flag)
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

    for(n=0; n < 16; n++)
    {
	if((16 + n) * 8 * RingRef >= 3100000) 
	{
	    n_ring = n;
	    break;
	}

	if(n == 15)   //n_ring not found
	{
	    //return -1;
	    n_ring=n;
	}
    }

    RingVCO = (16+n_ring)* 8 * RingRef;

    uint8_t ring_se23 = 0;
    uint8_t ring_seldiv  = 0;
    uint8_t pw_ring = 0;

    switch(IMR_MEM)
    {
	case 0:
	    RingFreq = RingVCO/48;
	    ring_se23 = 0x20;
	    ring_seldiv = 3;
	    pw_ring = 2;
	    break;
	case 1:
	    RingFreq = RingVCO/16;
	    ring_se23 = 0;
	    ring_seldiv = 2;
	    pw_ring = 0;
	    break;
	case 2:
	    RingFreq = RingVCO/8;
	    ring_se23 = 0;
	    ring_seldiv = 1;
	    pw_ring = 3;
	    break;
	case 3:
	    RingFreq = RingVCO/6;
	    ring_se23 = 0x20;
	    ring_seldiv = 0;
	    pw_ring = 3;
	    break;
	case 4:
	default:
	    RingFreq = RingVCO/4;
	    ring_se23 = 0;
	    ring_seldiv = 0;
	    pw_ring = 1;
	    break;
    }

    {
	int ret;
	const uint8_t seq[] = {
	    // addr, mask, value
	    0x18, 0xD0, ring_se23 | n_ring,
	    0x19, 0xFC, ring_seldiv,
	    0x1F, 0xFC, pw_ring,
	};

	ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	if(ret < 0)
	    return ret;
    }

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

static int R828_PLL(void *pTuner, uint64_t LO_freq, R828_Standard_Type R828_Standard)
{
    int  MixShift;
    int  PLL_div;
    const uint64_t VCO_Min = 1770000000;
    uint64_t VCO_Freq;
    uint32_t XTAL_freq;
    uint32_t PLL_Ref;
    uint32_t VCO_Fra;	//VCO contribution by SDM (kHz)
    uint16_t SDM;
    uint8_t  VCO_fine_tune;

    int ret;


    XTAL_freq = rtlsdr_get_tuner_clock(pTuner);
    {
	const uint8_t seq[] = {
	    // addr, mask, value
	    0x10, 0xEF, 0x00,	//xtal div = 1
	    0x1A, 0xF3, 0x00,	//set pll autotune = 128kHz
	    0x12, 0x1F, 0x80,	//Set VCO current = 100
	};

	ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	if(ret < 0)
	    return ret;
    }

    assert(LO_freq > 0);

    MixShift = (sizeof(int)*8 - __builtin_clz((int)(VCO_Min / LO_freq)));
    if(MixShift > 6) MixShift = 6;
    if(MixShift < 1) MixShift = 1;

    VCO_Freq = LO_freq << MixShift;
    
    PLL_div  = (uint8_t) (VCO_Freq  / (2 * XTAL_freq));
    VCO_Fra  = (uint16_t) ((VCO_Freq % (2 * XTAL_freq)) / 1000);

    PLL_Ref = XTAL_freq / 1000;

    if (PLL_div > 63) {
	fprintf(stderr, "[R820T] No valid PLL values for %u kHz!\n", (uint32_t)(LO_freq/1000));
	return -1;
    }

    //boundary spur prevention
    /*
    if (VCO_Fra < PLL_Ref/64)           //2*PLL_Ref/128
	VCO_Fra = 0;
    else if (VCO_Fra > PLL_Ref*127/64)  //2*PLL_Ref*127/128
    {
	VCO_Fra = 0;
	PLL_div ++;
    }
    else if((VCO_Fra > PLL_Ref*127/128) && (VCO_Fra < PLL_Ref)) //> 2*PLL_Ref*127/256,  < 2*PLL_Ref*128/256
	VCO_Fra = PLL_Ref*127/128;      // VCO_Fra = 2*PLL_Ref*127/256
    else if((VCO_Fra > PLL_Ref) && (VCO_Fra < PLL_Ref*129/128)) //> 2*PLL_Ref*128/256,  < 2*PLL_Ref*129/256
	VCO_Fra = PLL_Ref*129/128;      // VCO_Fra = 2*PLL_Ref*129/256
    else
	VCO_Fra = VCO_Fra;
    */
    

    SDM = (VCO_Fra << 15) / PLL_Ref;

    fprintf(stderr, "LO: %u kHz, MixDiv: %u, PLLDiv: %u, VCO %u kHz, SDM: %u \n", (uint32_t)(LO_freq/1000), 1 << MixShift, PLL_div,  (uint32_t)(VCO_Freq/1000), SDM);

    {
	R828_I2C_Len.RegAddr = 0x00;
	R828_I2C_Len.Len     = 5;
	if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
	    return -1;	

	VCO_fine_tune = (R828_I2C_Len.Data[4] & 0x30)>>4;

	MixShift--;
	if(VCO_fine_tune > VCO_pwr_ref)
	    MixShift --;
	else if(VCO_fine_tune < VCO_pwr_ref)
	    MixShift ++;

	assert(MixShift < 8);
    }

    {
	int ret;
	uint8_t  Ni = (PLL_div - 13) >> 2;
	uint8_t  Si = (PLL_div - 13) & 0x03;

	const uint8_t seq[] = {
	    // addr, mask, value
	    0x10, 0x1F, (MixShift << 5),
	    0x14, 0x00, (Ni + (Si << 6)), 
	    0x12, 0xF7, SDM ? 0x00 : 0x08,
	    0x15, 0x00, (uint8_t) (SDM & 0xff),
	    0x16, 0x00, (uint8_t) (SDM >> 8),
	};

	ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	if(ret < 0)
	    return ret;
    }
    
    R828_Delay_MS(pTuner, 10);

    //check PLL lock status
    R828_I2C_Len.RegAddr = 0x00;
    R828_I2C_Len.Len     = 3;
    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
	return -1;

    if( (R828_I2C_Len.Data[2] & 0x40) == 0x00 )
    {
	fprintf(stderr, "[R820T] PLL not locked for %u kHz!\n", (uint32_t)(LO_freq/1000));
    	ret = i2c_write_reg(pTuner, 0x12, (register_state[0x12] & 0x1F) | 0x60 ); //increase VCO current
    	if(ret < 0)
		return ret;

	return -1;
    }

    ret = i2c_write_reg(pTuner, 0x1A, (register_state[0x1A] & 0xFF) | 0x08 ); //set pll autotune = 8kHz
    if(ret < 0)
	return ret;

    return 0;
}

static int R828_MUX(void *pTuner, uint64_t freq)
{	
    uint8_t RT_Reg08 = 0;
    uint8_t RT_Reg09 = 0;
    R828_I2C_TYPE  R828_I2C;

    //Freq_Info_Type Freq_Info1;
    Freq_Info1 = R828_Freq_Sel(freq / 1000);

    // Open Drain
    R828_I2C.RegAddr = 0x17;
    register_state[18+5] = (register_state[18+5] & 0xF7) | Freq_Info1.OPEN_D;
    R828_I2C.Data = register_state[18+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // RF_MUX,Polymux 
    R828_I2C.RegAddr = 0x1A;
    register_state[21+5] = (register_state[21+5] & 0x3C) | Freq_Info1.RF_MUX_PLOY;
    R828_I2C.Data = register_state[21+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // TF BAND
    R828_I2C.RegAddr = 0x1B;
    register_state[22+5] &= 0x00;
    register_state[22+5] |= Freq_Info1.TF_C;	
    R828_I2C.Data = register_state[22+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // XTAL CAP & Drive
    R828_I2C.RegAddr = 0x10;
    register_state[11+5] &= 0xF4;
    switch(Xtal_cap_sel)
    {
	case XTAL_LOW_CAP_30P:
	case XTAL_LOW_CAP_20P:
	    register_state[11+5] = register_state[11+5] | Freq_Info1.XTAL_CAP20P | 0x08;
	    break;

	case XTAL_LOW_CAP_10P:
	    register_state[11+5] = register_state[11+5] | Freq_Info1.XTAL_CAP10P | 0x08;
	    break;

	case XTAL_LOW_CAP_0P:
	    register_state[11+5] = register_state[11+5] | Freq_Info1.XTAL_CAP0P | 0x08;
	    break;

	case XTAL_HIGH_CAP_0P:
	    register_state[11+5] = register_state[11+5] | Freq_Info1.XTAL_CAP0P | 0x00;
	    break;

	default:
	    register_state[11+5] = register_state[11+5] | Freq_Info1.XTAL_CAP0P | 0x08;
	    break;
    }
    R828_I2C.Data    = register_state[11+5];
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
    register_state[8] = (init_state[8] & 0xC0) | RT_Reg08;
    R828_I2C.Data = register_state[8];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x09;
    register_state[9] = (init_state[9] & 0xC0) | RT_Reg08;
    R828_I2C.Data =register_state[9]  ;
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;
}

static int R828_IQ(void *pTuner, R828_SectType* IQ_Pont)
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
	R828_I2C.Data    = (register_state[7+5] & 0xF0) + VGA_Count;  
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
    Compare_IQ[0].Gain_X  = init_state[8] & 0xC0; // Jason modified, clear b[5], b[4:0]
    Compare_IQ[0].Phase_Y = init_state[9] & 0xC0; //

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
    R828_I2C.Data    = init_state[8] & 0xC0; //Jason
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    R828_I2C.RegAddr = 0x09;
    R828_I2C.Data    = init_state[9] & 0xC0;
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
static int R828_IQ_Tree(void *pTuner, uint8_t FixPot, uint8_t FlucPot, uint8_t PotReg, R828_SectType* CompareTree)
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
static int R828_CompreCor(R828_SectType* CorArry)
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
static int R828_CompreStep(void *pTuner, R828_SectType* StepArry, uint8_t Pace)
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
static int R828_Muti_Read(void *pTuner, uint8_t IMR_Reg, uint16_t* IMR_Result_Data)  //jason modified
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

static int R828_Section(void *pTuner, R828_SectType* IQ_Pont)
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

static int R828_IMR_Cross(void *pTuner, R828_SectType* IQ_Pont, uint8_t* X_Direct)
{

    R828_SectType Compare_Cross[5]; //(0,0)(0,Q-1)(0,I-1)(Q-1,0)(I-1,0)
    R828_SectType Compare_Temp;
    uint8_t CrossCount = 0;
    uint8_t Reg08 = init_state[8] & 0xC0;
    uint8_t Reg09 = init_state[9] & 0xC0;	
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
static int R828_F_IMR(void *pTuner, R828_SectType* IQ_Pont)
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
	R828_I2C.Data    = (register_state[7+5] & 0xF0) + VGA_Count;
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

static int R828_GPIO(void *pTuner, char value)
{
    R828_I2C_TYPE  R828_I2C;
    if(value)
	register_state[10+5] |= 0x01;
    else
	register_state[10+5] &= 0xFE;

    R828_I2C.RegAddr = 0x0F;
    R828_I2C.Data    = register_state[10+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;
}

int r820t_SetStandardMode(void *pTuner, int StandardMode)
{
    R828_Standard_Type RT_Standard = (R828_Standard_Type)StandardMode;
    // Used Normal Arry to Modify
    R828_I2C_TYPE  R828_I2C;

    memcpy(register_state, init_state, sizeof(init_state));

    // Record Init Flag & Xtal_check Result
    if(R828_IMR_done_flag == true)
	register_state[7+5]    = (register_state[7+5] & 0xF0) | 0x01 | (Xtal_cap_sel<<1);
    else
	register_state[7+5]    = (register_state[7+5] & 0xF0) | 0x00;

    R828_I2C.RegAddr = 0x0C;
    R828_I2C.Data    = register_state[7+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // Record version
    R828_I2C.RegAddr = 0x13;
    register_state[14+5]    = (register_state[14+5] & 0xC0) | VER_NUM;
    R828_I2C.Data    = register_state[14+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;


    //for LT Gain test
    if(RT_Standard > SECAM_L1)
    {
	R828_I2C.RegAddr = 0x1D;  //[5:3] LNA TOP
	R828_I2C.Data = (register_state[24+5] & 0xC7) | 0x00;
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
    register_state[5+5]  = (register_state[5+5] & 0xE0) | Sys_Info1.FILT_Q | R828_Fil_Cal_code[RT_Standard];  
    R828_I2C.RegAddr  = 0x0A;
    R828_I2C.Data     = register_state[5+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // Set BW, Filter_gain, & HP corner
    register_state[6+5]= (register_state[6+5] & 0x10) | Sys_Info1.HP_COR;
    R828_I2C.RegAddr  = 0x0B;
    R828_I2C.Data     = register_state[6+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    // Set Img_R
    register_state[2+5]  = (register_state[2+5] & 0x7F) | Sys_Info1.IMG_R;  
    R828_I2C.RegAddr  = 0x07;
    R828_I2C.Data     = register_state[2+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;


    // Set filt_3dB, V6MHz
    register_state[1+5]  = (register_state[1+5] & 0xCF) | Sys_Info1.FILT_GAIN;  
    R828_I2C.RegAddr  = 0x06;
    R828_I2C.Data     = register_state[1+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //channel filter extension
    register_state[25+5]  = (register_state[25+5] & 0x9F) | Sys_Info1.EXT_ENABLE;  
    R828_I2C.RegAddr  = 0x1E;
    R828_I2C.Data     = register_state[25+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;


    //Loop through
    register_state[0+5]  = (register_state[0+5] & 0x7F) | Sys_Info1.LOOP_THROUGH;  
    R828_I2C.RegAddr  = 0x05;
    R828_I2C.Data     = register_state[0+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //Loop through attenuation
    register_state[26+5]  = (register_state[26+5] & 0x7F) | Sys_Info1.LT_ATT;  
    R828_I2C.RegAddr  = 0x1F;
    R828_I2C.Data     = register_state[26+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //filter extention widest
    register_state[10+5]  = (register_state[10+5] & 0x7F) | Sys_Info1.FLT_EXT_WIDEST;  
    R828_I2C.RegAddr  = 0x0F;
    R828_I2C.Data     = register_state[10+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //RF poly filter current
    register_state[20+5]  = (register_state[20+5] & 0x9F) | Sys_Info1.POLYFIL_CUR;  
    R828_I2C.RegAddr  = 0x19;
    R828_I2C.Data     = register_state[20+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    return 0;
}

static int R828_Filt_Cal(void *pTuner, uint32_t Cal_Freq,BW_Type R828_BW)
{
    R828_I2C_TYPE  R828_I2C;
    //set in Sys_sel()
    /*
       if(R828_BW == BW_8M)
       {
    //set filt_cap = no cap
    R828_I2C.RegAddr = 0x0B;  //reg11
    register_state[6+5]   &= 0x9F;  //filt_cap = no cap
    R828_I2C.Data    = register_state[6+5];		
    }
    else if(R828_BW == BW_7M)
    {
    //set filt_cap = +1 cap
    R828_I2C.RegAddr = 0x0B;  //reg11
    register_state[6+5]   &= 0x9F;  //filt_cap = no cap
    register_state[6+5]   |= 0x20;  //filt_cap = +1 cap
    R828_I2C.Data    = register_state[6+5];		
    }
    else if(R828_BW == BW_6M)
    {
    //set filt_cap = +2 cap
    R828_I2C.RegAddr = 0x0B;  //reg11
    register_state[6+5]   &= 0x9F;  //filt_cap = no cap
    register_state[6+5]   |= 0x60;  //filt_cap = +2 cap
    R828_I2C.Data    = register_state[6+5];		
    }


    if(I2C_Write(pTuner, &R828_I2C) != 0)
    return -1;	
    */

    {
	int ret;
	const uint8_t seq[] = {
	    // addr, mask, value
	    0x0B, 0x9F, (Sys_Info1.HP_COR & 0x60), // Set filt_cap
	    0x0F, 0xFF, 0x04, //calibration clk=on
	    0x10, 0xFC, 0x00, //X'tal cap 0pF for PLL
	};

	ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	if(ret < 0)
	    return ret;
    }

    //Set PLL Freq = Filter Cali Freq
    if(R828_PLL(pTuner, Cal_Freq * 1000, STD_SIZE) != 0)
	return -1;

    //Start Trigger
    R828_I2C.RegAddr = 0x0B;	//reg11
    register_state[6+5]   |= 0x10;	    //vstart=1	
    R828_I2C.Data    = register_state[6+5];
    if(I2C_Write(pTuner, &R828_I2C) != 0)
	return -1;

    //delay 0.5ms
    R828_Delay_MS(pTuner, 1);  


    {
	int ret;
	const uint8_t seq[] = {
	    // addr, mask, value
	    0x0B, 0xEF, 0x00, // Stop Trigger
	    0x0F, 0xFB, 0x00, //calibration clk=off
	};

	ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	if(ret < 0)
	    return ret;
    }
    return 0;
}

static int R828_SetFrequency(void *pTuner, uint64_t freq, R828_Standard_Type R828_Standard, char fast_mode)
{
    uint64_t LO_freq = freq;
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

    const uint8_t LNA_TOP = 0xE5;		// Detect BW 3, LNA TOP:4, PreDet Top:2
    const uint8_t MIXER_VTH_L=0x75;	// MIXER VTH 1.04, VTL 0.84

    const uint8_t PRE_DECT=0x40;
    const uint8_t LNA_DISCHARGE=14;
    const uint8_t FILTER_CUR=0x40;    // 10, low

    uint8_t LNA_VTH_L = 0x53;		// LNA VTH 0.84	,  VTL 0.64
    if(R828_Standard == ISDB_T)
	LNA_VTH_L=0x75;		// LNA VTH 1.04	,  VTL 0.84

    uint8_t MIXER_TOP=0x24;	    // MIXER TOP:13 , TOP-1, low-discharge
    uint8_t CP_CUR=0x38;            // 111, auto
    uint8_t DIV_BUF_CUR=0x30; // 11, 150u

    if( ((R828_Standard == DVB_T_6M) || (R828_Standard == DVB_T_7M) || 
		(R828_Standard == DVB_T_7M_2) || (R828_Standard == DVB_T_8M)) &&
	    ((freq/1000==506000) || (freq/1000==666000) || (freq/1000==818000))
      )
    {
	MIXER_TOP=0x14;	    // MIXER TOP:14 , TOP-1, low-discharge
	CP_CUR=0x28;            //101, 0.2
	DIV_BUF_CUR=0x20; // 10, 200u
    }

#if USE_DIPLEXER
    uint8_t AIR_CABLE1_IN=0x00;
    if ((CHIP_VARIANT==R820C) || (CHIP_VARIANT==R820T) || (CHIP_VARIANT==R828S))
    {
	if(freq/1000 < DIP_FREQ)
	    AIR_CABLE1_IN = 0x60; //cable-1 in, air off
    }
#else
    const uint8_t AIR_CABLE1_IN=0x00;
#endif

    {
	int ret;
	const uint8_t seq[] = {
	    // addr, mask, value
	    //0x05, 0x9F, AIR_CABLE1_IN,
	    0x05, 0x9F, 0x00,				//Air-In only for Astrometa
	    0x06, 0xF7, 0x00,		// cable 2 in off
	    0x0A, 0x9F, FILTER_CUR,	// Set channel filter current
	    0x0D, 0x00, LNA_VTH_L,	// write LNA VTHL
	    0x0E, 0x00, MIXER_VTH_L,	// write MIXER VTHL
	    0x11, 0xC7, CP_CUR,		//CP current
	    0x17, 0xCF, DIV_BUF_CUR,	//div buffer current
	    0x1D, 0x38, LNA_TOP & 0xC7, 	// write DectBW, pre_dect_TOP
	    0x1C, 0x07, MIXER_TOP & 0xF8,	// write MIXER TOP, TOP+-1

	};

	ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	if(ret < 0)
	    return ret;
    }

    //Set LNA
    if(R828_Standard > SECAM_L1)
    {
	{
	    int ret;
	    const uint8_t seq[] = {
		// addr, mask, value
		0x06, 0xBF, 0x00, // 0: PRE_DECT off
		0x1A, 0xCF, 0x30, // agc clk 250hz
		0x1C, 0xFB, 0x00, // 0: normal mode
		0x1D, 0xC7, 0x00, // LNA TOP:lowest
		// 0x1D, 0xC7, fast_mode ? 0x20 : 0x00, // LNA TOP: 4 / lowest
	    };

	    ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	    if(ret < 0)
		return ret;
	}

	if(!fast_mode)       //Normal mode 
	{
	    int ret;
	    const uint8_t seq[] = {
		// addr, mask, value
		//0x06, 0xBF, PRE_DECT,
		0x1A, 0xCF, 0x20, // agc clk 60hz
		0x1C, 0xFB, MIXER_TOP & 0x04, // write discharge mode
		0x1D, 0xC7, 0x18, // LNA TOP:3
		//0x1D, 0xC7, LNA_TOP & 0x38,
		0x1E, 0xE0, LNA_DISCHARGE,

	    };

	    ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	    if(ret < 0)
		return ret;
	}
    }
    else 
    {
	int ret;
	const uint8_t seq[] = {
	    // addr, mask, value
	    //0x06, 0xBF, PRE_DECT,
	    0x06, 0xBF, 0x00, // 0: PRE_DECT off
	    0x10, 0xFB, 0x00, // external det1 cap 1u
	    0x1A, 0xCF, 0x00, // agc clk 1Khz
	    0x1C, 0xFB, MIXER_TOP & 0x04, // write discharge mode
	    0x1D, 0xC7, LNA_TOP & 0x38,
	    0x1E, 0xE0, LNA_DISCHARGE,

	};

	ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	if(ret < 0)
	    return ret;
    }
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

    {
	int ret;
	const uint8_t seq[] = {
	    // addr, mask, value
	    0x05, 0xF0, lna_index, // LNA gain
	    0x07, 0xF0, mix_index, // Mixer gain
	};

	ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	if(ret < 0)
	    return ret;
    }
    return 0;
}

int R828_RfGainMode(void *pTuner, int manual)
{
    {
	int ret;
	const uint8_t seq[] = {
	    // addr, mask, value
	    0x05, 0xEF, (manual ? 0x10 : 0x00),	// LNA auto gain
	    0x07, 0xEF, (manual ? 0x00 : 0x10),	// Mixer auto gain
	    0x0C, 0x60, (manual ? 0x08 : 0x0B), // VGA gain 16.3 dB / 26.5 dB
	};

	ret = i2c_write_reg_seq_mask(pTuner, seq, sizeof(seq)/3);
	if(ret < 0)
	    return ret;
    }

    // is that needed ?
    R828_I2C_Len.RegAddr = 0x00;
    R828_I2C_Len.Len     = 4; 
    if(I2C_Read_Len(pTuner, &R828_I2C_Len) != 0)
	return -1;

    return 0;
}
