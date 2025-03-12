#include "Particle.h"

SYSTEM_MODE(AUTOMATIC);

// System thread defaults to on in 6.2.0 and later and this line is not required
#ifndef SYSTEM_VERSION_v620
SYSTEM_THREAD(ENABLED);
#endif

SerialLogHandler logHandler(LOG_LEVEL_INFO);

// Configuration
const int baudRate = 230400;
const std::chrono::milliseconds reportPeriod = 10s;
const std::chrono::milliseconds noDataResetPeriod = 4s;

// At 230400 baud, that's around 23040 bytes/sec
// This means each millisecond 24 bytes will be read

union PacketBuffer {
    uint8_t b[8];
    struct Value {
        uint8_t marker[4];
        uint32_t sequence;
    } value;
};
PacketBuffer packetBuf;


size_t packetOffset = 0;
enum class PacketState {
    MARKER_WAIT,
    SEQUENCE_WAIT,
};
PacketState packetState = PacketState::MARKER_WAIT;
uint32_t lastSequence = 0;
bool hasLastSequence = false;
int goodCount = 0;
int badCount = 0;
unsigned long lastReport = 0;
unsigned long lastData = 0;
unsigned long dataStart = 0;

hal_usart_buffer_config_t acquireSerial1Buffer()
{
    const size_t bufferSize = 512;
    hal_usart_buffer_config_t config = {
        .size = sizeof(hal_usart_buffer_config_t),
        .rx_buffer = new (std::nothrow) uint8_t[bufferSize],
        .rx_buffer_size = bufferSize,
        .tx_buffer = new (std::nothrow) uint8_t[bufferSize],
        .tx_buffer_size = bufferSize
    };

    return config;
}

void setup() {
    Serial1.begin(baudRate);
}

void loop() {
    int c;

    while(Serial1.available()) {
        switch(packetState) {    
            case PacketState::MARKER_WAIT:        
                c = Serial1.read();
                if (c == '*') {
                    packetBuf.b[packetOffset++] = (uint8_t)c;
                    if (packetOffset == sizeof(PacketBuffer::Value::marker)) {
                        packetState = PacketState::SEQUENCE_WAIT;
                    }
                } 
                else {
                    // Log.info("going back to START from MARKER_WAIT");
                    badCount++;
                    packetOffset = 0;
                    packetState = PacketState::MARKER_WAIT;
                }            
                break;
    
            case PacketState::SEQUENCE_WAIT:
                c = Serial1.read();
                if (c >= 0) {
                    packetBuf.b[packetOffset++] = (uint8_t)c;
                    if (packetOffset == sizeof(packetBuf)) {
                        // Validate sequence number in packetBuf.value.sequence
                        if (hasLastSequence) {
                            if (packetBuf.value.sequence == (lastSequence + 1)) {
                                goodCount++;
                                lastSequence = packetBuf.value.sequence;
                                hasLastSequence = true;
                                packetOffset = 0;
                            }
                            else {
                                Log.info("wrong sequence, expected %08lx got %08lx", lastSequence + 1, packetBuf.value.sequence);
                                hasLastSequence = false;
                                badCount++;

                                if (packetBuf.b[sizeof(PacketBuffer::Value::marker)] == '*') {
                                    // Reparse the data shifted
                                    packetOffset = sizeof(packetBuf) - 1;
                                    memmove(packetBuf.b, &packetBuf.b[1], packetOffset);
                                }
                                else {
                                    // Discard all of the data
                                    packetOffset = 0;
                                }
                            }
                        }
                        else {
                            lastSequence = packetBuf.value.sequence;
                            hasLastSequence = true;
                            packetOffset = 0;
                        }

                        packetState = PacketState::MARKER_WAIT;
                    }
                }
                break;
        }
        lastData = millis();
        if (dataStart == 0) {
            dataStart = lastData;
        }
    }
    if (lastData && (millis() - lastData) >= noDataResetPeriod.count()) {
        Log.info("resetting stats");
        goodCount = badCount = 0;
        dataStart = lastData = 0; 
        hasLastSequence = false;
        packetOffset = 0;
        packetState = PacketState::MARKER_WAIT;
    }

    if (reportPeriod.count() > 0) {
        if (millis() - lastReport >= reportPeriod.count()) {
            lastReport = millis();

            int totalCount = goodCount + badCount;
            if (totalCount > 0) {
                float kbytesRcvd = (goodCount * sizeof(packetBuf) / 1000);
                int bytesPerSec = (goodCount * sizeof(packetBuf) / ((millis() - dataStart) / 1000));

                Log.info("good=%d bad=%d pct=%d kbytesRcvd=%.1f %d bytes/sec", 
                    goodCount, badCount, goodCount * 100 / totalCount, kbytesRcvd, bytesPerSec);
                
            } 
        }
    }
}
