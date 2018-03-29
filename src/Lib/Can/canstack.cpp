#include "canstack.h"

#include "Comm/util.h"

#include <qdebug.h>

#include "App/ipcmsgproc.h"

CanStack * CanStack::m_instance = 0;

CanStack::CanStack():
    m_recvDataThd(this, &CanStack::recvDataThd),
    m_sendDataThd(this, &CanStack::sendDataThd),
    m_reSendDataThd(this, &CanStack::reSendDataThd)
{
    /*初始化多帧接收连接数组*/
    for (int i=0; i<MAX_TEMP_LINK_NUM;i++) {
        MultiFrameRecvLink[i].KeyPGN = 0;
    }

    startTimer(100);//启动定时器 超时时间是1ms

}

CanStack::~CanStack()
{
    if (DEBUG_FLAG) {
        qDebug("%s ***", __FUNCTION__);
    }

    m_recvDataThd.stop();
    m_sendDataThd.stop();
    m_reSendDataThd.stop();

    Close();
}


/**
 * @brief CanStack::Init 初始化函数
 * @param port 端口
 * @param baud 波特率
 * @param isMultiFrame
 * @param getPairedPF
 * @param pgnSendType
 * @param pgnRecvType
 * @param pgnAckTimeoutProc
 * @return
 */
int CanStack::Init(const char *port, int baud,
                   pIsMultiFrame isMultiFrame,
                   pGetPairedPF getPairedPF,
                   pPgnSendType pgnSendType,
                   pPgnRecvType pgnRecvType,
                   pPgnAckTimeoutProc pgnAckTimeoutProc)
{
    int ret = -1;

    m_isMultiFrame = isMultiFrame;
    m_getPairedPF = getPairedPF;
    m_pgnSendType = pgnSendType;
    m_pgnRecvType = pgnRecvType;
    m_pgnAckTimeoutProc = pgnAckTimeoutProc;

    if (Open(port, baud) > 0) {//打开CAN口
        m_sendDataThd.start();//启动数据发送线程
        m_recvDataThd.start();//启动数据接收线程
        m_reSendDataThd.start();//启动重发数据线程
        ret = 1;
    }

    return ret;
}

//打开CAN口
int CanStack::Open(const char *port,int baud)
{
    baud = baud;

    socket = ::socket(PF_CAN,SOCK_RAW,CAN_RAW);
    if (socket == -1) {
        return -1;
    }
    struct ifreq ifr;
    strcpy((char *)(ifr.ifr_name),port);
    ioctl(socket,SIOCGIFINDEX,&ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(socket,(struct sockaddr*)&addr,sizeof(addr)) < 0) {
        return -2;
    }

    return 1;
}

//关闭CAN口
void CanStack::Close()
{
    close(socket);
}


int CanStack::Write(const CanDataIF &canData)
{
    QMutexLocker locker(&m_sendMsgQueMutex);
    m_sendMsgQue.push(canData);//在队尾插入消息
    return canData.Len;
}

int CanStack::Write(const CanDataIF &canData, const UInt8 maxCnt, const UInt32 cycle)
{
    QMutexLocker locaker(&m_reSendMsgListMutex);

    ReCanData reCanData;
    reCanData.maxCnt = maxCnt;
    reCanData.sendCnt = 0;
    reCanData.cycle = cycle;
    reCanData.lastSndTime = QDateTime::currentMSecsSinceEpoch();
    reCanData.canData = canData;
    m_reSendMsgList.push_back(reCanData);

    return canData.Len;
}

bool CanStack::Read(CanDataIF &CanData)
{
    QMutexLocker locker(&m_recvMsgQueMutex);

    bool ret = false;
    if (!m_recvMsgQue.empty()) {
        CanData = m_recvMsgQue.front();
        m_recvMsgQue.pop();
        ret = true;
    }
    return ret;
}


/**
 * @brief CanStack::recvDataThd 接收数据线程
 */
void CanStack::recvDataThd()
{
    CAN_MSG CanFrame;
    UInt8 PF;

    while (!m_recvDataThd.isStopped()) {

        if (recvFramData(CanFrame) > 0) {//接收一帧数据
            //qDebug("%s 接收一帧数据成功", __FUNCTION__);

            PF = CanFrame.ID.bit.PF;
            if (!m_isMultiFrame(PF)) { //如果是单帧的数据包
                //qDebug("Recv Single Frame");
                procSingleFramePack(CanFrame);
            } else { //如果是多帧的数据包
                //qDebug("Recv Multi Frame %2x", PF);
                procMultiFramePack(CanFrame);
            }
        }
    }

}

/*接收一帧CAN数据*/
int CanStack::recvFramData(CAN_MSG &CanFrame)
{
    int ret = -1;
    int len = 0;
    struct can_frame frame;
    //struct sockaddr_can addr;

    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(socket, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    ret = select(socket+1, &rfds, NULL, NULL, &tv);
    if ( ret < 0) {
        perror("can select()");
    } else if (ret == 0) {
        //perror("can select() timeout");
    } else {
        if (FD_ISSET(socket, &rfds)) {
            ret = recvfrom(socket, &frame, sizeof(struct can_frame), 0, (struct sockaddr *)&addr, (socklen_t*)&len);
            if (ret <= 0) {
                perror("can recvfrom()");
                return -1;
            }
            CanFrame.DLC = frame.can_dlc;
            CanFrame.ID.ExtId = frame.can_id&0x1FFFFFFF;
            CanFrame.FrmDef.FF = frame.can_id>>31;
            CanFrame.FrmDef.RTR = (frame.can_id&0x40000000)>>30;
            memcpy(CanFrame.Buffer,frame.data,frame.can_dlc);

            //qDebug("ID:%x",CanFrame.ID.ExtId);

            //通过目标地址,命令过滤
            if ( !( (CanFrame.ID.bit.PS == CAN_SOURE_ADDR) &&
                    ( (CanFrame.ID.bit.PF == BOOT_LEAD_STARTUP) ||
                      (CanFrame.ID.bit.PF == REQUEST_APP_DATA_DLOAD) ||
                      (CanFrame.ID.bit.PF == UPGRADE_OVER) ||
                      (CanFrame.ID.bit.PF == READ_CHARGE_CTRL_INFO_ACK) ) ) ) {

                ret = -2;
            }

        } else {
            ret = -3;
        }

    }

    return ret;
}

/**
 * @brief CanStack::WaitForRecv 等待直到可以接收数据
 */
void CanStack::waitForRecv()
{
    int ret;
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(socket, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    ret = select(socket+1, &rfds, NULL, NULL, &tv);
    if ( ret < 0) {
        perror("select() can ");
    } else if (ret == 0) {
        perror("select() can timeout");
    }

}


/**
 * @brief CanStack::procSingleFramePack 单帧处理函数,接收到的帧如果是单帧包，则由该函数负责处理(或者丢弃)
 * @param CanFrame
 * @return
 */
void CanStack::procSingleFramePack(CAN_MSG& CanFrame)
{
    CanDataIF CanPack;

    /*保存单帧数据*/
    IDValueCopy(CanPack.ID, CanFrame.ID);
    FrmDefValueCopy(CanPack.FrmDef, CanFrame.FrmDef);
    CanPack.Len = CanFrame.DLC;
    for (int i=0; i<CanPack.Len; i++) {
        CanPack.Buffer[i] = CanFrame.Buffer[i];
    }
    putOneMsgToRecvMsgQue(CanPack);//将CAN数据包存入消息接收队列
}

//将CAN数据包存入消息接收队列
void CanStack::putOneMsgToRecvMsgQue(CanDataIF &CanPack)
{
    QMutexLocker locker(&m_recvMsgQueMutex);

    if (m_pgnRecvType(CanPack.ID.bit.PF) == CAN_PGN_TYPE_NOTIFY) {
        m_recvMsgQue.push(CanPack);
    } else if (m_pgnRecvType(CanPack.ID.bit.PF) == CAN_PGN_TYPE_REQ_ACK) {
        UInt8 pgn;
        UInt8 pgn_ack = CanPack.ID.bit.PF;
        if (m_getPairedPF(pgn_ack, pgn)) {
            //在重发队列中找到对应的请求命令
            if (findAndDel(pgn)) {
                m_recvMsgQue.push(CanPack);
            }
        }
    } else {
        qDebug("%s 未知的命令：%x", __FUNCTION__, CanPack.ID.ExtId);
    }

}


/**
 * @brief CanStack::procMultiFramePack 多帧处理函数,接收到的帧如果是多帧包，则由该函数负责处理重组数据包(或者丢弃)
 * @param CanFrame
 * @return
 */
void CanStack::procMultiFramePack(CAN_MSG& CanFrame)
{
    int LinkIndex = -1;

    /*判断该帧数据属于已有多帧数据包还是新的多帧数据包*/
    if (CanFrame.Buffer[0] == 1) {//如果是第一帧(第一个数据是1)，则可能是:1、已有的连接，重发的数据包，2、新的多帧数据包
        if ((LinkIndex=getUsedLinkIndex(CanFrame)) != -1) {//如果能找到已有连接序号，说明该帧为重发的数据包的第一帧
            saveFirstFrameData(LinkIndex, CanFrame);
        } else if((LinkIndex=getFreeLinkIndex()) != -1) {//有可用连接，则立即接收该帧
            saveFirstFrameData(LinkIndex, CanFrame);
        } else {
            //丢弃该帧数据
        }
    } else {//如果不是第一帧,则可能是:1、已有多帧数据包的帧(将数据添加到已有连接里)，2、错误的帧(直接丢弃)
        if((LinkIndex=getUsedLinkIndex(CanFrame)) != -1) { //获取到已有连接序号,说明是已有多帧数据包的帧
            saveOtherFrameData(LinkIndex, CanFrame);
        } else { //获取连接序号错误，则表示该帧是错误的帧
            //丢弃错误的帧
        }
    }

}

/**
 * @brief CanStack::getUsedLinkIndex 获取已有连接的序号
 * 根据帧数据，获取已有连接序号，如果没有找到，则返回错误
 * @param CanFrame
 * @return
 */
int CanStack::getUsedLinkIndex(CAN_MSG &CanFrame)
{
    int index = -1;

    for (int i=0; i<MAX_TEMP_LINK_NUM;i++) {
        if(CanFrame.ID.bit.PF == MultiFrameRecvLink[i].KeyPGN) { //只靠PF来标识一个连接可能有问题
            index = i;
            break;
        }
    }
    return index;
}


/**
 * @brief CanStack::getFreeLinkIndex 获取空闲连接序号
 * 根据keyPGN的值判断是否有空闲的连接可用，空闲则返回其序号，无空闲连接返回错误
 * @return
 */
int CanStack::getFreeLinkIndex()
{
    int index = -1;

    for (int i=0; i<MAX_TEMP_LINK_NUM;i++) {
        if (MultiFrameRecvLink[i].KeyPGN == 0) {
            index = i;
            break;
        }
    }
    return index;
}


/**
 * @brief CanStack::saveFirstFrameData 首帧处理
 * @param LinkIndex
 * @param CanFrame
 * @return
 */
void CanStack::saveFirstFrameData(int LinkIndex, CAN_MSG &CanFrame)
{
    //qDebug("FrameNum %d", CanFrame.Buffer[1]);

    MultiFrameRecvLink[LinkIndex].KeyPGN = CanFrame.ID.bit.PF;//仅仅靠PF来决定keyPGN可能有问题
    MultiFrameRecvLink[LinkIndex].Timeout = 50;//超时时间(ms)
    MultiFrameRecvLink[LinkIndex].PreNO = CanFrame.Buffer[0];//保存当前帧序号
    MultiFrameRecvLink[LinkIndex].Count = CanFrame.Buffer[1];//保存总帧数
    MultiFrameRecvLink[LinkIndex].Len = UInt16(CanFrame.Buffer[2]|(CanFrame.Buffer[3]<<8));//报文数据长度
    MultiFrameRecvLink[LinkIndex].Buffer.clear();

    for (int i=0; i<7; i++) { /*拷贝数据*/
        MultiFrameRecvLink[LinkIndex].Buffer[i] = CanFrame.Buffer[i+1];
    }

}


/**
 * @brief CanStack::saveOtherFrameData 其他帧处理
 * @param LinkIndex
 * @param CanFrame
 * @return
 */
bool CanStack::saveOtherFrameData(int LinkIndex, CAN_MSG &CanFrame)
{
    CanDataIF CanPack;
    unsigned int i;
    UInt8 buf[2048];

    //qDebug("Other Frame %d", CanFrame.Buffer[0]);

    if ((MultiFrameRecvLink[LinkIndex].PreNO+1) != CanFrame.Buffer[0]) { //如果是乱序帧
        MultiFrameRecvLink[LinkIndex].KeyPGN = 0;//释放连接
        MultiFrameRecvLink[LinkIndex].Buffer.clear();//清空数据
        return false;
    } else {

        MultiFrameRecvLink[LinkIndex].PreNO = CanFrame.Buffer[0];//保存当前帧序号
        for(i=0; i<7; i++) {//拷贝数据
            MultiFrameRecvLink[LinkIndex].Buffer[i+(CanFrame.Buffer[0]-1)*7] = CanFrame.Buffer[i+1];
        }
        if (MultiFrameRecvLink[LinkIndex].Count == CanFrame.Buffer[0]) {//如果是最后一帧
            //qDebug("Last Frame %d", CanFrame.Buffer[0]);
            for(int j=0; j<MultiFrameRecvLink[LinkIndex].Buffer.length(); j++) {
                buf[j] = MultiFrameRecvLink[LinkIndex].Buffer[j];
            }
            if (calcSum(buf, MultiFrameRecvLink[LinkIndex].Len+5, CAN_CHECKSUM_RECV)) {//数据校验通过

                /*将数据放入消息接收队列里*/
                IDValueCopy(CanPack.ID, CanFrame.ID);
                FrmDefValueCopy(CanPack.FrmDef, CanFrame.FrmDef);
                CanPack.Len = MultiFrameRecvLink[LinkIndex].Len;
                for (i=0; i<CanPack.Len; i++) {
                    CanPack.Buffer[i] = MultiFrameRecvLink[LinkIndex].Buffer[i+3];
                }

                putOneMsgToRecvMsgQue(CanPack);//将CAN数据包存入消息接收队列

                MultiFrameRecvLink[LinkIndex].KeyPGN = 0;//释放连接
                MultiFrameRecvLink[LinkIndex].Buffer.clear();//清空数据

                //qDebug("CalcSum Sucess");
                return true;
            } else {//数据校验失败

                MultiFrameRecvLink[LinkIndex].KeyPGN = 0;//释放连接
                MultiFrameRecvLink[LinkIndex].Buffer.clear();//清空数据
                //qDebug("CalcSum Failed");
                return false;
            }
        } else {
            //qDebug("Middle Frame %d", CanFrame.Data[0]);
            return false;
        }
    }
}


/**
 * @brief CanStack::sendDataThd 发送数据线程
 */
void CanStack::sendDataThd()
{
    CanDataIF CanData;
    UInt8 PF;

#if REMOTE_DEBUG
    QString strHead;
    IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();
#endif

    while(!m_sendDataThd.isStopped()) {

        if(getOneMsgFromSendMsgQue(CanData)) {  //取出消息

            /*判断数据包是否为多帧*/
            PF = CanData.ID.bit.PF;

            if(!m_isMultiFrame(PF)) { //如果是单帧的数据包
                //qDebug("Send Single Frame %2x", PF);
                sendSingleFramePack(CanData);

            } else { //如果是多帧的数据包
                //qDebug("Send Multi Frame %2x", PF);
                sendMultiFramePack(CanData);
            }

#if REMOTE_DEBUG
            strHead = QString("CAN发送 ID=%1 len=%2").arg(CanData.ID.ExtId,2,16).arg(CanData.Len);
            ipcMsgProc->uploadDebugBuffer(strHead,CanData.Buffer.data(),CanData.Len,DEBUG_ARG_SEND_CAN);
#endif
        }

        Util::delayMs(10);
    }
}

//从消息发送队列里取出一条消息
bool CanStack::getOneMsgFromSendMsgQue(CanDataIF &CanPack)
{
    QMutexLocker lock(&m_sendMsgQueMutex);

    bool ret = false;
    if (!m_sendMsgQue.empty()) {
        CanPack = m_sendMsgQue.front();//返回队头消息
        m_sendMsgQue.pop();//删除队头消息
        ret = true;
    }
    return ret;
}


/*发送单帧的CAN数据包*/
bool CanStack::sendSingleFramePack(const CanDataIF &CanPack)
{
    bool ret = true;
    CAN_MSG CanFrame;

    /*使用CanPack初始化CAN_MSG*/
    IDValueCopy(CanFrame.ID, CanPack.ID);
    FrmDefValueCopy(CanFrame.FrmDef, CanPack.FrmDef);
    CanFrame.DLC = CanPack.Len;
    for (unsigned int i=0; i<CanFrame.DLC; i++) {
        CanFrame.Buffer[i] = CanPack.Buffer[i];
    }

    if(sendFramData(CanFrame) < 0) {//发送一帧数据
        ret = false;
    }

    return ret;
}


/*发送多帧的CAN数据包*/
bool CanStack::sendMultiFramePack(const CanDataIF &CanPack)
{
    CAN_MSG CanFrame;
    UInt8 buf[2048];
    int i, len, count;

    /*使用CanPack初始化CAN_MSG*/
    IDValueCopy(CanFrame.ID, CanPack.ID);
    FrmDefValueCopy(CanFrame.FrmDef, CanPack.FrmDef);
    CanFrame.DLC = 8;
    len = CanPack.Len;

    buf[0] = UInt16((len+5)/7 + ((((len+5)%7)>0)?1:0)); //总帧数
    buf[1] = len&0xff;//报文有效数据长度低字节
    buf[2] = len>>8;//报文有效数据长度高字节
    for(i=0; i<len; i++){
        buf[i+3] = CanPack.Buffer[i];
    }
    calcSum(buf, len+3, CAN_CHECKSUM_SEND);

    for(count=0; count<buf[0]; count++) { //逐帧发送
        //qDebug("send %d Frame", count+1);
        CanFrame.Buffer[0] = count+1;              //当前帧序号，1~255
        for(i=0; i<7; i++) {
            CanFrame.Buffer[i+1] = buf[i+7*count];
        }

        if (sendFramData(CanFrame)< 0) {
            return false;
        }

        Util::delayMs(1.5);
    }

    return true;
}


/**
 * @brief CanStack::sendFramData 发送一帧CAN数据
 * @param CanFrame
 * @return
 */
int CanStack::sendFramData(const CAN_MSG &CanFrame)
{
    struct can_frame frame;
    //struct sockaddr_can addr;

    waitForSend();

    frame.can_dlc = CanFrame.DLC;
    frame.can_id = CanFrame.ID.ExtId;
    frame.can_id &= ~(1<<30);
    frame.can_id &= ~(1<<29);
    frame.can_id |= 1<<31;
    memcpy(frame.data,CanFrame.Buffer,CanFrame.DLC);

    int ret = sendto(socket,&frame,sizeof(can_frame),0,(struct sockaddr*)&addr,sizeof(addr));
    if( ret != sizeof(can_frame)) {
        ret = -1;
    }

    //qDebug("SendFrameData PF: %x",CanFrame.ID.bit.PF);
    //qDebug("SendFrameData PS: %x %d",CanFrame.ID.bit.PS,CanFrame.ID.bit.PS);

    return ret;
}


/**
 * @brief CanStack::WaitForSend 等待直到可以发送数据
 */
void CanStack::waitForSend()
{
    int ret;
    fd_set wfds;
    //struct timeval tv;

    FD_ZERO(&wfds);
    FD_SET(socket, &wfds);
    //tv.tv_sec = tv.tv_usec = 0;

    ret = select(socket+1, NULL, &wfds, NULL, NULL);
    if (ret < 0) {
        perror("select() can send");
    } else if (ret == 0){
        perror("select() can timeout");
    }
}


/**
 * @brief CanStack::calcSum 多帧校验计算
 * @param buffer
 * @param length
 * @param fig
 * @return
 */
UInt16 CanStack::calcSum(UInt8 * buffer, UInt16 length, UInt16 fig)
{
   UInt16 sum;
   UInt16 i;

   sum = 0;
   for (i = 0 ;i < length ;i++) {
       sum += buffer[i];
   }

   if (fig == CAN_CHECKSUM_SEND) {
       buffer[i++] = sum & 0xff;
       buffer[i] = sum >> 8;
       return sum;
   } else {
       sum -= (UInt16)(buffer[i-1] + buffer[i-2]);
       if(((sum&0xff)==buffer[i-2]) && ((sum>>8)==buffer[i-1]))
           return TRUE;
       else
           return FALSE;
   }
}


/**
 * @brief CanStack::reSendDataThd 重发数据线程
 */
void CanStack::reSendDataThd()
{
    while(!m_reSendDataThd.isStopped()) {

        reSendCanData();

        Util::delayMs(10);
    }
}


void CanStack::reSendCanData()
{
    QMutexLocker locker(&m_reSendMsgListMutex);

    vector<ReCanData>::iterator iter;
    ReCanData reCanData;
    UInt64 curTime;

    if (!m_reSendMsgList.empty()) {
        iter = m_reSendMsgList.begin();
        while (iter != m_reSendMsgList.end()) {
            reCanData = *iter;
            curTime = QDateTime::currentMSecsSinceEpoch();

            if (reCanData.sendCnt >= reCanData.maxCnt) {
                if (m_pgnSendType(reCanData.canData.ID.bit.PF) == CAN_PGN_TYPE_REQ_ACK) {
                    //超时处理 - 回调函数
                    m_pgnAckTimeoutProc(reCanData.canData);

                } else if (m_pgnSendType(reCanData.canData.ID.bit.PF) == CAN_PGN_TYPE_NOTIFY) {
                    //pgn 通知类型，不用处理
                }
                iter = m_reSendMsgList.erase(iter);
            } else {
                if (curTime > reCanData.lastSndTime + reCanData.cycle) {
                    reCanData.sendCnt++;
                    reCanData.lastSndTime = curTime;
                    *iter = reCanData;

                    Write(reCanData.canData);
                }

                iter++;
            }
        }
    }

}



bool CanStack::findAndDel(UInt8 pgn)
{
    QMutexLocker locker(&m_reSendMsgListMutex);

    bool ret = false;
    vector<ReCanData>::iterator iter;
    ReCanData reCanData;

    for (iter=m_reSendMsgList.begin(); iter!=m_reSendMsgList.end(); iter++) {
        reCanData = *iter;
        if (reCanData.canData.ID.bit.PF == pgn) {
            m_reSendMsgList.erase(iter);
            ret = true;
            break;
        }
    }

    return ret;
}




/**
 * @brief CanStack::IDValueCopy ID数据复制
 * @param dst
 * @param src
 */
void CanStack::IDValueCopy(CanID &dst, const CanID &src)
{
    dst.bit.SA = src.bit.SA;
    dst.bit.PS = src.bit.PS;
    dst.bit.PF = src.bit.PF;
    dst.bit.DP = src.bit.DP;
    dst.bit.R = src.bit.R;
    dst.bit.P = src.bit.P;
}


/**
 * @brief CanStack::FrmDefValueCopy ID数据复制
 * @param dst
 * @param src
 */
void CanStack::FrmDefValueCopy(CanFrmDef &dst, const CanFrmDef &src)
{
    dst.FF = src.FF;
    dst.RTR = src.RTR;
}




/**
 * @brief CanStack::timerEvent 定时器事件
 * @param e
 */
void CanStack::timerEvent(QTimerEvent *e)
{
    e = e;
    LinkTimeoutProc();
}

/**
 * @brief CanStack::LinkTimeoutProc 连接超时处理
 */
void CanStack::LinkTimeoutProc()
{
    for(int i=0; i<MAX_TEMP_LINK_NUM;i++) {
        if (MultiFrameRecvLink[i].KeyPGN == 0) {//该连接未使用
            continue;
        } else {
            if (MultiFrameRecvLink[i].Timeout <= 0) {//如果连接超时，则释放连接
               MultiFrameRecvLink[i].KeyPGN = 0;//释放连接
               MultiFrameRecvLink[i].Buffer.clear();//清空数据
               MultiFrameRecvLink[i].Timeout = 0;//清空倒计时
            } else {
               MultiFrameRecvLink[i].Timeout--;//超时时间减一
            }
        }
    }
}



void CanStack::addPGN(UInt8 pgn)
{
    QMutexLocker locker(&m_sendPGN_mutex);
    m_sendPGN_set.insert(pgn);
}

bool CanStack::removePGN(UInt8 pgn)
{
    bool ret = false;
    set<UInt8>::iterator it;
    QMutexLocker locker(&m_sendPGN_mutex);

    it = m_sendPGN_set.find(pgn);//查找发送PGN集合里是否有PGN
    if(it != m_sendPGN_set.end()) {//如果找到对应的PGN
        m_sendPGN_set.erase(it);//删除PGN
        ret = true;
    }

    return ret;
}




