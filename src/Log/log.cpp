#include "log.h"

#include <QDateTime>
#include <qdebug.h>
#include <qfile.h>
#include <qtextcodec.h>
#include <qdir.h>

namespace Logger
{

QMutex Log::m_mutex;

Log::Log()
{

}

Log::~Log()
{

}


void Log::logBuffer(QString strHead,char *buf, int len)
{
    QMutexLocker locker(&m_mutex);

    QDateTime dateTime = QDateTime::currentDateTime();
    QString strDate = dateTime.date().toString("yyyy-MM-dd ");
    QString strTime = dateTime.time().toString("hh:mm:ss.zzz ");
    QString str = strDate+strTime;
    str.append(strHead);

    for (int i=0;i<len;i++) {
        str.append(QString("%1").arg(buf[i],2,16,QChar('0')));
        str.append(" ");
    }
    qDebug() << str;
}

void Log::logMsg(QString msg)
{
    //下面把日志数据，写进文件LogInfo.log中
    QFile file;
    file.setFileName("LogInfo.log");
    if (file.open(QIODevice::WriteOnly  | QIODevice::Text|QIODevice::Append)) {
        QTextStream in(&file);
        QTextCodec *utf8 = QTextCodec::codecForName("utf-8");
        QByteArray encoded = utf8->fromUnicode(msg);
        QString dateTimeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss  : ");
        in<< dateTimeStr << encoded << "\n";
        file.close();
    }

}

void Log::logInfo(QString msg)
{
    QMutexLocker locker(&m_mutex);

#ifdef Q_OS_LINUX
    QString logPath = "Log";
    writeFile(logPath,"INFO : " + msg);
#endif
}

void Log::logError(QString msg)
{
    QMutexLocker locker(&m_mutex);

#ifdef Q_OS_LINUX
    QString logPath = "Log";
    writeFile(logPath,"ERROR: " + msg);
#endif

}


void Log::writeFile(QString logPath,QString msg)
{
    qDebug()<< msg;

    //下面把日志数据，写进文件LogInfo.log中
    QDir dir;
    dir.setPath(logPath);
    if(!dir.exists()) {
        if (!dir.mkdir(dir.absolutePath())) {
            qDebug("%s mkdir %s error",__FUNCTION__,dir.absolutePath().toUtf8().data());
            return;
        }
    }

    //检查文件夹内的文件数量，删除一个月以前的历史记录
    dir.setFilter(QDir::Files | QDir::NoSymLinks);
    QFileInfoList filelist = dir.entryInfoList();
    int file_count = filelist.count();
    QDateTime nowDate = QDateTime::currentDateTime();
    int year;
    int month;
    int day;
    QDate filedate;
    QDate tDate;

    if (file_count > 30) {
        int temp = file_count;
        for (int i=0; i<file_count; i++) {
            QFileInfo file_info = filelist.at(i);
            year=file_info.fileName().mid(0,4).toInt();
            month=file_info.fileName().mid(5,2).toInt();
            day=file_info.fileName().mid(8,2).toInt();
            filedate.setDate(year, month, day);
            tDate = filedate.addDays(30);
            if (tDate <= nowDate.date()) {
                QFile file(file_info.filePath());
                file.setPermissions(QFile::WriteOwner);
                file.remove();
                temp--;
                if(temp<=30)
                    break;
            }
        }
    }


    string dirpath= logPath.append(QString("/")).toStdString();
    dirpath+=nowDate.toString("yyyy-MM-dd").toStdString()+".log";
    QFile file(dirpath.c_str());
    if (!file.open(QIODevice::WriteOnly  | QIODevice::Text|QIODevice::Append)) {
        return;
    }
    QTextStream in(&file);
    QTextCodec *utf8 = QTextCodec::codecForName("utf-8");
    QByteArray encoded = utf8->fromUnicode(msg);
    QString dateTimeStr=nowDate.toString("HH:mm:ss  : ");
    in<<dateTimeStr<<encoded<<"\r\n";
    file.close();

}




}



