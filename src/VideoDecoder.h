#pragma once

struct AVBufferRef;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct AVStream;

// Decoder video. Memakai GPU (D3D11VA di Windows) bila tersedia, otomatis fallback ke software.
class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();
    VideoDecoder(const VideoDecoder &) = delete;
    VideoDecoder &operator=(const VideoDecoder &) = delete;

    bool open(const AVStream *vs, bool tryHardware);
    void setKeyframesOnly(bool on);        // kamera tersembunyi / tertinggal: decode keyframe saja
    bool send(const AVPacket *pkt);
    bool receive();                        // ambil frame berikutnya
    const AVFrame *systemFrame();          // frame di RAM (unduh dari GPU hanya bila dipanggil); null bila gagal

private:
    AVCodecContext *ctx_ = nullptr;
    AVBufferRef *hw_ = nullptr;
    AVFrame *frame_ = nullptr, *sw_ = nullptr;
};
