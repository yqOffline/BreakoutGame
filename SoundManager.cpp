#include "SoundManager.h"
#include <cmath>

SoundManager::SoundManager() {
    sndHit          = LoadSound("Hit2.wav");
    sndLevelComplete = LoadSound("LevelWin.wav");
    sndGameVictory  = LoadSound("GameWin.wav");
    sndLevelOver    = LoadSound("LevelOver.wav");
    sndGameOver     = LoadSound("GameOver.wav");

    sndPowerupGet   = GenerateRisingSound();
    sndPowerupEnd   = GenerateFallingSound();
}

SoundManager::~SoundManager() {
    UnloadSound(sndHit);
    UnloadSound(sndLevelComplete);
    UnloadSound(sndGameVictory);
    UnloadSound(sndPowerupGet);
    UnloadSound(sndPowerupEnd);
    UnloadSound(sndLevelOver);
    UnloadSound(sndGameOver);
}

void SoundManager::PlayHitSound() {
    PlaySound(sndHit);
}

void SoundManager::PlayPowerupGet() {
    PlaySound(sndPowerupGet);
}

void SoundManager::PlayPowerupEnd() {
    PlaySound(sndPowerupEnd);
}

void SoundManager::PlayLevelComplete() {
    PlaySound(sndLevelComplete);
}

void SoundManager::PlayGameVictory() {
    PlaySound(sndGameVictory);
}

void SoundManager::PlayLevelOver() {
    PlaySound(sndLevelOver);
}

void SoundManager::PlayGameOver() {
    PlaySound(sndGameOver);
}

// 生成上升/下降音效（同原实现）
Sound SoundManager::GenerateRisingSound() {
    const int sampleRate = 22050;
    const int totalSamples = sampleRate / 4;
    Wave wave;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.frameCount = totalSamples;
    
    short* data = (short*)malloc(totalSamples * sizeof(short));
    for (int i = 0; i < totalSamples; i++) {
        float t = (float)i / sampleRate;
        float freq = 400.0f + 400.0f * t / (totalSamples / (float)sampleRate);
        float value = sinf(2.0f * PI * freq * t);
        float envelope = sinf(PI * t / (totalSamples / (float)sampleRate));
        value *= envelope;
        data[i] = (short)(value * 20000);
    }
    wave.data = data;
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

Sound SoundManager::GenerateFallingSound() {
    const int sampleRate = 22050;
    const int totalSamples = sampleRate / 4;
    Wave wave;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.frameCount = totalSamples;
    
    short* data = (short*)malloc(totalSamples * sizeof(short));
    for (int i = 0; i < totalSamples; i++) {
        float t = (float)i / sampleRate;
        float freq = 800.0f - 400.0f * t / (totalSamples / (float)sampleRate);
        float value = sinf(2.0f * PI * freq * t);
        float envelope = sinf(PI * t / (totalSamples / (float)sampleRate));
        value *= envelope;
        data[i] = (short)(value * 20000);
    }
    wave.data = data;
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}