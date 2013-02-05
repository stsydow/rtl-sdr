#ifndef _R820T_TUNER_H
#define _R820T_TUNER_H

#define R820T_I2C_ADDR		0x34
#define R820T_CHECK_ADDR	0x00
#define R820T_CHECK_VAL		0x69

#define R820T_IF_FREQ		3570000

//***************************************************************
//*                       INCLUDES.H
//***************************************************************
#define VERSION   "R820T_v1.49_ASTRO"
#define VER_NUM  49

#define USE_16M_XTAL		false
#define R828_Xtal		28800

#define USE_DIPLEXER		false
#define TUNER_CLK_OUT		true

#define true	1
#define false	0

typedef enum _Rafael_Chip_Type  //Don't modify chip list
{
    R828 = 0,
    R828D,
    R828S,
    R820T,
    R820C,
    R620D,
    R620S
}Rafael_Chip_Type;
//----------------------------------------------------------//
//                   R828 Parameter                        //
//----------------------------------------------------------//

extern uint8_t R828_ADDRESS;

#define DIP_FREQ  	  320000
#define IMR_TRIAL    9
#define VCO_pwr_ref   0x02

extern uint32_t R828_IF_khz;
extern uint32_t R828_CAL_LO_khz;
extern uint8_t  R828_IMR_point_num;
extern uint8_t  R828_IMR_done_flag;
extern uint8_t  Rafael_Chip;

typedef enum _R828_Standard_Type  //Don't remove standand list!!
{
    NTSC_MN = 0,
    PAL_I,
    PAL_DK,
    PAL_B_7M,       //no use
    PAL_BGH_8M,     //for PAL B/G, PAL G/H
    SECAM_L,
    SECAM_L1_INV,   //for SECAM L'
    SECAM_L1,       //no use
    ATV_SIZE,
    DVB_T_6M = ATV_SIZE,
    DVB_T_7M,
    DVB_T_7M_2,
    DVB_T_8M,
    DVB_T2_6M,
    DVB_T2_7M,
    DVB_T2_7M_2,
    DVB_T2_8M,
    DVB_T2_1_7M,
    DVB_T2_10M,
    DVB_C_8M,
    DVB_C_6M,
    ISDB_T,
    DTMB,
    R828_ATSC,
    FM,
    STD_SIZE
}R828_Standard_Type;

extern uint8_t  R828_Fil_Cal_flag[STD_SIZE];

typedef enum _R828_InputMode_Type
{
    AIR_IN = 0,
    CABLE_IN_1,
    CABLE_IN_2
}R828_InputMode_Type;

typedef enum _R828_IfAgc_Type
{
    IF_AGC1 = 0,
    IF_AGC2
}R828_IfAgc_Type;

typedef struct _R828_Set_Info
{
    uint32_t        RF_Hz;
    uint32_t        RF_KHz;
    R828_Standard_Type R828_Standard;
    char loop_through;
    R828_InputMode_Type   RT_InputMode;
    R828_IfAgc_Type R828_IfAgc_Select; 
}R828_Set_Info;

typedef struct _R828_RF_Gain_Info
{
    uint8_t   RF_gain1;
    uint8_t   RF_gain2;
    uint8_t   RF_gain_comb;
}R828_RF_Gain_Info;

typedef enum _R828_RF_Gain_TYPE
{
    RF_AUTO = 0,
    RF_MANUAL
}R828_RF_Gain_TYPE;

typedef struct _R828_I2C_LEN_TYPE
{
    uint8_t RegAddr;
    uint8_t Data[50];
    uint8_t Len;
}R828_I2C_LEN_TYPE;

typedef struct _R828_I2C_TYPE
{
    uint8_t RegAddr;
    uint8_t Data;
}R828_I2C_TYPE;
//----------------------------------------------------------//
//                   R828 Function                         //
//----------------------------------------------------------//
int R828_Init(void *pTuner);
int R828_Standby(void *pTuner, char loop_through);
int R828_GPIO(void *pTuner, char value);
int R828_SetStandard(void *pTuner, R828_Standard_Type RT_Standard);
int R828_SetFrequency(void *pTuner, R828_Set_Info R828_INFO, char fast_mode);
int R828_GetRfGain(void *pTuner, R828_RF_Gain_Info *pR828_rf_gain);
int R828_SetRfGain(void *pTuner, int gain);
int R828_RfGainMode(void *pTuner, int manual);

int
r820t_SetRfFreqHz(
	void *pTuner,
	unsigned long RfFreqHz
	);

int
r820t_SetStandardMode(
	void *pTuner,
	int StandardMode
	);

int
r820t_SetStandby(
	void *pTuner,
	char loop_through
	);

#endif /* _R820T_TUNER_H */
