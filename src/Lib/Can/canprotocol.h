#ifndef CANPROTOCOL_H
#define CANPROTOCOL_H

#include <Comm/comm.h>
#include <Config/appinfo.h>


#define CAN_DEVICE_INTERFACE    "can0"  /*CAN设置定义*/
#define CAN_BPS                 125000  /*CAN设置定义*/

#define CAN_SOURE_ADDR          138     /*CAN源地址*/

#define CAN_CHECKSUM_SEND       1       /*CHKSUM 发送时校验标志*/
#define CAN_CHECKSUM_RECV       0       /*CHKSUM 接收时校验标志*/

#define MAX_TEMP_LINK_NUM       5       /*多帧接收连接时最大连接数*/

#define CAN_PGN_TYPE_NOTIFY     0       /*CAN帧PGN类型 通知型*/
#define CAN_PGN_TYPE_REQ_ACK    1       /*CAN帧PGN类型 请求应答型*/

/*支持的CAN命令*/
typedef enum {
    BOOT_LEAD_STARTUP = 0xF1,           //BOOT引导启动帧            +++++单帧
    BOOT_LEAD_STARTUP_ACK = 0xF2,       //BOOT引导应答帧            -----多帧
    REQUEST_APP_DATA_DLOAD = 0xF3,      //请求程序数据下发           +++++单帧
    REQUEST_APP_DATA_DLOAD_ACK = 0xF4,  //程序数据下发应答           -----多帧
    UPGRADE_OVER = 0xF5,                //软件升级完毕通知           +++++单帧
    UPGRADE_OVER_ACK = 0xF6,            //软件升级完毕应答           +++++单帧
    CHARGE_CONTROL_RESET = 0xF7,        //充电控制单元复位           +++++单帧
    READ_CHARGE_CTRL_INFO = 0xB0,       //读取充电控制单元内部信息    +++++单帧
    READ_CHARGE_CTRL_INFO_ACK = 0xB1,   //读取控制单元内部信息应答    -----多帧
    BROADCAST_UPGRADE_CLI = 0xFB        //广播通知客户端升级         -----多帧
} CAN_PGN;


#pragma pack(1)

/*CAN2.0协议格式*/
typedef union {
    UInt32 ExtId;
    struct {
        UInt8 SA;       //源地址
        UInt8 PS;       //目标地址
        UInt8 PF;       //PDU格式
        UInt8 DP:1;     //数据页
        UInt8 R:1;      //保留位
        UInt8 P:3;      //优先级
    }bit;
}CanID;

typedef struct {
    UInt8 FF:1;         //标准帧/扩展帧标志
    UInt8 RTR:1;        //数据帧或远程帧
}CanFrmDef;

typedef struct {
    CanID ID;
    CanFrmDef FrmDef;
    UInt8 DLC;          //数据长度
    UInt8 Buffer[8];    //数据
}CAN_MSG;

typedef struct {
    CanID ID;
    CanFrmDef FrmDef;
    UInt16 Len;
    QByteArray Buffer;  //数据
}CanDataIF;

typedef struct {
    UInt8 maxCnt;//最大重发次数
    UInt8 sendCnt;//已重发次数
    Int32 cycle;//发送周期
    UInt64 lastSndTime;//最近发送时间
    CanDataIF canData;//CAN消息
}ReCanData;


typedef struct {
    UInt8 KeyPGN;           //占用缓存的PGN,0-缓存空闲/其他-缓存被占用
    UInt64 Timeout;         //超时时间
    UInt8 PreNO;            //上一帧序号，帧序号不连续则无效
    UInt8 Count;            //总帧数
    UInt16 Len;             //数据总长度
    QByteArray Buffer;      //数据
}CanDataLink;


/*读取充电控制单元内部信息报文(查询版本)*/
typedef struct {
    UInt8 GunNo;                    /*枪编号，一桩一充此项为0*/
    UInt16 ReplyTime;               /*持续应答时间 0-按默认应答时间2s*/
    UInt16 ReplyPeriod;             /*应答周期 0-默认应答周期*/
}CanQueryVersion;

/*读取充电控制单元内部信息应答报文(应答版本)*/
typedef struct {
    UInt8 GunNo;                    /*枪编号，一桩一充此项为0*/
    UInt8 RackAddr;                 /*堆地址*/
    UInt8 BusAddr;                  /*母线地址*/
    UInt8 Version[4];               /*版本*/
}CanReplyVersion;

/*BOOT引导启动报文*/
typedef struct {
    UInt8 id;                       //充电接口标识
    UInt8 AV;                       //APP是否下载有效
    UInt8 version[4];                //软件版本
}CanBootReq;

/*BOOT引导启动应答报文*/
typedef struct {
    UInt8 id;                       //充电接口标识
    UInt8 version[4];         //软件版本
    UInt32 size;                    //软件大小
    UInt32 checkNum;                //软件校验码
    UInt8 flag;                     //成功标识 0:成功 1:失败
}CanBootAck;

/*请求程序数据下发报文*/
typedef struct {
    UInt8 id;                       //充电接口标识
    UInt32 addr;                    //数据地址
    UInt16 len;                     //请求数据长度
}CanDataReq;

/*程序数据下发报文*/
typedef struct {
    UInt8 id;                       //充电接口标识
    UInt32 addr;                    //数据地址
    UInt16 len;                     //请求数据长度
    UInt8 data[512];                //数据内容
    UInt8 flag;                     //成功标识 0:成功 1:失败
}CanDataAck;

/*软件升级完毕通知报文*/
typedef struct {
    UInt8 id;                       //充电接口标识
    UInt8 version[4];         //软件版本
    UInt8 flag;                     //完毕状态 0:成功 1:失败
}CanUpdateOver;


#pragma pack()



#endif // CANPROTOCOL_H

