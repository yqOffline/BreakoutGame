#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"

class Paddle {
    private:
        Rectangle rect;
    public:
        Paddle();
        Paddle(float x, float y, float w, float h);
        void Draw();
        void MoveLeft(float speed);
        void MoveRight(float speed);
        Rectangle GetRectangle() const {return rect;};
        Rectangle GetRectangle() {return rect;};
        void SetWidth(float w){ rect.width = w; }
        float GetWidth() const { return rect.width; }

        void SetPosition(float x, float y) { rect.x = x; rect.y = y; }
        void SetRect(Rectangle r){ rect.width = r.width;rect.height = r.height; }
};

#endif