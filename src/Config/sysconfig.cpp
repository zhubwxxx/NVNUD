#include "sysconfig.h"

#include <qfileinfo.h>
#include <qsettings.h>
#include <qstringlist.h>
#include <qdebug.h>
#include <qdir.h>
#include <qtextcodec.h>


SysConfig::SysConfig()
{

    this->readSysCfg();

}

SysConfig::~SysConfig()
{

}

void SysConfig::readSysCfg()
{

    //从配置文件读取程序名，工作目录,升级摘要文件名
    QFileInfo info;
    info.setFile(UpdateCfg);
    if (!info.exists()) {
        QString str_log = QString("File %1 not exist, can do nothing, exit ...").arg(UpdateCfg);
        Logger::Log::Instance().logError(str_log);
        exit(-1);
    }
    QSettings ini(UpdateCfg,QSettings::IniFormat);
    ini.setIniCodec(QTextCodec::codecForName(QString("gbk").toUtf8()));
    SERVER = ini.value("Which/server").toInt();
    if (!(SERVER==0 || SERVER==1)) {
        QString str_log = QString("In file %1, the value of 'Which/server' must be 0 or 1, exit ...").arg(UpdateCfg);
        Logger::Log::Instance().logError(str_log);
        exit(-1);
    }

    QStringList valueList;
    bool ret = true;

    if (SERVER == 1) {

        valueList                 = ini.value("Server/update").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            UpdateName                = valueList.at(0);
            UpdateDir                 = valueList.at(1);
        } else {
            ret = false;
        }

        valueList                 = ini.value("Server/ccu").toStringList();
        if ((valueList.size()==1) && (!valueList[0].isEmpty()) ) {
            ChargeCtrlName            = valueList.at(0);
            ChargeCtrlDir         = valueList.at(0);
        } else {
            ret = false;
        }

        valueList                 = ini.value("Server/daemon(10034)").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            DaemonName                = valueList.at(0);
            DaemonDir                 = valueList.at(1);
        } else {
            ret = false;
        }

        valueList                 = ini.value("Server/app(10000-10001)").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            GatewayName               = valueList.at(0);
            GatewayDir                = valueList.at(1);
        } else {
            ret = false;
        }

        valueList                 = ini.value("Server/app(10002-10003)").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            DebugName                 = valueList.at(0);
            DebugDir                  = valueList.at(1);
        } else {
            ret = false;
        }

        valueList                 = ini.value("Server/app(10004-10005)").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            BillingCtrlName           = valueList.at(0);
            BillingCtrlDir            = valueList.at(1);
        } else {
            ret = false;
        }


        valueList                 = ini.value("Server/app(10006-10007)").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            MatrixCtrlName            = valueList.at(0);
            MatrixCtrlDir             = valueList.at(1);
        } else {
            ret = false;
        }

        valueList                 = ini.value("Server/app(10008-10009)").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            ChargeOperateName         = valueList.at(0);
            ChargeOperateDir          = valueList.at(1);
        } else {
            ret = false;
        }

        valueList                 = ini.value("Server/app(10012-10013)").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            ConfigName                = valueList.at(0);
            ConfigDir                 = valueList.at(1);
        } else {
            ret = false;
        }


        valueList                 = ini.value("Ftp/user").toStringList();
        if ((valueList.size()==1) && (!valueList[0].isEmpty()) ) {
            FtpUser               = valueList.at(0); /*FTP服务器用户名*/
        } else {
            ret = false;
        }
        valueList                 = ini.value("Ftp/password").toStringList();
        if ((valueList.size()==1) && (!valueList[0].isEmpty()) ) {
            FtpPass               = valueList.at(0); /*FTP服务器密码*/
        } else {
            ret = false;
        }
        FtpPath = QDir::currentPath() +'/'+ TempDir; /*FTP服务器路径*/


    } else {

        valueList                 = ini.value("Client/update").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            UpdateName                = valueList.at(0);
            UpdateDir                 = valueList.at(1);
        } else {
            ret = false;
        }

        valueList                 = ini.value("Client/daemon(10034)").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            DaemonName                = valueList.at(0);
            DaemonDir                 = valueList.at(1);
        } else {
            ret = false;
        }

        valueList                 = ini.value("Client/app(10008-10009)").toStringList();
        if ((valueList.size()==2) && (!valueList[0].isEmpty()) && (!valueList[1].isEmpty()) ) {
            ChargeOperateName         = valueList.at(0);
            ChargeOperateDir          = valueList.at(1);
        } else {
            ret = false;
        }

    }

    valueList                 = ini.value("UpdatePackageName/filename").toStringList();
    if ((valueList.size()==1) && (!valueList[0].isEmpty()) ) {
        UpdatePackage               = valueList.at(0); //升级包文件名
    } else {
        ret = false;
    }

    valueList                 = ini.value("UpdateSummaryFileName/filename").toStringList();
    if ((valueList.size()==1) && (!valueList[0].isEmpty()) ) {
        UpdateSumary               = valueList.at(0); //升级摘要文件名
    } else {
        ret = false;
    }

    if (!ret) {
        QString str_log = QString("In file %1, every item's size must be 2 which belong Server or Client, exit ...").arg(UpdateCfg);
        Logger::Log::Instance().logError(str_log);
        exit(-1);
    }


}

