#ifndef CAMERA_H
#define CAMERA_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <utility>

/**
 * 一帧视频数据（视频帧）。
 *
 * data 指向的数据只在回调执行期间有效，回调返回后由摄像头驱动回收，
 * 需要异步发送时必须先拷贝。
 */
struct VideoFrame {
    const uint8_t* data;      // JPEG 负载首地址
    size_t len;               // JPEG 负载长度（字节）
    uint32_t timestamp_ms;    // 单调时间戳（毫秒）
};

class Camera {
public:
    virtual ~Camera() = default;

    virtual void SetExplainUrl(const std::string& url, const std::string& token) = 0;
    virtual bool Capture() = 0;
    virtual bool SetHMirror(bool enabled) = 0;
    virtual bool SetVFlip(bool enabled) = 0;
    virtual bool SetSwapBytes(bool enabled) { return false; }  // Optional, default no-op
    virtual std::expected<std::string, std::string> Explain(const std::string& question) = 0;

    /**
     * 启动视频推流（默认实现返回 false，即该摄像头不支持推流）。
     *
     * @param fps    目标帧率
     * @param width  期望宽度（当前实现以板级初始化参数为准）
     * @param height 期望高度（当前实现以板级初始化参数为准）
     * @return true 表示已启动或已在运行
     */
    virtual bool StartStream(int fps, int width, int height) {
        (void)fps;
        (void)width;
        (void)height;
        return false;
    }

    /** 停止视频推流（默认空实现）。 */
    virtual void StopStream() {}

    /**
     * 注册视频帧回调（默认实现仅保存回调）。
     *
     * 注意：回调可能运行在摄像头采集线程，不要在其中做网络发送或阻塞操作。
     */
    virtual void OnFrame(std::function<void(const VideoFrame&)> callback) {
        on_frame_ = std::move(callback);
    }

protected:
    std::function<void(const VideoFrame&)> on_frame_;
};

#endif  // CAMERA_H
