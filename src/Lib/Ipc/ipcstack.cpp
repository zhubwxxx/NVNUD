#include "ipcstack.h"

#include "Comm/util.h"


IpcStack *IpcStack::m_instance = 0;

IpcStack::IpcStack() :
    m_recvDataThd(this, &IpcStack::recvDataThd),
    m_sendDataThd(this, &IpcStack::sendDataThd),
    m_reSendDataThd(this, &IpcStack::reSendDataThd)
{
    port_BillingCtrlBind = 10004;
    port_BillingCtrlSend = 10005;
    port_MatrixCtrlBind = 10006;
    port_MatrixCtrlSend = 10007;
    port_ConfigBind = 10012;
    port_ConfigSend = 10013;
    port_GatewayBind = 10000;
    port_GatewaySend = 10001;
    port_DebugBind = 10002;
    port_DebugSend = 10003;
    //port_DaemonBind = 10033; //待定
    port_DaemonSend = 10034;
    port_ChargeOperateBind = 10008;
    port_ChargeOperateSend = 10009;


    initsockFd();
}

IpcStack::~IpcStack()
{
    if (DEBUG_FLAG) {
        qDebug("%s ***", __FUNCTION__);
    }

    m_recvDataThd.stop();
    m_sendDataThd.stop();
    m_reSendDataThd.stop();

    close(m_sockBillingCtrl);
    close(m_sockChargeOperate);
    close(m_sockConfig);
    close(m_sockDebug);
    close(m_sockGateway);
    close(m_sockMatrixCtrl);
}


/**
 * @brief IpcStack::Init
 * @param proc  cmd应答超时处理
 * @param cmdSendType   发送命令的类型
 * @param cmdRecvType   接收命令的类型
 * @param getPairedCMD  cmd对应的应答命令
 */
void IpcStack::Init(pCmdAckTimeoutProc proc,pCmdSendType cmdSendType,pCmdRecvType cmdRecvType,pGetPairedCMD getPairedCMD) {

    m_cmdAckTimeoutProc = proc;
    m_cmdSendType = cmdSendType;
    m_cmdRecvType = cmdRecvType;
    m_getPairedCMD = getPairedCMD;

    m_recvDataThd.start();
    m_sendDataThd.start();
    m_reSendDataThd.start();

}


bool IpcStack::Write(const IpcData &ipcData)
{
    QMutexLocker locker(&m_sendMsgQueMutex);
    m_sendMsgQue.push(ipcData);
    return true;
}


bool IpcStack::Write(const IpcData &ipcData, const UInt8 maxCnt, const UInt32 cycle)
{
    QMutexLocker locker(&m_reSendMsgListMutex);
    ReIpcData reIpcData;
    reIpcData.maxCnt = maxCnt;
    reIpcData.sendCnt = 0;
    reIpcData.cycle = cycle;
    reIpcData.lastSndTime = QDateTime::currentMSecsSinceEpoch();
    reIpcData.ipcData = ipcData;
    m_reSendMsgList.push_back(reIpcData);

    return true;
}

bool IpcStack::Read(IpcData &ipcData)
{
    QMutexLocker locker(&m_recvMsgQueMutex);

    bool ret = false;
    if (!m_recvMsgQue.empty()) {
        ipcData = m_recvMsgQue.front();//返回队头消息
        m_recvMsgQue.pop();//删除队头消息
        ret = true;
    }
    return ret;
}


/*初始化sockFd*/
void IpcStack::initsockFd()
{
    /*初始化读sockFd*/
    initReadSockFd(m_sockBillingCtrl, port_BillingCtrlBind);
    initReadSockFd(m_sockChargeOperate, port_ChargeOperateBind);
    initReadSockFd(m_sockConfig, port_ConfigBind);
    initReadSockFd(m_sockDebug, port_DebugBind);
    initReadSockFd(m_sockGateway, port_GatewayBind);
    initReadSockFd(m_sockMatrixCtrl, port_MatrixCtrlBind);

    priority_queue<int> sockFdQue;
    sockFdQue.push(m_sockBillingCtrl);
    sockFdQue.push(m_sockChargeOperate);
    sockFdQue.push(m_sockConfig);
    sockFdQue.push(m_sockDebug);
    sockFdQue.push(m_sockGateway);
    sockFdQue.push(m_sockMatrixCtrl);
    /*读sockd中最大的*/
    m_maxSockFd = sockFdQue.top();

    /*初始化写sockFd*/
    initWriteSockFd(m_writeSockFd);
}

/*初始化读sockFd*/
void IpcStack::initReadSockFd(int &sockFd, UInt16 port)
{
    if ((sockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; //IP协议
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int on = 1;
    if ((setsockopt(sockFd,SOL_SOCKET,SO_REUSEADDR, &on, sizeof(on))) < 0) {
        perror("setsockopt()");
        close(sockFd);
    }

    if (bind(sockFd,(sockaddr*)&addr,sizeof(addr)) < 0) {
        perror("bind()");
        close(sockFd);
        exit(EXIT_FAILURE);
    }
}

/*初始化写sockFd*/
void IpcStack::initWriteSockFd(int &sockFd)
{
    if ((sockFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket()");
    }
}

//接收数据线程
void IpcStack::recvDataThd()
{
    IpcData ipcData;

    while (!m_recvDataThd.isStopped()) {
        readData(ipcData);
    }
}

void IpcStack::readData(IpcData &ipcData)
{
    int ret;
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(m_sockBillingCtrl, &rfds);
    FD_SET(m_sockChargeOperate, &rfds);
    FD_SET(m_sockConfig, &rfds);
    FD_SET(m_sockDebug, &rfds);
    FD_SET(m_sockGateway, &rfds);
    FD_SET(m_sockMatrixCtrl, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    ret = select(m_maxSockFd+1, &rfds, NULL, NULL, &tv); /*阻塞一段时间*/

    if (ret < 0) {
        perror("select() ipc read");
        Logger::Log::Instance().logError("select() ipc read");
        Util::delayMs(10);
    } else if (ret == 0) {
        //perror("timeout");
    } else {
        if (FD_ISSET(m_sockBillingCtrl, &rfds)) {
            saveData(ipcData, m_sockBillingCtrl, WHICH_BILLING_CONTROL_APP);
        }
        if (FD_ISSET(m_sockChargeOperate, &rfds)) {
            saveData(ipcData, m_sockChargeOperate, WHICH_CHARGE_OPERATE_APP);
        }
        if (FD_ISSET(m_sockConfig, &rfds)) {
            saveData(ipcData, m_sockConfig, WHICH_CONFIG_APP);
        }
        if (FD_ISSET(m_sockDebug, &rfds)) {
            saveData(ipcData, m_sockDebug, WHICH_DEBUG_APP);
        }
        if (FD_ISSET(m_sockGateway, &rfds)) {
            saveData(ipcData, m_sockGateway, WHICH_GATEWAY_APP);
        }
        if (FD_ISSET(m_sockMatrixCtrl, &rfds)) {
            saveData(ipcData, m_sockMatrixCtrl, WHICH_MATRIX_CONTROL_APP);
        }

    }
}

void IpcStack::saveData(IpcData &ipcData, int sockFd, WHICH_APP who)
{
    //qDebug("%s sockFd:%d who:%d",__FUNCTION__,sockFd,who);

    QString str_log;
    char buf[BUFFER_SIZE];
    struct sockaddr_in addr;
    int addrLen = sizeof(addr);

    int recvBytes;

    recvBytes = recvfrom(sockFd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, (socklen_t*)&addrLen);

    if ((recvBytes > 0) && (recvBytes <= BUFFER_SIZE)) {
        ipcData.who = who;
        if (parseData(ipcData, buf, recvBytes) > 0) {
            putOneMsgToRecvMsgQue(ipcData); //将数据放入消息接收队列
        } else {
            Logger::Log::Instance().logError("parseData() 出错");
        }
    } else if (recvBytes > BUFFER_SIZE) {
        str_log = QString("recvfrom() %1 bytes > buffer size %2")
                .arg(recvBytes)
                .arg(BUFFER_SIZE);
        Logger::Log::Instance().logError(str_log);
    } else {
        perror("recvfrom()");
    }
}


int IpcStack::parseData(IpcData &dest,char *src, int len)
{
    //协议头
    Int32 pos = 0;
    memcpy(&dest.head, src+pos,sizeof(dest.head));
    pos += sizeof(dest.head);
    if (dest.head != PROTOCOL_HEAD) {
        return -1;
    }
    //帧序号
    memcpy(&dest.frameNum,src+pos, sizeof(dest.frameNum));
    pos += sizeof(dest.frameNum);
    //命令码
    memcpy(&dest.commond,src+pos, sizeof(dest.commond));
    pos += sizeof(dest.commond);
    //异常标志
    memcpy(&dest.flag,src+pos, sizeof(dest.flag));
    pos += sizeof(dest.flag);
    //数据域长度
    memcpy(&dest.len,src+pos, sizeof(dest.len));
    pos += sizeof(dest.len);
    //数据域
    dest.buffer.clear();
    dest.buffer.append(src+pos,dest.len);
    pos += dest.len;
    //校验和
    memcpy(&dest.checkSum,src+pos, sizeof(dest.checkSum));
    pos += sizeof(dest.checkSum);
    UInt8 checkSum = confirmCheckSum(src, len);
    if (checkSum != dest.checkSum) {//校验和错误
        qDebug("%s 校验和错误 data_size:%d dest_checksum:%d checkSum:%d",
               __FUNCTION__, len, dest.checkSum, checkSum);

        return -2;
    }
    //协议尾
    memcpy(&dest.end,src+pos, sizeof(dest.end));
    if (dest.end != PROTOCOL_END) {
        return -3;
    }

    return 1;
}

//确认校验和
UInt8 IpcStack::confirmCheckSum(char *buf, int len)
{
    UInt8 checkSum = 0;
    UInt16 i;

    for (i=0; i<len; i++) {
        checkSum += buf[i];
    }
    checkSum -= buf[len-1];
    checkSum -= buf[len-2];
    //checkSum == buf[i-2]
    return checkSum;
}

void IpcStack::putOneMsgToRecvMsgQue(IpcData &ipcData)
{
    QMutexLocker locker(&m_recvMsgQueMutex);

    if (m_cmdRecvType(ipcData.commond) == CMD_TYPE_NOTIFY) {
        m_recvMsgQue.push(ipcData);
    } else if (m_cmdRecvType(ipcData.commond) == CMD_TYPE_REQ_ACK) {
        UInt16 cmd_ack = ipcData.commond;
        UInt16 cmd;
        if (m_getPairedCMD(cmd_ack,cmd)) {
            //在重发队列中找到对应的请求命令
            if (findAndDel(cmd)){
                m_recvMsgQue.push(ipcData);
            } else {
                qDebug("%s 未在重发队列中找到对应的请求命令%4x", __FUNCTION__,ipcData.commond);

            }
        }

    } else {
        qDebug("%s 未知的命令： %d", __FUNCTION__, ipcData.commond);
    }

    return;
}





//发送数据线程
void IpcStack::sendDataThd()
{
    IpcData ipcData;


    while (!m_sendDataThd.isStopped()) {

        if (!m_sendMsgQue.empty()) {
            if(getOneMsgFromSendMsgQue(ipcData)) {//取出消息

                /*发送数据*/
                writeData(ipcData);
            }
        }

        Util::delayMs(100);
    }
}


bool IpcStack::getOneMsgFromSendMsgQue(IpcData &ipcData)
{
    QMutexLocker locker(&m_sendMsgQueMutex);

    bool ret = false;
    if (!m_sendMsgQue.empty()) {
        ipcData = m_sendMsgQue.front();//返回队头消息
        m_sendMsgQue.pop();//删除队头消息
        ret = true;
    }
    return ret;
}


int IpcStack::writeData(IpcData &ipcData)
{
    UInt16 portSend = 0;

    switch(ipcData.who) {

    case WHICH_BILLING_CONTROL_APP:
        portSend = port_BillingCtrlSend;
        break;
    case WHICH_GATEWAY_APP:
        portSend = port_GatewaySend;
        break;
    case WHICH_MATRIX_CONTROL_APP:
        portSend = port_MatrixCtrlSend;
        break;
    case WHICH_CONFIG_APP:
        portSend = port_ConfigSend;
        break;
    case WHICH_DEBUG_APP:
        portSend = port_DebugSend;
        break;
    case WHICH_UPDATE_APP:
        break;
    case WHICH_DAEMON_APP:
        portSend = port_DaemonSend;
        break;
    case WHICH_CHARGE_OPERATE_APP:
        portSend = port_ChargeOperateSend;
        break;
    case WHICH_CHARGE_CONTROL_APP:
        break;
    default:
        break;
    }

    if(portSend == 0) {
        qDebug("%s 发送IPC数据时，没有找到对应的端口 who = %d",__FUNCTION__,ipcData.who);
        return -1;
    }

    struct sockaddr_in addr;
    int addrLen = sizeof(addr);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; //IP协议
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(portSend);

    char buf[BUFFER_SIZE];
    int pos = 0;
    if ((ipcData.len+10) > BUFFER_SIZE) {
        Logger::Log::Instance().logError(QString("sendto() %1 bytes > buffer size %2")
                                         .arg(ipcData.len+10)
                                         .arg(BUFFER_SIZE));
        return -2;
    }

    memcpy(buf+pos,(char*)&ipcData.head, sizeof(ipcData.head));
    pos += sizeof(ipcData.head);
    memcpy(buf+pos,(char*)&ipcData.frameNum, sizeof(ipcData.frameNum));
    pos += sizeof(ipcData.frameNum);
    memcpy(buf+pos,(char*)&ipcData.commond, sizeof(ipcData.commond));
    pos += sizeof(ipcData.commond);
    memcpy(buf+pos,(char*)&ipcData.flag, sizeof(ipcData.flag));
    pos += sizeof(ipcData.flag);
    memcpy(buf+pos,(char*)&ipcData.len, sizeof(ipcData.len));
    pos += sizeof(ipcData.len);
    memcpy(buf+pos,ipcData.buffer.constData(), ipcData.len);
    pos += ipcData.len;
    //计算校验和
    ipcData.checkSum = calcCheckSum(buf, pos);
    memcpy(buf+pos,(char*)&ipcData.checkSum, sizeof(ipcData.checkSum));
    pos += sizeof(ipcData.checkSum);
    memcpy(buf+pos,(char*)&ipcData.end, sizeof(ipcData.end));
    pos += sizeof(ipcData.end);


    waitForSend();



    int ret = sendto(m_writeSockFd, buf, pos, 0, (sockaddr*)&addr, addrLen);
    if (ret < 0) {
        perror("sendto()");
    }

    return ret;
}


//计算校验和
UInt8 IpcStack::calcCheckSum(char *buf, int len)
{
    UInt8 checkSum = 0;
    UInt16 i;
    for (i=0; i<len; i++) {
        checkSum += buf[i];
    }
    return checkSum;
}

void IpcStack::waitForSend()
{
    int ret;
    fd_set wfds;

    FD_ZERO(&wfds);
    FD_SET(m_writeSockFd, &wfds);

    ret = select(m_writeSockFd+1, NULL, &wfds, NULL, NULL);
    if (ret < 0) {
        perror("select() ipc write");
    } else if (ret == 0){
        perror("select() ipc timeout");
    }

}


void IpcStack::reSendDataThd()
{
    while (!m_reSendDataThd.isStopped()) {

        reWriteData();

        Util::delayMs(10);
    }
}

void IpcStack::reWriteData()
{
    QMutexLocker locker(&m_reSendMsgListMutex);

    vector<ReIpcData>::iterator iter;
    ReIpcData reIpcData;
    UInt64 curTime;

    if (!m_reSendMsgList.empty()) {
        iter = m_reSendMsgList.begin();
        while (iter != m_reSendMsgList.end()) {
            reIpcData = *iter;
            curTime = QDateTime::currentMSecsSinceEpoch();

            if (reIpcData.sendCnt >= reIpcData.maxCnt) {
                if (m_cmdSendType(reIpcData.ipcData.commond) == CMD_TYPE_REQ_ACK) {
                    //超时处理 - 回调函数
                    m_cmdAckTimeoutProc(reIpcData.ipcData);

                } else if (m_cmdSendType(reIpcData.ipcData.commond) == CMD_TYPE_NOTIFY) {
                    //cmd 通知类型，不用处理
                }

                iter = m_reSendMsgList.erase(iter);
            } else {
                if (curTime > reIpcData.lastSndTime + reIpcData.cycle) {
                    reIpcData.sendCnt++;
                    reIpcData.lastSndTime = curTime;
                    *iter = reIpcData;

                    Write(reIpcData.ipcData); //重发ipc消息
                }

                iter++;
            }
        }
    }

}


bool IpcStack::findAndDel(int cmd)
{
    QMutexLocker locker(&m_reSendMsgListMutex);

    bool ret = false;
    vector<ReIpcData>::iterator iter;
    ReIpcData reIpcData;

    for(iter=m_reSendMsgList.begin(); iter!=m_reSendMsgList.end();iter++) {
        reIpcData = *iter;
        if (reIpcData.ipcData.commond == cmd) {
            m_reSendMsgList.erase(iter);
            ret = true;
            break;
        }
    }

    return ret;
}


void IpcStack::addCMD(UInt16 cmd)
{
    QMutexLocker locker(&m_sendCMDSetMutex);
    m_sendCMDSet.insert(cmd);
}

bool IpcStack::removeCMD(UInt16 cmd)
{
    bool ret = false;
    set<UInt16>::iterator it;
    QMutexLocker locker(&m_sendCMDSetMutex);

    it = m_sendCMDSet.find(cmd);//查找发送CMD集合里是否有cmd
    if(it != m_sendCMDSet.end()) {//如果找到对应的cmd
        m_sendCMDSet.erase(it);//删除cmd
        ret = true;
    }
    return ret;
}


