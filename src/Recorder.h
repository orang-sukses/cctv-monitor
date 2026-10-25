#pragma once
#include <QString>

struct AVFormatContext;
struct AVPacket;
struct AVStream;

// Merekam stream tanpa re-encode (remux ke .mkv), dipecah per 10 menit pada keyframe.
class Recorder {
public:
    Recorder(QString dir, const QString &camName);
    ~Recorder() { close(); }
    Recorder(const Recorder &) = delete;
    Recorder &operator=(const Recorder &) = delete;

    // `pkt` dipakai/dikonsumsi (timestamp diubah, isinya diserahkan ke muxer).
    void write(AVPacket *pkt, const AVStream *in, bool keyframe, double now);
    void close();

private:
    bool open(const AVStream *in);

    static constexpr double kSegmentSec = 600;
    QString dir_, safeName_;
    AVFormatContext *oc_ = nullptr;
    AVStream *out_ = nullptr;
    double start_ = 0;
};
