#include "audioinput.h"

AudioInput::AudioInput() {
    m_audioInputs.push_back(this);
}
AudioInput::~AudioInput() {
    m_audioInputs.erase(std::find(m_audioInputs.begin(), m_audioInputs.end(), this));
}

void AudioInput::refreshStreams() {
    for(AudioInput* i : m_audioInputs) i->refreshStream();
}
