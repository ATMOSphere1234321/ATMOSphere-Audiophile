package com.androidexpert35.audiophilemusicplayer.data.playback

import android.media.AudioFormat
import com.androidexpert35.audiophilemusicplayer.data.playback.native_.AudioFormatInfo
import com.androidexpert35.audiophilemusicplayer.data.playback.usb.UsbRoutingEvent

/**
 * Identifies the PCM bytes delivered to Android independently of source precision.
 *
 * @property sampleRateHz Rate of the delivered frames.
 * @property encoding Android encoding of those bytes, including integer/float distinction.
 * @property channelMask Android output channel mask.
 */
internal data class MixerPcmFormat(
    val sampleRateHz: Int,
    val encoding: Int,
    val channelMask: Int,
) {
    companion object {
        /** Returns the decoder's actual carrier, or null for inconsistent PCM metadata. */
        fun fromDecoded(format: AudioFormatInfo): MixerPcmFormat? {
            val bytes = when (format.androidPcmEncoding) {
                AudioFormat.ENCODING_PCM_16BIT -> 2
                AudioFormat.ENCODING_PCM_24BIT_PACKED -> 3
                AudioFormat.ENCODING_PCM_32BIT, AudioFormat.ENCODING_PCM_FLOAT -> 4
                else -> return null
            }
            if (format.sampleRateHz <= 0 || format.bytesPerSample != bytes) return null
            val mask = AudioFormatConverter.mapChannelCountToOutMask(format.channelCount)
                ?: return null
            return MixerPcmFormat(format.sampleRateHz, format.androidPcmEncoding, mask)
        }
    }
}

/** Confirms only a current-device preference matching every actual carrier field. */
internal fun UsbRoutingEvent?.confirms(format: MixerPcmFormat?, deviceId: Int?): Boolean {
    if (format == null || deviceId == null) return false
    return when (this) {
        is UsbRoutingEvent.Applied -> this.deviceId == deviceId &&
            sampleRate == format.sampleRateHz && encoding == format.encoding &&
            channelMask == format.channelMask
        is UsbRoutingEvent.Changed -> isBitPerfect && this.deviceId == deviceId &&
            sampleRate == format.sampleRateHz && encoding == format.encoding &&
            channelMask == format.channelMask
        else -> false
    }
}
