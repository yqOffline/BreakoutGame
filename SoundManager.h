#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include "raylib.h"

class SoundManager {
public:
    SoundManager();
    ~SoundManager();

    void PlayBallPaddle();
    void PlayBallBrick();
    void PlayBallWall();
    void PlayBallBall();
    void PlayPowerupGet();
    void PlayPowerupEnd();
    void PlayLevelComplete();
    void PlayGameVictory();
    void PlayLevelOver();
    void PlayGameOver();

private:
    Sound sndBallPaddle;
    Sound sndBallBrick;
    Sound sndBallWall;
    Sound sndBallBall;
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