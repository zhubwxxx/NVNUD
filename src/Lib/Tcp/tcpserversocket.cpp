#include "tcpserversocket.h"
#include <qhostaddress.h>



TcpServerSocket::TcpServerSocket(QObject *parent) : QTcpSocket(parent)
{
    connect(this, SIGNAL(readyRead()), this, SLOT(on_readyRead()));
    connect(this, SIGNAL(disconnected()), this, SLOT(on_disconnected()));

}

void TcpServerSocket::on_readyRead()
{
    QByteArray data = this->readAll();
    emit sig_readyRead(this->peerAddress().toString(),data);
}


void TcpServerSocket::on_disconnected()
{
    emit sig_disconnected(this->peerAddress().toString());
}
