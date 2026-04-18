#include "SoundManager.h"
#include <cmath>
#include <vector>

// ======================== 构造函数 ========================
SoundManager::SoundManager() {
    // 加载普通音效文件（请确保文件存在于指定路径，或修改为你的实际路径）
    // 支持 WAV、MP3、OGG 等格式
    sndBallPaddle   = LoadSound("碰墙.wav");
    sndBallBrick    = LoadSound("碰墙.wav");
    sndBallWall     = LoadSound("碰墙.wav");
    sndBallBall     = LoadSound("碰墙.wav");
    sndLevelComplete = LoadSound("通关(Level).wav");
    sndGameVictory  = LoadSound("通关掌声.wav");
    sndLevelOver = LoadSound("levelover.wav");
    sndGameOver = LoadSound("失败-生活.wav");

    // 程序生成上升/下降音效
    sndPowerupGet = GenerateRisingSound();
    sndPowerupEnd = GenerateFallingSound();
}

// ======================== 析构函数 ========================
SoundManager::~SoundManager() {
    UnloadSound(sndBallPaddle);
    UnloadSound(sndBallBrick);
    UnloadSound(sndBallWall);
    UnloadSound(sndBallBall);
    UnloadSound(sndLevelComplete);
    UnloadSound(sndGameVictory);
    UnloadSound(sndPowerupGet);
    UnloadSound(sndPowerupEnd);
    UnloadSound(sndLevelOver);
    UnloadSound(sndGameOver);
}

// ======================== 生成上升音效（频率从 400Hz 升至 800Hz） ========================
Sound SoundManager::GenerateRisingSound() {
    const int sampleRate = 22050;
    const int totalSamples = sampleRate / 4; // 0.25 秒
    Wave wave;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.frameCount = totalSamples;
    
    short* data = (short*)malloc(totalSamples * sizeof(short));
    for (int i = 0; i < totalSamples; i++) {
        float t = (float)i / sampleRate;
        // 频率线性增加：从 400Hz 到 800Hz
        float freq = 400.0f + (800.0f - 400.0f) * t / (totalSamples / (float)sampleRate);
        float value = sinf(2.0f * PI * freq * t);
        // 应用淡入淡出包络
        float envelope = sinf(PI * t / (totalSamples / (float)sampleRate));
        value *= envelope;
        data[i] = (short)(value * 20000);
    }
    wave.data = data;
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);  // 释放 wave 内存
    return sound;
}

// ======================== 生成下降音效（频率从 800Hz 降至 400Hz） ========================
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
        float freq = 800.0f - (800.0f - 400.0f) * t / (totalSamples / (float)sampleRate);
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

// ======================== 播放接口 ========================
void SoundManager::PlayBallPaddle() {
    TraceLog(LOG_INFO, "Playing BallPaddle sound");
    PlaySound(sndBallPaddle);
}

void SoundManager::PlayBallBrick() {
    TraceLog(LOG_INFO, "Playing BallBrick sound");
    PlaySound(sndBallBrick);
}

void SoundManager::PlayBallWall() {
    TraceLog(LOG_INFO, "Playing BallWall sound");
    PlaySound(sndBallWall);
}

void SoundManager::PlayBallBall() {
    TraceLog(LOG_INFO, "Playing BallBall sound");
    PlaySound(sndBallBall);
}

void SoundManager::PlayPowerupGet() {
    TraceLog(LOG_INFO, "Playing PowerupGet sound");
    PlaySound(sndPowerupGet);
}

void SoundManager::PlayPowerupEnd() {
    TraceLog(LOG_INFO, "Playing PowerupEnd sound");
    PlaySound(sndPowerupEnd);
}

void SoundManager::PlayLevelComplete() {
    TraceLog(LOG_INFO, "Playing LevelComplete sound");
    PlaySound(sndLevelComplete);
}

void SoundManager::PlayGameVictory() {
    TraceLog(LOG_INFO, "Playing GameVictory sound");
    PlaySound(sndGameVictory);
}

void SoundManager::PlayLevelOver(){
    TraceLog(LOG_INFO, "Playing LevelOver sound");
    PlaySound(sndLevelOver);
}

void SoundManager::PlayGameOver(){
    TraceLog(LOG_INFO, "Playing GameOver sound");
    PlaySound(sndGameOver);
}