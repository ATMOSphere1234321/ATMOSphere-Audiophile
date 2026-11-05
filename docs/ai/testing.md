# Testing & Verification

> Part of the Audiophile AI guide. Start at [`/AGENTS.md`](../../AGENTS.md).

Stack: JUnit 4, MockK, Turbine (Flow), Compose UI Test, `kotlinx-coroutines-test`.
`MainDispatcherRule` (in `src/test/`) swaps the main dispatcher for `runTest`.

---

## What to test

Real, committed tests already cover representative logic — follow their style:
- **ViewModels**: `LibraryViewModelTest`, `OnboardingViewModelTest`,
  `SettingsViewModelTest`.
- **Use cases / repositories**: `PlaybackRepositoryImplTest`.
- **Pure logic**: `TrackArtistParserTest`, `SeekBarStateResolverTest`,
  `MediaStoreColumnsTest` (folder-scoped scan query),
  `DspInfoBodyParserTest`, `SueProfileResolverTest`.
- **Playback/engine units**: `AudioTelemetryCollectorTest`, `AudioEngineManagerTest`,
  `HiResRemasterSettingsCoordinatorTest`, `StandardEngineTelemetryMapperTest`,
  `UsbAudioSinkFactoryTest`.

Priorities for new code:
1. Unit-test new use cases and ViewModel `handleEvent` logic first.
2. Unit-test pure resolvers/parsers/mappers/codecs (cheap, high value — this is where
   audio-format and telemetry logic should be made testable).
3. Add Compose UI tests for interaction-heavy components when warranted.
4. ✅ Ship new non-trivial logic **with** tests — don't extend the placeholder-only
   baseline (`ExampleUnitTest`).

Native pure-format tests live in `app/src/main/cpp/tests/`. Build them in Debug
(assertions enabled) with `AUDIPHILE_ENABLE_SANITIZERS=ON` and run the resulting
`audiophile_native_core_tests`. They cover codec carrier choice, integer/float
format dispatch, exact representable PCM, rounding, clipping, and DSD formatting.
`MixerPcmFormatTest` checks actual encoding and mixer-confirmation identity;
`AudioTrackEncodingFallbackTest` rejects integer-byte FLOAT fallback.
Host tests do not establish device bit-perfect behavior. Verify the resulting APK
with actual AudioFlinger route evidence and full USB-loop sample comparison.

---

## Conventions

Backtick test names, Arrange/Act/Assert, MockK for collaborators, Turbine for flows.

```kotlin
@Test
fun `given queue has next item when skip next invoked then repository advances`() = runTest {
    coEvery { playbackRepository.skipNext() } returns Resource.Success(Unit)

    val result = skipNextUseCase()

    assertTrue(result is Resource.Success)
}

@Test
fun `given playback updates when observed then ui model emits new snapshot`() = runTest {
    viewModel.uiState.test {
        assertEquals(UIStatus.IDLE, awaitItem().status)
        assertNotNull(awaitItem().data?.playbackState)
    }
}
```

---

## Build & verify commands (Windows / PowerShell)

```powershell
# Fast correctness loop
.\gradlew.bat :app:compileDebugKotlin

# Unit tests (all / focused)
.\gradlew.bat :app:testDebugUnitTest
.\gradlew.bat :app:testDebugUnitTest --tests "*SettingsViewModelTest"

# Full debug build (runs the native CMake build too — slower)
.\gradlew.bat :app:assembleDebug
```

Prefer `compileDebugKotlin` + a targeted `--tests` run while iterating. The native
build is slow; only run a full assemble when the change touches native code or you
need an APK.
