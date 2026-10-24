# EpochMediaEngine

MPL-2.0 permits commercial use, including proprietary larger works subject to its covered-file obligations. See [Commercial use and giving back](#commercial-use-and-giving-back) for licensing, origin links and voluntary credit requests.

C++23 media timeline metadata and presentation geometry, with an optional
interactive EpochGui demonstration. The core `Epoch::MediaEngine` target owns
`media.timeline_preview`: metadata admission, revision ordering, transport,
pixel aspect, rotation and contain-fit geometry. It does not decode video or
claim audio/video synchronization.

## Source distribution

EpochGui accompanies the Media demonstration as a separate public dependency;
it remains GUI-only. The pure metadata target does not acquire GUI, Platform or
Audio dependencies. `dependencies.json` records the reviewed source graph.
No configuration fetches from the network or falls back to Engine source.

The optional demonstration reuses EpochGui_Demo's real renderer/native host.
Its Libraries page advances generated timeline metadata, draws the admitted
aspect-fit rectangle and GUI playhead. The media-only composition requires no
Audio or Platform dependency and introduces no duplicate renderer.

Configure core/tests with `cmake -S . -B build` and a module-capable generator,
then build and run CTest. To build the interactive demonstration, additionally
set `EPOCHMEDIA_BUILD_GUI_DEMO=ON`, `EPOCHGUI_DEMO_ROOT`, `EPOCHGUI_ROOT`,
and `EPOCH_MEDIA_ROOT` to the explicit
local repositories (the last points here). Build `epoch_gui_demo_app`; the
result is named `EpochMediaDemo` and opens its Libraries page. This is a source
composition, not a packaged release or runtime-acceptance claim.

The original repository history is retained. First-party source, including the
owner-authorized migrated implementation, is MPL-2.0. Historical release licenses
remain unchanged. Current source version is 0.1.0; no release artifacts or native runtime acceptance are claimed.

Reviewed EpochGui source dependency: `f83b2f15385598b8efbb028a6f391ca637e07634` (0.90.0).
Reviewed Demo source dependency: `a01214e3666ef7fc657c270d9cbd208de50064bc` (0.6.0).
All ten Windows Demo composition/configuration builds and CPU contracts pass.
The Media-owned optional composition check and native runtime acceptance remain
separate; this source graph is not a binary-release acceptance claim.

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

- [ ] Make the original author and [source link](https://github.com/Autodidac/EpochMediaEngine) easy for other
  developers to find. Use your dependency list, credits, notices or technical docs;
  no particular README or About-screen placement is requested.
- [ ] If you list similar libraries or tools your project uses, list this one
  alongside them. A link to [EpochEngine](https://github.com/Autodidac/EpochEngine)
  also helps support the work.
- [ ] Say what you added and what came from this library. A new name, wrapper or
  small extra feature does not make the underlying library your invention.
- [ ] Share useful fixes, features, tests and documentation through a
  [pull request](https://github.com/Autodidac/EpochMediaEngine/pulls); accepted contributions receive author credit.
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
