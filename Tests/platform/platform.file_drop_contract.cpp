// Native shell transfer contract: synthetic HDROP only; no Explorer or windows.
#include "../../Engine/src/platform/platform.file_drop_win.hpp"
#include <shlobj_core.h>
#include <cassert>
#include <cstring>
#include <type_traits>
#include <utility>
using epochengine::platform_file_drop::OwnedDrop;
static_assert(!std::is_copy_constructible_v<OwnedDrop> && !std::is_move_constructible_v<OwnedDrop>);
static HDROP payload(const std::vector<std::wstring>& paths)
{
    std::wstring joined;
    for (const auto& path : paths) { joined += path; joined += L'\0'; }
    joined += L'\0';
    const auto bytes = sizeof(DROPFILES) + joined.size() * sizeof(wchar_t);
    const auto memory = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    assert(memory);
    auto* header = static_cast<DROPFILES*>(::GlobalLock(memory));
    assert(header);
    header->pFiles = sizeof(DROPFILES);
    header->fWide = TRUE;
    std::memcpy(reinterpret_cast<unsigned char*>(header) + sizeof(DROPFILES), joined.data(),
        joined.size() * sizeof(wchar_t));
    ::GlobalUnlock(memory);
    return static_cast<HDROP>(memory);
}
int main()
{
    assert(OwnedDrop{nullptr}.files().empty());
    const std::wstring longPath = L"C:\\references\\" + std::wstring(300u, L'x') + L".png";
    const auto transfer = payload({L"C:\\references\\caf\u00e9.png", longPath});
    {
        OwnedDrop owned{transfer};
        const auto files = owned.files();
        assert(files.size() == 2u);
        assert(files[0] == "C:\\references\\caf\xc3\xa9.png");
        assert(files[1].size() == longPath.size()); // No MAX_PATH truncation.
    }
    assert(::GlobalFlags(transfer) == GMEM_INVALID_HANDLE); // Exactly one owner retired it.
    const OwnedDrop tooMany{payload(std::vector<std::wstring>(17u, L"C:\\reference.png"))};
    assert(tooMany.files().empty());
}
