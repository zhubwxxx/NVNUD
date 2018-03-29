#include "tcpserver.h"

#include <QFileInfo>
#include <qsettings.h>

#include "Comm/util.h"
#include "Lib/Ipc/ipcstack.h"


TcpServer *TcpServer::m_instance = 0;

TcpServer::TcpServer(QObject *parent) : QTcpServer(parent)
{

}

void TcpServer::launch()
{
    this->listen(QHostAddress::Any, TCP_PORT);
}

void TcpServer::incomingConnection(int socketDescriptor)
{
    TcpServerSocket *socket = new TcpServerSocket(this);
    socket->setSocketDescriptor(socketDescriptor);

    connect(socket,SIGNAL(sig_readyRead(QString,QByteArray)),this,SLOT(on_readData(QString,QByteArray)));
    connect(socket,SIGNAL(sig_disconnected(QString)),this,SLOT(on_disconnected(QString)));

    m_serverSocketList.append(socket);
    Logger::Log::Instance().logInfo(QString("The update client %1 connected").arg(socket->peerAddress().toString()));

}

void TcpServer::sendData(QString IP, QByteArray data)
{
    for (int i=0; i<m_serverSocketList.size(); i++) {
        if (m_serverSocketList[i]->peerAddress().toString() == IP) {
            m_serverSocketList[i]->write(data);
            m_serverSocketList[i]->waitForBytesWritten(300);
            break;
        }
    }
}

void TcpServer::on_readData(QString IP, QByteArray data)
{
    Int32 head = 0;
    memcpy(&head, data.constData(), data.size());
    //qDebug("%s ip:%s head:%d",__FUNCTION__,IP.toUtf8().data(),head);

    if(head == TCP_HEAD) {
        QFileInfo info(Md5Cfg);
        if (info.exists()) { /*本地md5.ini文件存在*/
            QSettings ini(Md5Cfg,QSettings::IniFormat);
            QString md5 = ini.value("MD5/md5").toString();
            StakeArg stakeArg;
            strncpy(stakeArg.updateArg.MD5, md5.toUtf8().data(), sizeof(stakeArg.updateArg.MD5));
            stakeArg.stakeAddr = 0; /*桩地址，客户端升级时无用*/

            stakeArg.updateArg.lenUser = FtpUser.length();
            stakeArg.updateArg.lenPassword = FtpPass.length();
            stakeArg.updateArg.lenPath = FtpPath.length();
            strncpy(stakeArg.updateArg.user,FtpUser.toUtf8().data(),FtpUser.length());
            strncpy(stakeArg.updateArg.password,FtpPass.toUtf8().data(),FtpPass.length());
            strncpy(stakeArg.updateArg.path,FtpPath.toUtf8().data(),FtpPath.length());
            stakeArg.updateArg.user[stakeArg.updateArg.lenUser] = 0;
            stakeArg.updateArg.password[stakeArg.updateArg.lenPassword] = 0;
            stakeArg.updateArg.path[stakeArg.updateArg.lenPath] = 0;
            char stakeCode[] = "0000000000000001"; /*此处桩编码只是占位*/
            Util::str2bcd(stakeCode,2*STAKE_CODE_LEN,(unsigned char*)stakeArg.updateArg.stakeCode,STAKE_CODE_LEN);
            stakeArg.updateArg.port = 21;

            QNetworkAddressEntry eth0;
            if (Util::getEth0Entry(eth0)) { /*得到设备eth0 的ip地址*/
                stakeArg.updateArg.ip[0] = eth0.ip().toIPv4Address()>>24;
                stakeArg.updateArg.ip[1] = eth0.ip().toIPv4Address()>>16;
                stakeArg.updateArg.ip[2] = eth0.ip().toIPv4Address()>>8;
                stakeArg.updateArg.ip[3] = eth0.ip().toIPv4Address()&0xff;

                QByteArray ba;
                ba.clear();
                ba.append((char*)&stakeArg, sizeof(stakeArg));

                this->sendData(IP, ba);
            }

        }

    }

}

void TcpServer::on_disconnected(QString IP)
{
    for (int i=0; i<m_serverSocketList.size(); i++) {
        if (m_serverSocketList[i]->peerAddress().toString() == IP) {
            m_serverSocketList.removeAt(i);
            //m_serverSocketList[i]->deleteLater();
            Logger::Log::Instance().logError(QString("The update client %1 disconnected").arg(IP));
            break;
        }
    }
}
