#include <gtest/gtest.h>
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>

TEST(CollisionTest, BallPaddleCollisionReversesY) {
    Ball ball({400, 540}, {0, 5}, 10);
    Paddle paddle(350, 550, 100, 20);
    float speedY_before = ball.GetSpeed().y;
    EXPECT_GT(speedY_before, 0);
    ball.CheckCollisionPaddle(paddle);
    float speedY_after = ball.GetSpeed().y;
    EXPECT_LT(speedY_after, 0);
    EXPECT_NEAR(std::abs(speedY_after), std::abs(speedY_before), 0.1);
}

TEST(CollisionTest, BallBrickCollisionDeactivatesBrick) {
    Ball ball({100, 100}, {0, 5}, 10);
    Brick brick(90, 110, 20, 10, RED);
    EXPECT_TRUE(brick.IsActive());
    std::vector<Brick> bricks = {brick};
    int score = 0;
    float speedY_before = ball.GetSpeed().y;
    ball.CheckCollisionBricks(bricks, score);
    EXPECT_FALSE(bricks[0].IsActive());
    EXPECT_EQ(score, 1);
    EXPECT_NEAR(ball.GetSpeed().y, -speedY_before, 0.1);
}
