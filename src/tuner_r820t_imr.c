#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "rtlsdr_i2c.h"
#include "tuner_r820t.h"

typedef struct _R828_I2C_TYPE
{
    uint8_t RegAddr;
    uint8_t Data;
}R828_I2C_TYPE;

static int I2C_Write(void *pTuner, R828_I2C_TYPE *I2C_Info)
{
    uint8_t WritingBuffer[2] = {
	I2C_Info->RegAddr,
	I2C_Info->Data
    };

    if (rtlsdr_i2c_write_fn(pTuner, R820T_I2C_ADDR, WritingBuffer, 2) < 0)
	return -1;
    return 0;
}

static int R828_IMR(void *pTuner, uint8_t IMR_MEM, int IM_Flag);
static int R828_IQ(void *pTuner, R828_SectType* IQ_Pont);
static int R828_IQ_Tree(void *pTuner, uint8_t FixPot, uint8_t FlucPot, uint8_t PotReg, R828_SectType* CompareTree);
static int R828_CompreCor(R828_SectType* CorArry);
static int R828_CompreStep(void *pTuner, R828_SectType* StepArry, uint8_t Pace);
static int R828_Multi_Read(void *pTuner, uint8_t IMR_Reg, uint16_t* IMR_Result_Data);
static int R828_F_IMR(void *pTuner, R828_SectType* IQ_Pont);
static int R828_IMR_Cross(void *pTuner, R828_SectType* IQ_Pont, uint8_t* X_Direct);
static int R828_Section(void *pTuner, R828_SectType* SectionArry);


static int R828_IMR(void *pTuner, uint8_t IMR_MEM, int IM_Flag)
{

    uint32_t RingVCO = 0;
    uint32_t RingFreq = 0;
    uint32_t RingRef = 0;
    uint8_t n_ring = 0;

    R828_SectType IMR_POINT;

    if (R828_Xtal>24000)
	RingRef = R828_Xtal /2;
    else
	RingRef = R828_Xtal;

    n_ring = (3100000 / (8 * RingRef)) - 16;
    if(n_ring > 15)
	n_ring = 15;

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

    if(R828_PLL(pTuner, (RingFreq - 5300) * 1000) != 0)	//set pll freq = ring freq - 6M
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

static int R828_IQ(void *pTuner, R828_SectType* IQ_Pont)
{
    R828_SectType Compare_IQ[3];
    //	R828_SectType CompareTemp;
    //	uint8_t IQ_Count  = 0;
    uint8_t VGA_Count;
    uint16_t VGA_Read;
    uint8_t  X_Direction;  // 1:X, 0:Y
    int ret;
    R828_I2C_TYPE  R828_I2C;

    VGA_Count = 0;
    VGA_Read = 0;

    // increase VGA power to let image significant
    for(VGA_Count = 12;VGA_Count < 16;VGA_Count ++)
    {
	ret = i2c_write_reg(pTuner, 0x0C, (register_state[0x0C] & 0xF0) | VGA_Count );
	if(ret < 0)
	    return ret;

	R828_Delay_MS(pTuner, 10);

	if(R828_Multi_Read(pTuner, 0x01, &VGA_Read) != 0)
	    return -1;

	if(VGA_Read > 40*4)
	    break;
    }

    Compare_IQ[0].Gain_X  = init_state[0x08] & 0xC0; // Jason modified, clear b[5], b[4:0]
    Compare_IQ[0].Phase_Y = init_state[0x09] & 0xC0; //

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

	if(R828_Multi_Read(pTuner, 0x01, &CompareTree[TreeCount].Value) != 0)
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

	if(R828_Multi_Read(pTuner, 0x01, &StepTemp.Value) != 0)
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
static int R828_Multi_Read(void *pTuner, uint8_t IMR_Reg, uint16_t* IMR_Result_Data)  //jason modified
{
    int _acc = 0;
    int _min = 1<<7;
    int _max = 0;
    int i;
    uint8_t value;

    R828_Delay_MS(pTuner, 5);

    for(i = 0;  i < 6; i++)
    {
	int ret = i2c_read_reg(pTuner, IMR_Reg, &value); //IMR_Reg = 0x01
	if(ret < 0)
	    return ret;

	_acc += value;

	if(value < _min)
	    _min = value;

	if(value > _max)
	    _max = value;
    }
    *IMR_Result_Data = _acc - _max - _min;

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

	if(R828_Multi_Read(pTuner, 0x01, &Compare_Cross[CrossCount].Value) != 0)
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

	if(R828_Multi_Read(pTuner, 0x01, &VGA_Read) != 0)
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

