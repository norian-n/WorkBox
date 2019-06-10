/*
 *   The Workbox - variadic templates based thread pool implementation,
 * usage sample code
 *
 * Copyright (c) 2017 Dmitry 'Norian' Solodkiy
 *
 * License: defined in license.txt file located in the root sources dir
 *
 */

#include <thread>
#include <iostream>

#include "workbox.h"

// sample workloads for thread pool
void workFunctionSample(int& x, int& y)
{
    y = x + 1;
    // cout << x + y << " " ; // << endl; // "workFunction1 result: " <<
    cout << "*";
}

void workFunction2(int& x, int& y)
{
    y = x + 1;
    // cout << x + y << " " ; // << endl; // "workFunction1 result: " <<
    cout << ".";
}

// workersPool usage sample & smoke test

int main() // int argc, char *argv[]
{
    const bool useNotification { true };

        // different pools to show request modes, could be one
    workersPool theWorkersPool(useNotification);    // final notification on
    workersPool theWorkersPoolNotify;

    theWorkersPool.runThreads(5);
    theWorkersPoolNotify.runThreads(15);

        // sample data
    const int dataSize = 20000;

    int argA[dataSize];
    int argB[dataSize];

    int argC[dataSize];
    int argD[dataSize];

    for (int i = 0; i < dataSize; i++)
    {
            // init data pools
        argA[i] = i;
        argB[i] = i+1;

        argC[i] = i;
        argD[i] = i-1;
    }

    std::vector<WorkRequest<int&, int&>*> messagesPool;
    messagesPool.resize(dataSize, nullptr);

        // send work requests
    for (int i = 0; i < dataSize; i++)
    {
            // messages pool example - wait for notification for each message
        messagesPool[i] = new WorkRequest<int&, int&> (useNotification, &workFunctionSample, argA[i], argB[i]);
        theWorkersPoolNotify.sendWork(messagesPool[i]);

            // fire & forget example - no individual notifications
        WorkRequest<int&, int&>* newMessage = new WorkRequest<int&, int&> (&workFunction2, argC[i], argD[i]);
        theWorkersPool.sendWork(newMessage);
    }

        // wait for results and clean up
    for (int i = 0; i < dataSize; i++)
    {
        messagesPool[i]-> waitForWorkDone();

        delete messagesPool[i];
    }

    messagesPool.clear();

    cout << endl << "WorkersPoolNotify complete " << endl;

    theWorkersPoolNotify.joinThreads();

    // cout << "WorkersPool active workers: " << theWorkersPool.activeWorkersCount << endl;
    // cout << "WorkersPool jobs remains: " << theWorkersPool.poolWorkBox.messages_count() << endl;

    theWorkersPool.waitForWorkDone();

    cout << endl << "WorkersPool complete " << endl;

    theWorkersPool.joinThreads();

    cout << "Number of hardware cores = "
              <<  std::thread::hardware_concurrency() << endl;
}
