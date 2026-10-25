#pragma once
#include "FrameBuffer.h"

struct AVFrame;
struct SwsContext;

// Mengskala frame ke ukuran area tampil sekaligus mengonversi ke RGB (satu kali jalan).
class FrameScaler {
public:
    FrameScaler() = default;
    ~FrameScaler();
    FrameScaler(const FrameScaler &) = delete;
    FrameScaler &operator=(const FrameScaler &) = delete;

    // Isi `out` dengan frame yang sudah diskala dan commit. Return false bila gagal.
    bool scale(const AVFrame *f, int boxW, int boxH, FrameBuffer &out);

private:
    SwsContext *sws_ = nullptr;
    int sw_ = 0, sh_ = 0, dw_ = 0, dh_ = 0, fmt_ = -1;
};
