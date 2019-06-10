/*
 * The Postbox - thread pool communication implementation
 *
 * Copyright (c) 2017 Dmitry 'Norian' Solodkiy
 *
 * License: defined in license.txt file located in the root sources dir
 *
 */

#ifndef POSTBOX_H
#define POSTBOX_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <chrono>

// CXXFLAGS += -std=c++0x

/*
  A. message sample, could be any class or simple type:

struct simple_post_message // should be deleted by receiever after use
{
    char* charMessage;
    int intValue;
};

  B. usage sample:

postbox<simple_post_message> my_box;

    std::thread1:

simple_post_message * my_message = new simple_post_message(content0, strlen(content0) + 1);
my_box.send(my_message);

    std::thread2:

my_message = my_box.receive();
cout << "receiver_thread receive: " << my_message-> charMessage << ", " << my_message-> intValue << endl;
delete my_message;

*/

using namespace std;

template <typename post_message> class postbox
{
    mutex doorLockMutex;                   // anti-race lock
    condition_variable doorBell;           // wake up bell
    deque<post_message*> messagesQueue;    // post messages

 public:

    postbox() {}
    ~postbox();

   void send (post_message* message);       // send to postbox
   post_message* receive();                 // receive or wait for message forever
   post_message* receive(int timeout_ms);   // receive with timeout, NULL on expire
   post_message* snatch();                  // receive if have messages, else NULL

   int messages_count();                    // to check box overflows, not lock protected
};

template <typename post_message> postbox<post_message>::~postbox()
{
    std::lock_guard<std::mutex> postLock(doorLockMutex);

        // delete undelivered messages
    for (auto queueIter = messagesQueue.begin(); queueIter != messagesQueue.end(); ++queueIter)
    {
        post_message* aMessage = *queueIter;
        if (aMessage)
            delete aMessage;
    }

    messagesQueue.clear();
}


template <typename post_message> void postbox<post_message>::send (post_message* message)
{       // lock door
    // unique_lock<std::mutex> aLock(doorLockMutex);
        // add message and wake receiver thread
    {
        std::lock_guard<std::mutex> postLock(doorLockMutex);
        messagesQueue.push_front(message);
    }
    // aLock.unlock();
    doorBell.notify_one();
}

template <typename post_message> post_message* postbox<post_message>::receive()
{       // lock door
    unique_lock<std::mutex> aLock(doorLockMutex);
    post_message* aMessage = nullptr;
        // wait for a bell
    while (messagesQueue.empty())
    {
        // cout << "receive waiting for messages" << endl;
        doorBell.wait(aLock);
    }
        // pick message from queue
    aMessage = messagesQueue.back();
    messagesQueue.pop_back();
            // charity action - wake up neighbor thread
    if (! messagesQueue.empty())
    {
        aLock.unlock();
        doorBell.notify_one();
    }

    return aMessage;
}

template <typename post_message> post_message* postbox<post_message>::receive(int timeout_ms)
{       // lock door
    unique_lock<std::mutex> aLock(doorLockMutex);
    post_message* aMessage = nullptr;

    if (messagesQueue.empty())
    {
        if(doorBell.wait_for(aLock, chrono::milliseconds(timeout_ms)) == cv_status::no_timeout) // wait
        {
            if (! messagesQueue.empty()) // check to be sure
            {
                aMessage = messagesQueue.back();
                messagesQueue.pop_back();
                    // charity action - wake up neighbor thread
                if (! messagesQueue.empty())
                {
                    aLock.unlock();
                    doorBell.notify_one();
                }
            }
        }
        else // timeout
        {
            return nullptr;
        }
    }
    else    // not empty
    {
        aMessage = messagesQueue.back();
        messagesQueue.pop_back();
            // charity action - wake up neighbor thread
        if (! messagesQueue.empty())
        {
            aLock.unlock();
            doorBell.notify_one();
        }
    }
    return aMessage;
}

template <typename post_message> post_message* postbox<post_message>::snatch()
{       // lock door
    unique_lock<std::mutex> aLock(doorLockMutex);
    post_message* a_message = nullptr;
        // look for a message
    if (! messagesQueue.empty())
    {
        a_message = messagesQueue.back();
        messagesQueue.pop_back();
    }
        // charity action - wake up neighbor thread
    if (! messagesQueue.empty())
    {
        aLock.unlock();
        doorBell.notify_one();
    }

    return a_message;
}

template <typename post_message> int postbox<post_message>::messages_count()
{
    lock_guard<std::mutex> aLock(doorLockMutex);

    return messagesQueue.size();
}


#endif // POSTBOX_H
