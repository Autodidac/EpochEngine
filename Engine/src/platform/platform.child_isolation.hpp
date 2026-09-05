#pragma once

#if defined(_WIN32)

#include <Windows.h>
#include <aclapi.h>
#include <bcrypt.h>
#include <userenv.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace epochengine::platform::child_process::native_isolation
{
    // The caller owns the designated disposable root and serializes its
    // preparation. This adapter proves path shape and applies OS permissions;
    // it cannot establish ownership from an arbitrary caller-supplied pathname.
    class Lease final
    {
        struct Handle final
        {
            HANDLE value{INVALID_HANDLE_VALUE};
            Handle() = default;
            explicit Handle(HANDLE handle) noexcept : value{handle} {}
            Handle(const Handle&) = delete;
            Handle& operator=(const Handle&) = delete;
            Handle(Handle&& other) noexcept
                : value{std::exchange(other.value, INVALID_HANDLE_VALUE)} {}
            Handle& operator=(Handle&& other) noexcept
            {
                if (this != &other)
                {
                    close();
                    value = std::exchange(other.value, INVALID_HANDLE_VALUE);
                }
                return *this;
            }
            ~Handle() noexcept { close(); }
            void close() noexcept
            {
                if (value != INVALID_HANDLE_VALUE && value != nullptr)
                    (void)::CloseHandle(value);
                value = INVALID_HANDLE_VALUE;
            }
            [[nodiscard]] bool valid() const noexcept
            { return value != INVALID_HANDLE_VALUE && value != nullptr; }
        };

        struct LocalAllocation final
        {
            void* value{};
            ~LocalAllocation() noexcept { if (value != nullptr) (void)::LocalFree(value); }
        };

        struct Node final
        {
            std::filesystem::path path{};
            Handle handle{};
            ULONGLONG volume{};
            std::array<BYTE, 16u> file_id{};
            DWORD links{};
            bool directory{};
            bool writable{};
            bool revoke_acl{true};
        };

        static constexpr std::size_t maximum_roots = 16u;
        static constexpr std::size_t maximum_nodes = 65'536u;
        static constexpr std::size_t maximum_depth = 128u;
        static constexpr DWORD maximum_token_bytes = 65'536u;
        std::filesystem::path owned_root_{};
        std::vector<Handle> boundary_handles_{};
        std::vector<Node> roots_{};
        std::wstring profile_name_{};
        PSID sid_{};
        SECURITY_CAPABILITIES capabilities_{};
        bool profile_created_{};
        bool ready_{};

        [[nodiscard]] static bool fail(std::string& error, const char* operation, DWORD code) noexcept
        {
            try { error = std::string{operation} + " (native status " + std::to_string(code) + ")."; }
            catch (...) { error.clear(); }
            return false;
        }

        [[nodiscard]] static bool same_component(
            const std::filesystem::path& left, const std::filesystem::path& right) noexcept
        {
            return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE)
                == CSTR_EQUAL;
        }

        [[nodiscard]] static bool beneath(
            const std::filesystem::path& path, const std::filesystem::path& parent)
        {
            auto part = path.begin();
            for (const auto& expected : parent)
            {
                if (part == path.end() || !same_component(*part, expected)) return false;
                ++part;
            }
            return part != path.end();
        }

        [[nodiscard]] static bool checked_path(
            const std::filesystem::path& path, std::string& error)
        {
            const auto& native = path.native();
            const auto drive = path.root_name().native();
            if (!path.is_absolute() || path == path.root_path()
                || native.empty() || native.size() > 32'000u
                || native.find(L'\0') != std::wstring::npos
                || drive.size() != 2u || drive[1] != L':'
                || !((drive[0] >= L'A' && drive[0] <= L'Z')
                    || (drive[0] >= L'a' && drive[0] <= L'z'))
                || path != path.lexically_normal())
                return fail(error, "Isolation requires a canonical local directory path", ERROR_INVALID_NAME);
            std::size_t depth{};
            for (const auto& part : path.relative_path())
            {
                const auto text = part.native();
                if (++depth > maximum_depth || text.empty() || text == L"." || text == L".."
                    || text.back() == L'.' || text.back() == L' '
                    || text.find_first_of(L":*?\"<>|") != std::wstring::npos)
                    return fail(error, "Isolation path component is invalid or exceeds its bound", ERROR_INVALID_NAME);
            }
            return true;
        }

        [[nodiscard]] static bool open_node(const std::filesystem::path& path,
            bool requireDirectory, bool securityWrite, Node& node, std::string& error,
            bool retirement = false)
        {
            const DWORD access = FILE_READ_ATTRIBUTES | READ_CONTROL
                | (securityWrite ? WRITE_DAC : 0u);
            Handle handle{::CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                nullptr)};
            if (!handle.valid()) return fail(error, "Isolation path could not be locked", ::GetLastError());
            BY_HANDLE_FILE_INFORMATION information{};
            if (::GetFileInformationByHandle(handle.value, &information) == FALSE)
                return fail(error, "Isolation path identity could not be read", ::GetLastError());
            const bool directory = (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u;
            if ((information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u
                || (requireDirectory && !directory)
                || (!directory && (information.nNumberOfLinks == 0u
                    || (!retirement && information.nNumberOfLinks != 1u)))
                || ::GetFileType(handle.value) != FILE_TYPE_DISK)
                return fail(error, "Isolation refuses reparse, hard-linked or nonordinary paths", ERROR_INVALID_DATA);
            FILE_ID_INFO identity{};
            if (::GetFileInformationByHandleEx(handle.value, FileIdInfo, &identity, sizeof(identity)) == FALSE)
                return fail(error, "Isolation stable file identity could not be read", ::GetLastError());
            // Lexically disjoint roots can still alias through DOS short names
            // or alternate drive mappings. Require the locked object's resolved
            // DOS spelling so those aliases cannot merge RX and writable trees.
            const DWORD needed = ::GetFinalPathNameByHandleW(handle.value, nullptr, 0u,
                FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (needed == 0u || needed > 32'005u)
                return fail(error, "Isolation path resolution failed or exceeds its bound", ERROR_INVALID_NAME);
            std::wstring resolved(needed, L'\0');
            const DWORD received = ::GetFinalPathNameByHandleW(handle.value, resolved.data(), needed,
                FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (received == 0u || received >= needed)
                return fail(error, "Isolation path resolution was not stable", ERROR_INVALID_NAME);
            resolved.resize(received);
            if (!resolved.starts_with(L"\\\\?\\")
                || !same_component(std::filesystem::path{resolved.substr(4u)}, path))
                return fail(error, "Isolation refuses an aliased path spelling", ERROR_INVALID_NAME);
            node.path = path;
            node.handle = std::move(handle);
            node.volume = identity.VolumeSerialNumber;
            std::copy(std::begin(identity.FileId.Identifier), std::end(identity.FileId.Identifier), node.file_id.begin());
            node.links = information.nNumberOfLinks;
            node.directory = directory;
            return true;
        }

        [[nodiscard]] bool hold_ancestors(const std::filesystem::path& path, std::string& error)
        {
            auto current = path.root_path();
            Node root{};
            if (!open_node(current, true, false, root, error)) return false;
            boundary_handles_.emplace_back(std::move(root.handle));
            for (const auto& component : path.relative_path())
            {
                current /= component;
                Node node{};
                if (!open_node(current, true, false, node, error)) return false;
                boundary_handles_.emplace_back(std::move(node.handle));
            }
            return true;
        }

        [[nodiscard]] static bool validate_retirement_nodes(std::vector<Node>& nodes, std::string& error)
        {
            std::vector<std::size_t> files{};
            files.reserve(nodes.size());
            // Requery only after the complete tree is pinned. No grant is
            // revoked until all identities, types and link counts are stable.
            for (std::size_t index = 0u; index < nodes.size(); ++index)
            {
                const auto& node = nodes[index];
                BY_HANDLE_FILE_INFORMATION information{};
                FILE_ID_INFO identity{};
                if (::GetFileInformationByHandle(node.handle.value, &information) == FALSE
                    || ::GetFileInformationByHandleEx(node.handle.value, FileIdInfo, &identity, sizeof(identity)) == FALSE)
                    return fail(error, "Isolation retirement identity could not be requeried", ::GetLastError());
                if ((information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u
                    || ((information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) != node.directory
                    || ::GetFileType(node.handle.value) != FILE_TYPE_DISK
                    || identity.VolumeSerialNumber != node.volume
                    || !std::equal(node.file_id.begin(), node.file_id.end(), std::begin(identity.FileId.Identifier))
                    || information.nNumberOfLinks != node.links)
                    return fail(error, "Isolation retirement path identity or link count changed", ERROR_INVALID_DATA);
                if (!node.directory) files.emplace_back(index);
            }
            const auto same_identity = [&](std::size_t left, std::size_t right)
            {
                return nodes[left].volume == nodes[right].volume
                    && nodes[left].file_id == nodes[right].file_id;
            };
            std::sort(files.begin(), files.end(), [&](std::size_t left, std::size_t right)
            {
                const auto& a = nodes[left];
                const auto& b = nodes[right];
                if (a.volume != b.volume) return a.volume < b.volume;
                if (a.file_id != b.file_id) return a.file_id < b.file_id;
                return ::CompareStringOrdinal(a.path.c_str(), -1, b.path.c_str(), -1, TRUE) == CSTR_LESS_THAN;
            });
            for (std::size_t first = 0u; first < files.size();)
            {
                std::size_t after = first + 1u;
                while (after < files.size() && same_identity(files[first], files[after])) ++after;
                const auto& representative = nodes[files[first]];
                if (after - first != representative.links)
                    return fail(error, "Isolation retirement found an unaccounted hardlink", ERROR_INVALID_DATA);
                for (std::size_t at = first + 1u; at < after; ++at)
                {
                    auto& node = nodes[files[at]];
                    if (node.writable != representative.writable || node.links != representative.links
                        || same_component(nodes[files[at - 1u]].path, node.path))
                        return fail(error, "Isolation retirement hardlinks cross grant modes or repeat a path", ERROR_INVALID_DATA);
                    node.revoke_acl = false;
                }
                first = after;
            }
            return true;
        }

        [[nodiscard]] bool collect_nodes(std::vector<Node>& nodes, std::string& error,
            bool retirement = false)
        {
            // All existing nodes stay nonrenameable during ACL propagation.
            // Descendant handles are released before a compiler runs so normal
            // output deletion/replacement is not held in delete-pending state.
            for (const Node& root : roots_)
            {
                Node duplicate{};
                if (!open_node(root.path, true, true, duplicate, error)) return false;
                duplicate.writable = root.writable;
                nodes.emplace_back(std::move(duplicate));
            }
            for (std::size_t index = 0u; index < nodes.size(); ++index)
            {
                if (!nodes[index].directory) continue;
                const auto directory = nodes[index].path;
                const bool writable = nodes[index].writable;
                std::error_code filesystemError{};
                std::filesystem::directory_iterator cursor{directory, filesystemError}, end{};
                if (filesystemError)
                    return fail(error, "Isolation directory enumeration failed", filesystemError.value());
                while (cursor != end)
                {
                    if (nodes.size() >= maximum_nodes)
                        return fail(error, "Isolation tree exceeds the node bound", ERROR_NOT_ENOUGH_QUOTA);
                    const auto path = cursor->path();
                    if (!checked_path(path, error)) return false;
                    Node node{};
                    if (!open_node(path, false, true, node, error, retirement)) return false;
                    node.writable = writable;
                    nodes.emplace_back(std::move(node));
                    cursor.increment(filesystemError);
                    if (filesystemError)
                        return fail(error, "Isolation directory enumeration failed", filesystemError.value());
                }
            }
            return !retirement || validate_retirement_nodes(nodes, error);
        }

        [[nodiscard]] static bool preflight_acls(const std::vector<Node>& nodes, std::string& error)
        {
            // An inheritable root ACE can immediately propagate to children.
            // Validate every pinned DACL before the first such mutation so a
            // null child DACL cannot be narrowed as a side effect of admission.
            for (const auto& node : nodes)
            {
                PACL acl{};
                PSECURITY_DESCRIPTOR descriptor{};
                const DWORD read = ::GetSecurityInfo(node.handle.value, SE_FILE_OBJECT,
                    DACL_SECURITY_INFORMATION, nullptr, nullptr, &acl, nullptr, &descriptor);
                LocalAllocation descriptorOwner{descriptor};
                if (read != ERROR_SUCCESS) return fail(error, "Isolation DACL preflight read failed", read);
                if (acl == nullptr || ::IsValidAcl(acl) == FALSE)
                    return fail(error, "Isolation refuses an absent or invalid DACL before granting access", ERROR_INVALID_ACL);
            }
            return true;
        }

        [[nodiscard]] bool change_acl(const Node& node, bool grant, std::string& error)
        {
            PACL previous{};
            PSECURITY_DESCRIPTOR descriptor{};
            const DWORD read = ::GetSecurityInfo(node.handle.value, SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION, nullptr, nullptr, &previous, nullptr, &descriptor);
            LocalAllocation descriptorOwner{descriptor};
            if (read != ERROR_SUCCESS) return fail(error, "Isolation DACL read failed", read);
            if (previous == nullptr || ::IsValidAcl(previous) == FALSE)
                return fail(error, "Isolation refuses an absent or invalid DACL", ERROR_INVALID_ACL);
            EXPLICIT_ACCESSW entry{};
            entry.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;
            if (node.writable)
                entry.grfAccessPermissions |= FILE_GENERIC_WRITE | DELETE
                    | (node.directory ? FILE_DELETE_CHILD : 0u);
            entry.grfAccessMode = grant ? GRANT_ACCESS : REVOKE_ACCESS;
            entry.grfInheritance = node.directory ? SUB_CONTAINERS_AND_OBJECTS_INHERIT : NO_INHERITANCE;
            entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            entry.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
            entry.Trustee.ptstrName = static_cast<LPWSTR>(sid_);
            PACL replacement{};
            const DWORD merged = ::SetEntriesInAclW(1u, &entry, previous, &replacement);
            LocalAllocation replacementOwner{replacement};
            if (merged != ERROR_SUCCESS) return fail(error, "Isolation DACL merge failed", merged);
            if (replacement == nullptr || ::IsValidAcl(replacement) == FALSE)
                return fail(error, "Isolation DACL merge produced no valid ACL", ERROR_INVALID_ACL);
            const DWORD written = ::SetSecurityInfo(node.handle.value, SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION, nullptr, nullptr, replacement, nullptr);
            return written == ERROR_SUCCESS
                || fail(error, grant ? "Isolation permission grant failed" : "Isolation permission revocation failed", written);
        }

        [[nodiscard]] static bool token_data(HANDLE token, TOKEN_INFORMATION_CLASS kind,
            std::vector<std::uintptr_t>& storage, std::string& error)
        {
            DWORD needed{};
            if (::GetTokenInformation(token, kind, nullptr, 0u, &needed) != FALSE
                || ::GetLastError() != ERROR_INSUFFICIENT_BUFFER
                || needed == 0u || needed > maximum_token_bytes)
                return fail(error, "Isolation token information size is unavailable or out of bounds", ERROR_INVALID_DATA);
            storage.resize((needed + sizeof(std::uintptr_t) - 1u) / sizeof(std::uintptr_t));
            DWORD received{};
            if (::GetTokenInformation(token, kind, storage.data(), needed, &received) == FALSE)
                return fail(error, "Isolation token information query failed", ::GetLastError());
            return (received != 0u && received <= needed)
                || fail(error, "Isolation token information is malformed", ERROR_INVALID_DATA);
        }

        [[nodiscard]] static bool verify_package_access(HANDLE token, std::string& error)
        {
            // Some desktop builds reject TokenIsLessPrivilegedAppContainer.
            // Verify its actual access-check semantics instead of treating an
            // unavailable flag as success. No thread impersonation or resource
            // ACL mutation is involved: both descriptors exist only in memory.
            HANDLE duplicate{};
            if (::DuplicateTokenEx(token, TOKEN_QUERY, nullptr, SecurityIdentification,
                    TokenImpersonation, &duplicate) == FALSE)
                return fail(error, "Isolation access-check token could not be duplicated", ::GetLastError());
            Handle client{duplicate};
            std::vector<std::uintptr_t> userData{};
            if (!token_data(token, TokenUser, userData, error)) return false;
            PSID user = reinterpret_cast<const TOKEN_USER*>(userData.data())->User.Sid;
            if (!user || !::IsValidSid(user))
                return fail(error, "Isolation user identity is invalid", ERROR_INVALID_SID);
            for (const DWORD packageRid : {SECURITY_BUILTIN_PACKAGE_ANY_PACKAGE,
                    SECURITY_BUILTIN_PACKAGE_ANY_RESTRICTED_PACKAGE})
            {
                struct SidOwner final
                {
                    PSID value{};
                    ~SidOwner() noexcept { if (value) (void)::FreeSid(value); }
                } package{};
                SID_IDENTIFIER_AUTHORITY authority = SECURITY_APP_PACKAGE_AUTHORITY;
                if (::AllocateAndInitializeSid(&authority, 2u, SECURITY_APP_PACKAGE_BASE_RID,
                        packageRid, 0u, 0u, 0u, 0u, 0u, 0u, &package.value) == FALSE)
                    return fail(error, "Isolation package identity could not be formed", ::GetLastError());
                std::array<EXPLICIT_ACCESSW, 2u> entries{};
                for (auto& entry : entries)
                {
                    entry.grfAccessPermissions = 1u;
                    entry.grfAccessMode = GRANT_ACCESS;
                    entry.grfInheritance = NO_INHERITANCE;
                    entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
                    entry.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
                }
                entries[0].Trustee.ptstrName = static_cast<LPWSTR>(user);
                entries[1].Trustee.ptstrName = static_cast<LPWSTR>(package.value);
                PACL acl{};
                const DWORD merged = ::SetEntriesInAclW(static_cast<ULONG>(entries.size()),
                    entries.data(), nullptr, &acl);
                LocalAllocation aclOwner{acl};
                if (merged != ERROR_SUCCESS) return fail(error, "Isolation access-check ACL failed", merged);
                SECURITY_DESCRIPTOR descriptor{};
                if (::InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) == FALSE
                    || ::SetSecurityDescriptorOwner(&descriptor, user, FALSE) == FALSE
                    || ::SetSecurityDescriptorGroup(&descriptor, user, FALSE) == FALSE
                    || ::SetSecurityDescriptorDacl(&descriptor, TRUE, acl, FALSE) == FALSE)
                    return fail(error, "Isolation access-check descriptor failed", ::GetLastError());
                GENERIC_MAPPING mapping{1u, 2u, 4u, 7u};
                alignas(PRIVILEGE_SET) std::array<std::byte, 4096u> privileges{};
                DWORD privilegeBytes = static_cast<DWORD>(privileges.size()), granted{};
                BOOL allowed{};
                if (::AccessCheck(&descriptor, client.value, 1u, &mapping,
                        reinterpret_cast<PRIVILEGE_SET*>(privileges.data()),
                        &privilegeBytes, &granted, &allowed) == FALSE)
                    return fail(error, "Isolation package access check failed", ::GetLastError());
                const bool expected = packageRid == SECURITY_BUILTIN_PACKAGE_ANY_RESTRICTED_PACKAGE;
                if ((allowed != FALSE) != expected || (expected && (granted & 1u) == 0u))
                    return fail(error, "Suspended child does not enforce LPAC package access restrictions", ERROR_ACCESS_DENIED);
            }
            return true;
        }

    public:
        Lease() = default;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&&) = delete;
        Lease& operator=(Lease&&) = delete;

        ~Lease() noexcept
        {
            // Dropping a C++ owner is never evidence that native children have
            // stopped. The supervisor must call retire on a verified safe path.
            if (sid_ != nullptr) (void)::FreeSid(sid_);
        }

        [[nodiscard]] bool prepare(const std::filesystem::path& owned_root,
            const std::vector<std::filesystem::path>& read_only,
            const std::vector<std::filesystem::path>& writable, std::string& error)
        {
            try
            {
                error.clear();
                if (profile_created_ || !roots_.empty() || !boundary_handles_.empty()
                    || (read_only.empty() && writable.empty())
                    || read_only.size() > maximum_roots || writable.size() > maximum_roots
                    || read_only.size() + writable.size() > maximum_roots)
                    return fail(error, "Isolation lease is already used or exceeds its root bound", ERROR_INVALID_PARAMETER);
                if (!checked_path(owned_root, error)) return false;
                owned_root_ = owned_root;
                const auto admit = [&](const std::filesystem::path& path, bool write)
                {
                    if (!checked_path(path, error)) return false;
                    if (!beneath(path, owned_root_))
                        return fail(error, "Isolation grants must be proper descendants of the owned root", ERROR_ACCESS_DENIED);
                    for (const auto& existing : roots_)
                        if (same_component(path, existing.path) || beneath(path, existing.path)
                            || beneath(existing.path, path))
                            return fail(error, "Isolation grant roots must be disjoint", ERROR_INVALID_PARAMETER);
                    if (!hold_ancestors(path, error)) return false;
                    Node root{};
                    if (!open_node(path, true, true, root, error)) return false;
                    root.writable = write;
                    roots_.emplace_back(std::move(root));
                    return true;
                };
                for (const auto& path : read_only) if (!admit(path, false)) return false;
                for (const auto& path : writable) if (!admit(path, true)) return false;
                std::vector<Node> nodes{};
                if (!collect_nodes(nodes, error) || !preflight_acls(nodes, error)) return false;
                std::array<unsigned char, 16u> entropy{};
                const NTSTATUS random = ::BCryptGenRandom(nullptr, entropy.data(),
                    static_cast<ULONG>(entropy.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
                if (random < 0)
                    return fail(error, "Isolation identity randomness is unavailable", static_cast<DWORD>(random));
                profile_name_ = L"epoch.candidate.";
                constexpr wchar_t digits[] = L"0123456789abcdef";
                for (const auto byte : entropy)
                {
                    profile_name_.push_back(digits[byte >> 4u]);
                    profile_name_.push_back(digits[byte & 15u]);
                }
                const HRESULT created = ::CreateAppContainerProfile(profile_name_.c_str(),
                    L"Epoch isolated candidate", L"Disposable Epoch candidate execution identity",
                    nullptr, 0u, &sid_);
                if (created != S_OK)
                    return fail(error, "Fresh isolation profile creation failed; existing profiles are never adopted",
                        static_cast<DWORD>(created));
                profile_created_ = true;
                if (sid_ == nullptr || ::IsValidSid(sid_) == FALSE)
                    return fail(error, "Isolation profile returned an invalid identity", ERROR_INVALID_SID);
                capabilities_.AppContainerSid = sid_;
                capabilities_.Capabilities = nullptr;
                capabilities_.CapabilityCount = 0u;
                capabilities_.Reserved = 0u;
                for (const auto& node : nodes)
                    if (!change_acl(node, true, error)) return false;
                ready_ = true;
                return true;
            }
            catch (...)
            {
                return fail(error, "Isolation lease preparation failed", ERROR_NOT_ENOUGH_MEMORY);
            }
        }

        [[nodiscard]] SECURITY_CAPABILITIES* capabilities() noexcept
        { return ready_ ? &capabilities_ : nullptr; }

        // The supervisor supplies its still-suspended child and resumes only
        // after this check and successful assignment to the owned Job Object.
        [[nodiscard]] bool verify(HANDLE suspendedProcess, std::string& error)
        {
            try
            {
                error.clear();
                if (!ready_ || suspendedProcess == nullptr || suspendedProcess == INVALID_HANDLE_VALUE)
                    return fail(error, "Isolation verification requires a prepared lease and child", ERROR_INVALID_HANDLE);
                HANDLE opened{};
                if (::OpenProcessToken(suspendedProcess, TOKEN_QUERY | TOKEN_DUPLICATE, &opened) == FALSE)
                    return fail(error, "Suspended child token could not be opened", ::GetLastError());
                Handle token{opened};
                for (const auto kind : {TokenIsAppContainer, TokenIsLessPrivilegedAppContainer})
                {
                    DWORD enabled{}, received{};
                    if (::GetTokenInformation(token.value, kind, &enabled, sizeof(enabled), &received) == FALSE)
                    {
                        const DWORD failure = ::GetLastError();
                        if (kind == TokenIsLessPrivilegedAppContainer && failure == ERROR_INVALID_PARAMETER)
                        {
                            if (!verify_package_access(token.value, error)) return false;
                            continue;
                        }
                        return fail(error, kind == TokenIsAppContainer
                            ? "Suspended child AppContainer identity could not be queried"
                            : "Suspended child LPAC identity could not be queried", failure);
                    }
                    if (received != sizeof(enabled) || enabled == 0u)
                        return fail(error, "Suspended child is not a verified LPAC", ERROR_ACCESS_DENIED);
                }
                std::vector<std::uintptr_t> data{};
                if (!token_data(token.value, TokenAppContainerSid, data, error)) return false;
                const auto* container = reinterpret_cast<const TOKEN_APPCONTAINER_INFORMATION*>(data.data());
                if (container->TokenAppContainer == nullptr
                    || ::IsValidSid(container->TokenAppContainer) == FALSE
                    || ::EqualSid(container->TokenAppContainer, sid_) == FALSE)
                    return fail(error, "Suspended child AppContainer identity differs from its lease", ERROR_ACCESS_DENIED);
                if (!token_data(token.value, TokenCapabilities, data, error)) return false;
                if (reinterpret_cast<const TOKEN_GROUPS*>(data.data())->GroupCount != 0u)
                    return fail(error, "Suspended child has unapproved capabilities", ERROR_ACCESS_DENIED);
                if (!token_data(token.value, TokenIntegrityLevel, data, error)) return false;
                const auto* label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(data.data());
                if (label->Label.Sid == nullptr || ::IsValidSid(label->Label.Sid) == FALSE)
                    return fail(error, "Suspended child integrity identity is invalid", ERROR_INVALID_SID);
                const auto count = *::GetSidSubAuthorityCount(label->Label.Sid);
                if (count == 0u || *::GetSidSubAuthority(label->Label.Sid, count - 1u) != SECURITY_MANDATORY_LOW_RID)
                    return fail(error, "Suspended child integrity is not low", ERROR_ACCESS_DENIED);
                for (const auto kind : {TokenElevation, TokenUIAccess})
                {
                    DWORD enabled{}, received{};
                    if (::GetTokenInformation(token.value, kind, &enabled, sizeof(enabled), &received) == FALSE)
                        return fail(error, "Suspended child privilege state could not be queried", ::GetLastError());
                    if (received != sizeof(enabled) || enabled != 0u)
                        return fail(error, "Suspended child has elevated or UI access", ERROR_ACCESS_DENIED);
                }
                return true;
            }
            catch (...)
            {
                return fail(error, "Suspended child token verification failed", ERROR_NOT_ENOUGH_MEMORY);
            }
        }

        // Call only before any child is created, or after the supervisor has
        // positively observed retirement of every process using this identity.
        // A failed return preserves the lease so cleanup can be retried.
        [[nodiscard]] bool retire(std::string& error)
        {
            try
            {
                error.clear();
                ready_ = false;
                if (profile_created_ && sid_ != nullptr)
                {
                    std::vector<Node> nodes{};
                    if (!collect_nodes(nodes, error, true) || !preflight_acls(nodes, error)) return false;
                    for (const auto& node : nodes)
                        if (node.revoke_acl && !change_acl(node, false, error)) return false;
                }
                if (profile_created_)
                {
                    const HRESULT deleted = ::DeleteAppContainerProfile(profile_name_.c_str());
                    if (FAILED(deleted))
                        return fail(error, "Isolation profile retirement is incomplete", static_cast<DWORD>(deleted));
                    profile_created_ = false;
                }
                capabilities_ = {};
                if (sid_ != nullptr) (void)::FreeSid(std::exchange(sid_, nullptr));
                roots_.clear();
                boundary_handles_.clear();
                return true;
            }
            catch (...)
            {
                return fail(error, "Isolation retirement failed", ERROR_NOT_ENOUGH_MEMORY);
            }
        }
    };
}

#endif
