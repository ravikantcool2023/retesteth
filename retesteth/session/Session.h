#pragma once

#include <stdio.h>
#include <boost/noncopyable.hpp>
#include <boost/test/unit_test.hpp>
#include <map>
#include <string>
#include <thread>

#include <retesteth/configs/ClientConfig.h>
#include <retesteth/dataObject/DataObject.h>
#include <retesteth/session/SessionInterface.h>

using namespace dataobject;

// Session connections to an instance of a client
class RPCSession : public boost::noncopyable
{
public:
    enum SessionStatus
    {
        Working,      // test execution in progress
        Available,    // test execution has finished
        HasFinished,  // has just finished execution
        NotExist      // socket yet not initialized
    };

    static SessionInterface& instance(thread::id const& _threadID);
    static void sessionStart(thread::id const& _threadID);
    static void sessionEnd(thread::id const& _threadID, SessionStatus _status);
    static SessionStatus sessionStatus(thread::id const& _threadID);
    static void clear();

    SessionInterface& getImplementation() { return *m_implementation; }
    ~RPCSession() { delete m_implementation; }

private:
    explicit RPCSession(SessionInterface* _impl);
    static void runNewInstanceOfAClient(thread::id const& _threadID, test::ClientConfig const& _config);
    SessionInterface* m_implementation;
};
