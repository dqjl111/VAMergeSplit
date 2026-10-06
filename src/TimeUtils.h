#pragma once

#include <QString>

namespace TimeUtils {

// 解析时间字符串，支持 "SS"、"SS.mmm"、"MM:SS"、"HH:MM:SS.mmm"
// 解析失败返回 false（通过 ok）
double parseTime(const QString &text, bool *ok = nullptr);

// 格式化为 "HH:MM:SS.mmm"
QString formatTime(double seconds);

// 时间（秒）转帧号（四舍五入）
qint64 frameFromTime(double seconds, double fps);

// 帧号转时间（秒）
double timeFromFrame(qint64 frame, double fps);

} // namespace TimeUtils
