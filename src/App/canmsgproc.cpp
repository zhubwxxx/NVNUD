#include "canmsgproc.h"

#include "Comm/util.h"
#include "ipcmsgproc.h"

#include <qfile.h>
#include <qfileinfo.h>

CanMsgProc *CanMsgProc::m_instance = 0;

CanMsgProc::CanMsgProc(QObject *parent) : QObject(parent),
    m_msgProcThd(this, &CanMsgProc::msgProcThd)
{
    m_CanStack = CanStack::Instance();

    if (SERVER == 1) { /*在线升级服务端*/
        if (m_CanStack->Init(CAN_DEVICE_INTERFACE,
                             CAN_BPS,
                             isMultiFrame,
                             getPairedPF,
                             pgnSendType,
                             pgnRecvType,
                             pgnAckTimeoutProc) > 0) {

            m_msgProcThd.start();

        } else {
            Logger::Log::Instance().logError("CAN socket init failed");
        }
    }

}

CanMsgProc::~CanMsgProc()
{
    if (DEBUG_FLAG) {
        qDebug("%s ***", __FUNCTION__);
    }

    m_msgProcThd.stop();

    delete m_CanStack;
}



/**
 * @brief CanMsgProc::msgProcThd 消息处理线程
 */
void CanMsgProc::msgProcThd()
{
    CanDataIF canData;

#if REMOTE_DEBUG
    QString strHead;
    IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();
#endif
    while(!m_msgProcThd.isStopped()) {

        packCanHead(canData, 0, 0, 0, 0, 0, 0); //打包Can头部

        if (m_CanStack->Read(canData)) {

#if REMOTE_DEBUG
            strHead = QString("CAN接收 ID=%1 len=%2").arg(canData.ID.ExtId,2,16).arg(canData.Len);
            ipcMsgProc->uploadDebugBuffer(strHead,canData.Buffer.data(),canData.Len,DEBUG_ARG_RECV_CAN);
#endif

                switch(canData.ID.bit.PF) {

                case READ_CHARGE_CTRL_INFO_ACK:/*查询版本*/
                        //qDebug()<< "读取控制单元内部信息应答帧";
                        replyVersionProc(canData);
                        break;

                case BOOT_LEAD_STARTUP://BOOT引导启动帧
                        //qDebug()<<"收到BOOT引导启动帧";
                        bootLeadStartupProc(canData);
                        break;

                case REQUEST_APP_DATA_DLOAD://请求程序数据下发
                        //qDebug()<<"收到请求程序数据下发命令";
                        reqAppDataDLoadProc(canData);
                        break;

                case UPGRADE_OVER: //软件升级完毕通知
                        //qDebug()<<"收到软件升级完毕命令通知";
                        upgradeOverProc(canData);
                        break;

                default:
                        break;
                }
        }

        Util::delayMs(100);
    }
}



/**
 * @brief CanMsgProc::replyVersionProc 充电控制单元应答信息处理
 * @param canData
 */
void CanMsgProc::replyVersionProc(CanDataIF &canData)
{
    QString str_log;
    CanReplyVersion canReplyVersion;
    IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();

    memcpy(&canReplyVersion,canData.Buffer.data(), sizeof(canReplyVersion));
    UInt8 addr = canData.ID.bit.SA; //充电控制单元源地址

    if (Util::cmpVersion(NewVersion[WHICH_CHARGE_CONTROL_APP], canReplyVersion.Version) != 0) { //版本不同
        //发送充电控制单元重启指令
        packCanHead(canData, CAN_SOURE_ADDR,addr,CHARGE_CONTROL_RESET,0x06,0,0);
        UInt8 ganNo = 0;
        canData.Buffer.clear();
        canData.Len = 1;
        canData.Buffer.append((char*)&ganNo,canData.Len);
        m_CanStack->Write(canData,20,1000);

        str_log = QString("App %1 (addr=%2) reply current version V%3.%4.%5.%6, need update to V%7.%8.%9.%10")
                .arg(Util::toAppName(WHICH_CHARGE_CONTROL_APP))
                .arg(addr)
                .arg(canReplyVersion.Version[0])
                .arg(canReplyVersion.Version[1])
                .arg(canReplyVersion.Version[2])
                .arg(canReplyVersion.Version[3])
                .arg(NewVersion[WHICH_CHARGE_CONTROL_APP][0])
                .arg(NewVersion[WHICH_CHARGE_CONTROL_APP][1])
                .arg(NewVersion[WHICH_CHARGE_CONTROL_APP][2])
                .arg(NewVersion[WHICH_CHARGE_CONTROL_APP][3]);
        Logger::Log::Instance().logInfo(str_log);

    } else { //版本相同
        str_log = QString("App %1 (addr=%2) reply version V%3.%4.%5.%6 is equal")
                .arg(Util::toAppName(WHICH_CHARGE_CONTROL_APP))
                .arg(addr)
                .arg(canReplyVersion.Version[0])
                .arg(canReplyVersion.Version[1])
                .arg(canReplyVersion.Version[2])
                .arg(canReplyVersion.Version[3]);
        Logger::Log::Instance().logInfo(str_log);

        /*标记为升级成功*/
        ipcMsgProc->setUpdateRet(WHICH_CHARGE_CONTROL_APP, UPDATE_RET_SUCCESS);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        ipcMsgProc->setFlagUpdateApp(true);
    }

#if REMOTE_DEBUG
    ipcMsgProc->uploadDebugLogInfo(str_log);
#endif

}



/**
 * @brief CanMsgProc::BootLeadStartupProc BOOT引导启动命令处理
 * @param canData
 */
void CanMsgProc::bootLeadStartupProc(CanDataIF &canData)
{
    QString str_log;
    CanBootReq req;       //BOOT引导请求参数
    CanBootAck ack;       //BOOT引导应答参数
    IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();

    memcpy(&req, canData.Buffer.data(), sizeof(req));
    UInt8 addr = canData.ID.bit.SA; //充电控制单元源地址

    int ret = Util::cmpVersion(NewVersion[WHICH_CHARGE_CONTROL_APP], req.version);
    if (req.AV==0 || ret!=0) { /*APP没有下载过或版本不同*/
        ack.id = req.id; //充电接口标识
        memcpy(&ack.version, &NewVersion[WHICH_CHARGE_CONTROL_APP],sizeof(ack.version));

        QFileInfo info;
        QString appPath = TempDir +'/'+ UpdatePackage.section('.',0,0) +'/'
                          + Util::toAppName(WHICH_CHARGE_CONTROL_APP) + '/'
                          + Util::toAppName(WHICH_CHARGE_CONTROL_APP);
        info.setFile(appPath);
        if (!info.exists()) {
            str_log = QString("File %1 is not exist, update failed").arg(appPath);
            Logger::Log::Instance().logError(str_log);

            /*标记为升级失败*/
            ipcMsgProc->setUpdateRet(WHICH_CHARGE_CONTROL_APP, UPDATE_RET_FAILED);
            /*设置继续从升级APP队列读取参数，而且删除队列头*/
            ipcMsgProc->setFlagUpdateApp(true);
            return;
        }
        ack.size = info.size(); //软件大小

        QFile file;
        file.setFileName(appPath);
        if (!file.open(QIODevice::ReadOnly)) {
            str_log = QString("File %1 open failed").arg(appPath);
            Logger::Log::Instance().logError(str_log);

            /*标记为升级失败*/
            ipcMsgProc->setUpdateRet(WHICH_CHARGE_CONTROL_APP, UPDATE_RET_FAILED);
            /*设置继续从升级APP队列读取参数，而且删除队列头*/
            ipcMsgProc->setFlagUpdateApp(true);
            return;
        }
        char buf[ack.size+3];
        buf[ack.size+1] = 0;
        buf[ack.size+2] = 0;
        buf[ack.size+3] = 0;
        file.read(buf,info.size());
        ack.checkNum = 0;
        UInt32 devNum = ack.size/4 + ((ack.size % 4) == 0 ? 0 : 1);
        for (UInt32 i=0; i<devNum; i++) {
            ack.checkNum += *((UInt32*)(((char*)buf)+i*4)); //软件校验码
        }

        ack.flag = 0; //成功标识

        if (DEBUG_FLAG) {
            qDebug("%s size:%d devNum:%d checkSum:%d",
                   __FUNCTION__,ack.size,devNum,ack.checkNum);
        }

        packCanHead(canData,CAN_SOURE_ADDR,addr,BOOT_LEAD_STARTUP_ACK,0x06,0,0);
        canData.Buffer.clear();
        canData.Len = sizeof(ack);
        canData.Buffer.append((char*)&ack, canData.Len);
        m_CanStack->Write(canData);


        str_log = QString("App %1 (addr=%2) current boot version V%3.%4.%5.%6, need update to V%7.%8.%9.%10")
                .arg(Util::toAppName(WHICH_CHARGE_CONTROL_APP))
                .arg(addr)
                .arg(req.version[0])
                .arg(req.version[1])
                .arg(req.version[2])
                .arg(req.version[3])
                .arg(NewVersion[WHICH_CHARGE_CONTROL_APP][0])
                .arg(NewVersion[WHICH_CHARGE_CONTROL_APP][1])
                .arg(NewVersion[WHICH_CHARGE_CONTROL_APP][2])
                .arg(NewVersion[WHICH_CHARGE_CONTROL_APP][3]);
        Logger::Log::Instance().logInfo(str_log);

    } else { /*版本相同，无须更新*/
        str_log = QString("App %1 (addr=%2) boot version V%3.%4.%5.%6 is equal")
                .arg(Util::toAppName(WHICH_CHARGE_CONTROL_APP))
                .arg(addr)
                .arg(req.version[0])
                .arg(req.version[1])
                .arg(req.version[2])
                .arg(req.version[3]);
        Logger::Log::Instance().logInfo(str_log);

        /*标记为升级成功*/
        ipcMsgProc->setUpdateRet(WHICH_CHARGE_CONTROL_APP, UPDATE_RET_SUCCESS);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        ipcMsgProc->setFlagUpdateApp(true);

    }
#if REMOTE_DEBUG
    ipcMsgProc->uploadDebugLogInfo(str_log);
#endif

}

/**
 * @brief CanMsgProc::ReqAppDataDLoadProc 请求程序数据下发命令处理
 * @param canData
 */
void CanMsgProc::reqAppDataDLoadProc(CanDataIF &canData)
{
    QString str_log;
    CanDataReq req;
    CanDataAck ack;

    memcpy(&req, canData.Buffer.data(), sizeof(req));

    ack.id = req.id;
    ack.addr = req.addr;
    ack.len = req.len;

    IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();

    //读取App可执行文件数据
    QString appPath = TempDir +'/'+ UpdatePackage.section('.',0,0) +'/'
                      + Util::toAppName(WHICH_CHARGE_CONTROL_APP) + '/'
                      + Util::toAppName(WHICH_CHARGE_CONTROL_APP);
    QFileInfo info;
    info.setFile(appPath);
    if (!info.exists()) { //App可执行文件是否存在
        str_log = QString("File %1 is not exist, update failed").arg(appPath);
        Logger::Log::Instance().logError(str_log);

        /*标记为升级失败*/
        ipcMsgProc->setUpdateRet(WHICH_CHARGE_CONTROL_APP, UPDATE_RET_FAILED);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        ipcMsgProc->setFlagUpdateApp(true);
        return;
    }
    QFile file;
    file.setFileName(appPath);
    if (!file.open(QIODevice::ReadOnly)) {
        str_log = QString("File %1 open failed").arg(appPath);
        Logger::Log::Instance().logError(str_log);

        /*标记为升级失败*/
        ipcMsgProc->setUpdateRet(WHICH_CHARGE_CONTROL_APP, UPDATE_RET_FAILED);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        ipcMsgProc->setFlagUpdateApp(true);
        return;
    }
    file.seek(req.addr);
    memset(ack.data,0,512);
    file.read((char*)ack.data, req.len);
    file.close();
    ack.flag = 0;

    UInt8 addr = canData.ID.bit.SA;
    packCanHead(canData,CAN_SOURE_ADDR,addr,REQUEST_APP_DATA_DLOAD_ACK,0x06,0,0);
    canData.Len = sizeof(ack);
    canData.Buffer.clear();
    canData.Buffer.append((char*)&ack, canData.Len);
    m_CanStack->Write(canData);

    if (DEBUG_FLAG) {
        qDebug("%s Charge unit %d, send app data location %d",__FUNCTION__,addr,req.addr);
    }

    return;
}


/**
 * @brief CanMsgProc::upgradeOverProc 软件升级完毕命令通知
 * @param canData
 */
void CanMsgProc::upgradeOverProc(CanDataIF &canData)
{
    QString str_log;
    CanUpdateOver over;
    IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();

    memcpy(&over, canData.Buffer.data(), sizeof(over));

    if (DEBUG_FLAG) {
        qDebug("%s id:%d version:V%d.%d.%d.%d flag:%d",__FUNCTION__,over.id,
               over.version[0],over.version[1],over.version[2],over.version[3],over.flag);
    }

    UInt8 addr = canData.ID.bit.SA;//充电控制单元源地址
    packCanHead(canData,CAN_SOURE_ADDR,addr,UPGRADE_OVER_ACK,0x06,0,0);
    canData.Buffer.clear();
    canData.Len = sizeof(over.id);
    canData.Buffer.append((char*)&over.id, canData.Len);
    m_CanStack->Write(canData);


    WHICH_APP who = WHICH_CHARGE_CONTROL_APP;
    QString str_ver = QString("V%1.%2.%3.%4").arg(NewVersion[who][0]).arg(NewVersion[who][1]).arg(NewVersion[who][2]).arg(NewVersion[who][3]);

    if (over.flag == 0) { //升级成功
        str_log = QString("App %1 (addr: %2) update to %3 success").arg(Util::toAppName(who)).arg(addr).arg(str_ver);
        Logger::Log::Instance().logInfo(str_log);

        /*标记为升级成功*/
        ipcMsgProc->setUpdateRet(WHICH_CHARGE_CONTROL_APP, UPDATE_RET_SUCCESS);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        ipcMsgProc->setFlagUpdateApp(true);
    } else { //升级失败
        str_log = QString("App %1 (addr: %2) update to %3 failed").arg(Util::toAppName(who)).arg(addr).arg(str_ver);
        Logger::Log::Instance().logError(str_log);

        /*标记为升级失败*/
        ipcMsgProc->setUpdateRet(WHICH_CHARGE_CONTROL_APP, UPDATE_RET_FAILED);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        ipcMsgProc->setFlagUpdateApp(true);
    }

#if REMOTE_DEBUG
    ipcMsgProc->uploadDebugLogInfo(str_log);
#endif

}

/**
 * @brief CanMsgProc::queryVersion 查询充电控制单元版本
 * @param stakeAddr 桩地址（充电控制单元地址）
 */
void CanMsgProc::queryVersion(UInt8 stakeAddr)
{
    CanDataIF canData;
    packCanHead(canData, CAN_SOURE_ADDR, stakeAddr,READ_CHARGE_CTRL_INFO, 0x06, 0, 0);

    CanQueryVersion canQueryVersion;
    canQueryVersion.GunNo = 0;
    canQueryVersion.ReplyTime = 0;
    canQueryVersion.ReplyPeriod = 0;

    canData.Buffer.clear();
    canData.Len = sizeof(canQueryVersion);
    canData.Buffer.append((char*)&canQueryVersion, canData.Len);
    m_CanStack->Write(canData,10,500);
}


/**
 * @brief CanMsgProc::packCanHead 打包Can头部
 * @param canData
 * @param SA
 * @param PS
 * @param PF
 * @param P
 * @param FF
 * @param RTR
 */
void CanMsgProc::packCanHead(CanDataIF &canData, UInt8 SA, UInt8 PS, UInt8 PF, UInt8 P, int FF, int RTR)
{
    canData.ID.bit.SA = SA; //源地址
    canData.ID.bit.PS = PS; //目标地址
    canData.ID.bit.PF = PF; //报文类型

    canData.ID.bit.DP = 0;
    canData.ID.bit.R = 0;

    canData.ID.bit.P = P; //优先级

    canData.FrmDef.FF = FF;
    canData.FrmDef.RTR = RTR;

    return;
}


/**
 * @brief pgnAckTimeoutProc CAN命令应答超时处理
 * @param canData
 */
void pgnAckTimeoutProc(CanDataIF &canData)
{
    if (DEBUG_FLAG) {
        qDebug("%s 应答超时处理 CAN ID: %x",__FUNCTION__, canData.ID.ExtId);
    }

    QString str_log;
    IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();

    switch (canData.ID.bit.PF) {

    case READ_CHARGE_CTRL_INFO:
        str_log = QString("APP %1 (addr=%2) reply version timeout")
                .arg(Util::toAppName(WHICH_CHARGE_CONTROL_APP))
                .arg(canData.ID.bit.PS);
        Logger::Log::Instance().logError(str_log);
        ipcMsgProc->setUpdateRet(WHICH_CHARGE_CONTROL_APP, UPDATE_RET_FAILED);
        ipcMsgProc->setFlagUpdateApp(true);
        break;

    case CHARGE_CONTROL_RESET:
        ipcMsgProc->chargeCtrlResetTimeout();
        break;

    default:
        qDebug("%s 未知的超时命令 CAN ID: %x", __FUNCTION__, canData.ID.ExtId);
        break;
    }

#if REMOTE_DEBUG
    ipcMsgProc->uploadDebugLogInfo(str_log);
#endif

}


bool isMultiFrame(UInt8 pgn)
{
    bool ret;

    switch (pgn) {

    case BOOT_LEAD_STARTUP:                 //BOOT引导启动帧             +++++单帧
        ret = false;
        break;
    case BOOT_LEAD_STARTUP_ACK:             //BOOT引导应答帧             -----多帧
        ret = true;
        break;
    case REQUEST_APP_DATA_DLOAD:            //请求程序数据下发            +++++单帧
        ret = false;
        break;
    case REQUEST_APP_DATA_DLOAD_ACK:        //程序数据下发应答            -----多帧
        ret = true;
        break;
    case UPGRADE_OVER:                      //软件升级完毕通知            +++++单帧
        ret = false;
        break;
    case UPGRADE_OVER_ACK:                  //软件升级完毕应答            +++++单帧
        ret = false;
        break;
    case CHARGE_CONTROL_RESET:              //充电控制单元复位            +++++单帧
        ret = false;
        break;
    case READ_CHARGE_CTRL_INFO:             //读取充电控制单元内部信息     +++++单帧
        ret = false;
        break;
    case READ_CHARGE_CTRL_INFO_ACK:         //读取充电控制单元内部信息应答  -----多帧
        ret = true;
        break;
    case BROADCAST_UPGRADE_CLI:             //广播通知客户端升级          -----多帧
        ret = true;
        break;
    default:
        ret = false;
        break;
    }

    return ret;
}

bool getPairedPF(UInt8 pgn_ack, UInt8 &pgn)
{
    bool ret = false;

    switch(pgn_ack) {

    case READ_CHARGE_CTRL_INFO_ACK:         //充电控制单元信息应答帧
        pgn = READ_CHARGE_CTRL_INFO;        //读取充电控制单元信息帧
        ret = true;
        break;
    case BOOT_LEAD_STARTUP:                 //BOOT引导启动帧
        pgn = CHARGE_CONTROL_RESET;         //充电控制单元复位
        ret = true;
        break;
    default:
        ret = false;
        break;
    }

    return ret;
}


int pgnSendType(UInt8 pgn)
{
    int ret = -1;

    switch (pgn) {
    case READ_CHARGE_CTRL_INFO:
        ret = CAN_PGN_TYPE_REQ_ACK;         /*请求应答型*/
        break;
    case CHARGE_CONTROL_RESET:
        ret = CAN_PGN_TYPE_REQ_ACK;         /*请求应答型*/
        break;
    case BOOT_LEAD_STARTUP_ACK:
        ret = CAN_PGN_TYPE_NOTIFY;
        break;
    case REQUEST_APP_DATA_DLOAD_ACK:
        ret = CAN_PGN_TYPE_NOTIFY;
        break;
    case UPGRADE_OVER_ACK:
        ret = CAN_PGN_TYPE_NOTIFY;
        break;

    default:
        qDebug("%s 未知的CAN PGN命令", __FUNCTION__);
        break;
    }

    return ret;

}

int pgnRecvType(UInt8 pgn)
{
    int ret = -1;

    switch (pgn) {
    case READ_CHARGE_CTRL_INFO_ACK:
        ret = CAN_PGN_TYPE_REQ_ACK;         /*请求应答型*/
        break;
    case BOOT_LEAD_STARTUP:
        ret = CAN_PGN_TYPE_REQ_ACK;         /*请求应答型*/
        break;
    case REQUEST_APP_DATA_DLOAD:
        ret = CAN_PGN_TYPE_NOTIFY;
        break;
    case UPGRADE_OVER:
        ret = CAN_PGN_TYPE_NOTIFY;
        break;

    default:
        qDebug("%s 未知的CAN PGN命令", __FUNCTION__);
        break;
    }

    return ret;

}
