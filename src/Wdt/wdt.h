#ifndef WDT_H
#define WDT_H


#include "Lib/Ipc/ipcstack.h"

#include <qthread.h>
#include <qtimer.h>

class WDT : public QThread
{
    Q_OBJECT
public:
    static WDT& Instance()
    {
        return Singleton<WDT>::instance();
    }

    explicit WDT(QObject *parent = 0);
    ~WDT();

protected:
    void run();

private:
    volatile bool m_flag;

    IpcStacker::IpcStack *ipcStack;

};

#endif // WDT_H
