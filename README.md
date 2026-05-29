# 2D Breakout — 打砖块游戏

一个使用 **raylib** 和 **ENet** 开发的跨平台打砖块游戏，支持单机闯关、竞速联机、对战联机三种模式，拥有完整的 UI 系统、网络同步、关卡编辑器和音量控制。


## ✨ 功能特性

- **三种游戏模式**  
  - 单人模式：经典打砖块闯关，支持读档/存档  
  - 竞速模式：两名玩家左右分屏，比拼过关时间与死亡次数  
  - 对战模式：上下分屏竞技，共享砖块区域，互相释放技能干扰

- **丰富的技能系统**  
  砖块掉落技能球：加长/缩短挡板、增大/缩小弹球、爆炸（双倍伤害）、无敌、分裂弹球

- **现代 UI 风格**  
  - 圆角半透明面板、按钮悬停/按下效果  
  - 背景图片（`1.png` / `3.png`）  
  - 可拖拽的音量滑块，设置自动保存

- **关卡编辑器**  
  按 `E` 进入编辑器，支持增删砖块、清空、填充、随机生成，保存布局到 `config.json`

- **网络联机**  
  基于 ENet 实现可靠 UDP 传输，支持状态同步（快照插值）与丢包模拟

- **高级视觉效果**  
  - 莫兰迪配色砖块（按血量显示不同颜色及数字）  
  - 径向渐变弹球（中心亮边缘淡）  
  - 纹理挡板（`2.png`）  
  - 粒子特效、弹球拖尾

## 🛠️ 技术栈

| 组件           | 技术                                 |
| -------------- | ------------------------------------ |
| 图形与音频     | [raylib 4.5](https://www.raylib.com) |
| 网络传输       | [ENet](http://enet.bespin.org)       |
| 配置与序列化   | nlohmann/json                        |
| 多线程         | C++11 std::thread, std::future       |
| 构建系统       | CMake 3.10+                          |
| 平台           | Linux (Ubuntu 24.04) / Windows (WSL) |

### 技术栈详解

#### 语言与标准
- **C++17**：使用 `std::variant`（可选）、结构化绑定、`std::filesystem`（存档路径）、`std::invoke_result` 等特性。
- **跨平台兼容**：所有代码均避免使用平台相关 API，ENet 和 raylib 提供跨平台抽象。

#### 第三方库集成
- **raylib**：提供窗口管理、OpenGL 渲染、音频播放、输入处理、数学工具（`raymath.h`）。
- **ENet**：轻量级 UDP 网络库，支持可靠/不可靠信道、连接管理、多路复用。
- **nlohmann/json**：现代 C++ JSON 库，用于解析 `config.json` 以及存档/读档。

#### 多线程与异步
- **线程池**：`ThreadPool` 封装任务队列（`ThreadSafeQueue`），用于关卡数据生成和纹理加载的后台计算。
- **异步关卡加载**：`std::future` + `std::async` 或线程池提交，避免复杂关卡生成阻塞主线程帧率。
- **纹理缓存异步加载**：`TextureCache::RequestTextureAsync` 在后台线程读取图片文件，主线程批量上传 GPU，通过回调或轮询完成。

#### 网络协议设计
- **二进制序列化**：手写 `Serialize/Deserialize` 函数，支持固定长度整数（大端网络字节序）和浮点数转换。
- **协议分层**：
  - `VersusNetMessage`：控制消息（种子、输入、效果、游戏结束）。
  - `GameStateSnapshot`：完整游戏快照，约 200 字节，每秒发送 10～20 次。
  - `SkillBallSpawnMsg` / `ParticleSpawnMsg`：事件型消息，不可靠但低延迟。
- **同步策略**：主机权威，客户端预测 + 快照插值（缓冲区 50ms～100ms）。

#### 数据结构与内存管理
- **固定容量容器**：粒子系统（`Particle[512]`）、拖尾（`TrailBuffer` 环形数组）、快照缓冲区（`std::deque` 限制大小）—— 避免运行时动态分配抖动。
- **智能指针**：`std::unique_ptr<Effect>` 管理效果生命周期，多态容器自动释放。
- **对象池**：`SkillBall` 和 `Particle` 使用 `vector` 存储，通过 `active` 标志复用，减少频繁 push/erase。

#### 设计模式应用
- **工厂模式**：`EffectFactory` 根据 `SkillType` 创建不同效果对象。
- **策略模式**：`Effect` 基类定义效果行为，派生类实现具体策略。
- **状态机**：`GameState` 枚举驱动单人模式界面流转。
- **观察者模式**：`VersusGame` 通过 `std::function` 回调通知外部（生命丢失、球发射、效果应用等），解耦游戏逻辑与网络层。
- **单例模式**：`TextureCache` 全局纹理缓存，确保同一图片只加载一次。

#### 构建与部署
- **CMake**：自动查找 raylib 和 ENet 的 pkg-config 配置，生成可执行文件。
- **资源管理**：图片、音频文件放置在运行目录，支持相对路径读取。
- **配置文件**：`config.json` 无需编译即可调整关卡布局、技能参数、屏幕尺寸。

---


## 📦 编译与运行

### 编译

```
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行

```
./game
```

## 🎮 操作说明

| 操作               | 按键                          |
| ------------------ | ----------------------------- |
| 移动挡板（单人/竞速） | ←/→ 或 A/D                    |
| 发射弹球（待机状态） | 空格键                        |
| 暂停/继续          | 空格键（游戏中）              |
| 切换编辑器         | E（单机菜单或暂停时）          |
| 保存布局（编辑器）  | S                             |
| 退出编辑器         | E                             |
| 返回上一级菜单     | B                             |
| 音量设置           | 模式选择 → SETTINGS（O键）     |

**竞速模式（主机）**  
- 等待对方连接后按 **S** 或点击 **START** 开始  
- 空格键暂停/恢复（主机控制）

**对战模式**  
- 主机同样按 **S** 开始  
- 上下分屏：上方玩家（主机）使用 A/D 或 ←/→，下方玩家（客户端）使用 A/D 或 ←/→（由客户端通过输入消息发送）


## 🧩 项目结构（部分核心文件）

```
├── game.h / game.cpp            // 单人模式核心
├── RacePlayer.h / .cpp          // 竞速模式玩家逻辑
├── RaceManager.h / .cpp         // 竞速联机会话
├── VersusGame.h / .cpp          // 对战模式游戏逻辑
├── VersusManager.h / .cpp       // 对战联机会话
├── VersusNetMessage.h / .cpp    // 网络协议序列化
├── NetworkThread.h / .cpp       // ENet 线程封装
├── Effect.h / EffectFactory.h   // 效果系统
├── Ball.h / Paddle.h / Brick.h  // 基本游戏对象
├── SkillBall.h / .cpp           // 技能球（派生自 Ball）
├── Particle.h / .cpp            // 粒子系统
├── Grid.h / .cpp                // 空间网格碰撞优化
├── TrailBuffer.h                // 拖尾循环缓冲区
├── LevelManager.h / .cpp        // 关卡配置与加载
├── SoundManager.h / .cpp        // 音效管理
├── TextureCache.h / .cpp        // 纹理缓存与异步加载
├── ThreadPool.h / .cpp          // 线程池
└── config.json                  // 游戏配置与关卡数据
```

## 💻依赖安装（Ubuntu）

```bash
sudo apt update
sudo apt install libraylib-dev libenet-dev cmake g++ make
```

## 👨‍💻 作者

### 开发环境：Ubuntu 24.04 + raylib 4.5 + ENet 1.3.17  
### 联系方式：`yqOffline`（GitHub）
