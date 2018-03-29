#include "wdt.h"

#include "Comm/util.h"

WDT::WDT(QObject *parent) : QThread(parent)
{
    ipcStack = IpcStacker::IpcStack::Instance();
    m_flag = true;
}

WDT::~WDT()
{
}

void WDT::run()
{
    IpcData ipcData;
    ipcData.head = PROTOCOL_HEAD;
    ipcData.commond = CMD_SEND_HEART;
    ipcData.frameNum = 0;
    ipcData.flag = 0;
    ipcData.buffer.clear();
    ipcData.len = 1;
    UInt8 id = 4; //升级软件心跳包代码
    ipcData.buffer.append((char*)&id, ipcData.len);
    ipcData.end = PROTOCOL_END;
    ipcData.who = WHICH_DAEMON_APP;

    while (m_flag) {

        ipcStack->Write(ipcData);
        Util::delayMs(100);
    }

}



