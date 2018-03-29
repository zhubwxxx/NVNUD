#ifndef TCPSERVERSOCKET_H
#define TCPSERVERSOCKET_H

#include <qtcpsocket.h>

class TcpServerSocket : public QTcpSocket
{
    Q_OBJECT
public:
    explicit TcpServerSocket(QObject *parent = 0);

signals:
    void sig_readyRead(QString IP, QByteArray data);
    void sig_disconnected(QString IP);

private slots:
    void on_readyRead();
    void on_disconnected();

};

#endif // TCPSERVERSOCKET_H
