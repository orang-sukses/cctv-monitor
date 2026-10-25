#pragma once
#include <atomic>
#include <string>

struct AVFormatContext;
struct AVStream;
struct AVPacket;

// Paket FFmpeg dengan pembebasan otomatis.
class Packet {
public:
    Packet();
    ~Packet();
    Packet(const Packet &) = delete;
    Packet &operator=(const Packet &) = delete;
    AVPacket *get() const { return p_; }
    AVPacket *operator->() const { return p_; }
    void unref();

private:
    AVPacket *p_;
};

// Koneksi RTSP (TCP, tanpa buffer) dengan timeout dan pembatalan lewat flag stop.
class StreamInput {
public:
    explicit StreamInput(const std::atomic<bool> &stop) : stop_(stop) {}
    ~StreamInput();
    StreamInput(const StreamInput &) = delete;
    StreamInput &operator=(const StreamInput &) = delete;

    bool open(const std::string &url);   // buka + cari stream video
    bool read(Packet &pkt);              // false: putus / timeout / dihentikan
    const AVStream *video() const;
    int videoIndex() const { return videoIndex_; }

private:
    static int interruptCb(void *self);

    AVFormatContext *fmt_ = nullptr;
    int videoIndex_ = -1;
    const std::atomic<bool> &stop_;
    std::atomic<double> deadline_{1e18};
};
