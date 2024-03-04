#ifndef AUDIOINPUT_H
#define AUDIOINPUT_H

#include <vector>

class AudioInput {
    public:
        AudioInput();
        virtual ~AudioInput();
        static void refreshStreams();

    private:
        static inline std::vector<AudioInput*> m_audioInputs;
        virtual void refreshStream() = 0;
};

#endif
