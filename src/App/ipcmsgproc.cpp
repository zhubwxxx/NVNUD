#include "ipcmsgproc.h"

#include "Comm/util.h"
#include "canmsgproc.h"
#include "Lib/Tcp/tcpclientsocket.h"
#include "Lib/Tcp/tcpserver.h"

#include <qsettings.h>
#include <qfileinfo.h>
#include <qdir.h>
#include <qcoreapplication.h>
#include <qprocess.h>
#include <qtextcodec.h>


IpcMsgProc *IpcMsgProc::m_instance = 0;

IpcMsgProc::IpcMsgProc(QObject *parent) : QObject(parent),
    m_wdtThd(this, &IpcMsgProc::wdtThd),
    m_queryStackInfoThd(this,&IpcMsgProc::queryStackInfoThd),
    m_downloadFileThd(this,&IpcMsgProc::downloadFileThd),
    m_updateAppThd(this,&IpcMsgProc::updateAppThd),
    m_ipcMsgThd(this, &IpcMsgProc::ipcMsgThd)
{
    m_IpcStack = IpcStack::Instance();
    m_IpcStack->Init(cmdAckTimeoutProc,cmdSendType,cmdRecvType,getPairedCMD);

    m_wdtThd.start();
    m_queryStackInfoThd.start();
    m_downloadFileThd.start();
    m_updateAppThd.start();
    m_ipcMsgThd.start();

    transport = new Transport(this);
    connect(this, SIGNAL(sigDownloadFile(QString)),this, SLOT(downloadFile(QString)));
    connect(transport, SIGNAL(done(bool)), this, SLOT(downloadFileOver(bool)));

    connect(this, SIGNAL(sigIp(UInt32)), this, SLOT(on_ip(UInt32)));

    connect(this, SIGNAL(sigQuit()), this, SLOT(on_quit()));

    /*将升级版本全部重置0*/
    for(int i=0; i<APP_NUM;i++) {
        NewVersion[i][0] = 0;
        NewVersion[i][1] = 0;
        NewVersion[i][2] = 0;
        NewVersion[i][3] = 0;
    }

    m_flagUpdateArg = true;
    m_flagStakeArg  = true;
    m_flagUpdateApp = true;

    m_debugIp = 0;
    m_debugPort = 0;
    m_debugSwitch        = DEBUG_SWITCH_CLOSE;/*远程诊断-上传总开关*/
    m_debugLogSwitch     = DEBUG_SWITCH_CLOSE;/*远程诊断-日志信息上传开关*/
    m_debugRecvCanSwitch = DEBUG_SWITCH_CLOSE;/*远程诊断-CAN接收信息上传开关*/
    m_debugSendCanSwitch = DEBUG_SWITCH_CLOSE;/*远程诊断-CAN发送信息上传开关*/
    m_debugRecvUdpSwitch = DEBUG_SWITCH_CLOSE;/*远程诊断-UDP接收信息上传开关*/
    m_debugSendUdpSwitch = DEBUG_SWITCH_CLOSE;/*远程诊断-UDP发送信息上传开关*/

}

IpcMsgProc::~IpcMsgProc()
{

    if (DEBUG_FLAG) {
        qDebug("%s ***", __FUNCTION__);
    }

    m_wdtThd.stop();
    m_queryStackInfoThd.stop();
    m_downloadFileThd.stop();
    m_updateAppThd.stop();
    m_ipcMsgThd.stop();

    disconnect(this, SIGNAL(sigQuit()), this, SLOT(on_quit()));
    disconnect(this, SIGNAL(sigIp(UInt32)), this, SLOT(on_ip(UInt32)));
    disconnect(transport, SIGNAL(done(bool)), this, SLOT(downloadFileOver(bool)));
    disconnect(this, SIGNAL(sigDownloadFile(QString)),this, SLOT(downloadFile(QString)));

    delete transport;
    delete m_IpcStack;
}

/**
 * @brief IpcMsgProc::wdtThd 看门狗线程
 */
void IpcMsgProc::wdtThd()
{
    IpcData ipcData;
    packIpcHead(ipcData, 0, CMD_SEND_HEART, 0);

    ipcData.len = 1;
    UInt8 id = 4; //升级软件心跳包代码
    ipcData.buffer.append((char*)&id, ipcData.len);
    ipcData.who = WHICH_DAEMON_APP;

    while (!m_wdtThd.isStopped()) {

        m_IpcStack->Write(ipcData);

        Util::delayMs(1000);
    }
}


/**
 * @brief IpcMsgProc::queryStackInfoThd 查询桩信息线程
 */
void IpcMsgProc::queryStackInfoThd()
{
    /*查询桩信息*/
    IpcData ipcData;
    packIpcHead(ipcData,0,CMD_UFLASH_UPDATE,0);
    ipcData.who = WHICH_CONFIG_APP;

    StakeInfo stakeInfo; /*桩信息*/
    QString str_log;
    QString strHead;

    while (!m_queryStackInfoThd.isStopped()) {

        if (m_flagUpdateArg) {

            if (this->getUpdateArg(m_updateArg)) {

                m_flagUpdateArg = false;

                /*桩编码为BCD编码，显示时转为str*/
                char buf[2*STAKE_CODE_LEN+1];
                Util::bcd2str((const unsigned char*)m_updateArg.stakeCode, STAKE_CODE_LEN, buf, 2*STAKE_CODE_LEN);

                if (Util::isAppRun(Util::toAppName(WHICH_CONFIG_APP)) == 0) {
                    stakeInfo.cmd = CLOUD_ADDRESS_REQUEST;
                    stakeInfo.data1size = 2*STAKE_CODE_LEN;
                    strncpy(stakeInfo.data1, buf, stakeInfo.data1size);

                    ipcData.buffer.clear();
                    ipcData.len = sizeof(stakeInfo);
                    ipcData.buffer.append((char*)&stakeInfo,ipcData.len);
                    m_IpcStack->Write(ipcData);
#if REMOTE_DEBUG
                    strHead = QString("UDP发送数据 cmd=%1 len=%2").arg(ipcData.commond,2,16).arg(ipcData.len);
                    this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len, (char*)DEBUG_ARG_SEND_UDP);
#endif

                    str_log = QString("Query stake(%1) info").arg(buf);
                    Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
                    this->uploadDebugLogInfo(str_log);
#endif

                } else {
                    /*发送升级失败命令到后台网关*/
                    this->sendUpdateRet(UPDATE_RET_FAILED);
                    /*设置继续从升级参数队列读取参数，而且删除队列头*/
                    this->setFlagUpdateArg(true);

                    str_log = QString("APP %1 is not running, can't query stake(%2) info")
                            .arg(Util::toAppName(WHICH_CONFIG_APP))
                            .arg(buf);
                    Logger::Log::Instance().logError(str_log);
#if REMOTE_DEBUG
                    this->uploadDebugLogInfo(str_log);
#endif
                }
            }
        }

        Util::delayMs(1000);
    }
}


/**
 * @brief IpcMsgProc::downloadFileThd 下载升级包线程
 */
void IpcMsgProc::downloadFileThd()
{
    int ret = 0;

    while (!m_downloadFileThd.isStopped()) {

        if (m_flagStakeArg) {

            if (this->getStakeArg(m_stakeArg)) {

                m_flagStakeArg = false;

                QFileInfo info(Md5Cfg);
                QSettings ini(Md5Cfg,QSettings::IniFormat);

                if (!info.exists()) { /*本地保存MD5的文件不存在*/

                   /*下载升级包*/
                    emit sigDownloadFile(UpdatePackage);

                } else {
                    QString md5 = ini.value("MD5/md5").toString();
                    ret = strncmp(md5.toUtf8().constData(), m_stakeArg.updateArg.MD5, sizeof(m_stakeArg.updateArg.MD5));
                    if (ret != 0) { /*MD5值不相等*/

                        /*下载升级包*/
                        emit sigDownloadFile(UpdatePackage);

                    } else { /*MD5值相等，直接解析*/
                        if (SERVER == 1) { /*在线升级服务端*/
                            //根据版本选择需要升级的APP，加入升级队列
                            this->seletcUpdateApp();
                        } else {
                            if (DEBUG_FLAG) {
                                qDebug("升级客户端：Md5值相等，跳过");
                            }
                            /*设置继续从桩参数队列读取参数，而且删除队列头*/
                            this->setFlagStakeArg(true);
                        }
                    }
                }
            }
        }

        Util::delayMs(1000);
    }
}


/**
 * @brief IpcMsgProc::updateAppThd 升级APP线程
 */
void IpcMsgProc::updateAppThd()
{
    QString str_log;
    QString strHead;

    /*查询版本*/
    IpcData ipcData;
    packIpcHead(ipcData, 0, CMD_QUERY_VERSION,0);
    ipcData.len = 0;

    while(!m_updateAppThd.isStopped()) {

        if (m_flagUpdateApp) {

            if (this->getUpdateApp(m_who)) { /*得到要升级APP,此为优先级队列，保证最后升级自身*/

                m_flagUpdateApp = false;

                if (m_who == WHICH_UPDATE_APP) { /*升级软件服务端本身,先比较版本*/

                    QString oldVer = QString("V%1.%2.%3.%4")
                            .arg(APP_VERSION[0])
                            .arg(APP_VERSION[1])
                            .arg(APP_VERSION[2])
                            .arg(APP_VERSION[3]);
;
                    /*摘要文件中解析得到的新版本*/
                    QString newVer = QString("V%1.%2.%3.%4")
                            .arg(NewVersion[m_who][0])
                            .arg(NewVersion[m_who][1])
                            .arg(NewVersion[m_who][2])
                            .arg(NewVersion[m_who][3]);

                    if (Util::cmpVersion(NewVersion[m_who], APP_VERSION) != 0) { //新版本不等于旧版本

                        str_log = QString("APP %1 current version %2,need update to %3")
                                .arg(Util::toAppName(m_who))
                                .arg(oldVer)
                                .arg(newVer);
                        Logger::Log::Instance().logInfo(str_log);

                        if (this->stakeArgQueSize() <= 1) { /*桩参数队列只剩一个值*/
                            /*升级自身*/
                            updateApp(m_who);
                        } else {

                            str_log = QString("App %1 update to %2 success").arg(Util::toAppName(m_who)).arg(newVer);
                            Logger::Log::Instance().logInfo(str_log);

                            /*标记为升级成功*/
                            this->setUpdateRet(m_who, UPDATE_RET_SUCCESS);
                            /*设置继续从升级APP队列读取参数，而且删除队列头*/
                            this->setFlagUpdateApp(true);
                        }
                    } else {
                        str_log = QString("APP %1 version(%2) is equal")
                                .arg(Util::toAppName(m_who))
                                .arg(newVer);
                        Logger::Log::Instance().logInfo(str_log);

                        /*标记为升级成功*/
                        this->setUpdateRet(m_who, UPDATE_RET_SUCCESS);
                        /*设置继续从升级APP队列读取参数，而且删除队列头*/
                        this->setFlagUpdateApp(true);
                    }
                } else if (m_who == WHICH_CHARGE_CONTROL_APP) {
                    /*查询充电控制单元版本*/
                    str_log = QString("Query %1 (addr=%2) version")
                            .arg(Util::toAppName(m_who))
                            .arg(m_stakeArg.stakeAddr);
                    Logger::Log::Instance().logInfo(str_log);
                    CanMsgProc *canMsgProc = CanMsgProc::Instance();
                    canMsgProc->queryVersion(m_stakeArg.stakeAddr);

                } else if (m_who == WHICH_DAEMON_APP) {
                    /*守护进程,直接升级*/
                    updateApp(m_who);

                } else if (m_who == WHICH_GATEWAY_APP) { //后台网关-动态库文件,通过计费管理单元判断其是否运行
                    if (Util::isAppRun(Util::toAppName(WHICH_BILLING_CONTROL_APP)) == 0) { //正在运行,查询版本
                        str_log = QString("Query %1 version").arg(Util::toAppName(m_who));
                        Logger::Log::Instance().logInfo(str_log);
                        ipcData.who = m_who; //版本号后台网关自己应答
                        m_IpcStack->Write(ipcData,10,500);
#if REMOTE_DEBUG
                    strHead = QString("UDP发送数据 cmd=%1 len=%2").arg(ipcData.commond,2,16).arg(ipcData.len);
                    this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len, DEBUG_ARG_SEND_UDP);
#endif

                    } else {

#if UPDATE_APP_NOT_RUN
                        //没有运行,直接升级                     
                        updateApp(m_who);
#else
                        //没有运行，不升级
                        str_log = QString("APP %1 is not run, can't update")
                                .arg(Util::toAppName(m_who));
                        Logger::Log::Instance().logError(str_log);
                        /*标记为升级失败*/
                        this->setUpdateRet(m_who, UPDATE_RET_FAILED);
                        /*设置继续从升级APP队列读取参数，而且删除队列头*/
                        this->setFlagUpdateApp(true);

#endif

                    }
                } else {
                    if (Util::isAppRun(Util::toAppName(m_who)) == 0) { //正在运行,查询版本
                        str_log = QString("Query %1 version").arg(Util::toAppName(m_who));
                        Logger::Log::Instance().logInfo(str_log);
                        ipcData.who = m_who;
                        m_IpcStack->Write(ipcData,10,500);
#if REMOTE_DEBUG
                        strHead = QString("UDP发送数据 cmd=%1 len=%2").arg(ipcData.commond,2,16).arg(ipcData.len);
                        this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len, DEBUG_ARG_SEND_UDP);
#endif


                    } else {
#if UPDATE_APP_NOT_RUN
                        //没有运行,直接升级
                        updateApp(m_who);
#else
                        //没有运行，不升级
                        str_log = QString("APP %1 is not run, can't update")
                                .arg(Util::toAppName(m_who));
                        Logger::Log::Instance().logError(str_log);
                        /*标记为升级失败*/
                        this->setUpdateRet(m_who, UPDATE_RET_FAILED);
                        /*设置继续从升级APP队列读取参数，而且删除队列头*/
                        this->setFlagUpdateApp(true);

#endif

                    }
                }
            }
        }

        Util::delayMs(5000);
    }
}



/**
 * @brief IpcMsgProc::ipcMsgThd ipcMsg处理线程
 */
void IpcMsgProc::ipcMsgThd()
{
    IpcData ipcData;
    QString strHead;
#if 0
    //DEBUG: 查询APP版本
    packIpcHead(ipcData, 0, CMD_QUERY_VERSION,0);
    ipcData.len = 0;
    ipcData.who = WHICH_GATEWAY_APP;
    m_IpcStack->Write(ipcData);
    qDebug("查询版本：");
#endif

#if 0
    //DEBUG: 查询运行状态
    packIpcHead(ipcData, 0, CMD_QUERY_RUN_STATUS,0);
    ipcData.len = 0;
    ipcData.who = WHICH_MATRIX_CONTROL_APP;
    m_IpcStack->Write(ipcData);    
#endif

    while (!m_ipcMsgThd.isStopped()) {

        if (m_IpcStack->Read(ipcData)) { //从接收消息队列读取到一包数据

#if REMOTE_DEBUG
            strHead = QString("UDP接收 cmd=%1 len=%2").arg(ipcData.commond,2,16).arg(ipcData.len);
            this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len,DEBUG_ARG_RECV_UDP);
#endif

            switch (ipcData.commond) {

            case CMD_SEND_UPDATE_ARG: //收到升级参数
                //qDebug()<< "收到升级参数";
                recvUpdateArgProc(ipcData);
                break;

            case CMD_REPLY_VERSION: //应答版本
                //qDebug()<<"应答版本";
                replyVersionProc(ipcData);
                break;

            case CMD_REPLY_RUN_STATUS: //应答运行状态(是否允许升级)
                //qDebug()<<"应答运行状态(是否允许升级)";
                replyRunStatusProc(ipcData);
                break;

            case CMD_QUERY_VERSION: //查询升级软件版本
                //qDebug()<<"查询升级软件版本";
                queryVersionProc(ipcData);
                break;

            case CMD_REPLY_IP: //应答集中计费终端IP
                //qDebug()<< "应答集中计费终端IP";
                recvIpProc(ipcData);
                break;

            case CMD_UFLASH_UPDATE:
                //qDebug()<<"U盘升级交互处理";
                uFlashUpdateProc(ipcData);
                break;

            case CMD_SET_DEBUG_SWITCH_REQ:
                //qDebug()<<"设置诊断开关请求";
                setDebugSwitchReqProc(ipcData);
                break;

            case CMD_GET_DEBUG_SWITCH_REQ:
                //qDebug()<<"获取诊断开关请求";
                getDebugSwitchReqProc(ipcData);
                break;

            case CMD_SET_DEBUG_ARG_REQ:
                //qDebug()<<"设置诊断参数请求";
                setDebugArgReqProc(ipcData);
                break;

            case CMD_GET_DEBUG_ARG_REQ:
                //qDebug()<<"获取诊断参数请求";
                getDebugArgReqProc(ipcData);
                break;

            default:
                break;
            }

        }

        Util::delayMs(10);
    }

}

/**
 * @brief IpcMsgProc::packIpcHead
 * @param ipcData
 * @param frameNum
 * @param cmd
 * @param flag
 */
void IpcMsgProc::packIpcHead(IpcData &ipcData, UInt16 frameNum, UInt16 cmd, UInt8 flag)
{
    ipcData.head = PROTOCOL_HEAD;
    ipcData.frameNum = frameNum;
    ipcData.commond = cmd;
    ipcData.flag = flag;
    ipcData.buffer.clear();
    ipcData.len = 0;
    ipcData.end = PROTOCOL_END;
}




/**
 * @brief IpcMsgProc::recvUpdateArgProc 收到升级参数处理
 * @param ipcData
 */
void IpcMsgProc::recvUpdateArgProc(IpcData &ipcData)
{
    UpdateArg updateArg;
    if (ipcData.len == sizeof(updateArg)) {      
        memcpy(&updateArg, ipcData.buffer.data(), ipcData.len);

        updateArg.user[updateArg.lenUser] = 0;
        updateArg.password[updateArg.lenPassword] = 0;
        updateArg.path[updateArg.lenPath] = 0;

        /*桩编码为BCD编码，显示时转为str*/
        char buf[2*STAKE_CODE_LEN+1];
        Util::bcd2str((const unsigned char*)updateArg.stakeCode, STAKE_CODE_LEN, buf, 2*STAKE_CODE_LEN);

        if (DEBUG_FLAG) {
            qDebug("%s ip:%d.%d.%d.%d port:%d "
                   "lenUser:%d lenPass:%d lenPath:%d "
                   "user:%s pass:%s path:%s "
                   "stakeCode:%s MD5:%s",
                   __FUNCTION__,updateArg.ip[0],updateArg.ip[1],updateArg.ip[2],updateArg.ip[3],updateArg.port,
                   updateArg.lenUser,updateArg.lenPassword,updateArg.lenPath,
                   updateArg.user,updateArg.password,updateArg.path,
                   buf,QByteArray(updateArg.MD5,sizeof(updateArg.MD5)).data());
        }

        /*确认升级参数*/
        ipcData.commond = CMD_REPLY_UPDATE_ARG;
        ipcData.frameNum++;
        UInt8 stat = 1; // 0:失败 1：成功
        ipcData.len = sizeof(stat);
        ipcData.buffer.clear();
        ipcData.buffer.append((char*)&stat, ipcData.len);
        m_IpcStack->Write(ipcData);
#if REMOTE_DEBUG
        QString strHead = QString("UDP发送数据 cmd=%1 len=%2").arg(ipcData.commond,2,16).arg(ipcData.len);
        this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len, DEBUG_ARG_SEND_UDP);
#endif

        /*添加到升级参数队列*/
        this->pushUpdateArg(updateArg);

        //升级方式标志 true-U盘升级 false-运营平台升级
        UFLASH_FLAG = false;

    } else {
        Logger::Log::Instance().logError("升级参数: 长度错误");
    }
}




/**
 * @brief IpcMsgProc::replyVersionProc 应答版本处理
 * @param ipcData
 */
void IpcMsgProc::replyVersionProc(IpcData &ipcData)
{
    QString str_log;

    UInt8 version[4]; //版本参数
    memcpy(&version, ipcData.buffer.data(),sizeof(version));
    //qDebug()<< version.a << version.b << version.c << version.d;

    /*App当前版本*/
    QString str_ver = QString("V%1.%2.%3.%4")
            .arg(version[0])
            .arg(version[1])
            .arg(version[2])
            .arg(version[3]);
    /*摘要文件中解析得到的新版本*/
    QString str_new_ver = QString("V%1.%2.%3.%4")
            .arg(NewVersion[ipcData.who][0])
            .arg(NewVersion[ipcData.who][1])
            .arg(NewVersion[ipcData.who][2])
            .arg(NewVersion[ipcData.who][3]);

    if (Util::cmpVersion(NewVersion[ipcData.who], version) == 0) { /*版本相同*/
        str_log = QString("APP %1 version(%2) is equal")
                .arg(Util::toAppName(ipcData.who))
                .arg(str_ver);
        Logger::Log::Instance().logInfo(str_log);

#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        /*标记为升级成功*/
        this->setUpdateRet(ipcData.who, UPDATE_RET_SUCCESS);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        this->setFlagUpdateApp(true);

    } else { /*版本不相同*/
        str_log = QString("APP %1 current version %2,need update to %3")
                .arg(Util::toAppName(ipcData.who))
                .arg(str_ver)
                .arg(str_new_ver);
        Logger::Log::Instance().logInfo(str_log);

#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        /*查询运行状态*/
        if (ipcData.who == WHICH_GATEWAY_APP) {/*后台网关-动态库文件,计费管理单元允许是否升级*/
            ipcData.who = WHICH_BILLING_CONTROL_APP;
        }
        ipcData.buffer.clear();
        ipcData.len = 0;
        ipcData.commond = CMD_QUERY_RUN_STATUS;
        m_IpcStack->Write(ipcData,10,500);
#if REMOTE_DEBUG
        QString strHead = QString("UDP发送数据 cmd=%1 len=%2").arg(ipcData.commond,2,16).arg(ipcData.len);
        this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len, DEBUG_ARG_SEND_UDP);
#endif

    }
}


/**
 * @brief IpcMsgProc::replyRunStatusProc 应答运行状态处理
 * @param ipcData
 */
void IpcMsgProc::replyRunStatusProc(IpcData &ipcData)
{
    QString appName = Util::toAppName(ipcData.who);
    QString str_log;
    UInt8 status; //运行状态参数
    memcpy(&status, ipcData.buffer.data(), sizeof(status));


    if (ipcData.who != m_who) {
        if (ipcData.who == WHICH_BILLING_CONTROL_APP) {
            ipcData.who = WHICH_GATEWAY_APP;
        } else {
            str_log = QString("升级APP队列，升级%1时，提早被删除")
                    .arg(Util::toAppName(ipcData.who));
            Logger::Log::Instance().logError(str_log);

            /*清空升级APP队列*/
            this->clearUpdateApp();
            /*设置继续从升级APP队列读取参数，而且删除队列头*/
            this->setFlagUpdateApp(true);

            /*发送升级失败命令到后台网关*/
            this->sendUpdateRet(UPDATE_RET_FAILED);
            /*设置继续从桩参数队列读取参数，而且删除队列头*/
            this->setFlagStakeArg(true);

            return;
        }
    }


    if (status == 1) { /*允许升级*/
        str_log = QString("App %1 allow to update").arg(appName);
        Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        /*升级程序*/
        updateApp(ipcData.who);

    } else { /*不允许升级*/

        if (UFLASH_FLAG) { /*U盘升级*/
            str_log = QString("App %1 not allow to update").arg(appName);
            Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
            this->uploadDebugLogInfo(str_log);
#endif

            /*标记为升级失败*/
            this->setUpdateRet(ipcData.who, UPDATE_RET_FAILED);
            /*设置继续从升级APP队列读取参数，而且删除队列头*/
            this->setFlagUpdateApp(true);

        } else { /*运营平台升级*/
            str_log = QString("App %1 not allow to update，once more...").arg(appName);
            Logger::Log::Instance().logInfo(str_log);

#if REMOTE_DEBUG
            this->uploadDebugLogInfo(str_log);
#endif

            /*休眠10秒钟*/
            Util::delayMs(10000);

            /*清空升级APP队列*/
            this->clearUpdateApp();
            /*设置继续从升级APP队列读取参数，而且删除队列头*/
            this->setFlagUpdateApp(true);

            /*将当前桩参数加入队列尾*/
            this->pushStakeArg(m_stakeArg);
            /*设置继续从桩参数队列读取参数，而且删除队列头*/
            this->setFlagStakeArg(true);
        }
    }

}


/**
 * @brief IpcMsgProc::queryVersionProc 查询升级软件版本处理
 * @param ipcData
 */
void IpcMsgProc::queryVersionProc(IpcData &ipcData)
{
    UInt8 version[4]; /*版本参数*/
    version[0] = APP_VERSION[0];
    version[1] = APP_VERSION[1];
    version[2] = APP_VERSION[2];
    version[3] = APP_VERSION[3];

    ipcData.commond = CMD_REPLY_VERSION;
    ipcData.buffer.clear();
    ipcData.len = sizeof(version);
    ipcData.buffer.append((char*)version, ipcData.len);
    m_IpcStack->Write(ipcData);

#if REMOTE_DEBUG
    QString strHead = QString("UDP发送数据 cmd=%1 len=%2").arg(ipcData.commond,2,16).arg(ipcData.len);
    this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len, DEBUG_ARG_SEND_UDP);
#endif

}


/**
 * @brief IpcMsgProc::queryIp 作为客户端部署在刷卡显示终端时，查询集中计费端IP
 */
void IpcMsgProc::queryIp()
{
    //qDebug()<< __FUNCTION__;

    IpcData ipcData;
    packIpcHead(ipcData, 0, CMD_QUERY_IP, 0);
    ipcData.len = 0;
    ipcData.who = WHICH_CHARGE_OPERATE_APP;
    m_IpcStack->Write(ipcData,10,500);

#if REMOTE_DEBUG
    QString strHead = QString("UDP发送数据 cmd=%1 len=%2").arg(ipcData.commond,2,16).arg(ipcData.len);
    this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len, DEBUG_ARG_SEND_UDP);
#endif

}


/**
 * @brief IpcMsgProc::recvIpProc 作为客户端部署在刷卡显示终端时，收到集中计费终端的IP处理
 * @param ipcData
 */
void IpcMsgProc::recvIpProc(IpcData &ipcData)
{
    if (SERVER == 0) { /*在线升级客户端*/

        UInt8 ip[4];
        if (ipcData.len == sizeof(ip)) {
            memcpy(ip, ipcData.buffer.constData(), 4);

            QString strIp = QString("%1.%2.%3.%4")
                    .arg(ip[0])
                    .arg(ip[1])
                    .arg(ip[2])
                    .arg(ip[3]);

            emit sigIp(QHostAddress(strIp).toIPv4Address());

        } else {
            Logger::Log::Instance().logError(QString("Recv ip wrong from App %1").arg(Util::toAppName(ipcData.who)));
        }
    } else { /*在线升级服务端*/
        Logger::Log::Instance().logError("The update server should not recv ip");
    }
}

/**
 * @brief IpcMsgProc::on_ip 作为客户端部署在刷卡显示终端时，尝试连接服务端
 * @param ip
 */
void IpcMsgProc::on_ip(UInt32 ip)
{
    TcpClientSocket *tcpClientSocket = TcpClientSocket::Instance();
    tcpClientSocket->connectServer(QHostAddress(ip));
}


/**
 * @brief IpcMsgProc::uFlashUpdateProc U盘升级交互处理
 * @param ipcData
 */
void IpcMsgProc::uFlashUpdateProc(IpcData &ipcData)
{
    StakeInfo stakeInfo; /*桩信息*/
    if (ipcData.len == sizeof(stakeInfo)) {

        memcpy(&stakeInfo, ipcData.buffer.data(), ipcData.len);

        switch (stakeInfo.cmd) {

        case CLOUD_ADDRESS_SUCCESS: /*应答桩信息*/
            stakeExist(stakeInfo);
            break;

        case CLOUD_ADDRESS_FAIL: /*应答桩信息*/
            stakeNotExist(stakeInfo);
            break;

        case UFLASH_UPGRADE_LAUNCH: /*U盘启动升级命令*/
            uflashLaunch(ipcData,stakeInfo);
            break;

        case UFLASH_UPGRADE_NUMADDR: /*U盘启动发送的桩信息*/
            uFlashStakeInfo(stakeInfo);
            break;

        default:
            Logger::Log::Instance().logError(QString("Unknow UFlash update cmd %1").arg(stakeInfo.cmd));
            break;
        }
    } else {
        Logger::Log::Instance().logError("U盘升级交互命令：数据长度出错");
    }
}


/**
 * @brief IpcMsgProc::stakeExist 桩在当前站
 * @param stakeInfo
 */
void IpcMsgProc::stakeExist(StakeInfo &stakeInfo)
{
    QString str_log;
    int ret;

    /*桩编码为BCD编码，显示时转为str*/
    char buf[2*STAKE_CODE_LEN+1];
    Util::bcd2str((const unsigned char*)m_updateArg.stakeCode, STAKE_CODE_LEN, buf, 2*STAKE_CODE_LEN);

    ret = strncmp(buf, stakeInfo.data1, 2*STAKE_CODE_LEN);
    if (ret == 0) {
        /*将桩参数存入队列*/
        StakeArg stakeArg;
        stakeArg.updateArg = m_updateArg;
        stakeArg.stakeAddr = stakeInfo.data0;
        this->pushStakeArg(stakeArg);

        str_log = QString("The stake (%1) is located at the current station")
                .arg(buf);
        Logger::Log::Instance().logInfo(str_log);

    } else {
        str_log = "配置管理应答的桩编码（存在）和当前桩编码不同";
        Logger::Log::Instance().logError(str_log);
    }

    /*设置继续从升级参数队列读取参数，而且删除队列头*/
    this->setFlagUpdateArg(true);
}


/**
 * @brief IpcMsgProc::stakeNotExist 桩不在当前站
 * @param stakeInfo
 */
void IpcMsgProc::stakeNotExist(StakeInfo &stakeInfo)
{
    QString str_log;
    int ret;

    /*桩编码为BCD编码，显示时转为str*/
    char buf[2*STAKE_CODE_LEN+1];
    Util::bcd2str((const unsigned char*)m_updateArg.stakeCode, STAKE_CODE_LEN, buf, 2*STAKE_CODE_LEN);

    ret = strncmp(buf, stakeInfo.data1, 2*STAKE_CODE_LEN);
    if (ret == 0) {
        str_log = QString("The stake (%1) is not located at the current station")
                .arg(buf);
        Logger::Log::Instance().logError(str_log);

    } else {
        str_log = "配置管理应答的桩编码（不存在）和当前桩编码不同";
        Logger::Log::Instance().logError(str_log);
    }

    /*设置继续从升级参数队列读取参数，而且删除队列头*/
    this->setFlagUpdateArg(true);

}



/**
 * @brief IpcMsgProc::uflashLaunch U盘启动升级命令
 * @param ipcData
 * @param stakeInfo
 */
void IpcMsgProc::uflashLaunch(IpcData &ipcData, StakeInfo &stakeInfo)
{
    QString str_log;
    QByteArray ba = QByteArray(stakeInfo.data1, stakeInfo.data1size);
    QString pkgUFlashPath = QString(ba) +'/'+ UpdatePackage;
    QString pkgTempPath = TempDir +'/'+ UpdatePackage;

    QFile fileUFlash(pkgUFlashPath);
    QFile fileTemp(pkgTempPath);

    if (DEBUG_FLAG) {
        qDebug("%s UFlash-Update path:%s",__FUNCTION__,pkgUFlashPath.toUtf8().constData());
    }
    bool bAllawUpdate = false; /*允许升级标志*/

    if (fileUFlash.exists()) {
       if (this->isUpdateArgQueEmpty()) {
           if (this->isStakeArgQueEmpty()) {
               if (this->isUpdateAppQueEmpty()) {
                   /*允许升级*/
                   bAllawUpdate = true;
                   /*升级方式标志 true-U盘升级 false-运营平台升级*/
                   UFLASH_FLAG = true;
               }
           }
       }
    }

    if (bAllawUpdate) { /*允许升级*/
        stakeInfo.cmd = UFLASH_LAUNCH_SUCCESS;
        stakeInfo.data1size = 0;
        /*发送是否允许升级到配置管理单元*/
        ipcData.buffer.clear();
        ipcData.len = sizeof(stakeInfo);
        ipcData.buffer.append((char*)&stakeInfo, ipcData.len);
        m_IpcStack->Write(ipcData);
#if REMOTE_DEBUG
        QString strHead = QString("UDP发送数据 cmd=%1 len=%2").arg(ipcData.checkSum,2,16).arg(ipcData.len);
        this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len, DEBUG_ARG_SEND_UDP);
#endif

        str_log = QString("UFlash-update start ...");
        Logger::Log::Instance().logInfo(str_log);

        /*临时存储目录不存在则创建*/
        QDir dir(TempDir);
        if (!dir.exists()) {
            dir.mkdir(dir.absolutePath());
        }

        /*升级包已存在则删除*/
        if (fileTemp.exists()) {
            fileTemp.remove();
        }

        /*拷贝文件*/
        if (fileUFlash.copy(pkgTempPath)) {
            str_log = QString("Copy file %1 ...").arg(UpdatePackage);
            Logger::Log::Instance().logInfo(str_log);

            /*解压缩升级包*/
            str_log = QString("Unzip file %1 ...").arg(UpdatePackage);
            Logger::Log::Instance().logInfo(str_log);
            QString unzip = "unzip -o " + TempDir+'/'+UpdatePackage + " -d " + TempDir;
            system(unzip.toUtf8().constData());

            /*生成升级包MD5并保存*/
            QByteArray ba = Util::getFileMd5(pkgTempPath);
            QSettings ini(Md5Cfg, QSettings::IniFormat);
            ini.beginGroup("MD5");
            ini.setValue("md5", QString(ba.toHex()));
            ini.endGroup();

        } else {
            str_log = QString("Copy file %1 failed").arg(UpdatePackage);
            Logger::Log::Instance().logError(str_log);
        }


    } else { /*不允许升级*/
        stakeInfo.cmd = UFLASH_LAUNCH_FAIL;
        stakeInfo.data1size = 0;
        /*发送是否允许升级到配置管理单元*/
        ipcData.buffer.clear();
        ipcData.len = sizeof(stakeInfo);
        ipcData.buffer.append((char*)&stakeInfo, ipcData.len);
        m_IpcStack->Write(ipcData);
#if REMOTE_DEBUG
        QString strHead = QString("UDP发送数据 cmd=%1 len=%2").arg(ipcData.checkSum,2,16).arg(ipcData.len);
        this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len, DEBUG_ARG_SEND_UDP);
#endif

    }

}

/**
 * @brief IpcMsgProc::uFlashStakeInfo U盘升级时配置管理发送的桩信息
 * @param stakeInfo
 */
void IpcMsgProc::uFlashStakeInfo(StakeInfo &stakeInfo)
{
    if (DEBUG_FLAG) {
        qDebug("%s stakeAddr:%d stakeCodeLen长度:%d stakCode:%s",
               __FUNCTION__,stakeInfo.data0,stakeInfo.data1size,stakeInfo.data1);
    }

    QString str_log = QString("UFlash-Update: recv stake(%1) info")
            .arg(QByteArray(stakeInfo.data1,stakeInfo.data1size).constData());
    Logger::Log::Instance().logInfo(str_log);


    StakeArg stakeArg;
    QFileInfo info(Md5Cfg);
    QSettings ini(Md5Cfg,QSettings::IniFormat);
    if (info.exists()) {
        QString md5 = ini.value("MD5/md5").toString();
        memcpy(stakeArg.updateArg.MD5, md5.toUtf8().constData(), sizeof(stakeArg.updateArg.MD5));

        /*将U盘升级时配置管理发送的桩编码转为BCD编码*/
        Util::str2bcd(stakeInfo.data1,2*STAKE_CODE_LEN,(unsigned char*)stakeArg.updateArg.stakeCode,STAKE_CODE_LEN);
        stakeArg.stakeAddr = stakeInfo.data0;

        /*U盘升级 将需要升级的桩信息加到队列中*/
        this->pushStakeArg(stakeArg);

    } else {
        str_log = QString("UFlash update, can't find file %1").arg(Md5Cfg);
        Logger::Log::Instance().logError(str_log);
    }

}




/**
 * @brief IpcMsgProc::setDebugSwitchReqProc 设置诊断开关请求处理
 * @param ipcData
 */
void IpcMsgProc::setDebugSwitchReqProc(IpcData &ipcData)
{
    SetDebugSwitchReq req;
    SetDebugSwitchAck ack;
    if (ipcData.len == sizeof(req)) {
        memcpy(&req, ipcData.buffer.constData(), ipcData.len);

        if ( (m_debugSwitch == DEBUG_SWITCH_CLOSE) &&
             (req.Switch == DEBUG_SWITCH_OPEN) ) {

            m_debugIp = req.ip;
            m_debugPort = req.port;
            m_debugSwitch = DEBUG_SWITCH_OPEN; /*设置诊断总开关*/

            ack.flag = DEBUG_FLAG_NORMAL;
            ack.ret = DEBUG_RET_SUCCESS;

        } else if ( (m_debugSwitch == DEBUG_SWITCH_OPEN) &&
                    (req.Switch == DEBUG_SWITCH_CLOSE) ) {

            if ( (req.ip == m_debugIp) && (req.port == m_debugPort) ) {
                m_debugIp = 0;
                m_debugPort = 0;
                m_debugSwitch     = DEBUG_SWITCH_CLOSE; /*远程诊断-上传总开关*/
                m_debugLogSwitch  = DEBUG_SWITCH_CLOSE; /*远程诊断-日志信息上传开关*/
                m_debugRecvCanSwitch = DEBUG_SWITCH_CLOSE; /*远程诊断-CAN接收信息上传开关*/
                m_debugSendCanSwitch = DEBUG_SWITCH_CLOSE; /*远程诊断-CAN发送信息上传开关*/
                m_debugRecvUdpSwitch = DEBUG_SWITCH_CLOSE; /*远程诊断-UDP接收信息上传开关*/
                m_debugSendUdpSwitch = DEBUG_SWITCH_CLOSE; /*远程诊断-UDP发送信息上传开关*/

                ack.flag = DEBUG_FLAG_NORMAL;
                ack.ret = DEBUG_RET_SUCCESS;
            } else {
                ack.flag = DEBUG_FLAG_EXCEPTION;
                ack.ret = DEBUG_RET_FAILED;
            }

        } else {
            ack.flag = DEBUG_FLAG_EXCEPTION;
            ack.ret = DEBUG_RET_FAILED;
        }

        ack.ip = req.ip;
        ack.port = req.port;

        ipcData.commond = CMD_SET_DEBUG_SWITCH_ACK;
        ipcData.buffer.clear();
        ipcData.len = sizeof(ack);
        ipcData.buffer.append((char*)&ack, ipcData.len);
        m_IpcStack->Write(ipcData);

    } else {
        Logger::Log::Instance().logError("设置诊断开关请求命令：数据长度出错");
    }
}

/**
 * @brief IpcMsgProc::getDebugSwitchReqProc 获取诊断开关请求处理
 * @param ipcData
 */
void IpcMsgProc::getDebugSwitchReqProc(IpcData &ipcData)
{
    GetDebugSwitchReq req;
    GetDebugSwitchAck ack;
    if (ipcData.len == sizeof(req)) {
        memcpy(&req, ipcData.buffer.constData(), ipcData.len);

        ack.ip = req.ip;
        ack.port = req.port;
        ack.flag = DEBUG_FLAG_NORMAL;
        ack.ret = m_debugSwitch;

        ipcData.commond = CMD_GET_DEBUG_SWITCH_ACK;
        ipcData.buffer.clear();
        ipcData.len = sizeof(ack);
        ipcData.buffer.append((char*)&ack, ipcData.len);
        m_IpcStack->Write(ipcData);

    } else {
        Logger::Log::Instance().logError("获取诊断开关请求命令：数据长度出错");
    }
}

/**
 * @brief IpcMsgProc::setDebugArgReqProc 设置诊断参数请求处理
 * @param ipcData
 */
void IpcMsgProc::setDebugArgReqProc(IpcData &ipcData)
{
    SetDebugArgReq req;
    SetDebugArgAck ack;

    if (ipcData.len == sizeof(req)) {

        memcpy(&req, ipcData.buffer.constData(), ipcData.len);

        if (strncmp(req.arg, DEBUG_ARG_LOG, strlen(DEBUG_ARG_LOG)) == 0) {
            m_debugLogSwitch  = DEBUG_SWITCH_OPEN;  /*诊断调试-日志开关*/
            ack.flag = DEBUG_FLAG_NORMAL;
            ack.ret = DEBUG_RET_SUCCESS;

        } else if (strncmp(req.arg, DEBUG_ARG_RECV_CAN, strlen(DEBUG_ARG_RECV_CAN)) == 0) {
            m_debugRecvCanSwitch  = DEBUG_SWITCH_OPEN; /*诊断调试-CAN接收信息开关*/
            ack.flag = DEBUG_FLAG_NORMAL;
            ack.ret = DEBUG_RET_SUCCESS;

        } else if (strncmp(req.arg, DEBUG_ARG_SEND_CAN, strlen(DEBUG_ARG_SEND_CAN)) == 0) {
            m_debugSendCanSwitch  = DEBUG_SWITCH_OPEN; /*诊断调试-CAN发送信息开关*/
            ack.flag = DEBUG_FLAG_NORMAL;
            ack.ret = DEBUG_RET_SUCCESS;

        } else if (strncmp(req.arg, DEBUG_ARG_RECV_UDP, strlen(DEBUG_ARG_RECV_UDP)) == 0) {
            m_debugRecvUdpSwitch  = DEBUG_SWITCH_OPEN; /*诊断调试-UDP接收信息开关*/
            ack.flag = DEBUG_FLAG_NORMAL;
            ack.ret = DEBUG_RET_SUCCESS;

        } else if (strncmp(req.arg, DEBUG_ARG_SEND_UDP, strlen(DEBUG_ARG_SEND_UDP)) == 0) {
            m_debugSendUdpSwitch  = DEBUG_SWITCH_OPEN; /*诊断调试-UDP发送信息开关*/
            ack.flag = DEBUG_FLAG_NORMAL;
            ack.ret = DEBUG_RET_SUCCESS;

        } else {
            ack.flag = DEBUG_FLAG_EXCEPTION;
            ack.ret = DEBUG_RET_FAILED;
        }

        ack.ip = req.ip;
        ack.port = req.port;

        ipcData.commond = CMD_SET_DEBUG_ARG_ACK;
        ipcData.buffer.clear();
        ipcData.len = sizeof(ack);
        ipcData.buffer.append((char*)&ack, ipcData.len);
        m_IpcStack->Write(ipcData);

    } else {
        Logger::Log::Instance().logError("设置诊断参数请求命令：数据长度出错");
    }
}

/**
 * @brief IpcMsgProc::getDebugArgReqProc 获取诊断参数请求处理
 * @param ipcData
 */
void IpcMsgProc::getDebugArgReqProc(IpcData &ipcData)
{
    GetDebugArgReq req;
    GetDebugArgAck ack;
    if (ipcData.len == sizeof(req)) {
        memcpy(&req, ipcData.buffer.constData(), ipcData.len);

        ack.ip = req.ip;
        ack.port = req.port;
        ack.flag = DEBUG_FLAG_NORMAL;
        memset(ack.arg, 0, sizeof(ack.arg));

        int pos = 0;
        QString str  = QString("可设置诊断参数：%1|%2|%3|%4|%5\n已设置诊断参数：")
                .arg(DEBUG_ARG_LOG)
                .arg(DEBUG_ARG_RECV_CAN)
                .arg(DEBUG_ARG_SEND_CAN)
                .arg(DEBUG_ARG_RECV_UDP)
                .arg(DEBUG_ARG_SEND_UDP);

        strncpy(&ack.arg[pos], str.toUtf8().data(), str.toUtf8().size());
        pos += str.toUtf8().size();

        if (m_debugLogSwitch == DEBUG_SWITCH_OPEN) {
            strncpy(&ack.arg[pos], DEBUG_ARG_LOG, strlen(DEBUG_ARG_LOG));
            pos += strlen(DEBUG_ARG_LOG);
            ack.arg[pos++] = '|';
        }
        if (m_debugRecvCanSwitch == DEBUG_SWITCH_OPEN) {
            strncpy(&ack.arg[pos], DEBUG_ARG_RECV_CAN, strlen(DEBUG_ARG_RECV_CAN));
            pos += strlen(DEBUG_ARG_RECV_CAN);
            ack.arg[pos++] = '|';
        }
        if (m_debugSendCanSwitch == DEBUG_SWITCH_OPEN) {
            strncpy(&ack.arg[pos], DEBUG_ARG_SEND_CAN, strlen(DEBUG_ARG_SEND_CAN));
            pos += strlen(DEBUG_ARG_SEND_CAN);
            ack.arg[pos++] = '|';
        }
        if (m_debugRecvUdpSwitch == DEBUG_SWITCH_OPEN) {
            strncpy(&ack.arg[pos], DEBUG_ARG_RECV_UDP, strlen(DEBUG_ARG_RECV_UDP));
            pos += strlen(DEBUG_ARG_RECV_UDP);
            ack.arg[pos++] = '|';
        }
        if (m_debugSendUdpSwitch == DEBUG_SWITCH_OPEN) {
            strncpy(&ack.arg[pos], DEBUG_ARG_SEND_UDP, strlen(DEBUG_ARG_SEND_UDP));
            pos += strlen(DEBUG_ARG_SEND_UDP);
            //ack.arg[pos++] = '|';
        }

        ipcData.commond = CMD_GET_DEBUG_ARG_ACK;
        ipcData.buffer.clear();
        ipcData.len = sizeof(ack);
        ipcData.buffer.append((char*)&ack, ipcData.len);
        m_IpcStack->Write(ipcData);

    } else {
        Logger::Log::Instance().logError("获取诊断参数请求命令：数据长度出错");
    }
}

void IpcMsgProc::uploadDebugLogInfo(QString info)
{
    if ((m_debugLogSwitch==DEBUG_SWITCH_OPEN) && (m_debugSwitch==DEBUG_SWITCH_OPEN)) {

        QString str = QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm:ss.zzz");
        QString tmp = QString(" %1 ").arg(DEBUG_ARG_LOG);
        str.append(tmp);
        str.append(info);

        DebugInfo debugInfo;
        debugInfo.ip = m_debugIp;
        debugInfo.port = m_debugPort;
        debugInfo.len = str.toUtf8().size();
        strncpy(debugInfo.info,str.toUtf8().data(),debugInfo.len);

        IpcData ipcData;
        packIpcHead(ipcData,0x00,CMD_SEND_DEBUG_INFO,0x00);
        ipcData.buffer.clear();
        ipcData.len = sizeof(debugInfo)-DEBUG_INFO_SIZE+debugInfo.len;
        ipcData.buffer.append((char*)&debugInfo, ipcData.len);
        ipcData.who = WHICH_DEBUG_APP;
        m_IpcStack->Write(ipcData);
    }

}


void IpcMsgProc::uploadDebugBuffer(QString strHead, char *buf, int len, string flag)
{
    bool ret = false;

    if ((m_debugRecvCanSwitch==DEBUG_SWITCH_OPEN)
            && (m_debugSwitch==DEBUG_SWITCH_OPEN)
            && (strncmp(flag.c_str(), DEBUG_ARG_RECV_CAN, strlen(DEBUG_ARG_RECV_CAN)) == 0)) {

        ret = true;

    } else if ((m_debugSendCanSwitch==DEBUG_SWITCH_OPEN)
                   && (m_debugSwitch==DEBUG_SWITCH_OPEN)
                   && (strncmp(flag.c_str(), DEBUG_ARG_SEND_CAN, strlen(DEBUG_ARG_SEND_CAN)) == 0)) {
        ret = true;

    } else if ((m_debugRecvUdpSwitch==DEBUG_SWITCH_OPEN)
                   && (m_debugSwitch==DEBUG_SWITCH_OPEN)
                   && (strncmp(flag.c_str(), DEBUG_ARG_RECV_UDP, strlen(DEBUG_ARG_RECV_UDP)) == 0)) {
        ret = true;

    } else if ((m_debugSendUdpSwitch==DEBUG_SWITCH_OPEN)
                   && (m_debugSwitch==DEBUG_SWITCH_OPEN)
                   && (strncmp(flag.c_str(), DEBUG_ARG_SEND_UDP, strlen(DEBUG_ARG_SEND_UDP)) == 0)) {
        ret = true;
    }

    if (ret) {
        QString str = QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm:ss.zzz");
        QString tmp = QString(" %1 ").arg(strHead);
        str.append(tmp);

        for(int i=0;i<len;i++)
        {
            str.append(QString("%1").arg(buf[i],2,16,QChar('0')));
            str.append(" ");
        }

        DebugInfo debugInfo;
        debugInfo.ip = m_debugIp;
        debugInfo.port = m_debugPort;
        debugInfo.len = str.toUtf8().size();
        strncpy(debugInfo.info,str.toUtf8().data(),debugInfo.len);

        IpcData ipcData;
        packIpcHead(ipcData,0x00,CMD_SEND_DEBUG_INFO,0x00);
        ipcData.buffer.clear();
        ipcData.len = sizeof(debugInfo)-DEBUG_INFO_SIZE+debugInfo.len;
        ipcData.buffer.append((char*)&debugInfo, ipcData.len);
        ipcData.who = WHICH_DEBUG_APP;
        m_IpcStack->Write(ipcData);
    }

}


/**
 * @brief IpcMsgProc::downloadFile 下载文件
 * @param filename 文件名
 */
void IpcMsgProc::downloadFile(QString filename)
{

    QString str_log = QString("Start download %1 ...").arg(filename);
    Logger::Log::Instance().logInfo(str_log);

    QUrl url;
    url.setScheme("ftp");
    QString ip = QString("%1.%2.%3.%4")
            .arg(m_stakeArg.updateArg.ip[0])
            .arg(m_stakeArg.updateArg.ip[1])
            .arg(m_stakeArg.updateArg.ip[2])
            .arg(m_stakeArg.updateArg.ip[3]);
    QString user = QString(m_stakeArg.updateArg.user);
    QString pd = QString(m_stakeArg.updateArg.password);
    QString filepath = QString(m_stakeArg.updateArg.path) + '/' + filename;
    url.setHost(ip);
    url.setPort(m_stakeArg.updateArg.port);
    url.setUserName(user);
    url.setPassword(pd);
    url.setPath(filepath);

    if (DEBUG_FLAG) {
        qDebug()<< __FUNCTION__ << ip << url.port() << user << pd << filepath;
    }

    if (transport->getFile(url) < 0) {
        str_log = QString("Download %1 failed").arg(filename);
        Logger::Log::Instance().logError(str_log);

        /*发送升级失败命令到后台网关*/
        this->sendUpdateRet(UPDATE_RET_FAILED);
        /*设置继续从桩参数队列读取参数，而且删除队列头*/
        this->setFlagStakeArg(true);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif
    }


    return;
}

/**
 * @brief IpcMsgProc::downloadFileOver 下载文件完毕处理
 * @param flag
 */
void IpcMsgProc::downloadFileOver(bool flag)
{
    QString str_log;

    if (flag == false) {
        str_log = QString("Download %1 failed").arg(UpdatePackage);
        Logger::Log::Instance().logError(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        /*发送升级失败命令到后台网关*/
        this->sendUpdateRet(UPDATE_RET_FAILED);
        /*设置继续从桩参数队列读取参数，而且删除队列头*/
        this->setFlagStakeArg(true);


    } else {
        str_log = QString("Download %1 success").arg(UpdatePackage);
        Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        /*校验MD5*/
        QByteArray md5 = Util::getFileMd5(TempDir +'/'+ UpdatePackage);
        int ret = strncmp(m_stakeArg.updateArg.MD5, md5.toHex().constData(), sizeof(m_stakeArg.updateArg.MD5));
        if (ret != 0) {
            str_log = QString("Verify %1 MD5 failed").arg(UpdatePackage);
            Logger::Log::Instance().logError(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

            /*发送升级失败命令到后台网关*/
            this->sendUpdateRet(UPDATE_RET_FAILED);
            /*设置继续从桩参数队列读取参数，而且删除队列头*/
            this->setFlagStakeArg(true);

            return;
        }
        str_log = QString("Verify %1 MD5 OK").arg(UpdatePackage);
        Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        /*保存升级包的MD5值*/
        QSettings ini(Md5Cfg, QSettings::IniFormat);
        ini.beginGroup("MD5");
        ini.setValue("md5", QString(md5.toHex()));
        ini.endGroup();


        //解压升级包到临时文件夹
        str_log = QString("Unzip file %1 ...").arg(UpdatePackage);
        Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        QString unzip = "unzip -o " + TempDir+'/'+UpdatePackage + " -d " + TempDir;
        system(unzip.toUtf8().data());

        seletcUpdateApp();

    }
}


/**
 * @brief IpcMsgProc::seletcUpdateApp 根据版本选择需要升级的APP，加入升级队列
 */
void IpcMsgProc::seletcUpdateApp()
{    
    QString str_log;

    /*当前升级结果*/
    char buf[2*STAKE_CODE_LEN+1];
    Util::bcd2str((const unsigned char*)m_stakeArg.updateArg.stakeCode, STAKE_CODE_LEN, buf, 2*STAKE_CODE_LEN);
    Util::str2bcd(buf,2*STAKE_CODE_LEN,(unsigned char*)m_updateRet.stakeCode,STAKE_CODE_LEN);
    //strncpy(m_updateRet.stakeCode, m_stakeArg.updateArg.stakeCode, STAKE_CODE_LEN);
    m_updateRet.stakeAddr = m_stakeArg.stakeAddr;
    for (int i=0; i<APP_NUM; i++) {
        m_updateRet.updateRet[i] = UPDATE_RET_INIT; /*升级结果初始化为未升级*/
    }

    if (!this->isUpdateAppQueEmpty()) {
        /*清空升级APP队列*/
        this->clearUpdateApp();
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        this->setFlagUpdateApp(true);

        str_log = "从升级参数队列取出参数时，升级APP队列未全部升级完毕";
        Logger::Log::Instance().logError(str_log);
    }

    /*将升级版本全部重置0*/
    for(int i=0; i<APP_NUM;i++) {
        NewVersion[i][0] = 0;
        NewVersion[i][1] = 0;
        NewVersion[i][2] = 0;
        NewVersion[i][3] = 0;
    }

    //从升级摘要文件中解析得到新的版本参数(升级摘要文件每次重新下载),如果对应app无须升级，版本号全为0
    if (parseUpdateSummary(NewVersion) < 0) { //解析不正确
        /*发送升级失败命令到后台网关*/
        this->sendUpdateRet(UPDATE_RET_FAILED);
        /*设置继续从桩参数队列读取参数，而且删除队列头*/
        this->setFlagStakeArg(true);

        str_log = QString("Parse file %1 failed").arg(UpdateSumary);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

    } else {
        str_log = QString("Parse file %1 success").arg(UpdateSumary);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        WHICH_APP who;
        for (int i = 0; i < APP_NUM; ++i) {
            who = WHICH_APP(i);
            if ((NewVersion[who][0] == 0) &&
                (NewVersion[who][1] == 0) &&
                (NewVersion[who][2] == 0) &&
                (NewVersion[who][3] == 0)) { //版本号全0,不需要升级

                /*标记为升级成功*/
                this->setUpdateRet(who, UPDATE_RET_SUCCESS);

            } else { /*需要升级,加入升级APP队列*/
                this->pushUpdateApp(who);

            }
        }
    }

}


/**
 * @brief IpcMsgProc::updateApp 升级who
 * @param who
 */
void IpcMsgProc::updateApp(WHICH_APP who)
{
    QString str_log;
    QString appName = Util::toAppName(who);             //app程序名
    QString appWorkDir = Util::toAppWorkDir(who);       //app工作目录
    QString appPath = appWorkDir + "/" + appName;       //app路径
    QString tempAppDir = TempDir +"/"+ UpdatePackage.section('.',0,0) +'/'+ appName;    //临时文件夹中app对应的目录
    QString tempAppPath = tempAppDir +'/'+ appName;    //临时文件夹中app对应的路径

    if (who == WHICH_GATEWAY_APP) {
        tempAppPath = tempAppDir +"/lib"+ appName + ".so";
    }

    if (DEBUG_FLAG) {
        qDebug()<< __FUNCTION__ << "appName        :" << appName;
        qDebug()<< __FUNCTION__ << "appWorkDir     :" << appWorkDir;
        qDebug()<< __FUNCTION__ << "appPath        :" << appPath;
        qDebug()<< __FUNCTION__ << "tempAppDir     :" << tempAppDir;
        qDebug()<< __FUNCTION__ << "tempAppPath    :" << tempAppPath;
    }

    str_log = QString("Start update App %1 ...").arg(appName);
    Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

    QFileInfo info(tempAppPath);
    if (!info.exists()) {
        str_log = QString("File %1 not exist,update failed").arg(info.fileName());
        Logger::Log::Instance().logError(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        /*标记为升级失败*/
        this->setUpdateRet(who, UPDATE_RET_FAILED);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        this->setFlagUpdateApp(true);
        return;
    }

    //备份旧的程序文件
    str_log = QString("Backup app %1 ...").arg(appName);
    Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
    this->uploadDebugLogInfo(str_log);
#endif
    QDir dir;
    dir.setPath(BackupDir); /*备份目录*/
    if (!dir.exists()) {
        dir.mkdir(dir.absolutePath());
    }

    QString backupAppDir = BackupDir +'/'+ appName;
    dir.setPath(backupAppDir); /*备份目录下对应的个APP目录*/
    if (!dir.exists()) {
        dir.mkdir(dir.absolutePath());
    }
    QString cp;
    if (who == WHICH_UPDATE_APP) {
        /*备份升级可执行文件*/
        cp = "cp " + appPath +" "+ backupAppDir +" -rf";
        system(cp.toUtf8().data());
        /*备份升级配置文件*/
        cp = "cp " + appWorkDir +'/'+ UpdateCfg +" "+ backupAppDir +" -rf";
        system(cp.toUtf8().data());
        /*备份升级日志文件*/
        cp = "cp " + appWorkDir +'/'+ "Log" +" "+ backupAppDir +" -rf";
        system(cp.toUtf8().data());

    } else {
        dir.setPath(appWorkDir);
        if (dir.exists()) { /*APP工作目录存在*/
            /*备份app工作目录*/
            cp = "cp " + appWorkDir +"/* "+ backupAppDir +" -rf";
            system(cp.toUtf8().data());
        }
    }


    //终止守护进程
    QString kill;
    str_log = QString("Kill daemon app  %1 ...").arg(Util::toAppName(WHICH_DAEMON_APP));
    Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
    this->uploadDebugLogInfo(str_log);
#endif

    QString daemonName = Util::toAppName(WHICH_DAEMON_APP);
    if (Util::isAppRun(daemonName) == 0) {
        kill = "killall " + daemonName;
        system(kill.toUtf8().data());
    }

    //终止需要升级的程序
    str_log = QString("Kill is updating app %1 ...").arg(appName);
    Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
    this->uploadDebugLogInfo(str_log);
#endif

    if (who == WHICH_UPDATE_APP) {
        //升级自身,升级完成启动新进程后退出老进程
    } else if (who == WHICH_GATEWAY_APP) { //如果升级后台网关,杀死计费管理模块
        QString billingCtrlName = Util::toAppName(WHICH_BILLING_CONTROL_APP); //计费管理文件名
        if (Util::isAppRun(billingCtrlName) == 0) {
            kill = "killall " + billingCtrlName;
            system(kill.toUtf8().data());
        }
    } else { //升级其他模块,杀死对应的模块(包括计费管理模块)
        if (Util::isAppRun(appName) == 0) {
            kill = "killall " + appName;
            system(kill.toUtf8().data());
        }
    }

    //用新的程序文件集覆盖旧的程序文件集
    str_log = QString("Cover app %1 file ...").arg(appName);
    Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
    this->uploadDebugLogInfo(str_log);
#endif

    dir.setPath(appWorkDir);
    if (!dir.exists()) { /*APP工作目录不存在,则创建*/
        dir.mkdir(dir.absolutePath());
    }
    QString copy = "cp " + tempAppPath +' '+ appWorkDir + " -rf";
    system(copy.toUtf8().data());

    //添加可执行权限
    QString chmod = "chmod +x " + appPath;
    system(chmod.toUtf8().data());

    //启动新的程序文件
    str_log = QString("Start-up new app %1 ...").arg(appName);
    Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
    this->uploadDebugLogInfo(str_log);
#endif
    QStringList args;
    args << "-qws" << "&";
    bool ret;

    if (who == WHICH_DAEMON_APP) { /*守护进程*/

        if (QProcess::startDetached(appPath,args, appWorkDir)) { /*启动成功*/
            ret = true;
        } else { /*启动失败*/
            ret = false;
        }

    } else if (who == WHICH_GATEWAY_APP) {

        QString billingCtrlWorkDir = Util::toAppWorkDir(WHICH_BILLING_CONTROL_APP);
        QString billingCtrlPath =  billingCtrlWorkDir+ "/" + Util::toAppName(WHICH_BILLING_CONTROL_APP); //计费管理文件路径
        if (QProcess::startDetached(billingCtrlPath, args, billingCtrlWorkDir)) { /*启动成功*/
            ret = true;
        } else { /*启动失败*/
            ret = false;
        }

    } else {
        if (QProcess::startDetached(appPath,args, appWorkDir)) { /*启动成功*/
            ret = true;
        } else { /*启动失败*/
            ret = false;
        }
    }


    QString str_ver = QString("V%1.%2.%3.%4").arg(NewVersion[who][0]).arg(NewVersion[who][1]).arg(NewVersion[who][2]).arg(NewVersion[who][3]);
    QString back;


    if (ret) { /*重启成功*/

        if (who != WHICH_DAEMON_APP) {
            /*重启守护进程*/

#if REBOOT_DAEMON
            QString daemonWorkDir = Util::toAppWorkDir(WHICH_DAEMON_APP);
            QString daemonPath = daemonWorkDir + "/" + Util::toAppName(WHICH_DAEMON_APP);
            QProcess::startDetached(daemonPath, args, daemonWorkDir);
#endif
        }


        str_log = QString("App %1 update to %2 success").arg(Util::toAppName(who)).arg(str_ver);
        Logger::Log::Instance().logInfo(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        /*标记为升级成功*/
        this->setUpdateRet(who, UPDATE_RET_SUCCESS);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        this->setFlagUpdateApp(true);

        if (who == WHICH_UPDATE_APP) {
            Util::delayMs(1000);
            emit sigQuit();
        }

    } else { /*重启失败*/

        /*恢复备份的程序可执行文件*/
        if (who == WHICH_UPDATE_APP) {
            /*恢复升级可执行文件*/
            back = "cp " + backupAppDir +"/"+ appName +" "+ appWorkDir +" -rf";
            system(back.toUtf8().data());
            /*恢复升级配置文件*/
            back = "cp " + backupAppDir +"/"+ UpdateCfg +" "+ appWorkDir +" -rf";
            system(back.toUtf8().data());

        } else {
            back = "cp " + backupAppDir +"/* "+ appWorkDir +" -rf";
            system(back.toUtf8().data());
        }

        //添加可执行权限
        QString chmod = "chmod +x " + appPath;
        system(chmod.toUtf8().data());

        if (who == WHICH_GATEWAY_APP) {
            QString billingCtrlWorkDir = Util::toAppWorkDir(WHICH_BILLING_CONTROL_APP);
            QString billingCtrlPath =  billingCtrlWorkDir+ "/" + Util::toAppName(WHICH_BILLING_CONTROL_APP); //计费管理文件路径
            QProcess::startDetached(billingCtrlPath, args, billingCtrlWorkDir);
        } else {
            QProcess::startDetached(appPath,args, appWorkDir);
        }

        if (who != WHICH_DAEMON_APP) {
            /*重启守护进程*/
#if REBOOT_DAEMON
            QString daemonWorkDir = Util::toAppWorkDir(WHICH_DAEMON_APP);
            QString daemonPath = daemonWorkDir + "/" + Util::toAppName(WHICH_DAEMON_APP);           
            QProcess::startDetached(daemonPath, args, daemonWorkDir);
#endif
        }

        str_log = QString("App %1 update to %2 failed").arg(Util::toAppName(who)).arg(str_ver);
        Logger::Log::Instance().logError(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

        /*标记为升级失败*/
        this->setUpdateRet(who, UPDATE_RET_FAILED);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        this->setFlagUpdateApp(true);

        if (who == WHICH_UPDATE_APP) {
            Util::delayMs(1000);
            emit sigQuit();
        }

    }

    if (DEBUG_FLAG) {
        qDebug()<< __FUNCTION__ << "cp             :" << cp;
        qDebug()<< __FUNCTION__ << "copy           :" << copy;
        qDebug()<< __FUNCTION__ << "kill           :" << kill;
        qDebug()<< __FUNCTION__ << "chmod          :" << chmod;
        qDebug()<< __FUNCTION__ << "backupAppDir   :" << backupAppDir;
    }


}

/**
 * @brief IpcMsgProc::on_quit 收到退出信号退出程序
 */
void IpcMsgProc::on_quit()
{
    /*Tcp客户端*/
    TcpClientSocket *tcpClientSocket = TcpClientSocket::Instance();
    tcpClientSocket->deleteLater();

    /*Tcp服务端*/
    TcpServer *tcpServer = TcpServer::Instance();
    tcpServer->deleteLater();

    /*Can消息处理*/
    CanMsgProc *canMsgProc = CanMsgProc::Instance();
    canMsgProc->deleteLater();

    /*Ipc消息处理*/
    this->deleteLater();

    QCoreApplication::quit();
}



/**
 * @brief IpcMsgProc::parseUpdateSummary 解析升级摘要文件
 * @param argVersion
 * @return
 */
int IpcMsgProc::parseUpdateSummary(UInt8 version[][4])
{
    QString str_log;
    QString filePath = TempDir +'/'+ UpdatePackage.section('.',0,0) +'/'+ UpdateSumary;

    QFileInfo info(filePath);
    if (!info.exists()) { /*升级摘要文件不存在*/
        str_log = QString("The update summary file %1 is not exist").arg(filePath);
        Logger::Log::Instance().logError(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif
        return -1;
    }
    /*从升级摘要文件获取版本参数*/
    QSettings ini(filePath, QSettings::IniFormat);
    ini.setIniCodec(QTextCodec::codecForName(QString("gbk").toUtf8()));
    int num = ini.value("Summary/num").toInt();
    QString key;
    QStringList value;
    QString filename;
    QString ver;

    WHICH_APP who;
    QString appName;

    for (int i=1; i<=num; i++) {
        key = "Summary/app" + QString::number(i);
        value       = ini.value(key).toStringList();

        if ((value.empty()) || (value.size()!=2)) {
            Logger::Log::Instance().logError(QString("升级摘要文件%1格式错误").arg(UpdateSumary));
            return -2;
        }

        filename    = value.at(0);
        ver         = value.at(1);

        if (DEBUG_FLAG) {
            qDebug("%s filename:%s ver:%s ", __FUNCTION__,
                   filename.toUtf8().data(),ver.toUtf8().data());
        }

        for (int j=0; j<APP_NUM; j++) {
            who = WHICH_APP(j);
            appName = Util::toAppName(who);
            if (DEBUG_FLAG) {
                qDebug("%s AppName:%s",__FUNCTION__,appName.toUtf8().data());
            }
            if (filename == appName) {
                version[who][0] = ver.section('.',0,0).toInt();
                version[who][1] = ver.section('.',1,1).toInt();
                version[who][2] = ver.section('.',2,2).toInt();
                version[who][3] = ver.section('.',3,3).toInt();
                break;
            }
        }
    }

    return 1;
}






/**
 * @brief IpcMsgProc::pushUpdateArg 升级参数队列，添加到队尾
 * @param arg
 */
void IpcMsgProc::pushUpdateArg(UpdateArg arg)
{
    QMutexLocker locker(&m_updateArgQueMutex);
    if (m_updateArgQue.size() < 65) {
        m_updateArgQue.push(arg);
    } else {
        QString str_log = QString("recv too many update args from %1")
                .arg(Util::toAppName(WHICH_GATEWAY_APP));
        Logger::Log::Instance().logError(str_log);
    }

}


/**
 * @brief IpcMsgProc::getUpdateArg 升级参数队列，取出队头参数
 * @param arg
 * @return
 */
bool IpcMsgProc::getUpdateArg(UpdateArg &arg)
{
    bool ret = false;
    QMutexLocker locker(&m_updateArgQueMutex);

    if (!m_updateArgQue.empty()) {
        arg = m_updateArgQue.front();
        ret = true;
    }
    return ret;
}


/**
 * @brief IpcMsgProc::popUpdateArg 升级参数队列，删除队头
 */
void IpcMsgProc::popUpdateArg()
{
    QMutexLocker locker(&m_updateArgQueMutex);
    if (!m_updateArgQue.empty()) {
        m_updateArgQue.pop();
    }
}


/**
 * @brief IpcMsgProc::setFlagUpdateArg 升级参数队列，设置能否取出参数，如果true，先删除队头参数
 * @param flag
 */
void IpcMsgProc::setFlagUpdateArg(bool flag)
{
    QMutexLocker locker(&m_updateArgQueMutex);
    if (flag) {
        if (!m_updateArgQue.empty()) {
            m_updateArgQue.pop();
        }
    }
    m_flagUpdateArg = flag;
}


/**
 * @brief IpcMsgProc::isUpdateArgQueEmpty 升级参数队列，是否位空
 * @return
 */
bool IpcMsgProc::isUpdateArgQueEmpty()
{
    return m_updateArgQue.empty();
}





/**
 * @brief IpcMsgProc::pushStakeArg 桩参数队列，添加到队尾
 * @param arg
 */
void IpcMsgProc::pushStakeArg(StakeArg arg)
{
    QMutexLocker locker(&m_stakeArgQueMutex);
    m_stakeArgQue.push(arg);

}


/**
 * @brief IpcMsgProc::getStakeArg 桩参数队列，取出队头参数
 * @param arg
 * @return
 */
bool IpcMsgProc::getStakeArg(StakeArg &arg)
{
    bool ret = false;
    QMutexLocker locker(&m_stakeArgQueMutex);

    if (!m_stakeArgQue.empty()) {
        arg = m_stakeArgQue.front();
        ret = true;
    }
    return ret;
}


/**
 * @brief IpcMsgProc::popStakeArg 桩参数队列，删除队头参数
 */
void IpcMsgProc::popStakeArg()
{
    QMutexLocker locker(&m_stakeArgQueMutex);
    if (!m_stakeArgQue.empty()) {
        m_stakeArgQue.pop();
    }
}

/**
 * @brief IpcMsgProc::setFlagStakeArg 桩参数队列，设置能否取出参数，如果flag为true，先删除队头参数
 * @param flag
 */
void IpcMsgProc::setFlagStakeArg(bool flag)
{
    QMutexLocker locker(&m_stakeArgQueMutex);
    if (flag) {
        if (!m_stakeArgQue.empty()) {
            m_stakeArgQue.pop();
        }
    }
    m_flagStakeArg = flag;
}


/**
 * @brief IpcMsgProc::isStakeArgQueEmpty 桩参数队列，是否为空
 * @return
 */
bool IpcMsgProc::isStakeArgQueEmpty()
{
    return m_stakeArgQue.empty();
}


/**
 * @brief IpcMsgProc::stakeArgQueSize 桩参数队列，大小
 * @return
 */
int IpcMsgProc::stakeArgQueSize()
{
    return m_stakeArgQue.size();
}




/**
 * @brief IpcMsgProc::pushUpdateApp 升级APP队列，添加到队尾
 * @param who
 */
void IpcMsgProc::pushUpdateApp(WHICH_APP who)
{
    QMutexLocker locker(&m_updateAppQueMutex);
    m_updateAppQue.push(who);
}


/**
 * @brief IpcMsgProc::getUpdateApp 升级APP队列，取出队头参数
 * @param who
 * @return
 */
bool IpcMsgProc::getUpdateApp(WHICH_APP &who)
{
    bool ret = false;
    QMutexLocker locker(&m_updateAppQueMutex);

    if (!m_updateAppQue.empty()) {
        who = m_updateAppQue.top();
        ret = true;
    }
    return ret;
}


/**
 * @brief IpcMsgProc::popUpdateApp 升级APP队列，删除队头参数
 */
void IpcMsgProc::popUpdateApp()
{
    QMutexLocker locker(&m_updateAppQueMutex);
    if (!m_updateAppQue.empty()) {
        m_updateAppQue.pop();
    }
}


/**
 * @brief IpcMsgProc::setFlagUpdateApp 升级APP队列，设置能否取出参数，如果可以，先删除队头参数
 * @param flag
 */
void IpcMsgProc::setFlagUpdateApp(bool flag)
{
    QMutexLocker locker(&m_updateAppQueMutex);
    if (flag) {
        if (!m_updateAppQue.empty()) {
            m_updateAppQue.pop();
        }
    }
    m_flagUpdateApp = flag;
}


/**
 * @brief IpcMsgProc::clearUpdateApp 升级APP队列，清空
 */
void IpcMsgProc::clearUpdateApp()
{
    QMutexLocker locker(&m_updateAppQueMutex);
    while (!m_updateAppQue.empty()) {
        m_updateAppQue.pop();
    }
}


/**
 * @brief IpcMsgProc::isUpdateAppQueEmpty 升级APP队列，是否为空
 * @return
 */
bool IpcMsgProc::isUpdateAppQueEmpty()
{
    return m_updateAppQue.empty();
}




/**
 * @brief IpcMsgProc::setUpdateRet 设置who升级结果，如果当前桩的所有APP都升级结束，则发送升级结果
 * @param who
 * @param flag -1-未生级 0-升级失败 1-升级成功 这只能设置0或1
 */
void IpcMsgProc::setUpdateRet(WHICH_APP who, Int8 flag)
{
    if (!((flag==UPDATE_RET_FAILED) || (flag==UPDATE_RET_SUCCESS))) {
        qDebug("%s wrong flag -1-未生级 0-升级失败 1-升级成功 只能为0或1",__FUNCTION__);
        return;
    }

    QString str_log;

    m_updateRet.updateRet[who] = flag;

    bool ret = true;
    bool fail = false;

    for (int i=0; i<APP_NUM; i++) {
        if (m_updateRet.updateRet[i] == UPDATE_RET_INIT) {
            ret = false;
            break;
        } else if (m_updateRet.updateRet[i] == UPDATE_RET_FAILED) {
            fail = true;
        }
    }

    if (m_updateRet.stakeAddr != m_stakeArg.stakeAddr) {
        str_log = "当前结果中的桩地址不等于当前桩参数中的桩地址";
        Logger::Log::Instance().logError(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

    }
    if (strncmp(m_updateRet.stakeCode,m_stakeArg.updateArg.stakeCode, STAKE_CODE_LEN) != 0) {
        str_log = "当前结果中的桩编码不等于当前桩参数中的桩编码";
        Logger::Log::Instance().logError(str_log);
#if REMOTE_DEBUG
        this->uploadDebugLogInfo(str_log);
#endif

    }

#if 0
    /*桩编码为BCD编码，显示时转为str*/
    char buf1[2*STAKE_CODE_LEN+1];
    char buf2[2*STAKE_CODE_LEN+1];
    Util::bcd2str((const unsigned char*)m_updateRet.stakeCode, STAKE_CODE_LEN, buf1, 2*STAKE_CODE_LEN);
    Util::bcd2str((const unsigned char*)m_stakeArg.updateArg.stakeCode, STAKE_CODE_LEN, buf2, 2*STAKE_CODE_LEN);
    qDebug()<< "buf1: " << buf1;
    qDebug()<< "buf2: " << buf2;
#endif


    if (ret == true) { /*升级所有APP完成*/

        if (fail == true) { /*有APP升级失败*/
            /*发送升级结果*/
            this->sendUpdateRet(UPDATE_RET_FAILED);

        } else { /*所有APP升级成功*/
            /*发送升级结果*/
            this->sendUpdateRet(UPDATE_RET_SUCCESS);
        }

        /*设置继续从桩参数队列读取参数，而且删除队列头*/
        this->setFlagStakeArg(true);
    }

}


/**
 * @brief IpcMsgProc::sendUpdateRet 发送升级结果
 * @param flag 0-升级失败 1-升级成功
 */
void IpcMsgProc::sendUpdateRet(UInt8 flag)
{
    if (!((flag==UPDATE_RET_FAILED) || (flag==UPDATE_RET_SUCCESS))) {
        qDebug("%s wrong flag 0-升级失败 1-升级成功",__FUNCTION__) ;
        return;
    }

    QString str_log;
    IpcData ipcData;
    packIpcHead(ipcData,0,0,0);

    if (UFLASH_FLAG) { /*U盘升级*/

        /*发送升级结果到配置管理*/
        StakeInfo stakeInfo;
        if (flag == UPDATE_RET_FAILED) { /*0-升级失败*/
            stakeInfo.cmd = UFLASH_UPGRADE_FAIL;
        } else { /*1-升级成功*/
            stakeInfo.cmd = UFLASH_UPGRADE_SUCCESS;
        }

        stakeInfo.data1size = 16;
        Util::bcd2str((unsigned char*)m_stakeArg.updateArg.stakeCode,STAKE_CODE_LEN,stakeInfo.data1,2*STAKE_CODE_LEN);
        //strncpy(stakeInfo.data1, m_updateRet.stakeCode,stakeInfo.data1size);

        ipcData.commond = CMD_UFLASH_UPDATE;
        ipcData.who = WHICH_CONFIG_APP;

        ipcData.len = sizeof(stakeInfo);
        ipcData.buffer.clear();
        ipcData.buffer.append((char*)&stakeInfo, ipcData.len);

    } else { /*运营平台升级*/

        /*发送升级结果到后台网关*/
        FinalUpdateRet finalUpdateRet;
        strncpy(finalUpdateRet.stakeCode, m_stakeArg.updateArg.stakeCode,sizeof(m_updateRet.stakeCode));
        finalUpdateRet.ret = flag;

        ipcData.commond = CMD_SEND_UPDATE_RESULT;
        ipcData.who = WHICH_GATEWAY_APP;

        ipcData.len = sizeof(finalUpdateRet);
        ipcData.buffer.clear();
        ipcData.buffer.append((char*)&finalUpdateRet, ipcData.len);

    }

    m_IpcStack->Write(ipcData);
#if REMOTE_DEBUG
    QString strHead = QString("UDP发送数据 cmd=%1 len=%2").arg(ipcData.commond,2,16).arg(ipcData.len);
    this->uploadDebugBuffer(strHead,ipcData.buffer.data(),ipcData.len, DEBUG_ARG_SEND_UDP);
#endif


    /*桩编码为BCD编码，显示时转为str*/
    char buf[2*STAKE_CODE_LEN+1];
    Util::bcd2str((const unsigned char*)m_stakeArg.updateArg.stakeCode, STAKE_CODE_LEN, buf, 2*STAKE_CODE_LEN);

    if (UFLASH_FLAG) {
        if (flag == UPDATE_RET_SUCCESS) {
            str_log = QString("The stake (%1) uflash-update over，success").arg(buf);
            Logger::Log::Instance().logInfo(str_log);
        } else {
            str_log = QString("The stake (%1) uflash-update over，failed").arg(buf);
            Logger::Log::Instance().logError(str_log);
        }
    } else {
        if (flag == UPDATE_RET_SUCCESS) {
            str_log = QString("The stake (%1) update over，success").arg(buf);
            Logger::Log::Instance().logInfo(str_log);
        } else {
            str_log = QString("The stake (%1) update over，failed").arg(buf);
            Logger::Log::Instance().logError(str_log);
        }
    }

#if REMOTE_DEBUG
    this->uploadDebugLogInfo(str_log);
#endif

}


void IpcMsgProc::chargeCtrlResetTimeout()
{
    QString str_log;

    if (UFLASH_FLAG) { /*U盘升级*/
        str_log = QString("App %1 reply time out").arg(Util::toAppName(WHICH_CHARGE_CONTROL_APP));
        Logger::Log::Instance().logError(str_log);

        /*标记为升级失败*/
        this->setUpdateRet(WHICH_CHARGE_CONTROL_APP, UPDATE_RET_FAILED);
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        this->setFlagUpdateApp(true);

    } else { /*运营平台升级*/
        str_log = QString("App %1 reply time out, wait once more ...").arg(Util::toAppName(WHICH_CHARGE_CONTROL_APP));
        Logger::Log::Instance().logError(str_log);

        /*休眠10秒钟*/
        Util::delayMs(10000);

        /*清空升级APP队列*/
        this->clearUpdateApp();
        /*设置继续从升级APP队列读取参数，而且删除队列头*/
        this->setFlagUpdateApp(true);

        /*将当前桩参数加入队列尾*/
        this->pushStakeArg(m_stakeArg);
        /*设置继续从桩参数队列读取参数，而且删除队列头*/
        this->setFlagStakeArg(true);
    }

#if REMOTE_DEBUG
    this->uploadDebugLogInfo(str_log);
#endif

}

/**
 * @brief cmdAckTimeoutProc 命令应答超时处理
 * @param ipcData
 */
void cmdAckTimeoutProc(IpcData &ipcData)
{
    //qDebug("%s 应答超时处理 cmd:%d",__FUNCTION__ ,ipcData.commond);

    QString str_log;
    IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();

    switch (ipcData.commond) {

    case CMD_QUERY_VERSION:
        str_log = QString("APP %1 reply version timeout")
                .arg(Util::toAppName(ipcData.who));
        Logger::Log::Instance().logError(str_log);
        ipcMsgProc->setUpdateRet(ipcData.who, UPDATE_RET_FAILED);
        ipcMsgProc->setFlagUpdateApp(true);
        break;

    case CMD_QUERY_RUN_STATUS:
        str_log = QString("APP %1 reply run status timeout")
                .arg(Util::toAppName(ipcData.who));
        Logger::Log::Instance().logError(str_log);
        ipcMsgProc->setUpdateRet(ipcData.who, UPDATE_RET_FAILED);
        ipcMsgProc->setFlagUpdateApp(true);
        break;

    case CMD_QUERY_IP:
        str_log = QString("APP %1 reply ip timeout")
                .arg(Util::toAppName(ipcData.who));
        Logger::Log::Instance().logError(str_log);
        break;

    default:
        qDebug("%s 未知的超时命令: %d",__FUNCTION__,ipcData.commond);
        break;
    }

#if REMOTE_DEBUG
        ipcMsgProc->uploadDebugLogInfo(str_log);
#endif

}


/**
 * @brief cmdSendType 发送命令的类型
 * @param cmd
 * @return
 */
int cmdSendType(int cmd)
{
    int ret = -1;

    switch (cmd) {
    case CMD_QUERY_VERSION:
        ret = CMD_TYPE_REQ_ACK;         /*请求应答型*/
        break;
    case CMD_REPLY_VERSION:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_REPLY_UPDATE_ARG:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_QUERY_RUN_STATUS:
        ret = CMD_TYPE_REQ_ACK;         /*请求应答型*/
        break;
    case CMD_SEND_HEART:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_UFLASH_UPDATE:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_SEND_UPDATE_RESULT:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_QUERY_IP:
        ret = CMD_TYPE_REQ_ACK;         /*请求应答型*/
        break;
    case CMD_SET_DEBUG_SWITCH_ACK:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_GET_DEBUG_SWITCH_ACK:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_SEND_DEBUG_INFO:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_SET_DEBUG_ARG_ACK:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_GET_DEBUG_ARG_ACK:
        ret = CMD_TYPE_NOTIFY;
        break;
    default:
        qDebug()<< "未知的cmd命令，错误！！！";
        break;
    }

    return ret;
}


/**
 * @brief cmdRecvType 接收命令的类型
 * @param cmd
 * @return
 */
int cmdRecvType(int cmd)
{
    int ret = -1;

    switch (cmd) {
    case CMD_QUERY_VERSION:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_REPLY_VERSION:
        ret = CMD_TYPE_REQ_ACK;         /*请求应答型*/
        break;
    case CMD_SEND_UPDATE_ARG:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_REPLY_RUN_STATUS:
        ret = CMD_TYPE_REQ_ACK;         /*请求应答型*/
        break;
    case CMD_UFLASH_UPDATE:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_REPLY_IP:
        ret = CMD_TYPE_REQ_ACK;         /*请求应答型*/
        break;
    case CMD_SET_DEBUG_SWITCH_REQ:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_GET_DEBUG_SWITCH_REQ:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_SET_DEBUG_ARG_REQ:
        ret = CMD_TYPE_NOTIFY;
        break;
    case CMD_GET_DEBUG_ARG_REQ:
        ret = CMD_TYPE_NOTIFY;
        break;

    default:
        qDebug()<< "未知的cmd命令，错误！！！";
        break;
    }

    return ret;
}


bool getPairedCMD(UInt16 cmd_ack, UInt16 &cmd)
{
    bool ret = false;

    switch(cmd_ack) {

    case CMD_REPLY_VERSION:
        cmd = CMD_QUERY_VERSION;
        ret = true;
        break;

    case CMD_REPLY_RUN_STATUS:
        cmd = CMD_QUERY_RUN_STATUS;
        ret = true;
        break;

    case CMD_REPLY_IP:
        cmd = CMD_QUERY_IP;
        ret = true;
        break;

    default:
        ret = false;
        break;
    }

    return ret;
}


