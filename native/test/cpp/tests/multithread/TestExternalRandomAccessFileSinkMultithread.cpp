/**
  * Copyright 2014 Google Inc. All rights reserved.
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *     http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
*/

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "dataset/dataset_api.h"
#include "dataset/DatasetTestUtil.h"
#include "external/external_api.h"
#include "multithread/SimultaneousThreadRunner.h"

namespace MR4C {

class ConflictCheckingRandomAccessFile {

    friend class TestExternalRandomAccessFileSinkMultithread;

public:

    static bool read(char* buf, size_t num, size_t* read) {
        instance().check("read()");
        return true;
    }

    static bool write(char* buf, size_t num) {
        instance().check("write()");
        return true;
    }

    static bool getLocation(size_t* loc) {
        instance().check("getLocation()");
        return true;
    }

    static bool setLocation(size_t loc) {
        instance().check("setLocation()");
        return true;
    }

    static bool setLocationFromEnd(size_t loc) {
        instance().check("setLocationFromEnd()");
        return true;
    }

    static bool skipForward(size_t num) {
        instance().check("skipForward()");
        return true;
    }

    static bool skipBackward(size_t num) {
        instance().check("skipBackward()");
        return true;
    }

    static bool getFileSize(size_t* size) {
        instance().check("getFileSize()");
        return true;
    }

    static bool setFileSize(size_t size) {
        instance().check("setFileSize()");
        return true;
    }

private:

    mutable bool m_doingSomething;
    mutable bool m_didHaveConflict;

    ConflictCheckingRandomAccessFile()
    : m_doingSomething(false), m_didHaveConflict(false) {}

    static ConflictCheckingRandomAccessFile& instance() {
        static ConflictCheckingRandomAccessFile f;
        return f;
    }

    void noop() const {
        // Do nothing
    }

    inline void check(const std::string& method) const {
        if (m_doingSomething) {
            m_didHaveConflict = true;
            throw std::runtime_error("Thread-safety conflict in DataFile when calling DataFileSource::" + method);
        }
        m_doingSomething = true;
        noop(); // Allow for thread conflict if another method is executed here
        m_doingSomething = false;
    }

};

class ExternalRandomAccessFileSinkActionsThreadRunner : public SimultaneousThreadRunner {

    friend class TestExternalRandomAccessFileSinkMultithread;

private:
    CExternalRandomAccessFileCallbacks m_callbacks;
    ExternalRandomAccessFileSink* m_externalRaf;
    int m_callsPerThread;

public:
    ExternalRandomAccessFileSinkActionsThreadRunner(int numThreads, int callsPerThread)
            : SimultaneousThreadRunner(numThreads), m_callsPerThread(callsPerThread) {
        m_callbacks.readCallback = ConflictCheckingRandomAccessFile::read;
        m_callbacks.writeCallback = ConflictCheckingRandomAccessFile::write;
        m_callbacks.getLocationCallback = ConflictCheckingRandomAccessFile::getLocation;
        m_callbacks.setLocationCallback = ConflictCheckingRandomAccessFile::setLocation;
        m_callbacks.setLocationFromEndCallback = ConflictCheckingRandomAccessFile::setLocationFromEnd;
        m_callbacks.skipForwardCallback = ConflictCheckingRandomAccessFile::skipForward;
        m_callbacks.skipBackwardCallback = ConflictCheckingRandomAccessFile::skipBackward;
        m_callbacks.getSizeCallback = ConflictCheckingRandomAccessFile::getFileSize;
        m_callbacks.setSizeCallback = ConflictCheckingRandomAccessFile::setFileSize;

        m_externalRaf = new ExternalRandomAccessFileSink(m_callbacks);
    }

    virtual ~ExternalRandomAccessFileSinkActionsThreadRunner() {
        delete m_externalRaf;
    }

protected:
    void run() {
        for ( int i = 0; i < m_callsPerThread; ++i ) {
            m_externalRaf->getFileSink()->read(NULL, 1);
            m_externalRaf->getFileSink()->getLocation();
            m_externalRaf->getFileSink()->setLocation(1);
            m_externalRaf->getFileSink()->setLocationFromEnd(1);
            m_externalRaf->getFileSink()->skipForward(1);
            m_externalRaf->getFileSink()->skipBackward(1);
            m_externalRaf->getFileSink()->getFileSize();

            m_externalRaf->getFileSink()->write(NULL, 1);
            m_externalRaf->getFileSink()->setFileSize(1);
        }
    }
};

class TestExternalRandomAccessFileSinkMultithread : public CPPUNIT_NS::TestFixture {

    CPPUNIT_TEST_SUITE(TestExternalRandomAccessFileSinkMultithread);
    CPPUNIT_TEST(testMethodsMultithread);
    CPPUNIT_TEST_SUITE_END();

    private:

        DatasetTestUtil m_datasetUtil;

    public:

        void setUp() {}

        void tearDown() {}

        /**
         * Threading tests are nondeterministic, and thus have the possibility to pass
         * even though the code is not properly synchronized. These take a few seconds
         * to try to make it more likely to catch an error.
         */
        void testMethodsMultithread() {
            ExternalRandomAccessFileSinkActionsThreadRunner runner(10, 1000000);

            runner.start();
            runner.join();

            CPPUNIT_ASSERT(ConflictCheckingRandomAccessFile::instance().m_didHaveConflict == false);
        }

};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestExternalRandomAccessFileSinkMultithread, "TestExternalRandomAccessFileSinkMultithread");

}


