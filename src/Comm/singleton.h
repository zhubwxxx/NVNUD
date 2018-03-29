/*
 * Copyright (C) 2016 Alexander Makarov
 *
 * Source:
 * https://wiki.qt.io/Qt_thread-safe_singleton
 *
 */
#ifndef SINGLETON
#define SINGLETON

#include "call_once.h"

#include <QtGlobal>
#include <QScopedPointer>

template <class T>
class Singleton
{
public:
    static T& instance()
    {
        qCallOnce(init, flag);
        return *tptr;
    }
    static void init()
    {
        tptr.reset(new T);
    }
private:
    Singleton() {}
    ~Singleton() {}
    Q_DISABLE_COPY(Singleton)
    static QScopedPointer<T> tptr;
    static QBasicAtomicInt flag;
};

template<class T> QScopedPointer<T> Singleton<T>::tptr(0);
template<class T> QBasicAtomicInt Singleton<T>::flag = Q_BASIC_ATOMIC_INITIALIZER(CallOnce::CO_Request);

#endif // SINGLETON
