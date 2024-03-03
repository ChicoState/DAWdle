#ifndef BUFFERDATA_H
#define BUFFERDATA_H

#include <QtNodes/NodeData>

#define BUFFERSIZE 48000
#define SAMPLERATE 48000

class BufferData : public QtNodes::NodeData {
    public:
        QtNodes::NodeDataType type() const override {
            return QtNodes::NodeDataType{ "buffer", "Decimal" };
        }

        void setAll(float input) {
            for(size_t i = 0; i < BUFFERSIZE; i++) m_buffer[i] = input;
        }

        float m_buffer[BUFFERSIZE];
};

#endif
