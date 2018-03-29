#ifndef UTIL_H
#define UTIL_H


#include "Config/appinfo.h"

#include <qobject.h>
#include <qdebug.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qdatetime.h>
#include <qnetworkinterface.h>
#include <qcryptographichash.h>

#include <stdio.h>
#include <time.h>

#include <iostream>
using namespace std;

class Util
{
public:

    static WHICH_APP toAppWho(QString appName)
    {
        WHICH_APP who;

        if (appName == BillingCtrlName) {
            who = WHICH_BILLING_CONTROL_APP;
        } else if (appName == GatewayName) {
            who = WHICH_GATEWAY_APP;
        } else if (appName == MatrixCtrlName) {
            who = WHICH_MATRIX_CONTROL_APP;
        } else if (appName == ConfigName) {
            who = WHICH_CONFIG_APP;
        } else if (appName == DebugName) {
            who = WHICH_DEBUG_APP;
        } else if (appName == UpdateName) {
            who = WHICH_UPDATE_APP;
        } else if (appName == DaemonName) {
            who = WHICH_DAEMON_APP;
        }  else if (appName == ChargeOperateName) {
            who = WHICH_CHARGE_OPERATE_APP;
        } else if (appName == ChargeCtrlName) {
            who = WHICH_CHARGE_CONTROL_APP;
        } else {
            Logger::Log::Instance().logError(QString("%1 unknown appName %1")
                                             .arg(__FUNCTION__)
                                             .arg(appName));
        }

        return who;
    }


    static QString toAppName(WHICH_APP who)
    {
        QString appName = "";

        if (who == WHICH_BILLING_CONTROL_APP) {
            appName =  BillingCtrlName;
        } else if (who == WHICH_GATEWAY_APP) {
            appName = GatewayName;
        } else if (who == WHICH_MATRIX_CONTROL_APP) {
            appName = MatrixCtrlName;
        } else if (who == WHICH_CONFIG_APP) {
            appName = ConfigName;
        } else if (who == WHICH_DEBUG_APP) {
            appName = DebugName;
        } else if (who == WHICH_UPDATE_APP) {
            appName = UpdateName;
        } else if (who == WHICH_DAEMON_APP) {
            appName = DaemonName;
        } else if (who == WHICH_CHARGE_OPERATE_APP) {
            appName = ChargeOperateName;
        } else if (who == WHICH_CHARGE_CONTROL_APP) {
            appName = ChargeCtrlName;
        } else {
            Logger::Log::Instance().logError(QString("%1 unknown app who = %1")
                                             .arg(__FUNCTION__)
                                             .arg(who));
        }

        return appName;
    }


    //返回who对应的本地工作目录
    static QString toAppWorkDir(WHICH_APP who)
    {
        QString workDir = "";

        if (who == WHICH_BILLING_CONTROL_APP) {
            workDir =  BillingCtrlDir;
        } else if (who == WHICH_GATEWAY_APP) {
            workDir = GatewayDir;
        } else if (who == WHICH_MATRIX_CONTROL_APP) {
            workDir = MatrixCtrlDir;
        } else if (who == WHICH_CONFIG_APP) {
            workDir = ConfigDir;
        } else if (who == WHICH_DEBUG_APP) {
            workDir = DebugDir;
        } else if (who == WHICH_UPDATE_APP) {
            workDir = UpdateDir;
        } else if (who == WHICH_DAEMON_APP) {
            workDir = DaemonDir;
        } else if (who == WHICH_CHARGE_OPERATE_APP) {
            workDir = ChargeOperateDir;
        } else if (who == WHICH_CHARGE_CONTROL_APP) {
            workDir = ChargeCtrlDir;
        }else {
            Logger::Log::Instance().logError(QString("%1 unknown appWorkDir %1")
                                             .arg(__FUNCTION__)
                                             .arg(who));
        }

        return workDir;
    }


    /**
     * @brief cmpVersion 比较版本号是否一致
     * @param newVersion
     * @param oldVersion
     * @return  0:版本一致
     *          1: newVersion > oldVersion
     *         -1: newVersion < oldVersion
     */
    static int cmpVersion(UInt8 newVersion[], UInt8 oldVersion[])
    {
        int ret = 0;

        if ((newVersion[0]==0) && (newVersion[1]==0) && (newVersion[2]==0) && (newVersion[3]==0)) {
            ret = 0;
        } else if ((oldVersion[0]==0) && (oldVersion[1]==0) && (oldVersion[2]==0) && (oldVersion[3]==0)) {
            ret = 0;
        } else {
            if (newVersion[0] > oldVersion[0]) {
                ret = 1;
            } else if(newVersion[0] < oldVersion[0]) {
                ret = -1;
            } else {
                if (newVersion[1] > oldVersion[1]) {
                    ret = 1;
                } else if(newVersion[1] < oldVersion[1]) {
                    ret = -1;
                } else {
                    if (newVersion[2] > oldVersion[2]) {
                        ret = 1;
                    } else if(newVersion[2] < oldVersion[2]) {
                        ret = -1;
                    } else {
                        if (newVersion[3] > oldVersion[3]) {
                            ret = 1;
                        } else if(newVersion[3] < oldVersion[3]) {
                            ret = -1;
                        } else {
                            ret = 0;
                        }

                    }

                }

            }

        }

        return ret;
    }


    //通过程序文件名判断程序是否在运行
    static int isAppRun(QString appName)
    {
        QDir dir("/proc");
        dir.setFilter(QDir::Dirs);
        QFileInfoList infoList = dir.entryInfoList();
        QFileInfo info;
        QFile file;
        QString status;
        char line[1024];
        QString strLine;

        for (int i=0; i<infoList.size(); i++) {
            info = infoList.at(i);
            if (!info.isDir()) {
                continue;
            }
            status = info.absoluteFilePath()+'/'+"status";
            //qDebug()<< __FUNCTION__ << info.absoluteFilePath();
            info.setFile(status);
            if(!info.exists()) {
                continue;
            }
            file.setFileName(status);
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            file.readLine(line,1024);
            file.close();
            strLine = QString(line);
            //qDebug()<< __FUNCTION__ << strLine.section(':', 1,1).simplified();
            if (appName == strLine.section(':', 1, 1).simplified()) {
                return 0;
            }
        }

        return -1;
    }

    //获取eth0设备信息
    static bool getEth0Entry(QNetworkAddressEntry &eth0)
    {
        QString LINUX_NETWORK_DEVICE = "eth0";
        bool find = false;
        QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();

        foreach(QNetworkInterface ni, list) {
            if(ni.name() == LINUX_NETWORK_DEVICE) {
                if ( (ni.flags() & QNetworkInterface::IsUp) != QNetworkInterface::IsUp) {
                    Logger::Log::Instance().logError("device eth0 is down");
                    find = false;
                } else {
                    QList<QNetworkAddressEntry> entryList = ni.addressEntries();
                    if (entryList.size() > 0) {
                        find = true;
                        eth0 = entryList[0];
                        break;
                    }
                }
            }
        }

        return find;

    }


    static QByteArray getFileMd5(QString filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            Logger::Log::Instance().logError(QString("Open file %1 failed").arg(filePath));
            return 0;
        }

        QCryptographicHash ch(QCryptographicHash::Md5);

        char buf[4096];
        Int64 maxLen = 4096;
        Int64 len = 0;

        do {
            len = file.read(buf,maxLen);
            if (len > 0) {
                ch.addData(buf,len);
            }
        } while (len == maxLen);

        file.close();

        return ch.result();
    }

    static bool copyRecursively(const QString &srcFilePath, const QString &tgtFilePath)
    {

        QFileInfo srcFileInfo(srcFilePath);
        if (srcFileInfo.isDir()) {
            QDir targetDir(tgtFilePath);

            qDebug()<< targetDir.currentPath();
            targetDir.cdUp();
            qDebug()<< targetDir.currentPath();

            qDebug()<< QFileInfo(tgtFilePath).fileName();
            if (!targetDir.mkdir(QFileInfo(tgtFilePath).fileName())) {
                qDebug()<< "11122";
                return false;
            }
            QDir sourceDir(srcFilePath);
            QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
            foreach (const QString &fileName, fileNames) {
                const QString newSrcFilePath
                        = srcFilePath + QLatin1Char('/') + fileName;
                const QString newTgtFilePath
                        = tgtFilePath + QLatin1Char('/') + fileName;
                if (!copyRecursively(newSrcFilePath, newTgtFilePath))
                    return false;
            }
        } else {

            if (!QFile::copy(srcFilePath, tgtFilePath))
                return false;
        }
        return true;
    }

    static void delayMs(float ms)
    {
        struct timeval tv;
        long long total_usec;

        total_usec = ms * 1000;
        tv.tv_sec = total_usec/1000000;
        tv.tv_usec = total_usec - tv.tv_sec*1000000;

        select(0, NULL, NULL, NULL, &tv);
    }

    // 获得小端值
    static UInt16 getLittleValue(UInt16 bigValue)
    {
        UInt16 littleValue = 0;
        littleValue = (bigValue&0x00ff)<<8|(bigValue&0xff00)>>8;
        return littleValue;
    }

    // 获得大端值
    static UInt16 getBigValue(UInt16 littleValue)
    {
        UInt16 bigValue = 0;
        bigValue = (littleValue&0x00ff)<<8|(littleValue&0xff00)>>8;
        return bigValue;
    }

    static string trim(const string& str)
    {
        string::size_type pos = str.find_first_not_of(' ');
        if (pos == string::npos)
        {
            return str;
        }
        string::size_type pos2 = str.find_last_not_of(' ');
        if (pos2 != string::npos)
        {
            return str.substr(pos, pos2 - pos + 1);
        }
        return str.substr(pos);
    }

    static int split(const string& str, vector<string>& ret_, string sep/* = ","*/)
    {
        if (str.empty())
        {
            return 0;
        }

        string tmp;
        string::size_type pos_begin = str.find_first_not_of(sep);
        string::size_type comma_pos = 0;

        while (pos_begin != string::npos)
        {
            comma_pos = str.find(sep, pos_begin);
            if (comma_pos != string::npos)
            {
                tmp = str.substr(pos_begin, comma_pos - pos_begin);
                pos_begin = comma_pos + sep.length();
            }
            else
            {
                tmp = str.substr(pos_begin);
                pos_begin = comma_pos;
            }

            if (!tmp.empty())
            {
                ret_.push_back(tmp);
                tmp.clear();
            }
        }
        return 0;
    }

    static string replace(const string& str, const string& src, const string& dest)
    {
        string ret;

        string::size_type pos_begin = 0;
        string::size_type pos       = str.find(src);
        while (pos != string::npos)
        {
            cout <<"replacexxx:" << pos_begin <<" " << pos <<"\n";
            ret.append(str.data() + pos_begin, pos - pos_begin);
            ret += dest;
            pos_begin = pos + 1;
            pos       = str.find(src, pos_begin);
        }
        if (pos_begin < str.length())
        {
            ret.append(str.begin() + pos_begin, str.end());
        }
        return ret;
    }

    static string UTF8ToGBK(const string& strUTF8)
    {
        //需要处理
        return strUTF8;
    }

    static vector<string> split(string str, string separator)
    {
        vector<string> result;
        unsigned int cutAt;
        while( (cutAt = str.find_first_of(separator)) != str.npos )
        {
            if(cutAt > 0)
            {
                result.push_back(str.substr(0, cutAt));
            }else{
                result.push_back("");
            }
            str = str.substr(cutAt + 1);
        }
        if(str.length() > 0)
        {
            result.push_back(str);
        }else{
            result.push_back("");
        }
        return result;
    }


    // 将16进制数转字符串
    static string byteToHexStr(char* str,UInt16 len)
    {
        string hexstr="";
        for (int i=0;i<len;i++)
        {
            char hex1;
            char hex2;
            int value=(UInt8)*(str+i); //直接将unsigned char赋值给整型的值，系统会正动强制转换
            int v1=value/16;
            int v2=value % 16;

            //将商转成字母
            if (v1>=0&&v1<=9)
                hex1=(char)(48+v1);
            else
                hex1=(char)(55+v1);

            //将余数转成字母
            if (v2>=0&&v2<=9)
                hex2=(char)(48+v2);
            else
                hex2=(char)(55+v2);

            //将字母连接成串
            hexstr += hex1;
            hexstr += hex2;
        }
        return hexstr;

    }


    // 32位高低转换
    static UInt32 htonl32(UInt32 A)
    {
        UInt32 ret = 0;
        ret |= (A&0xFF000000) >> 24;
        ret |= (A&0x00FF0000) >> 8;
        ret |= (A&0x0000FF00) << 8;
        ret |= (A&0x000000FF) << 24;
        return ret;
    }



    // 16位高低转换
    static UInt16 htonl16(UInt16 A)
    {
        UInt16 ret = 0;
        ret |= (A&0xFF00) >> 8;
        ret |= (A&0x00FF) << 8;
        return ret;
    }


    //判断字符串是否是有字母和数字组成，含其它字符则返回false
    static bool isLetterOrNumber(const string& str)
    {
        for (unsigned int i=0;i<str.size();i++)
        {
            char ch = str.data()[i];
            if (ch>='0' && ch<='9')
            {
                continue;
            }

            if (ch>='a' && ch<='z')
            {
                continue;
            }

            if (ch>='A' && ch<='Z')
            {
                continue;
            }

            return false;
        }

        return true;
    }

    static UInt8 hex2bcd8(UInt8 hex)
    {
        UInt8 ret = 0,retTemp = 0;
        ret = hex%10;
        retTemp = (hex/10)<<4;
        return ret + retTemp;
    }

    static UInt8 bcd2hex8(UInt8 bcd)
    {
        return ((bcd&0xf) + ((bcd&0xf0)>>4)*10);
    }


    //string转为小写
    static void str2lower(string &str)
    {
        transform(str.begin(), str.end(), str.begin(), (int (*)(int))tolower);
    }
    //string转为大写
    static void str2upper(string &str)
    {
        transform(str.begin(), str.end(), str.begin(), (int (*)(int))toupper);
    }

    static int str2bcd(const char* str,size_t strLen,unsigned char* BCD,size_t bcdLen)
    {
        size_t i;
        if (strLen%2 == 1) {
            return -1;
        }
        if (bcdLen != strLen/2) {
            return -1;
        }

        for (i=0; i<strLen; i+=2) {
            std::string s = string(str+i,2);
            QString qs = s.c_str();
            BCD[i/2] = hex2bcd8(qs.toInt());
        }

        return 0;
    }

    static void bcd2str(const unsigned char *BCD, size_t bcdLen, string &str)
    {
        str = "";
        char ch[16];
        for(unsigned int n=0; n<bcdLen; n++)
        {
            snprintf(ch, sizeof(ch), "%02x", BCD[n]);
            str += ch;
        }
    }

    static void bcd2str(const unsigned char *BCD, size_t bcdLen, char *str, size_t strLen)
    {
        char ch[16];
        size_t i;

        strLen = strLen;
        str[0] = '\0';
        for (i=0; i<bcdLen; i++)
        {
            snprintf(ch, sizeof(ch), "%02x", BCD[i]);
            strcat(str, ch);
        }
    }

    static void swapArrayOrder(UInt8* buf,int len)
    {
        for(int i=0;i<len/2;i++)
        {
            UInt8 tmp=0;
            tmp = buf[i];
            buf[i] = buf[len-1-i];
            buf[len-1-i] = tmp;
        }
    }

    static Int64 getSysRunningTimeSinceBoot()
    {
        #ifdef Q_OS_LINUX
            struct timespec ts;
            //clock_gettime(CLOCK_MONOTONIC, &ts);
            return (ts.tv_sec * (Int64)1000 + ts.tv_nsec / 1000000);
        #endif

        #ifdef Q_OS_WIN
            static UInt32 lastTimeInRegister = 0;
            static UInt64 baseTime = 0;

            unsigned int currentRegisterTime = GetTickCount();

            if (currentRegisterTime >= lastTimeInRegister ) {
                //寄存器没有重新计数
            } else {
                baseTime += ((UInt64)1)<<32;//寄存器已重新计数
            }
            lastTimeInRegister = currentRegisterTime;

            return baseTime + currentRegisterTime;
        #endif
    }

};

#endif // UTIL_H
