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
        packedBuffer.realloc(bytesPerBlock);
        for(int sample = 0; sample < samplesPerBlock; sample++) {
            packedBuffer[0 + sample * bytesPerSample] = 0x53;
            packedBuffer[1 + sample * bytesPerSample] = 0x2a;
            packedBuffer[2+ sample * bytesPerSample] = 0x13;
            packedBuffer[3+ sample * bytesPerSample] = 0x38;
            packedBuffer[4+ sample * bytesPerSample] = 0xaa;
            packedBuffer[5+ sample * bytesPerSample] = 0x2a;
            packedBuffer[6+ sample * bytesPerSample] = 0xa2;
            packedBuffer[7+ sample * bytesPerSample] = 0xd7;
            for(int i = 8; i < bytesPerSample; i++) {
                packedBuffer[i + sample*bytesPerSample] = 0x00;
            }
        }
    }
    
    
    HeapBlock<unsigned char> packedBuffer;
    std::unique_ptr<ProcessorTester> tester;
    RhythmNode::IntanRecordController* deviceThread;
    SourceNode* processor;
    int64_t current_sample_index = 0;
    
    //16-bit words
    //data frame = 35 words/stream * 32 streams + 4 words (magic number) + 2 words (time stamp) + 8 words (ADC results)+ 2 words (TTL input/output)
    const uint64_t bytesPerSample = 2272;
    const uint64_t samplesPerBlock = 256;
    const uint64_t bytesPerBlock = 581632;
};

TEST_F(RhythmPluginTests, DataAlignmentTest) {

    buildUSBDataBuffer();
    deviceThread->testBuffer = packedBuffer.get();

    AudioBuffer<float> inputBuffer (1024, 256);
    
    tester->startAcquisition(false);
    WriteBlock(inputBuffer);
    tester->stopAcquisition();

    ASSERT_EQ(deviceThread->lastReadIndex, 581632);

}

TEST_F(RhythmPluginTests, DataIntegrityTest) {
    GTEST_SKIP();
}


TEST_F(RhythmPluginTests, ReadIndexTest) {
    
    buildUSBDataBuffer();
    //Misalign 2nd data frame magic number
    packedBuffer[2274] = 0x00;
    deviceThread->testBuffer = packedBuffer.get();
    
    AudioBuffer<float> inputBuffer (1024, 256);
    tester->startAcquisition(false);
    WriteBlock(inputBuffer);
    tester->stopAcquisition();

    //Expect device thread to exit after 1 full frame after bad frame
    ASSERT_EQ(deviceThread->lastReadIndex, 2272);

}

