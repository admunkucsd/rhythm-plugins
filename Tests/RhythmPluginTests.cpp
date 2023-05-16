#include <stdio.h>

#include "gtest/gtest.h"


#include <ProcessorHeaders.h>
#include "../Source/DeviceThread.h"
#include "../Source/IntanRecordController.h"
#include <ModelProcessors.h>
#include <ModelApplication.h>
#include <TestFixtures.h>

class RhythmPluginTests : public ::testing::Test {
protected:
    void SetUp() override {
        DataThreadCreator dataThreadFactory = &createDataThread<RhythmNode::IntanRecordController>;
        tester = std::make_unique<ProcessorTester>(dataThreadFactory);
        GenericProcessor* sourceNodeGeneric = tester -> getSourceNode();
        processor = (SourceNode*) (tester->getSourceNode());
        deviceThread = (RhythmNode::IntanRecordController*)(processor)->getThread();
    }

    void TearDown() override {
    }
    
    void WriteBlock(AudioBuffer<float> &buffer) {
        auto audio_processor = (AudioProcessor *)processor;
        auto data_streams = processor->getDataStreams();
        ASSERT_EQ(data_streams.size(), 1);
        auto streamId = data_streams[0]->getStreamId();
        HeapBlock<char> data;
        size_t dataSize = SystemEvent::fillTimestampAndSamplesData(
            data,
            processor,
            streamId,
            current_sample_index,
            // NOTE: this timestamp is actually ignored in the current implementation?
            0,
            buffer.getNumSamples(),
            0);
        MidiBuffer eventBuffer;
        eventBuffer.addEvent(data, dataSize, 0);

        auto original_buffer = buffer;
        audio_processor->processBlock(buffer, eventBuffer);

        current_sample_index += buffer.getNumSamples();
    }

    void buildUSBDataBuffer() {
        int bytesPerSample = 2 * (35 * 32 + 16);
        int samplesPerBlock = 128;
        int bytesPerBlock =  (samplesPerBlock * bytesPerSample);
        packedBuffer.realloc(bytesPerBlock);
        for(int sample = 0; sample < samplesPerBlock; sample++) {
            packedBuffer[0 + sample * bytesPerBlock] = 0x53;
            packedBuffer[1 + sample * bytesPerBlock] = 0x2a;
            packedBuffer[2+ sample * bytesPerBlock] = 0x13;
            packedBuffer[3+ sample * bytesPerBlock] = 0x38;
            packedBuffer[4+ sample * bytesPerBlock] = 0xaa;
            packedBuffer[5+ sample * bytesPerBlock] = 0x2a;
            packedBuffer[6+ sample * bytesPerBlock] = 0xa2;
            packedBuffer[7+ sample * bytesPerBlock] = 0xd7;
            for(int i = 8; i < bytesPerBlock; i++) {
                packedBuffer[i + sample*bytesPerBlock] = 0x00;
            }
        }
    }
    
    
    HeapBlock<unsigned char> packedBuffer;
    std::unique_ptr<ProcessorTester> tester;
    RhythmNode::IntanRecordController* deviceThread;
    SourceNode* processor;
    int64_t current_sample_index = 0;
};

TEST_F(RhythmPluginTests, DataAlignmentTest) {

    buildUSBDataBuffer();
    AudioBuffer<float> inputBuffer (1024, 256);
    deviceThread->testBuffer = packedBuffer.get();
    tester->startAcquisition(false);
    WriteBlock(inputBuffer);
    tester->stopAcquisition();

}

TEST_F(RhythmPluginTests, DataIntegrityTest) {
	GTEST_SKIP();
}


TEST_F(RhythmPluginTests, ReadIndexTest) {
    GTEST_SKIP();

}

