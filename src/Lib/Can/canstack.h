#ifndef CANSTACK_H
#define CANSTACK_H

#include "canprotocol.h"

#include "Comm/comm.h"
#include "Config/appinfo.h"

#include <qobject.h>
#include <qmutex.h>

#include <queue>
#include <vector>
#include <set>
#include <map>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/can.h>
#include <unistd.h>

using namespace std;

/*回调函数*/
typedef bool (*pIsMultiFrame)(UInt8 MsgType);               /*判断是否为多帧*/
typedef bool (*pGetPairedPF)(UInt8 ACK_PF, UInt8 &CMD_PF);  /*获取对应PGN*/
typedef int  (*pPgnSendType)(UInt8 pgn);                    /*can发送帧类型*/
typedef int  (*pPgnRecvType)(UInt8 pgn);                    /*can接收帧类型*/
typedef void (*pPgnAckTimeoutProc)(CanDataIF &canData);     /*CAN命令应答超时处理*/

class CanStack:public QObject
{
    Q_OBJECT
public:
    static CanStack* Instance()
    {
        static QMutex Mutex;
        if(!m_instance)
        {
            QMutexLocker locker(&Mutex);
            if(!m_instance)
            {
                m_instance = new CanStack;
            }
        }
        return m_instance;
    }

    ~CanStack();

    int Init(const char* port,int baud,
             pIsMultiFrame isMultiFrame,
             pGetPairedPF getPairedPF,
             pPgnSendType pgnSendType,
             pPgnRecvType pgnRecvType,
             pPgnAckTimeoutProc pgnAckTimeoutProc);

    int  Write(const CanDataIF &canData); //向发送队列写入一帧数据
    int  Write(const CanDataIF &canData, const UInt8 maxCnt, const UInt32 cycle);
    bool Read(CanDataIF &CanData);//从接收队列读取一帧数据

private:
    explicit CanStack();

    int  Open(const char* port=CAN_DEVICE_INTERFACE,int baud=CAN_BPS);//打开CAN口
    void Close();//关闭CAN口

    void recvDataThd();//接收数据线程
    int  recvFramData(CAN_MSG &CanFrame);//接收一帧数据
    void waitForRecv();//等待接收数据
    void procSingleFramePack(CAN_MSG &CanFrame);//处理单帧的数据包
    void procMultiFramePack(CAN_MSG &CanFrame);//单帧多帧的数据包
    void putOneMsgToRecvMsgQue(CanDataIF &CanPack);

    int  getFreeLinkIndex();//获取空闲的连接序号
    int  getUsedLinkIndex(CAN_MSG &CanFrame);//获取被使用的连接序号

    void saveFirstFrameData(int LinkIndex, CAN_MSG &CanFrame);
    bool saveOtherFrameData(int LinkIndex, CAN_MSG &CanFrame);

    void sendDataThd();//发送数据线程
    bool getOneMsgFromSendMsgQue(CanDataIF &CanPack);
    bool sendSingleFramePack(const CanDataIF &CanPack);
    bool sendMultiFramePack(const CanDataIF &CanPack);
    int  sendFramData(const CAN_MSG &CanFrame);//发送一帧数据
    void waitForSend();//等待发送数据

    void reSendDataThd(); //重发数据线程
    void reSendCanData();
    bool findAndDel(UInt8 pgn);

    UInt16 calcSum(UInt8 *buffer, UInt16 length, UInt16 fig);//校验
    void IDValueCopy(CanID &dst, const CanID &src);//ID数据复制
    void FrmDefValueCopy(CanFrmDef &dst, const CanFrmDef &src);//FrmDef数据复制
    void LinkTimeoutProc();//连接超时处理

    void addPGN(UInt8 pgn);
    bool removePGN(UInt8 pgn);

private:
    static CanStack *m_instance;

    pIsMultiFrame m_isMultiFrame;
    pGetPairedPF m_getPairedPF;
    pPgnSendType m_pgnSendType;
    pPgnRecvType m_pgnRecvType;
    pPgnAckTimeoutProc m_pgnAckTimeoutProc;

    int socket;
    struct sockaddr_can addr;

    Worker<CanStack> m_recvDataThd;     //接收数据线程
    Worker<CanStack> m_sendDataThd;     //发送数据线程
    Worker<CanStack> m_reSendDataThd;   //重发数据线程

    queue<CanDataIF> m_recvMsgQue;//消息接收队列
    CanDataLink MultiFrameRecvLink[MAX_TEMP_LINK_NUM];//多帧接收连接
    QMutex m_recvMsgQueMutex;

    queue<CanDataIF> m_sendMsgQue;//消息发送队列
    QMutex m_sendMsgQueMutex;

    vector<ReCanData> m_reSendMsgList; //消息重发队列
    QMutex m_reSendMsgListMutex;

    set<UInt8> m_sendPGN_set; //PF
    QMutex m_sendPGN_mutex;


private slots:
    void timerEvent(QTimerEvent * e);//定时器事件
};


#endif // CANSTACK_H
