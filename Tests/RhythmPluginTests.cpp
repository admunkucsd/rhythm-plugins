#include <stdio.h>

#include "gtest/gtest.h"


//#include <ProcessorHeaders.h>
#include "../Source/DeviceThread.h"
#include <ModelProcessors.h>
#include <ModelApplication.h>
#include <TestFixtures.h>

class RhythmPluginTests : public ProcessorTest {
protected:
    RhythmPluginTests() : ProcessorTest(1, 150) {
    }

    ~RhythmPluginTests() override {
    }

    void SetUp() override {
        ProcessorTest::SetUp();
    }

    void TearDown() override {
        ProcessorTest::TearDown();
    }
    RhythmNode::DeviceThread* uut;

};

TEST_F(RhythmPluginTests, DataIntegrityTest) {
	GTEST_SKIP();
}

TEST_F(RhythmPluginTests, DataAlignmentTest) {
	GTEST_SKIP();

}
TEST_F(RhythmPluginTests, ReadIndexTest) {
    GTEST_SKIP();

}
