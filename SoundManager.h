#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include "raylib.h"
#include <vector>

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
    void PlayUIHover();
    void PlayUIClick();
    void SetMasterVolume(float volume);   // 新增：设置所有音效音量
    void RegisterSound(Sound snd);       // 新增：注册音效

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
    Sound sndUIHover;
    Sound sndUIClick;
    Sound GenerateSimpleTone(float frequency, float duration, float volume);
    std::vector<Sound> allSounds; 
};

#endif