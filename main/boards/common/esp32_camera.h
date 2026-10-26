#pragma once
#include "sdkconfig.h"

#include <atomic>
#include <lvgl.h>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "camera.h"
#include "esp_camera.h"
#include "jpg/image_to_jpeg.h"

struct JpegChunk {
    uint8_t* data;
    size_t len;
};

class Esp32Camera : public Camera {
private:
    bool streaming_on_ = false;
    bool swap_bytes_enabled_ = true;  // Swap pixel byte order for RGB565, enabled by default
    std::string explain_url_;
    std::string explain_token_;
    std::thread encoder_thread_;
    camera_fb_t* current_fb_ = nullptr;
    uint8_t* encode_buf_ = nullptr;  // Buffer for JPEG encoding (with optional byte swap)
    size_t encode_buf_size_ = 0;

    // 视频推流相关（StartStream/StopStream）
    std::atomic<bool> stream_running_{false};
    std::thread stream_thread_;
    int stream_fps_ = 10;
    // 保护推流线程内部的 JPEG 编码缓冲，避免与 Capture()/Explain() 共享数据
    std::mutex stream_mutex_;
    std::vector<uint8_t> stream_jpeg_buf_;

public:
    Esp32Camera(const camera_config_t& config);
    ~Esp32Camera();

    virtual void SetExplainUrl(const std::string& url, const std::string& token) override;
    virtual bool Capture() override;
    virtual bool SetHMirror(bool enabled) override;
    virtual bool SetVFlip(bool enabled) override;
    virtual bool SetSwapBytes(bool enabled) override;
    virtual std::expected<std::string, std::string> Explain(const std::string& question) override;

    // 视频推流：仅在 CONFIG_XIAOZHI_VIDEO_STREAM 打开时真正启用
    virtual bool StartStream(int fps, int width, int height) override;
    virtual void StopStream() override;
};
