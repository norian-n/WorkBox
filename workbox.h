/*
 * The Workbox - variadic templates based thread pool implementation
 *
 * Copyright (c) 2017 Dmitry 'Norian' Solodkiy
 *
 * License: defined in license.txt file located in the root sources dir
 *
 */

#ifndef WORKBOX_H
#define WORKBOX_H

#include <tuple>
#include <atomic>

// CXXFLAGS += -std=c++0x

#include "postbox.h"

using namespace std;

namespace helper    // variadic template arguments processing, pure magic
{
    template <std::size_t... Ts>
    struct index {};

    template <std::size_t N, std::size_t... Ts>
    struct gen_seq : gen_seq<N - 1, N - 1, Ts...> {};

    template <std::size_t... Ts>
    struct gen_seq<0, Ts...> : index<Ts...> {};
}

struct BaseWorkRequest  // base class for work function arguments' envelope message
{
    virtual void callWorkFunction() = 0;

    bool workDone { false };

    std::condition_variable* resultBell = nullptr;

    virtual ~BaseWorkRequest()  // just to suspend warning, could be moved to WorkRequest destuctor for performance
    {
        if (resultBell) delete resultBell;
        // cout << "BaseWorkMessage destructor" << endl;
    }
};

template <typename... valuesTypes>
class WorkRequest :  public BaseWorkRequest // work function arguments' envelope message
{
    typedef void (*workFunctionType)(valuesTypes... values);
    workFunctionType workFunction { nullptr };

    std::tuple<valuesTypes...> storeArgs;
    std::mutex* bellMutex = nullptr; // lock for BaseWorkMessage::condition_variable* resultBell

public:

    WorkRequest() = delete;

    WorkRequest(bool useResultBell, workFunctionType theWorkFunctionPtr, valuesTypes... args) : workFunction(theWorkFunctionPtr), storeArgs(args...)
    {
        if (useResultBell)
        {
            resultBell = new std::condition_variable;
            bellMutex = new std::mutex;
        }
    }

    WorkRequest(workFunctionType theWorkFunctionPtr, valuesTypes... args) : workFunction(theWorkFunctionPtr), storeArgs(args...)
    {
        workDone = false;
    }

    virtual ~WorkRequest()
    {
        if (bellMutex) delete bellMutex;
    }

    void waitForWorkDone()
    {
        if (resultBell)
        {
            std::unique_lock<std::mutex> mLock {*bellMutex};
            resultBell-> wait(mLock, [&]{return workDone;});
        }
    }

    void callWorkFunction()
    {
        callWorkFunctionWithTuple(storeArgs, helper::gen_seq<sizeof...(valuesTypes)>{});

        if (resultBell)
        {
            {
                std::lock_guard<std::mutex> tmpLock(*bellMutex);
                workDone = true;
            }
            resultBell-> notify_one();
        }
    }

    template<std::size_t... Is> void callWorkFunctionWithTuple(const std::tuple<valuesTypes...>& tuple, helper::index<Is...>)
    {
        workFunction(std::get<Is>(tuple)...);
    }

};

class workersPool // runs threads pool - threads're waiting for work requests
{
public:

    std::atomic<bool> isRunning {false};
    std::atomic<int>  threadsCount {0};

    std::atomic<int>  activeWorkersCount {0};

        // global pool notification
    bool workDone { false };

    std::condition_variable* resultBell { nullptr };
    std::mutex* bellMutex { nullptr }; // lock for resultBell

    BaseWorkRequest* terminateMessage { nullptr };

        // postbox for work requests
    postbox<BaseWorkRequest> poolWorkBox;

    workersPool() {}

    workersPool(bool sendPoolNotification)
    {
        if (sendPoolNotification)
        {
            resultBell = new std::condition_variable;
            bellMutex = new std::mutex;
        }
    }

    ~workersPool()
    {
        if (bellMutex) delete bellMutex;
        if (resultBell) delete resultBell;
    }

    workersPool(workersPool const &) = delete;
    workersPool& operator=(const workersPool&) = delete;

    vector<std::thread*> threads;

    void sendWork(BaseWorkRequest* workMessage) // wrapper for myWorkPool.poolWorkBox.send();
    {
        poolWorkBox.send(workMessage);
    }

    void waitForWorkDone()
    {
        if (resultBell)
        {
            std::unique_lock<std::mutex> mLock {*bellMutex};
            resultBell-> wait(mLock, [&]{return workDone;});
        }
    }

    void workProcessingLoop()   // primary work thread loop
    {
        BaseWorkRequest* theWorkMessage;

        bool isNotTermMessage = true;

        while(isRunning || isNotTermMessage)
        {
            theWorkMessage = poolWorkBox.receive();

            if (theWorkMessage)
            {
                activeWorkersCount++;

                theWorkMessage-> callWorkFunction();

                if (! theWorkMessage-> resultBell)
                {
                    // cout << "del message ";
                        delete theWorkMessage;  // fire & forget case
                }

                activeWorkersCount--;
            }
            else
                isNotTermMessage = false;

                // no active work, empty box
            if (resultBell && !activeWorkersCount && !poolWorkBox.messages_count())
            {
                    // sendWorkEndNotification
                {
                    std::lock_guard<std::mutex> tmpLock(*bellMutex);
                    workDone = true;
                }
                resultBell-> notify_one();
            }

        }
    }

    void runThreads(int threadsToRun)
    {
        std::thread* newThread;

        if (isRunning)
            joinThreads();

        threadsCount = threadsToRun;
        isRunning = true;

        threads.resize(threadsToRun, nullptr);

        for (int i = 0; i < threadsToRun; i++)
        {
            newThread = new std::thread([&] {workProcessingLoop();});
            threads[i] = newThread;
        }
    }

    void joinThreads()
    {
        isRunning = false;

        for (int i = 0; i < threadsCount; i++)
        {
            poolWorkBox.send(terminateMessage);
        }

        for (int i = 0; i < threadsCount; i++)
        {
            threads[i]->join();
        }

        for (int i = 0; i < threadsCount; i++)
        {
            if (threads[i])
                delete(threads[i]);
        }

        threads.clear();
    }
};


#endif // WORKBOX_H
