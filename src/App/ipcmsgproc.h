#ifndef IPCMSGPROC_H
#define IPCMSGPROC_H

#include "Comm/comm.h"
#include "Config/appinfo.h"

#include "Lib/Ipc/ipcstack.h"
#include "Lib/Transport/transport.h"


using namespace std;

/*回调函数*/
int  cmdSendType(int cmd);
int  cmdRecvType(int cmd);
bool getPairedCMD(UInt16 cmd_ack, UInt16 &cmd);
void cmdAckTimeoutProc(IpcData &ipcData);

class IpcMsgProc : public QObject
{
    Q_OBJECT

public:
    ~IpcMsgProc();

    static IpcMsgProc *Instance()
    {
        static QMutex Mutex;
        if (!m_instance) {
            QMutexLocker locker(&Mutex);
            if (!m_instance) {
                m_instance = new IpcMsgProc;
            }
        }
        return m_instance;
    }

signals:
    void sigDownloadFile(QString filename);
    void sigIp(UInt32 ip);
    void sigQuit();

private slots:
    void downloadFile(QString filename);
    void downloadFileOver(bool flag);
    void on_ip(UInt32 ip);
    void on_quit();

public:
    void queryIp();
    void chargeCtrlResetTimeout();

    void uploadDebugLogInfo(QString info);         /*上传日志信息*/
    void uploadDebugBuffer(QString strHead, char *buf, int len, string flag); /*上传接收的信息*/

private:
    explicit IpcMsgProc(QObject *parent = 0);

    void wdtThd();                               /*看门狗线程*/
    void queryStackInfoThd();                    /*查询桩编码线程*/
    void downloadFileThd();                      /*下载App线程*/
    void updateAppThd();                         /*升级App线程*/
    void ipcMsgThd();                            /*消息处理线程*/

    /*消息处理线程处理函数*/
    void recvUpdateArgProc(IpcData &ipcData);
    void replyVersionProc(IpcData &ipcData);
    void replyRunStatusProc(IpcData &ipcData);
    void queryVersionProc(IpcData &ipcData);
    void recvIpProc(IpcData &ipcData);

    /*U盘升级处理*/
    void uFlashUpdateProc(IpcData &ipcData);
    void stakeExist(StakeInfo &stakeInfo);
    void stakeNotExist(StakeInfo &stakeInfo);
    void uflashLaunch(IpcData &ipcData, StakeInfo &stakeInfo);
    void uFlashStakeInfo(StakeInfo &stakeInfo);

    void setDebugSwitchReqProc(IpcData &ipcData);  /*设置诊断开关请求处理*/
    void getDebugSwitchReqProc(IpcData &ipcData);  /*获取诊断开关请求处理*/
    void setDebugArgReqProc(IpcData &ipcData);     /*设置诊断参数请求处理*/
    void getDebugArgReqProc(IpcData &ipcData);     /*获取诊断参数请求处理*/

    int  parseUpdateSummary(UInt8 version[][4]);
    void seletcUpdateApp();
    void updateApp(WHICH_APP who);

    void packIpcHead(IpcData &ipcData, UInt16 frameNum, UInt16 cmd, UInt8 flag);


private:
    static IpcMsgProc *m_instance;

    Worker<IpcMsgProc> m_wdtThd;                 /*看门狗线程*/
    Worker<IpcMsgProc> m_queryStackInfoThd;      /*查询桩编码线程*/
    Worker<IpcMsgProc> m_downloadFileThd;        /*下载App处理线程*/
    Worker<IpcMsgProc> m_updateAppThd;           /*升级App线程*/
    Worker<IpcMsgProc> m_ipcMsgThd;              /*消息处理线程*/

    IpcStack *m_IpcStack;
    Transport *transport;

    UInt32 m_debugIp;          /*远程诊断-IP*/
    UInt16 m_debugPort;        /*远程诊断-端口*/
    UInt8 m_debugSwitch;       /*远程诊断-上传总开关*/
    UInt8 m_debugLogSwitch;    /*远程诊断-日志信息上传开关*/
    UInt8 m_debugRecvCanSwitch;/*远程诊断-CAN接收信息上传开关*/
    UInt8 m_debugSendCanSwitch;/*远程诊断-CAN发送信息上传开关*/
    UInt8 m_debugRecvUdpSwitch;/*远程诊断-UDP接收信息上传开关*/
    UInt8 m_debugSendUdpSwitch;/*远程诊断-UDP发送信息上传开关*/

public:
    void pushUpdateArg(UpdateArg arg);
    bool getUpdateArg(UpdateArg &arg);
    void popUpdateArg();
    void setFlagUpdateArg(bool flag);
    bool isUpdateArgQueEmpty();

    void pushStakeArg(StakeArg arg);
    bool getStakeArg(StakeArg &arg);
    void popStakeArg();
    void setFlagStakeArg(bool flag);
    bool isStakeArgQueEmpty();
    int  stakeArgQueSize();

    void pushUpdateApp(WHICH_APP who);
    bool getUpdateApp(WHICH_APP &who);
    void popUpdateApp();
    void setFlagUpdateApp(bool flag);
    void clearUpdateApp();
    bool isUpdateAppQueEmpty();

    void sendUpdateRet(UInt8 flag);             /*发送桩升级结果*/
    void setUpdateRet(WHICH_APP who, Int8 flag);/*设置当前升级桩中某个APP升级结果*/


private:
    queue<UpdateArg> m_updateArgQue;            /*升级参数队列*/
    QMutex m_updateArgQueMutex;
    queue<StakeArg> m_stakeArgQue;              /*桩参数队列*/
    QMutex m_stakeArgQueMutex;
    priority_queue<WHICH_APP> m_updateAppQue;   /*升级APP优先级队列*/
    QMutex m_updateAppQueMutex;

    volatile bool m_flagUpdateArg;  /*升级参数队列标志（是否读取下一条记录）*/
    volatile bool m_flagStakeArg;   /*桩参数队列标志（是否读取下一条记录）*/
    volatile bool m_flagUpdateApp;  /*升级APP优先级队列标志（是否读取下一条记录）*/

    UpdateArg m_updateArg;          /*当前升级参数*/
    StakeArg m_stakeArg;            /*当前升级桩参数*/
    WHICH_APP m_who;                /*当前升级APP*/
    UpdateRet m_updateRet;          /*当前桩升级结果*/

};

#endif // IPCMSGPROC_H
