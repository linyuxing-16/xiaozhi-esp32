# 视频帧上行（Video Stream）

本文简述固件侧视频推流行为。帧格式与服务器侧约定见 [websocket.md](websocket.md) 第 11 节。

## 1. 触发时机

推流由设备状态驱动（`main/application.cc` 的 `HandleStateChangedEvent()`（状态变更处理））：

| 设备状态 | 行为 |
| --- | --- |
| `kDeviceStateListening`（监听） | 调用 `StartVideoStream()`（启动推流） |
| `kDeviceStateSpeaking`（说话） | 保持推流，重复调用不会重启 |
| `kDeviceStateIdle`（空闲）及其他状态 | 调用 `StopVideoStream()`（停止推流） |

此外，音频通道关闭（`OnAudioChannelClosed`）与 `ResetProtocol()`（协议重置）时也会停止推流。

## 2. 启动条件

`StartVideoStream()` 依次检查：

1. 编译开关 `CONFIG_XIAOZHI_VIDEO_STREAM` 已打开；
2. `protocol_` 非空且音频通道已打开；
3. `Board::GetInstance().GetCamera()` 返回非空摄像头；
4. 摄像头 `StartStream(fps, width, height)` 返回 `true`（抽象默认返回 `false`，即不支持）。

任意一项不满足则静默跳过，不影响音频会话。

## 3. 数据通路

```
摄像头采集线程                     主任务（Application::Run）
esp_camera_fb_get()
  -> VideoFrame{data, len, ts}
  -> on_frame_ 回调（拷贝入队）
  -> video_queue_（最多 2 帧）  --->  Schedule()
                                     -> protocol_->SendVideoFrame()
                                     -> WebsocketProtocol 组 BinaryProtocol4
                                     -> websocket_->Send(..., binary=true)
```

要点：

- 帧缓冲 `fb` 只在回调期间有效，因此回调内必须**深拷贝**（`std::vector<uint8_t>`）。
- 网络发送不在摄像头线程执行，而是通过 `Application::Schedule()` 回到主任务，避免阻塞采集。
- 队列容量固定为 2 帧，满时丢弃最旧帧（丢旧帧背压），因此延迟不会随网络拥塞无限增长。
- 发送失败（返回 `false`）时丢弃本轮剩余帧，不重试、不重排队。

## 4. 帧率与分辨率

- 帧率：`CONFIG_XIAOZHI_VIDEO_STREAM_FPS`（默认 10，范围 1–30）。采集线程用 `vTaskDelay(pdMS_TO_TICKS(1000 / fps))` 控制节奏。
- 分辨率：`CONFIG_XIAOZHI_VIDEO_STREAM_WIDTH` / `_HEIGHT`（默认 320×240）目前只作为接口参数传入，实际分辨率仍由板级 `camera_config_t.frame_size` 决定（`lckfb/szpi-esp32s3` 为 `FRAMESIZE_QVGA`）。
- JPEG 质量：非 JPEG 传感器路径使用质量 80 的软件编码。

## 5. JPEG 直出与 LCD 预览

`lckfb/szpi-esp32s3` 的 `InitializeCamera()`（摄像头初始化）改为方案 B：

```c
config.pixel_format = PIXFORMAT_JPEG;
config.fb_count = 2;
config.grab_mode = CAMERA_GRAB_LATEST;
```

代价是 **LCD 拍照预览失效**（`Capture()` 中 JPEG 分支只记录日志、不做解码预览），这是已接受的方案 B 取舍。
MCP 拍照（`self.camera.take_photo`）与视觉识别（`Explain`）仍然工作。

## 6. 已知限制 / TODO

- **未实现带宽自适应**：网络拥塞时不会自动降低 fps 或分辨率，目前需手动调小 `CONFIG_XIAOZHI_VIDEO_STREAM_FPS`。
- **未实现服务器端订阅/协商后的动态启停**：只要进入 listening/speaking 就固定推流，服务器无法在会话中临时关闭（仅能忽略 `type=2` 帧）。
- **未实现 I 帧请求/关键帧对齐**：JPEG 每帧独立，无需对齐，但也没有帧间压缩，带宽占用较高。
- **未实现本地录像/回放**。
- 摄像头与 MQTT/UDP 传输组合下 `SendVideoFrame()` 默认返回 `false`，视频上行仅在 WebSocket 下可用。

## 7. 真机验证清单

- [ ] 进入 listening 状态后服务器能持续收到 `type=2` 二进制帧，且 JPEG 可正常解码。
- [ ] speaking 状态推流不中断，idle 后立即停止（可用抓包确认没有多余帧）。
- [ ] 音频无明显卡顿、`Application::Run()` 主循环无阻塞。
- [ ] 网络较差时观察丢帧是否符合预期（不积压、延迟不增长）。
- [ ] LCD 拍照预览确实失效、MCP 拍照与 Explain 仍可用。
