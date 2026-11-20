# Basic AI-Enabled Calculator

In Epoch Editor, choose **Asset → Package Manager → Project Examples** and import
**Basic AI-Enabled Calculator**. Open the new copy from **Project**. It is
an ordinary Software / Tool project, and the **UI** workspace shows its editable
layout. Use **Project → Run + Build → Play In Editor** on a dock-capable host.
For the basic keypad, Enter evaluates, Backspace removes the last digit, and
Delete clears the calculation. The example selects Software rendering on Windows.
To measure the renderer, choose **Unlimited** under **Project → Run + Build →
Frame limit** before running, or launch the built application with
`--backend software --frame-limit unlimited`. Keep its window focused for
the measurement.

The basic keypad and the **Scientific** tab can be used independently. In
Scientific, type `sin(30) + sqrt(16)` and select **Calculate** to get
`4.5` in Degrees mode. **Change angle unit** switches between degrees and
radians. The panel also accepts parentheses, powers, logarithms and the
constants `pi` and `e`.

**Worked steps** shows intermediate results in the order the calculator
evaluates them. For `2 + (3 * 4)^2`, it shows `3 * 4 = 12`, then
`(3 * 4)^2 = 144`, then `2 + (3 * 4)^2 = 146`. It summarizes longer
calculations and clears when an expression fails or the angle mode changes.
**Next tip** cycles through five short examples of functions, angle units and
operation order without changing your calculation. **Try tip** puts the
current example in the scientific input and shows its result and worked steps.
The trigonometry example follows the current Degrees or Radians setting.
**Reference** is an offline key for functions such as `sqrt(16)` and
`abs(-7)`, powers, angle units, logarithms and constants.

The GUI is saved in `Assets/Gui/main.epochgui`. The project owns its arithmetic
and input handling in `source/` and uses the engine's project GUI runtime to draw
the document. The Library and application binaries are regenerated locally.

To change or reproduce the scientific panel, use **UI → GUI Hierarchy**.
In **Widgets**, add a panel to the canvas, then add its expression input,
result and angle displays, and two buttons. Select the panel before adding a
control to place it inside the panel. Select each widget in the hierarchy
and edit its position, size, content and action in **Layout**. Use
**Properties → Identity → Rename Widget** to set the name. Name the editable
input `CalculatorExpression`. Name the result and angle text inputs
`CalculatorScientificResult` and `CalculatorAngleStatus` and disable them for
user input. Leave **Read only** off so the project can update those displays.
Initialize the angle display to `Degrees`. Set the button actions to
`calculator.scientific.evaluate` and `calculator.scientific.angle`. The project's
`source/project.gui_application.hpp` handles those actions. Save the document and
use the normal **Run + Build** menu to build and run it.
Add a multiline input named `CalculatorWorkedSteps` with the same settings as
the result display. The Editor's 256-byte limit is enough for the worked steps.
Add another multiline display named `CalculatorScientificTip`, and bind a
**Next tip** button to `calculator.scientific.next_tip`. Initialize the display
with “2^8 raises 2” and “to the eighth power.” Add a **Try tip** button
bound to `calculator.scientific.run_example`. The canvas and action bindings
are editable in Epoch. Project C++ is edited in a source editor and built by
Epoch.

For portrait, open **Layout → Canvas Layout** and enable **Stack on narrow
screens** with a gap of 12. Keep `CalculatorBody` before `CalculatorModeTabs`
in the GUI hierarchy. The wide layout still uses each panel's authored position.

The **Coach** tab is built into the project. Its response interpreter runs
proposed expressions through the same calculator and displays the result and
worked steps separately. It can also present reviewed help and clarification.
When the Coach has no connection, the status directs you to **Reference** for
offline help. Scientific mode and the manual keypad also work offline. A live
Coach connection depends on host isolation and model package verification.

For Android, use the [saved-project APK build](../../docs/platform/android/android.md#package-a-saved-project-for-android-debug)
on Windows. `-CalculatorCoachArchive` opts into the isolated offline Coach.
The 409 MiB model is separate from the APK. In **Coach**, tap **Ask** to pick the
supported Qwen3-0.6B Q4_0 GGUF from local storage. Once the model is ready, enter
a question and tap **Ask** again. Android 12 emulator acceptance passed. A
physical phone run remains open.

After saving the GUI in the Editor, you can also build from the example directory
with `./build_project.ps1` on Windows or `bash build_project.sh` on Linux.
The wrappers check the engine source edition against the manifest. For a copy
outside this tree, set `EPOCH_REPO_ROOT` to the matching checkout first.
