#include "Brick.h"

Brick::Brick(float x, float y, float w, float h,Color col) {
    rect = { x, y, w, h };
    active = true;
    color = col;
}

void Brick::Draw() {
    if (active) {
       DrawRectangleRec(rect,color);
    }
}
