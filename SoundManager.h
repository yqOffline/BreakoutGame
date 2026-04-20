#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include "raylib.h"

class SoundManager {
public:
    SoundManager();
    ~SoundManager();

    // 整合后的碰撞音效
    void PlayHitSound();
    void PlayPowerupGet();
    void PlayPowerupEnd();
    void PlayLevelComplete();
    void PlayGameVictory();
    void PlayLevelOver();
    void PlayGameOver();

private:
    Sound sndHit;               // 统一碰撞音效
    Sound sndPowerupGet;
    Sound sndPowerupEnd;
    Sound sndLevelComplete;
    Sound sndGameVictory;
    Sound sndLevelOver;
    Sound sndGameOver;

    Sound GenerateRisingSound();
    Sound GenerateFallingSound();
};

#endif