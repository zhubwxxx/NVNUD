#ifndef LOG_H
#define LOG_H

#include "Comm/comm.h"

using namespace std;
namespace Logger
{

class Log
{

    public:
        Log();
        virtual ~Log();
        static Log& Instance()
        {
            return Singleton<Log>::instance();
        }

        static void logBuffer(QString strHead,char *buf, int len); //打印报文
        //void LogMsgOnNet(const char* msg); //打印到网络上
        void logMsg(QString msg);

        void logInfo(QString msg); //显示日志信息
        void logError(QString msg); //显示错误信息

        static void writeFile(QString logPath, QString msg);

public:
        static QMutex m_mutex;

    };
}





































#endif // LOG_H
