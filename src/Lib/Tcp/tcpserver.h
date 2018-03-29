#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <qtcpserver.h>
#include "tcpserversocket.h"
#include <Comm/comm.h>

#include <list>
using namespace std;

class TcpServer : public QTcpServer
{
    Q_OBJECT
public:

    static TcpServer *Instance()
    {
        static QMutex Mutex;
        if (!m_instance) {
            QMutexLocker locker(&Mutex);
            if (!m_instance) {
                m_instance = new TcpServer;
            }
        }
        return m_instance;
    }

    void launch();
    void sendData(QString IP, QByteArray data);

public slots:
    void on_readData(QString IP, QByteArray data);
    void on_disconnected(QString IP);

protected:
    void incomingConnection(int socketDescriptor);

private:
    explicit TcpServer(QObject *parent = 0);


    static TcpServer *m_instance;

    QList<TcpServerSocket*> m_serverSocketList;


};

#endif // TCPSERVER_H
