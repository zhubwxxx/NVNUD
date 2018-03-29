#include "tcpclientsocket.h"

#include <Log/log.h>
#include <Config/appinfo.h>
#include "App/ipcmsgproc.h"

#include <qhostaddress.h>
#include <qfileinfo.h>
#include <qsettings.h>
#include <qabstractsocket.h>


TcpClientSocket *TcpClientSocket::m_instance = 0;

TcpClientSocket::TcpClientSocket(QObject *parent) : QObject(parent)
{
    m_pClientSocket = new QTcpSocket;
    connect(m_pClientSocket, SIGNAL(readyRead()), this, SLOT(on_readyRead()));
    connect(m_pClientSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(on_error(QAbstractSocket::SocketError)));
    connect(m_pClientSocket, SIGNAL(disconnected()),this , SLOT(on_disconnected()));

    m_pTimer = new QTimer;
    m_pTimer->setInterval(100000);
    connect(m_pTimer, SIGNAL(timeout()), this, SLOT(on_timeout()));

    /*连接在线升级服务端标志*/
    m_connectedOk = false;
}

void TcpClientSocket::launch()
{
    m_pTimer->start();
}

void TcpClientSocket::connectServer(QHostAddress addr)
{
    m_pClientSocket->connectToHost(addr,TCP_PORT);
    if (m_pClientSocket->waitForConnected(1000)) { /*连接在线升级服务端成功*/
        m_connectedOk = true;
        Logger::Log::Instance().logInfo(QString("Connect to server %1 OK").arg(addr.toString()));
    } else {
        Logger::Log::Instance().logError(QString("Connect to server %1 error: %2")
                                         .arg(addr.toString())
                                         .arg(m_pClientSocket->errorString()));
    }
}

void TcpClientSocket::sendData(QByteArray ba)
{
    if (m_connectedOk) {
        if (m_pClientSocket->isWritable()) {
            m_pClientSocket->write(ba);
            m_pClientSocket->waitForBytesWritten(300);
        }
    }
}

void TcpClientSocket::on_readyRead()
{

    QByteArray data = m_pClientSocket->readAll();
    if (data.size() == sizeof(StakeArg)) {
        StakeArg stakeArg;
        memcpy(&stakeArg, data.constData(),data.size());
        IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();
        ipcMsgProc->pushStakeArg(stakeArg);
        //qDebug()<<__FUNCTION__ << ipcStack->stakeArgQueSize();

    } else {
        Logger::Log::Instance().logError("The Data recv from server is wrong");
    }
}

void TcpClientSocket::on_error(QAbstractSocket::SocketError)
{
    m_connectedOk = false;
    Logger::Log::Instance().logError(QString("Connect to server %1 error: %2")
                                     .arg(m_pClientSocket->peerAddress().toString())
                                     .arg(m_pClientSocket->errorString()));
}

void TcpClientSocket::on_timeout()
{
    if (m_connectedOk) { /*连接成功，查询在线升级服务端升级参数*/
        QByteArray ba;
        ba.clear();
        ba.append(TCP_HEAD);
        if (m_pClientSocket->isValid()) {
            this->sendData(ba);
        }
    } else { /*无效*/
        /*通过充电操作软件查询集中计费端IP*/
        IpcMsgProc *ipcMsgProc = IpcMsgProc::Instance();
        ipcMsgProc->queryIp();
    }
}

void TcpClientSocket::on_disconnected()
{
    m_connectedOk = false;
    Logger::Log::Instance().logError(QString("Server %1 disconnect").arg(m_pClientSocket->peerAddress().toString()));
}
