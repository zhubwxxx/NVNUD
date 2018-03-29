#ifndef IPCSTACK_H
#define IPCSTACK_H

#include "ipcprotocol.h"

#include "Comm/comm.h"
#include "Config/appinfo.h"

#include <qmutex.h>

#include <vector>
#include <queue>
#include <set>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>

using namespace std;


/*回调函数*/
typedef int  (*pCmdSendType)(int cmd);
typedef int  (*pCmdRecvType)(int cmd);
typedef bool (*pGetPairedCMD)(UInt16 cmd_ack, UInt16 &cmd);
typedef void (*pCmdAckTimeoutProc)(IpcData &ipcData);

class IpcStack : public QObject
{
    Q_OBJECT
public:
    static IpcStack* Instance()
    {
        static QMutex Mutex;
        if (!m_instance) {
            QMutexLocker locker(&Mutex);
            if (!m_instance){
                m_instance = new IpcStack;
            }
        }
        return m_instance;
    }

    ~IpcStack();

    void Init(pCmdAckTimeoutProc proc,
              pCmdSendType cmdSendType,
              pCmdRecvType cmdRecvType,
              pGetPairedCMD getPairedCMD);
    bool Write(const IpcData &ipcData); //向发送队列写入一帧数据
    bool Write(const IpcData &ipcData, const UInt8 maxCnt, const UInt32 cycle);
    bool Read(IpcData &ipcData); //从接收队列读取一帧数据

private:
    explicit IpcStack();

    void initsockFd();
    void initReadSockFd(int &sockFd, UInt16 port);
    void initWriteSockFd(int &sockFd);

    void recvDataThd(); //消息接收线程
    void readData(IpcData &ipcData);
    void saveData(IpcData &ipcData, int sockFd, WHICH_APP who);
    int  parseData(IpcData &dest,char *src, int len);
    void putOneMsgToRecvMsgQue(IpcData &ipcData);
    UInt8 confirmCheckSum(char *buf, int len);

    void sendDataThd(); //消息发送线程
    bool getOneMsgFromSendMsgQue(IpcData &ipcData);
    int  writeData(IpcData &ipcData);
    void waitForSend();
    UInt8 calcCheckSum(char *buf, int len);

    void reSendDataThd(); //消息重发线程
    void reWriteData();
    bool findAndDel(int cmd);

    void addCMD(UInt16 cmd);
    bool removeCMD(UInt16 cmd);
    bool getPairedCMD(UInt16 cmd_ack, UInt16 &cmd);

    static IpcStack *m_instance;

    pCmdSendType m_cmdSendType;
    pCmdRecvType m_cmdRecvType;
    pGetPairedCMD m_getPairedCMD;
    pCmdAckTimeoutProc m_cmdAckTimeoutProc;

    UInt16 port_BillingCtrlBind;
    UInt16 port_BillingCtrlSend;
    UInt16 port_GatewayBind;
    UInt16 port_GatewaySend;
    UInt16 port_MatrixCtrlBind;
    UInt16 port_MatrixCtrlSend;
    UInt16 port_ConfigBind;
    UInt16 port_ConfigSend;
    UInt16 port_DebugBind;
    UInt16 port_DebugSend;
    //UInt16 port_DaemonBind;
    UInt16 port_DaemonSend;
    UInt16 port_ChargeOperateBind;
    UInt16 port_ChargeOperateSend;

    int m_sockBillingCtrl;      /*读sockFd*/
    int m_sockGateway;          /*读sockFd*/
    int m_sockMatrixCtrl;       /*读sockFd*/
    int m_sockConfig;           /*读sockFd*/
    int m_sockDebug;            /*读sockFd*/
    int m_sockChargeOperate;    /*读sockFd*/
    int m_maxSockFd;            /*读sockFd中最大的*/
    int m_writeSockFd;          /*专门用来写的sockFd*/

    Worker<IpcStack> m_recvDataThd;     //消息接收线程
    Worker<IpcStack> m_sendDataThd;     //消息发送线程
    Worker<IpcStack> m_reSendDataThd;   //消息重发线程

    queue<IpcData> m_recvMsgQue; //消息接收队列
    QMutex m_recvMsgQueMutex;

    queue<IpcData> m_sendMsgQue; //消息发送队列
    QMutex m_sendMsgQueMutex;

    vector<ReIpcData> m_reSendMsgList;  //重发消息队列
    QMutex m_reSendMsgListMutex;

    set<UInt16> m_sendCMDSet;
    QMutex m_sendCMDSetMutex;


};

#endif // IPCSTACK_H
