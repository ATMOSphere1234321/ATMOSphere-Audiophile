#pragma once

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/samplefmt.h>
}

/** PCM carrier selected before FFmpeg conversion; NONE means unsupported. */
struct PcmOutputFormat {
    AVSampleFormat sample_format = AV_SAMPLE_FMT_NONE;
    int android_encoding = 0;
};

/**
 * Selects a float carrier for floating PCM even when raw depth is absent.
 * The caller supplies the packed sample format; compressed-codec policy remains
 * unchanged. F64 is reduced to the supported F32 carrier; this is not lossless.
 * Native DSD bypasses this PCM resolver; DSD-to-PCM overrides it later.
 */
inline PcmOutputFormat choose_pcm_output_format(
        AVCodecID codec_id, AVSampleFormat packed, int src_bits) noexcept
{
    PcmOutputFormat result;
    switch (codec_id) {
        case AV_CODEC_ID_PCM_F32LE:
        case AV_CODEC_ID_PCM_F32BE:
        case AV_CODEC_ID_PCM_F64LE:
        case AV_CODEC_ID_PCM_F64BE:
            return {AV_SAMPLE_FMT_FLT, 4};
        default:
            break;
    }
    if (src_bits <= 0) {
        switch (packed) {
            case AV_SAMPLE_FMT_U8:
                src_bits = 8;
                break;
            case AV_SAMPLE_FMT_S16:
                src_bits = 16;
                break;
            case AV_SAMPLE_FMT_S32:
                // S32P is used by some lossless decoders for 24-bit sources even
                // when bits_per_raw_sample is absent — treat conservatively as 24.
                src_bits = 24;
                break;
            case AV_SAMPLE_FMT_FLT:
            case AV_SAMPLE_FMT_DBL:
                // Preserve the existing compressed-codec S16 output policy.
                // Missing raw depth alone does not describe source precision.
                src_bits = 16;
                break;
            default:
                return {};
        }
    }

    if (src_bits <= 16) {
        // Standard CD-quality and all lossy compressed formats.
        result.sample_format             = AV_SAMPLE_FMT_S16;
        result.android_encoding = 2;
    } else if (src_bits <= 24) {
        // High-resolution lossless (24-bit FLAC, ALAC, DSD-PCM).
        // Carried as zero-padded S32 — the only alignment-safe Android path.
        result.sample_format             = AV_SAMPLE_FMT_S32;
        result.android_encoding = 22;
    } else {
        // 32-bit sources: keep integer as S32, float as FLT.
        if (packed == AV_SAMPLE_FMT_FLT || packed == AV_SAMPLE_FMT_DBL) {
            result.sample_format             = AV_SAMPLE_FMT_FLT;
            result.android_encoding = 4;
        } else {
            result.sample_format             = AV_SAMPLE_FMT_S32;
            result.android_encoding = 22;
        }
    }
    return result;
}
