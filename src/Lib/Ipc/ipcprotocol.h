#ifndef IPCPROTOCOL_H
#define IPCPROTOCOL_H

#include <Comm/comm.h>
#include <Config/appinfo.h>


#define BUFFER_SIZE             4096    /*接收-发送数据的buf大小*/

#define UPDATE_RET_INIT        -1       /*升级结果 初始值*/
#define UPDATE_RET_FAILED       0       /*升级结果 失败*/
#define UPDATE_RET_SUCCESS      1       /*升级结果 成功*/

#define CMD_TYPE_NOTIFY         0       /*cmd类型 通知型*/
#define CMD_TYPE_REQ_ACK        1       /*cmd类型 请求应答型*/

/*Ipc命令*/
typedef enum {
    CMD_QUERY_VERSION               = 0x0001,       //查询版本号
    CMD_REPLY_VERSION               = 0x0002,       //应答版本号
    CMD_SEND_UPDATE_ARG             = 0x0300,       //发送升级参数
    CMD_REPLY_UPDATE_ARG            = 0x0301,       //确认升级参数
    CMD_QUERY_RUN_STATUS            = 0x0400,       //查询运行状态
    CMD_REPLY_RUN_STATUS            = 0x0401,       //应答运行状态
    CMD_SEND_HEART                  = 0x0600,       //发送心跳包
    CMD_UFLASH_UPDATE               = 0x0402,       //U盘升级交互
    //CMD_REPLY_STAKE_INFO            = 0x0403,     //应答桩编码
    CMD_SEND_UPDATE_RESULT          = 0x0404,       //发送升级结果
    //CMD_REPLY_UPDATE_RESULT         = 0x0405      //应答升级结果
    CMD_QUERY_IP                    = 0x0406,       //查询集中计费终端IP
    CMD_REPLY_IP                    = 0x0407,       //应答集中计费终端IP
    CMD_SET_DEBUG_SWITCH_REQ        = 0x0505,       //设置诊断开关请求  诊断代理->本地应用
    CMD_SET_DEBUG_SWITCH_ACK        = 0x0506,       //设置诊断开关应答  本地应用->诊断代理
    CMD_GET_DEBUG_SWITCH_REQ        = 0x0507,       //获取诊断开关请求  诊断代理->本地应用
    CMD_GET_DEBUG_SWITCH_ACK        = 0x0508,       //获取诊断开关应答  本地应用->诊断代理
    CMD_SEND_DEBUG_INFO             = 0x0509,       //输出诊断信息     本地应用->诊断代理
    CMD_SET_DEBUG_ARG_REQ           = 0x050A,       //设置诊断参数请求  诊断代理->本地应用
    CMD_SET_DEBUG_ARG_ACK           = 0x050B,       //设置诊断参数应答  本地应用->诊断代理
    CMD_GET_DEBUG_ARG_REQ           = 0x050C,       //获取诊断参数请求  诊断代理->本地应用
    CMD_GET_DEBUG_ARG_ACK           = 0x050D        //获取诊断参数应答  本地应用->诊断代理
}IPC_CMD;


#pragma pack(1)

/*IPC数据帧格式*/
#define PROTOCOL_HEAD 0x68      /*IPC数据帧头*/
#define PROTOCOL_END  0x16      /*IPC数据帧尾*/
typedef struct {
    UInt8 head;
    UInt16 frameNum;
    UInt16 commond;
    UInt8 flag;
    UInt16 len;
    QByteArray buffer;
    UInt8 checkSum;
    UInt8 end;
    WHICH_APP who;
}IpcData;

typedef struct {
    UInt8 maxCnt;       //最大重发次数
    UInt8 sendCnt;      //已重发次数
    Int32 cycle;        //发送周期
    UInt64 lastSndTime; //最近发送时间
    IpcData ipcData;    //ipc消息
}ReIpcData;


/*升级参数*/
#define MAX_ARRAY        128
#define STAKE_CODE_LEN   8           /*桩编码长度*/
typedef struct {
    UInt8 ip[4];                       //升级服务器ip
    UInt16 port;                     //端口
    UInt8 lenUser;                   //服务器登录用户名字节长度
    UInt8 lenPassword;               //服务器登录密码字节长度
    UInt8 lenPath;                   //升级文件路径字节长度
    char user[MAX_ARRAY];            //服务器登录用户名
    char password[MAX_ARRAY];        //服务器登录密码
    char path[MAX_ARRAY];            //升级文件路径
    char stakeCode[STAKE_CODE_LEN]; //桩编码
    char MD5[32];                    //升级压缩包校验码
}UpdateArg;

/*桩参数*/
typedef struct {
    UpdateArg updateArg;
    UInt8 stakeAddr; /*桩地址（拨码地址）*/
}StakeArg;

/*桩升级结果*/
typedef struct {
    char stakeCode[STAKE_CODE_LEN];         /*桩编码*/
    UInt8 stakeAddr;                        /*桩地址*/
    Int8 updateRet[APP_NUM];                /*升级结果 -1-未升级 0-升级失败 1-升级成功*/
}UpdateRet;

/*应答升级结果到后台网关*/
typedef struct {
    char stakeCode[STAKE_CODE_LEN];        /*桩编码*/
    UInt8 ret;                              /*升级总结果 0-升级失败 1-升级成功*/
}FinalUpdateRet;


/***********************远程诊断************************/

#define DEBUG_ARG_LOG                   "log"                   /*远程诊断调试参数 日志信息*/
#define DEBUG_ARG_RECV_CAN              "recv_can"              /*远程诊断调试参数 接收自CAN 命令及信息*/
#define DEBUG_ARG_SEND_CAN              "send_can"              /*远程诊断调试参数 发送到CAN 命令及信息*/
#define DEBUG_ARG_RECV_UDP              "recv_udp"              /*远程诊断调试参数 接收自UDP 命令及信息*/
#define DEBUG_ARG_SEND_UDP              "send_udp"              /*远程诊断调试参数 发送到UDP 命令及信息*/

#define DEBUG_SWITCH_CLOSE     0   /*远程诊断开关 关闭*/
#define DEBUG_SWITCH_OPEN      1   /*远程诊断开关 打开*/
#define DEBUG_FLAG_NORMAL      0   /*远程诊断异常标志 正常*/
#define DEBUG_FLAG_EXCEPTION   1   /*远程诊断异常标志 异常*/
#define DEBUG_RET_SUCCESS      0   /*远程诊断应答结果 成功*/
#define DEBUG_RET_FAILED       1   /*远程诊断应答结果 失败*/

/*设置诊断开关请求*/
typedef struct {
    UInt32 ip;                  /*ip*/
    UInt16 port;                /*端口*/
    UInt8 Switch;               /*是否启动 0-禁止 1-启用*/
}SetDebugSwitchReq;

/*设置诊断开关应答*/
typedef struct {
    UInt32 ip;                  /*ip*/
    UInt16 port;                /*端口*/
    UInt8 flag;                 /*异常标志 0-禁止 1-启用*/
    UInt8 ret;                  /*应答结果 0-成功 1-失败*/
}SetDebugSwitchAck;

/*获取诊断开关请求*/
typedef struct {
    UInt32 ip;                  /*ip*/
    UInt16 port;                /*端口*/
}GetDebugSwitchReq;

/*获取诊断开关应答*/
typedef struct {
    UInt32 ip;                  /*ip*/
    UInt16 port;                /*端口*/
    UInt8 flag;                 /*异常标志 0-正常 1-异常*/
    UInt8 ret;                  /*应答结果 0-成功 1-失败*/
}GetDebugSwitchAck;



/*设置诊断参数请求*/
typedef struct {
    UInt32 ip;                  /*ip*/
    UInt16 port;                /*端口*/
    char arg[16];               /*诊断参数*/
}SetDebugArgReq;

/*设置诊断参数应答*/
typedef struct {
    UInt32 ip;                  /*ip*/
    UInt16 port;                /*端口*/
    UInt8 flag;                 /*异常标志 0-正常 1-异常*/
    UInt8 ret;                  /*应答结果 0-成功 1-失败*/
}SetDebugArgAck;

/*获取诊断参数请求*/
typedef struct {
    UInt32 ip;                  /*ip*/
    UInt16 port;                /*端口*/
}GetDebugArgReq;

/*获取诊断参数应答*/
typedef struct {
    UInt32 ip;                  /*ip*/
    UInt16 port;                /*端口*/
    UInt8 flag;                 /*异常标志 0-正常 1-异常*/
    char arg[128];               /*诊断参数应答*/
}GetDebugArgAck;


/*输出诊断信息*/
#define DEBUG_INFO_SIZE 2048     /*输出诊断信息长度*/
typedef struct {
    UInt32 ip;                  /*ip*/
    UInt16 port;                /*端口*/
    UInt16 len;                 /*调试信息长度*/
    char info[DEBUG_INFO_SIZE]; /*调试信息*/
}DebugInfo;



/***********************远程诊断************************/



/***********************U盘升级************************/

/*升级软件&配置软件U盘升级通用数据*/
#define MAX_PATH_SIZE   30      /*升级软件&配置软件U盘升级通用数据区长度*/
typedef struct {
    UInt8 cmd;
    UInt8 data0;
    char data1[MAX_PATH_SIZE];
    UInt8 data1size;
}StakeInfo;

#pragma pack()

/*升级软件&配置软件U盘升级通用CMD*/
enum STATUS_UPGRADE{                                //单个桩的状态机cmd
    /*uflash*/
    UFLASH_UPGRADE_SUCCESS, UFLASH_UPGRADE_FAIL,    //升级最终结果返回
    UFLASH_UPGRADE_IDLE,                            //<=则该桩空闲状态可升级
    UFLASH_UPGRADE_LAUNCH,  UFLASH_LAUNCH_SUCCESS, UFLASH_LAUNCH_FAIL, //启动升级&命令返回(Par:Path)
    UFLASH_UPGRADE_NUMADDR,                          //启动升级命令返回成功后,发送桩编号和地址

    /*cloud*/
    CLOUD_ADDRESS_REQUEST, CLOUD_ADDRESS_SUCCESS, CLOUD_ADDRESS_FAIL //请求地址数据&应答地址数据(Par: code)
};

/***********************U盘升级************************/





#endif // IPCPROTOCOL_H

