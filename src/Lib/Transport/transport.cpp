#include "transport.h"


#include "Lib/Ipc/ipcstack.h"


#include <qdir.h>
#include <qdebug.h>


Transport::Transport(QObject *parent) : QObject(parent)
{
   //manager = new QNetworkAccessManager(this);
   //connect(manager,SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinshed(QNetworkReply*)));

   m_pFtp = 0;

   m_pTimer = new QTimer(this);
   m_pTimer->setInterval(5000);
   connect(m_pTimer, SIGNAL(timeout()), this, SLOT(on_timeout()));
}

Transport::~Transport()
{ 
    disconnect(m_pTimer, SIGNAL(timeout()), this, SLOT(on_timeout()));

    delete m_pTimer;
}


void Transport::replyFinshed(QNetworkReply *reply)
{   
    //qDebug()<<__FUNCTION__;

    QString str_log;
    QDir dir;
    dir.setPath(TempDir);
    if (!dir.exists()) {
        dir.mkdir(dir.absolutePath());
    }

    QString fileName = QFileInfo(reply->url().path()).fileName();
    QString filePath = TempDir + "/" + fileName;

    if (!reply->error()) {
        QFile file;
        file.setFileName(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            str_log = QString("Open file %1 failed").arg(filePath);
            Logger::Log::Instance().logError(str_log);
            emit done(false);
        } else { /*将数据写入文件中*/
            char buf[4096];
            Int64 maxLen = 4096;
            Int64 len;
            do {
                len = reply->read(buf, maxLen);
                if (len > 0) {
                    file.write(buf, len);
                }
            }while(len == maxLen);

            file.close();
            emit done(true);
        }
    } else {
        str_log = QString("Download file %1 error, QNetworkReply::NetworkError: %2").arg(fileName).arg(QString::number(reply->error(), 10));
        Logger::Log::Instance().logError(str_log);    
        emit done(false);
    }
    reply->deleteLater();
}


void Transport::timerEvent(QTimerEvent *e)
{
    e = e;
}



int Transport::getFile(const QUrl &url)
{
    QString str_log;

    if (!url.isValid()) {
        str_log = "invailed URL";
        Logger::Log::Instance().logError(str_log);
        return -1;
    }
    if (url.scheme() != "ftp") {
        str_log = "URL's schemeis is not'ftp'";
        Logger::Log::Instance().logError(str_log);
        return -2;
    }
    if (url.path().isEmpty()) {
        str_log = "URL's path is empty";
        Logger::Log::Instance().logError(str_log);
        return -3;
    }



    QDir dir;
    dir.setPath(TempDir);
    if (!dir.exists()) {
        dir.mkdir(dir.absolutePath());
    }

    QString fileName = QFileInfo(url.path()).fileName();
    QString filePath = TempDir + "/" + fileName;

    file.setFileName(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        str_log = QString("Open file %1 failed").arg(filePath);
        Logger::Log::Instance().logError(str_log);
        return -4;

    } else {
        if (m_pFtp) {
            m_pFtp->abort();
            m_pFtp->deleteLater();
            m_pFtp = 0;
        }
        m_pFtp = new QFtp(this);
        connect(m_pFtp, SIGNAL(stateChanged(int)), this, SLOT(on_stateChanged(int)));
        connect(m_pFtp, SIGNAL(done(bool)), this, SLOT(on_done(bool)));

        m_pFtp->setTransferMode(QFtp::Active);
        m_pFtp->connectToHost(url.host());
        m_pFtp->login(url.userName(),url.password());
        m_pFtp->get(url.path(), &file);
        m_pFtp->close();
        m_flag = true;
    }

    return 1;
}



void Transport::on_done(bool error)
{
    //qDebug()<<__FUNCTION__ << error << m_flag;

    if (file.isOpen()) {
        file.close();
    }

    if (error == true) { /*error==true，发生错误*/
        m_flag = false;
        //m_pFtp->close(); /*发生错误时，关闭ftp，会再次触发done信号*/
        //Logger::Log::Instance().logError(m_pFtp->error());
        Logger::Log::Instance().logError(m_pFtp->errorString());

        emit done(false);

    } else {
        emit done(true);
    }

}

void Transport::on_stateChanged(int state)
{
    QString str;

    switch (state) {
    case QFtp::Unconnected:
        str = "QFtp::Unconnected";
        break;
    case QFtp::HostLookup:
        //m_pTimer->start();
        str = "QFtp::HostLookup";
        break;
    case QFtp::Connecting:
        str = "QFtp::Connecting";
        break;
    case QFtp::Connected:
        str = "QFtp::Connected";
        //m_pTimer->stop();
        break;
    case QFtp::LoggedIn:
        str = "QFtp::LoggedIn";
        break;
    case QFtp::Closing:
        str = "QFtp::Closing";
        break;
    default:
        break;
    }

    if (DEBUG_FLAG) {
        qDebug("%s Ftp state: %s",__FUNCTION__,str.toUtf8().data());
    }

}

void Transport::on_timeout()
{
    //qDebug("%s",__FUNCTION__);

    m_pTimer->stop();
    m_pFtp->abort();

    if (file.isOpen()) {
        file.close();
    }
    Logger::Log::Instance().logError(m_pFtp->errorString());

    emit done(false);
}

