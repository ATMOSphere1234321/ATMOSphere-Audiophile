package com.androidexpert35.audiophilemusicplayer.data.playback

import android.media.AudioFormat
import com.androidexpert35.audiophilemusicplayer.data.playback.native_.AudioFormatInfo
import com.androidexpert35.audiophilemusicplayer.data.playback.usb.UsbRoutingEvent
import com.androidexpert35.audiophilemusicplayer.domain.model.audio.AudioCodec
import org.junit.Assert.*
import org.junit.Test

class MixerPcmFormatTest {
    private fun format(bits: Int, encoding: Int, bytes: Int) = AudioFormatInfo(
        sampleRateHz = 96000, channelCount = 2, sourceBitDepth = bits,
        androidPcmEncoding = encoding, bytesPerSample = bytes,
        durationMs = 1000, bitrateKbps = 0, codec = AudioCodec.WAV,
    )

    @Test fun `24-bit source in S32 negotiates S32 without changing source precision`() {
        val source = format(24, AudioFormat.ENCODING_PCM_32BIT, 4)
        assertEquals(AudioFormat.ENCODING_PCM_32BIT, MixerPcmFormat.fromDecoded(source)?.encoding)
        assertEquals(24, source.sourceBitDepth)
    }

    @Test fun `integer32 and float32 remain different mixer requests`() {
        val integer = MixerPcmFormat.fromDecoded(format(32, AudioFormat.ENCODING_PCM_32BIT, 4))
        val float = MixerPcmFormat.fromDecoded(format(32, AudioFormat.ENCODING_PCM_FLOAT, 4))
        assertEquals(AudioFormat.ENCODING_PCM_32BIT, integer?.encoding)
        assertEquals(AudioFormat.ENCODING_PCM_FLOAT, float?.encoding)
        assertNotEquals(integer, float)
    }

    @Test fun `S16 DoP carrier and DSD PCM float retain their exact encoding`() {
        for ((bits, encoding, bytes) in listOf(
            Triple(16, AudioFormat.ENCODING_PCM_16BIT, 2),
            Triple(24, AudioFormat.ENCODING_PCM_24BIT_PACKED, 3),
            Triple(32, AudioFormat.ENCODING_PCM_FLOAT, 4),
        )) assertEquals(encoding, MixerPcmFormat.fromDecoded(format(bits, encoding, bytes))?.encoding)
    }

    @Test fun `unknown encoding or inconsistent sample width cannot request a mixer profile`() {
        assertNull(MixerPcmFormat.fromDecoded(format(32, 0, 4)))
        assertNull(MixerPcmFormat.fromDecoded(format(24, AudioFormat.ENCODING_PCM_32BIT, 3)))
    }

    @Test fun `confirmation requires current device rate encoding and channels`() {
        val carrier = MixerPcmFormat.fromDecoded(format(24, AudioFormat.ENCODING_PCM_32BIT, 4))
        val applied = UsbRoutingEvent.Applied(7, 96000, AudioFormat.ENCODING_PCM_32BIT,
            AudioFormat.CHANNEL_OUT_STEREO)
        assertTrue(applied.confirms(carrier, 7))
        assertFalse(applied.confirms(carrier, 8))
        assertFalse(applied.confirms(carrier, null))
        assertFalse(applied.copy(sampleRate = 44100).confirms(carrier, 7))
        assertFalse(applied.copy(encoding = AudioFormat.ENCODING_PCM_FLOAT).confirms(carrier, 7))
        assertFalse(applied.copy(channelMask = AudioFormat.CHANNEL_OUT_MONO).confirms(carrier, 7))
        assertFalse(applied.confirms(null, 7))
    }

    @Test fun `changed default rejected and cleared profiles never confirm bit-perfect`() {
        val carrier = MixerPcmFormat.fromDecoded(format(32, AudioFormat.ENCODING_PCM_32BIT, 4))
        val changed = UsbRoutingEvent.Changed(7, 96000, AudioFormat.ENCODING_PCM_32BIT,
            AudioFormat.CHANNEL_OUT_STEREO, true)
        assertTrue(changed.confirms(carrier, 7))
        assertFalse(changed.copy(isBitPerfect = false).confirms(carrier, 7))
        assertFalse(UsbRoutingEvent.Rejected(7, "test").confirms(carrier, 7))
        assertFalse(UsbRoutingEvent.Cleared(7).confirms(carrier, 7))
        assertFalse((null as UsbRoutingEvent?).confirms(carrier, 7))
    }
}
