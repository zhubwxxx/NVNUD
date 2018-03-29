#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "Config/appinfo.h"


#include <qurl.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include <qmutex.h>
#include <QTimerEvent>
#include <qtimer.h>
#include <qfile.h>
#include <qftp.h>


class Transport : public QObject
{
    Q_OBJECT
public:
    explicit Transport(QObject *parent = 0);
    ~Transport();

    int getFile(const QUrl &url);

signals:
    void done(bool flag);

private slots:
    void replyFinshed(QNetworkReply *reply);
    void timerEvent(QTimerEvent *e);

    void on_done(bool error);
    void on_stateChanged(int state);
    void on_timeout();


private:
    //QNetworkAccessManager *manager;
    QFtp *m_pFtp;
    QFile file;
    bool m_flag;
    QTimer *m_pTimer;

};

#endif // TRANSPORT_H
