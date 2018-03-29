#ifndef APPINFO_H
#define APPINFO_H


#include "Comm/comm.h"

#include <qobject.h>
#include <qdatetime.h>

#define REBOOT_DAEMON       0           /*升级后是否重启守护程序 测试时关闭0 正常应打开1*/
#define REMOTE_DEBUG        1           /*远程诊断功能*/
#define UPDATE_APP_NOT_RUN  0           /*被升级程序没有运行时，是否直接升级 0-不升级 1-升级*/
#define TCP_PORT            6666        /*TCP端口    用于在线升级Server-Client*/
#define TCP_HEAD            99          /*TCP头部标识 用于在线升级Server-Client*/

/*本地软件代号*/
#define APP_NUM     9
typedef enum {
    WHICH_UPDATE_APP,               //在线升级
    WHICH_BILLING_CONTROL_APP,      //计费控制
    WHICH_GATEWAY_APP,              //后台通讯
    WHICH_MATRIX_CONTROL_APP,       //矩阵控制
    WHICH_CONFIG_APP,               //配置管理
    WHICH_DEBUG_APP,                //远程调试
    WHICH_DAEMON_APP,               //守护进程
    WHICH_CHARGE_OPERATE_APP,       //充电操作(人机交互)
    WHICH_CHARGE_CONTROL_APP        //充电控制 CAN通信
} WHICH_APP;

extern UInt8 APP_VERSION[4];        /*软件版本*/
extern UInt8 NewVersion[APP_NUM][4];/*保存其他APP的版本*/
extern int SERVER;                  /*部署位置标记 1：集中计费端 0：刷卡显示端*/
extern bool UFLASH_FLAG;            /*升级方式标志 true-U盘升级 false-运营平台升级*/
extern bool DEBUG_FLAG;             /*命令行执行时是否打开调试模式*/

extern QString UpdateCfg;           //配置文件
extern QString BackupDir;           //备份目录
extern QString TempDir;             //下载的程序文件临时存储目录
extern QString UpdatePackage;       //升级包
extern QString UpdateSumary;        //升级摘要文件名
extern QString Md5Cfg;              //本地保存升级包Md5文件
extern QString IpCfg;               //在线升级客户端保存的在线升级服务IP

extern QString FtpUser;             //ftp用户名
extern QString FtpPass;             //ftp密码
extern QString FtpPath;             //ftp路径

extern QString BillingCtrlDir;      //计费控制单元工作目录
extern QString GatewayDir;          //后台网关单元工作目录
extern QString MatrixCtrlDir;       //矩阵控制单元工作目录
extern QString ConfigDir;           //配置管理单元工作目录
extern QString DebugDir;            //远程调试单元工作目录
extern QString UpdateDir;           //在线升级单元工作目录
extern QString DaemonDir;           //守护进程工作目录
extern QString ChargeOperateDir;    //充电操作交互单元工作目录
extern QString ChargeCtrlDir;       //充电控制单元工作目录

extern QString BillingCtrlName;      //计费控制单元文件名
extern QString GatewayName;          //后台网关单元文件名
extern QString MatrixCtrlName;       //矩阵控制单元文件名
extern QString ConfigName;           //配置管理单元文件名
extern QString DebugName;            //远程调试单元文件名
extern QString UpdateName;           //在线升级单元文件名
extern QString DaemonName;           //守护进程文件名
extern QString ChargeOperateName;    //充电操作交互单元文件名
extern QString ChargeCtrlName;       //充电控制单元文件名

/*诊断参数*/
extern QString DebugArg;
extern QString DebugArg_QueryStakeInfo;
extern QString DebugArg_DownloadFile;
extern QString DebugArg_UpdateInfo;

#endif // APPINFO_H
