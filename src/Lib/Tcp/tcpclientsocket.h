#ifndef TCPCLIENTSOCKET_H
#define TCPCLIENTSOCKET_H

#include <QObject>
#include <qtcpsocket.h>
#include <qtimer.h>

#include <Comm/comm.h>

class TcpClientSocket : public QObject
{
    Q_OBJECT

public:

    static TcpClientSocket *Instance()
    {
        static QMutex Mutex;
        if (!m_instance) {
            QMutexLocker locker(&Mutex);
            if (!m_instance) {
                m_instance = new TcpClientSocket;
            }
        }
        return m_instance;
    }

    void launch();
    void connectServer(QHostAddress addr);

public slots:
    void on_readyRead();
    void on_error(QAbstractSocket::SocketError);
    void on_timeout();
    void on_disconnected();

private:
    explicit TcpClientSocket(QObject *parent = 0);

    void sendData(QByteArray ba);

private:

    static TcpClientSocket *m_instance;

    QTcpSocket *m_pClientSocket;
    QTimer *m_pTimer;
    bool m_connectedOk;
};

#endif // TCPCLIENTSOCKET_H
