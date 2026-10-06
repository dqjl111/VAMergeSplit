#pragma once

#include <QList>
#include <QString>

struct MediaInfo {
    bool ok = false;
    QString error;

    double duration = 0.0; // 秒
    double fps = 0.0;      // 视频帧率，音频文件为 0
    int width = 0;
    int height = 0;
    bool hasVideo = false;
    bool hasAudio = false;
    QString videoCodec;
    QString audioCodec;
};

// 同步调用 ffprobe 解析媒体信息（耗时很短，可在 GUI 线程调用）
MediaInfo probeMedia(const QString &ffprobePath, const QString &filePath);

// 返回视频所有关键帧的时间（秒，升序）；无视频流或失败时返回空列表
QList<double> probeKeyframeTimes(const QString &ffprobePath, const QString &filePath);
