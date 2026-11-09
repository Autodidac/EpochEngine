// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Native CPU helper. Hold mode is terminated by its owning test process.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <utility>
import platform.filesystem;
namespace filesystem=epochengine::platform::filesystem;
static_assert(!std::is_copy_constructible_v<filesystem::ExclusiveFileLease>);
static_assert(!std::is_copy_assignable_v<filesystem::ExclusiveFileLease>);
static_assert(std::is_nothrow_move_constructible_v<filesystem::ExclusiveFileLease>);
static_assert(std::is_nothrow_move_assignable_v<filesystem::ExclusiveFileLease>);
int main(int argc,char** argv)
{
    if(argc<3)return 2;
    const std::string_view mode{argv[1]};
    auto lease=filesystem::ExclusiveFileLease::acquire(std::filesystem::path{argv[2]});
    if(!lease)
    {
        if(lease.code()==filesystem::FileLeaseCode::busy)return 3;
        if(lease.code()==filesystem::FileLeaseCode::invalid_path)return 4;
        return 5;
    }
    if(mode=="--try"&&argc==3)return 0;
    if(mode=="--move"&&argc==3)
    {
        auto owner=std::move(lease);
        if(lease||lease.code()==filesystem::FileLeaseCode::acquired||!owner)return 6;
        auto blocked=filesystem::ExclusiveFileLease::acquire(std::filesystem::path{argv[2]});
        if(blocked||blocked.code()!=filesystem::FileLeaseCode::busy)return 6;
        lease=std::move(owner);
        if(owner||!lease)return 6;
        lease.reset();
        auto after=filesystem::ExclusiveFileLease::acquire(std::filesystem::path{argv[2]});
        return after?0:6;
    }
    if(mode=="--hold"&&argc==4)
    {
        std::ofstream ready{std::filesystem::path{argv[3]},std::ios::binary|std::ios::noreplace};
        ready<<"held\n";ready.close();if(!ready)return 7;
        // Only the driver controls this private pipe; no window or service starts.
        (void)std::cin.get();
        return 0;
    }
    return 2;
}
