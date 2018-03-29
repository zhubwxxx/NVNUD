#ifndef CANMSGPROC_H
#define CANMSGPROC_H

#include "Comm/comm.h"
#include "Lib/Can/canstack.h"

#include <qtimer.h>

/*回调函数*/
bool isMultiFrame(UInt8 pgn);               /*判断是否为多帧*/
bool getPairedPF(UInt8 pgn_ack, UInt8 &pgn);/*获取对应PGN*/
int  pgnSendType(UInt8 pgn);                /*can发送帧类型*/
int  pgnRecvType(UInt8 pgn);                /*can接收帧类型*/
void pgnAckTimeoutProc(CanDataIF &canData); /*CAN命令应答超时处理*/

class CanMsgProc : public QObject
{
    Q_OBJECT

public:
    ~CanMsgProc();

    static CanMsgProc *Instance()
    {
        static QMutex Mutex;
        if (!m_instance) {
            QMutexLocker locker(&Mutex);
            if (!m_instance) {
                m_instance = new CanMsgProc;
            }
        }
        return m_instance;
    }

public:
    void queryVersion(UInt8 stakeAddr);

private:
    explicit CanMsgProc(QObject *parent = 0);

    void msgProcThd(); //消息处理线程
    void packCanHead(CanDataIF &canData, UInt8 SA, UInt8 PS, UInt8 PF, UInt8 P, int FF, int RTR);//打包Can头部

    void replyVersionProc(CanDataIF &canData);
    void bootLeadStartupProc(CanDataIF &canData);
    void reqAppDataDLoadProc(CanDataIF &canData);
    void upgradeOverProc(CanDataIF &canData);

private:
    static CanMsgProc *m_instance;

    Worker<CanMsgProc> m_msgProcThd;        /*消息处理线程*/

    CanStack *m_CanStack;

};

#endif // CANMSGPROC_H
