# EpochAudioEngine

MPL-2.0 permits commercial use, including proprietary larger works subject to its covered-file obligations. See [Commercial use and giving back](#commercial-use-and-giving-back) for licensing, origin links and voluntary credit requests.

Version 0.1.0. Standalone C++23 audio scheduling and deterministic PCM mixing.

Canonical C++23 modules `audio.types`, `audio.manager`, `audio.mixer`,
`audio.device` and `audio.playback_runtime`. The library preserves bounded
command scheduling, deterministic PCM mixing and the existing abstract-device
playback lifecycle. Engine owns SDL integration, decoding/import and project glue.

Configure this directory with CMake 3.28+ and a C++23 module-capable compiler.
Build and run CTest in Debug and Release. Consumers link `Epoch::AudioEngine`.
The MSVC project is independently buildable and uses repository output layout
when present. CPU contracts prove the library boundary, not audible playback
or GUI presentation. Source licensing is preserved in LICENSE.

Build the library and offline demonstration:

```sh
cmake -S . -B out -DBUILD_TESTING=ON -DEPOCH_AUDIO_BUILD_EXAMPLES=ON
cmake --build out --config Release
ctest --test-dir out -C Release --output-on-failure
```

Run `out/Release/epoch_audio_demo example.wav` on a multi-configuration build,
or `out/epoch_audio_demo example.wav` on a single-configuration build. It uses
the real manager and mixer to write one second of stereo 440 Hz PCM audio.
Open the WAV in an ordinary audio player. Existing files are never overwritten.
This is offline rendering; it does not open a physical audio device.
The same executable accepts `--self-test` for a file-free synthesis check.

No other Epoch library is required. SDL device integration and Engine project
admission remain separate consumers. The optional EpochGui_Demo audio example
shows how a GUI application can use this library.


The source-only CI runs Debug/Release CPU contracts and verifies an actual
192044-byte stereo PCM16 48kHz WAV, including refusal to overwrite an existing
file. It uploads only test/source receipts, never WAVs or binaries. Reproduce
the file check with `python tests/audio.wav_contract.py <demo-executable> <local-proof-directory>`.

## Commercial use and giving back

This is a simple guide to [MPL-2.0](LICENSE). The full license is the legal text.

### Allowed

- [x] Use, change and sell this library, by itself or inside your app.
- [x] Keep changes to this library private while the software stays with you
  or inside your organization. Giving copies to others triggers the rules below.
- [x] Keep your own app's code private if it does not contain code copied from
  this library. Using or linking to the library does not make your whole app
  open source.

### Rules For Distribution

*Required when you give copies to people outside your organization, or publicly.*

- [ ] Give those people access to this library's source code, including your
  changes. Tell them where and how to get the matching source without undue delay.
- [ ] If you copied this library's code into another source file, make that whole
  file available too. You do not have to share your other, separate app files.
- [ ] Keep those shared library files under MPL-2.0. Include the license, or tell
  people how to get it.
- [ ] Keep the existing copyright, patent, license and warranty notices in the
  source. Correcting a notice that is factually wrong is allowed.
- [ ] Let recipients use, change and share those library files under MPL-2.0.
  Your app's terms must not take those rights away.
- [ ] If you charge for providing the source, charge no more than it costs you
  to provide it. This does not limit what you charge for your app.

You do not have to send changes to us or post them for the whole world. The
source-sharing duty is to the people who receive your copy of the software.

### Not allowed

- [ ] Do not give people the software while denying them the library source
  they are entitled to under the rules above.
- [ ] Do not erase the original authors' notices or pretend you wrote their code.
- [ ] Do not assume MPL lets you use our names or logos as your own brand, or
  claim we approved your product. See the separate [branding rules](TRADEMARKS.md).

### Recommended, entirely optional

- [ ] Make the original author and [source link](https://github.com/Autodidac/EpochAudioEngine) easy for other
  developers to find. Use your dependency list, credits, notices or technical docs;
  no particular README or About-screen placement is requested.
- [ ] If you list similar libraries or tools your project uses, list this one
  alongside them. A link to [EpochEngine](https://github.com/Autodidac/EpochEngine)
  also helps support the work.
- [ ] Say what you added and what came from this library. A new name, wrapper or
  small extra feature does not make the underlying library your invention.
- [ ] Share useful fixes, features, tests and documentation through a
  [pull request](https://github.com/Autodidac/EpochAudioEngine/pulls); accepted contributions receive author credit.
- [ ] Follow [CONTRIBUTING.md](CONTRIBUTING.md) when choosing to contribute.

**MPL does not require an upstream pull request. But it would be a lot cooler if
you shared!** Contributions help these libraries support themselves and continued
work on EpochEngine. These recommendations add no copyright-license conditions.

See [Mozilla's MPL FAQ](https://www.mozilla.org/en-US/MPL/2.0/FAQ/) for examples.

## License

This library's own code uses **MPL-2.0**. [LICENSE](LICENSE) contains the unchanged
legal terms. Other authors' code keeps its own license, and earlier copies keep
the rights already granted for them.

See [NOTICE](NOTICE) for the original author/source, [TRADEMARKS.md](TRADEMARKS.md)
for names and logos, and [CONTRIBUTING.md](CONTRIBUTING.md) for sending improvements.
