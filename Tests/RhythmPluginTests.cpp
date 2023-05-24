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
    
    void parseEventBuffer(const MidiBuffer& eventBuffer, int &newSamples) {
        if (eventBuffer.getNumEvents() > 0)
        {
            
            for (const auto meta : eventBuffer)
            {
                const uint8* dataptr = meta.data;
                
                if (static_cast<Event::Type> (*dataptr) == Event::Type::SYSTEM_EVENT
                    && static_cast<SystemEvent::Type>(*(dataptr + 1) == SystemEvent::Type::TIMESTAMP_AND_SAMPLES))
                {
                    uint16 sourceProcessorId = *reinterpret_cast<const uint16*>(dataptr + 2);
                    uint16 sourceStreamId = *reinterpret_cast<const uint16*>(dataptr + 4);
                    uint32 sourceChannelIndex = *reinterpret_cast<const uint16*>(dataptr + 6);
                    
                    int64 startSample = *reinterpret_cast<const int64*>(dataptr + 8);
                    double startTimestamp = *reinterpret_cast<const double*>(dataptr + 16);
                    uint32 nSamples = *reinterpret_cast<const uint32*>(dataptr + 24);
                    int64 initialTicks = *reinterpret_cast<const int64*>(dataptr + 28);

                    
                    newSamples = nSamples;
                    
                }
            }
        }
    }
    
    
    void WriteBlock(AudioBuffer<float> &buffer) {
        auto audio_processor = (AudioProcessor *)processor;
        auto data_streams = processor->getDataStreams();
        ASSERT_EQ(data_streams.size(), 1);
        auto streamId = data_streams[0]->getStreamId();

        MidiBuffer eventBuffer;
        
        AudioBuffer<float> inputBuffer (1024, 256);
        int samplesRead = 0;
        while(samplesRead < buffer.getNumSamples()) {
            audio_processor->processBlock(inputBuffer, eventBuffer);
            int newSamples = 0;
            parseEventBuffer(eventBuffer, newSamples);
            int samplesToCopy = (newSamples + samplesRead) > buffer.getNumSamples() ? buffer.getNumSamples() - samplesRead : newSamples;
            for(int channelIndex = 0; channelIndex < 1024; channelIndex++) {
                buffer.copyFrom(channelIndex, samplesRead, inputBuffer, channelIndex, 0, samplesToCopy);
            }
            samplesRead += samplesToCopy;
        }

        current_sample_index += buffer.getNumSamples();
    }

    void buildUSBDataBuffer() {
        packedBuffer.realloc(bytesPerBlock);
        int writeIndex = 0;
        for(int sample = 0; sample < samplesPerBlock; sample++) {            
            //Magic Number (64 bit)
            uint64 magicNumber = 0xd7a22aaa38132a53;
            for(int i = 0; i < 8; i ++) {
                packedBuffer[writeIndex++] = magicNumber & 0xff;
                magicNumber = magicNumber >> 8;

            }
            
            //Timestamp (32 bit)
            uint32 timestamp = sample;
            for(int i = 0; i< 4; i++) {
                packedBuffer[writeIndex++] = timestamp & 0xff;
                timestamp = timestamp >> 8;
            }
            
            //(3 * Aux Channels(16 bit)) * (32 Streams)
            for(int j = 0; j < 32; j++){
                for(int i = 0; i < 6; i++) {
                    packedBuffer[writeIndex++] = 0x00;
                }
            }
            
            //(32 Streams) * (32 Channels Data (16 bit))
            //Equal steps of 64 (0x40 = 0xffff/1024 channels)
            //Normally insert data into data frame so AudioBuffer data is linear with equal steps
            //For last sample, insert data into frame linearly, which will test "real" order
            //from data fram's perpective
            //AudioBuffer channel = data frame channel / 32 + (data frame channel  % 32) * 32
            //Equation works vice/versa
            if(sample == samplesPerBlock - 1) {
                for(int k = 0; k < 32; k++) {
                    for(int j = 0; j < 32; j++) {
                        uint16_t value = 0x40 * (j + k * 32);
                        for(int i = 0; i < 2; i++) {
                            packedBuffer[writeIndex++] = value;
                            value = value >> 8;
                        }
                    }
                }
            }
            else {
                for(int k = 0; k < 32; k++) {
                    for(int j = 0; j < 32; j++) {
                        uint16_t value = 0x40 * (k + 32*j);
                        for(int i = 0; i < 2; i++) {
                            packedBuffer[writeIndex++] = value;
                            value = value >> 8;
                        }
                    }
                }
            }
            
            
            //8 * ADC Results(16 bit)
            for(int j = 0; j < 8; j++){
                for(int i = 0; i < 2; i++) {
                    packedBuffer[writeIndex++] = 0x00;
                }
            }
            
            //2 * TTL Results(16 bit)
            for(int j = 0; j < 2; j++){
                for(int i = 0; i < 2; i++) {
                    packedBuffer[writeIndex++] = 0x00;
                }
            }
        }
    }
    
    
    HeapBlock<unsigned char> packedBuffer;
    std::unique_ptr<ProcessorTester> tester;
    RhythmNode::IntanRecordController* deviceThread;
    SourceNode* processor;
    int64_t current_sample_index = 0;
    
    //16-bit words
    //data frame = 4 words (magic number) + 2 words (time stamp) + 35 words/stream * 32 streams + 8 words (ADC results)+ 2 words (TTL input/output)
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

    //Check index to
    ASSERT_EQ(deviceThread->lastReadIndex, 581632);

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


TEST_F(RhythmPluginTests, DataIntegrityTest) {
    current_sample_index = 0;
    buildUSBDataBuffer();
    deviceThread->testBuffer = packedBuffer.get();

    AudioBuffer<float> inputBuffer (1024, 256);
    
    tester->startAcquisition(false);
    WriteBlock(inputBuffer);
    tester->stopAcquisition();
    
    
    //CH0
    //Input: 0x0000, Output: -6389 uV
    ASSERT_NEAR(inputBuffer.getSample(0, 0), -6389, 1);
    ASSERT_NEAR(inputBuffer.getSample(0, 127), -6389, 1);
    ASSERT_NEAR(inputBuffer.getSample(0, 200), -6389, 1);
    
    //Last Sample
    //Input: 0x0000, Output: -6389 uV
    ASSERT_NEAR(inputBuffer.getSample(0, 255), -6389, 1);
    

    
    //CH220
    //Input: 0x3200, Output: -3644 uV
    ASSERT_NEAR(inputBuffer.getSample(220, 0), -3644, 1);
    ASSERT_NEAR(inputBuffer.getSample(220, 127), -3644, 1);
    ASSERT_NEAR(inputBuffer.getSample(220, 200), -3644, 1);
    
    //Last Sample
    //Input: 0xe180, Output = 4867uV
    ASSERT_NEAR(inputBuffer.getSample(220, 255), 4867, 1);
    
    
    //CH512
    //Input: 0x8000, Ouput = 0 uV
    ASSERT_NEAR(inputBuffer.getSample(512, 0), 0, 1);
    ASSERT_NEAR(inputBuffer.getSample(512, 127), 0, 1);
    ASSERT_NEAR(inputBuffer.getSample(512, 200), 0, 1);
    
    //Last Sample
    //Input: 0x400, Output = -6190 uV
    ASSERT_NEAR(inputBuffer.getSample(512, 255), -6190, 1);
    
    //CH723
    //Input: 0xB4C0, Output: 2633 uV
    ASSERT_NEAR(inputBuffer.getSample(723, 0), 2633, 1);
    ASSERT_NEAR(inputBuffer.getSample(723, 127), 2633, 1);
    ASSERT_NEAR(inputBuffer.getSample(723, 200), 2633, 1);
    
    //Last Sample
    //Input: 0x9CC0, Output = 1435uV
    ASSERT_NEAR(inputBuffer.getSample(723, 255), 1472, 1);
    
    //CH1023
    //Input: 0xFFC0, Ouput = 6377 uV
    ASSERT_NEAR(inputBuffer.getSample(1023, 0), 6377, 1);
    ASSERT_NEAR(inputBuffer.getSample(1023, 0), 6377, 1);
    ASSERT_NEAR(inputBuffer.getSample(1023, 0), 6377, 1);
    
    //Last Sample
    //Input: 0xFFC0, Ouput = 6377 uV
    ASSERT_NEAR(inputBuffer.getSample(1023, 0), 6377, 1);

    
}

