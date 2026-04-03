#include <iostream>
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"

using namespace std;

int main()
{
    cout << "=== 简单单元测试 ===" << endl;

    // ---------------------
    // 测试1：Ball 初始化
    // ---------------------
    Ball b({100, 200}, {2, -3}, 10);
    cout << "Test 1 Ball 初始化: ";
    if (b.GetRadius() == 10) cout << "PASS ✅" << endl;
    else cout << "FAIL ❌" << endl;

    // ---------------------
    // 测试2：Paddle 初始化
    // ---------------------
    Paddle p(300, 500, 100, 20);
    cout << "Test 2 Paddle 初始化: ";
    if (p.GetRectangle().width == 100) cout << "PASS ✅" << endl;
    else cout << "FAIL ❌" << endl;

    // ---------------------
    // 测试3：Brick 初始化
    // ---------------------
    Brick br(50,50,60,20,RED);
    cout << "Test 3 Brick 初始化: ";
    if (br.GetRectangle().x == 50) cout << "PASS ✅" << endl;
    else cout << "FAIL ❌" << endl;

    cout << endl << "=== 测试完成 ===" << endl;
    return 0;
}