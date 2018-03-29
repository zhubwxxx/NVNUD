#ifndef WORKER_H
#define WORKER_H

#include <qthread.h>
#include <qmutex.h>


template<class T>
class Worker : public QThread
{
public:
    typedef void (T::*CallBack)();
    Worker(T *object, CallBack fun) : _pObject(object),
                                             _fun(fun),
                                        _stopped(true),
                                        _running(false)
    {

    }

    virtual ~Worker()
    {

    }

    void start()
    {
        QMutexLocker locker(&_mutex);
        if (!_running) {
            try {
                QThread::start();
                _stopped = false;
                _running = true;
            } catch (...) {
                _running = false;
                throw;
            }
        }
    }

    void stop()
    {
        _stopped = true;
        _running = false;
        QMutexLocker locker(&_mutex);

    }

    void wait(unsigned long milliseconds)
    {
        if (_running) {
            QMutexLocker locker(&_mutex);
        }
    }

    void wait()
    {
        QMutexLocker locker(&_mutex);
    }

    bool isStopped() const
    {
        return _stopped;
    }

    bool isRunning() const
    {
        return _running;
    }

    void msleep(unsigned long milliseconds)
    {
        QThread::msleep(milliseconds);
    }

    void sleep(unsigned long seconds)
    {
        QThread::sleep(seconds);
    }

protected:
    void run()
    {
        QMutexLocker locker(&_mutex);
        try {
            (_pObject->*_fun) ();
        } catch (...) {
            _running = false;
            throw;
        }
        _running = false;
    }

private:
    T *_pObject;
    CallBack _fun;

    volatile bool _stopped;
    volatile bool _running;

    QMutex _mutex;
};


#endif // WORKER_H
