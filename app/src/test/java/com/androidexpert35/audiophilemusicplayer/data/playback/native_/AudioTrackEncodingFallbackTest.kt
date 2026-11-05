package com.androidexpert35.audiophilemusicplayer.data.playback.native_

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.util.Log
import com.androidexpert35.audiophilemusicplayer.domain.model.audio.AudioCodec
import io.mockk.*
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test

class AudioTrackEncodingFallbackTest {
    private val context = mockk<Context>()
    private val manager = mockk<AudioManager>()
    private val attributes = mockk<AudioAttributes>()
    private val track = mockk<AudioTrack>()
    private val report = mockk<PipelinePathReport>()
    private val attempts = mutableListOf<Pair<Int, Boolean>>()

    @Before fun setUp() {
        mockkStatic("com.androidexpert35.audiophilemusicplayer.data.playback.native_.AudioTrackSinkBuilderKt")
        mockkStatic(Log::class)
        every { context.getSystemService(Context.AUDIO_SERVICE) } returns manager
        every { manager.getProperty(any()) } returns null
        every { Log.w(any<String>(), any<String>()) } returns 0
        every { tryBuildAudioTrack(any(), any(), any(), any(), any(), any()) } answers {
            val direct = lastArg<Boolean>()
            attempts += firstArg<Int>() to direct
            if (direct) null else track to 32
        }
        every { buildAudioTrackPathReport(any(), any(), any(), any(), any(), any(), any(), any(), any()) } returns report
    }

    @After fun tearDown() { unmockkAll() }

    private fun format(encoding: Int, bytes: Int) = AudioFormatInfo(
        96000, 2, if (bytes == 3) 24 else 32, encoding, bytes, 1000, 0, AudioCodec.WAV,
    )

    @Test fun `refused S32 direct output never retries integer bytes as float`() {
        val result = buildAudioTrackWithFallback(context,
            format(AudioFormat.ENCODING_PCM_32BIT, 4), 3, attributes)
        assertSame(track, result.first)
        assertEquals(listOf(AudioFormat.ENCODING_PCM_32BIT to true,
            AudioFormat.ENCODING_PCM_32BIT to false), attempts)
    }

    @Test fun `float source stays float across same-encoding fallback`() {
        buildAudioTrackWithFallback(context, format(AudioFormat.ENCODING_PCM_FLOAT, 4), 3, attributes)
        assertEquals(listOf(AudioFormat.ENCODING_PCM_FLOAT to true,
            AudioFormat.ENCODING_PCM_FLOAT to false), attempts)
    }

    @Test fun `DoP requiring direct output fails without float or mixer fallback`() {
        assertThrows(IllegalStateException::class.java) {
            buildAudioTrackWithFallback(context,
                format(AudioFormat.ENCODING_PCM_24BIT_PACKED, 3), 3, attributes,
                requireDirectOutput = true)
        }
        assertEquals(listOf(AudioFormat.ENCODING_PCM_24BIT_PACKED to true), attempts)
    }
}
