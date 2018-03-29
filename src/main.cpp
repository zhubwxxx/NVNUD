
#include "Comm/util.h"
#include "Config/sysconfig.h"
#include "App/canmsgproc.h"
#include "App/ipcmsgproc.h"
#include "Lib/Tcp/tcpserver.h"
#include "Lib/Tcp/tcpclientsocket.h"

#include <QCoreApplication>
#include <qtextcodec.h>
#include <qmetatype.h>


int main(int argc, char *argv[])
{

    QCoreApplication a(argc, argv);

    /*是否打开调试模式*/
    char buf[] = "debug";
    for (int i=1; i<argc; i++) {
        if (strncmp(argv[i],buf, sizeof(buf)) == 0) {
            DEBUG_FLAG = true;
        } else {
            DEBUG_FLAG = false;
        }
    }

    Logger::Log::Instance().logInfo("##################################################");
    Logger::Log::Instance().logInfo(QString("# VERSION = V%1.%2.%3.%4").arg(APP_VERSION[0]).arg(APP_VERSION[1]).arg(APP_VERSION[2]).arg(APP_VERSION[3]));
    Logger::Log::Instance().logInfo(QString("# RUNNING_TIME= %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    Logger::Log::Instance().logInfo(QString("# COMPILE_TIME= %1 %2").arg(__DATE__).arg(__TIME__));
    Logger::Log::Instance().logInfo("##################################################");

    /*设置编码*/
    QTextCodec *codec = QTextCodec::codecForName(QString("utf-8").toUtf8().data());
    QTextCodec::setCodecForCStrings(codec);
    QTextCodec::setCodecForLocale(codec);
    QTextCodec::setCodecForTr(codec);

    /*注册信号类型*/
    qRegisterMetaType<WHICH_APP>("WHICH_APP");
    qRegisterMetaType<UInt32>("UInt32");

    /*设置程序文件所在目录为工作目录*/
    QDir::setCurrent(a.applicationDirPath());
    //qDebug()<<"程序工作目录：" << QDir::currentPath();
    //qDebug()<<"程序所在目录：" << a.applicationDirPath();

    /*配置信息*/
    SysConfig sysConfig;

    /*Ipc消息处理*/
    IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();

    /*Can消息处理*/
    CanMsgProc *canMsgProc = CanMsgProc::Instance();

    /*Tcp服务端*/
    TcpServer *tcpServer = TcpServer::Instance();

    /*Tcp客户端*/
    TcpClientSocket *tcpClientSocket = TcpClientSocket::Instance();

    if (SERVER == 1) { /*在线升级服务端*/
        Logger::Log::Instance().logInfo("NVNUD Server launch ... ");
        tcpServer->launch();
    } else { /*在线升级客户端*/
        Logger::Log::Instance().logInfo("NVNUD Client launch ... ");
        tcpClientSocket->launch();
    }

    a.exec();

    /*消息撤回*/

    return 0;
}
