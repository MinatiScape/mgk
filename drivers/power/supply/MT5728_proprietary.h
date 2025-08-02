
#ifndef __MT5728PROPRIETARY_H__
#define __MT5728PROPRIETARY_H__

typedef struct {
    unsigned char cmd;
    unsigned char data[20];
} __attribute__ ((packed)) MsgType;
/*
typedef enum {
    CMD_NONE                      = 0x00,

    CMD_NAK                       = 0x00,
    CMD_ND                        = 0x55,
    CMD_ACK                       = 0xff,

    CMD_HANDSHAKE                 = 0x58,
    CMD_AUTHENTICATION            = 0x59,

    CMD_EPPEXTENDSYNC             = 0x90,

    CMD_FODSTATE_CLEAN            = 0x93,

    CMD_PRODUCTVERSION            = 0x9a,
    CMD_GET_MAXPOWER              = 0x9b,
    CMD_GET_COILID                = 0x9c,


    CMD_RESET                     = 0xa0,
    CMD_EPT                       = 0xa1,
    CMD_READ_HW_VER               = 0xa2,
    CMD_READ_FW_VER               = 0xa3,
    CMD_ADAPTER_TYPE              = 0xa4,
    CMD_SET_TA                    = 0xa5,
    CMD_SET_AUTH                  = 0xa6,
    CMD_GET_AUTH                  = 0xa7,
    CMD_GET_TACAP                 = 0xa8,
    CMD_GET_CEPCNT                = 0xa9,
    CMD_DEEPSLEEP                 = 0xaa,

    CMD_TOGGLE_LOOPMODE           = 0xb0,
    CMD_SET_FIXEDPERIOD           = 0xb1,
    CMD_GET_PERIOD                = 0xb2,
    CMD_SET_FREQ                  = 0xb3,
    CMD_GET_FREQ                  = 0xb4,
    CMD_SET_DUTYCYCLE             = 0xb5,
    CMD_SET_MINFREQ               = 0xb6,
    CMD_SET_MAXFREQ               = 0xb7,
    CMD_SET_PINGFREQ              = 0xb8,
    CMD_GET_DUTYCYCLE             = 0xb9,
    CMD_GET_MINFREQ               = 0xba,
    CMD_GET_MAXFREQ               = 0xbb,
    CMD_GET_PINGFREQ              = 0xbc,
    CMD_SET_DEADTIME              = 0xbd,
    CMD_GET_DEADTIME              = 0xbe,
    CMD_SET_BUCKVOUT              = 0xbf,

    CMD_GET_Vdm                   = 0xce,
    CMD_GET_Vcoil                 = 0xcf,
    CMD_GET_Iin                   = 0xd0,
    CMD_GET_Vin                   = 0xd1,
    CMD_SET_Vin                   = 0xd2,
    CMD_GET_Vbrg                  = 0xd3,
    CMD_SET_Vbrg                  = 0xd4,
    CMD_GET_PTx                   = 0xd5,
    CMD_GET_TEMP                  = 0xd6,

    CMD_READ_REG                  = 0xd7,
    CMD_WRITE_REG                 = 0xd8,
    CMD_READ_SRAM                 = 0xd9,
    CMD_WRITE_SRAM                = 0xda,

    CMD_READ_FCP                  = 0xdb,
    CMD_WRITE_FCP                 = 0xdc,
    CMD_GET_Q                     = 0xdd,

    CMD_TEST                      = 0xee,
} PrivateCmdType;
*/
#define TECNO_ERR_HEADER           0x1A

#define TECNO_ERR_NONE             0x00
#define TECNO_ERR_NEGEND           0x55
#define TECNO_ERR_OFFCENTER        0xC3
#define TECNO_ERR_ONCENTER         0xCC
#define TECNO_ERR_OVERLOAD         0xAD
#define TECNO_ERR_OVERLOAD_CANCEL  0xA2


typedef struct {
    unsigned char header;
    unsigned char msg[21];
} __attribute__ ((packed)) PktType_t;

typedef struct {
    volatile unsigned int header;
    void (*exec)(MsgType *msg, PktType_t *bc);
} PPFCType;




extern const unsigned char WPC_PACKET[29];
extern const PPFCType HUAWEIPPList[4];
extern const PPFCType SAMSUNGPPList[2];
extern const PPFCType IPHONEList[2];


// proprietary
void pp_cmd_confirm_timeout(void);
void pp_response_comfirm(unsigned char cmd);
void pp_ack(void);
void pp_nak(void);
void pp_nd(void);

// tecno
void TECNO_authentication(MsgType *msg, PktType_t *bc);
void TECNO_state_report(void);

#endif
