#ifndef TEXTURE_CACHE_H
#define TEXTURE_CACHE_H

#include "raylib.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <vector>
#include <thread>
#include <future>

class TextureCache {
public:
    static TextureCache& Instance();

    // 同步获取纹理（若未缓存则主线程直接加载）
    Texture2D GetTexture(const std::string& path);

    // 异步请求纹理（工作线程加载 Image，主线程调用 UploadPendingTextures 完成上传）
    void RequestTextureAsync(const std::string& path, std::function<void(Texture2D)> onLoaded = nullptr);

    // 使用 std::packaged_task 返回 future 的异步加载（展示用）
    std::future<Texture2D> LoadTextureAsyncPackaged(const std::string& path);

    // 每帧主线程调用，完成所有待上传纹理的上传工作
    void UploadPendingTextures();

    // 清空缓存（卸载所有纹理）
    void Clear();

private:
    TextureCache() = default;
    ~TextureCache();
    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;

    std::unordered_map<std::string, Texture2D> cache_;
    std::mutex cacheMutex_;

    // 待上传任务队列（用于 RequestTextureAsync）
    std::vector<std::function<void()>> pendingUploads_;
    std::mutex uploadMutex_;
};

#endif // TEXTURE_CACHE_H