# EpochEngine 0.90.39 - Windows resource compiler compatibility

## Fix

`Engine/resource/WinResource.rc` was accidentally saved with a UTF-8 BOM in
0.90.37 and 0.90.38. MSVC `rc.exe` can misinterpret the BOM and report:

- `RC2135: file not found: ICON`
- `RC2255: named STRINGTABLEs are not allowed`

The script is now written as plain ASCII without a BOM (same format as
0.90.35 and 0.90.36). Its icon, string resources and version information
remain intact. Development source version is 0.90.39; published packaged
runtime remains 0.90.35.

## Regression

Run `python Tests/windows/test_resource_script_encoding.py` to confirm the
encoding, existing icon resource, and version consistency. MSVC `rc.exe`
compilation must still be confirmed on Windows.
