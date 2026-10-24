        [[nodiscard]] inline std::string unescape_json_string_basic(std::string text)
        {
            std::string out;
            out.reserve(text.size());

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] != '\\' || (i + 1) >= text.size())
                {
                    out.push_back(text[i]);
                    continue;
                }

                const char next = text[++i];
                switch (next)
                {
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/'); break;
                case '"':  out.push_back('"'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                default:
                    out.push_back(next);
                    break;
                }
            }

            return out;
        }

        inline constexpr std::size_t k_release_json_max_input_bytes = 256u * 1024u;
        inline constexpr std::size_t k_release_json_max_depth = 16u;
        inline constexpr std::size_t k_release_json_max_array_items = 64u;
        inline constexpr std::size_t k_release_json_max_string_bytes = 192u * 1024u;
        inline constexpr std::size_t k_release_json_max_object_fields = 64u;
        inline constexpr std::size_t k_release_json_max_field_bytes = 96u;

        [[nodiscard]] inline bool json_space(const char ch) noexcept
        {
            return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
        }

        class BoundedJsonReader final
        {
          public:
            explicit BoundedJsonReader(const std::string_view text) noexcept : text_{text} {}

            [[nodiscard]] bool document() noexcept
            {
                if (text_.empty() || text_.size() > k_release_json_max_input_bytes)
                    return false;
                skip_space();
                if (!value(0u))
                    return false;
                skip_space();
                return position_ == text_.size();
            }

          private:
            std::string_view text_;
            std::size_t position_{};

            void skip_space() noexcept
            {
                while (position_ < text_.size() && json_space(text_[position_]))
                    ++position_;
            }

            [[nodiscard]] bool consume(const std::string_view token) noexcept
            {
                if (text_.substr(position_, token.size()) != token)
                    return false;
                position_ += token.size();
                return true;
            }

            [[nodiscard]] bool string(const std::size_t maximum_bytes) noexcept
            {
                if (position_ >= text_.size() || text_[position_] != '"')
                    return false;
                const std::size_t content_start = ++position_;
                bool escaping = false;
                for (; position_ < text_.size(); ++position_)
                {
                    if (position_ - content_start > maximum_bytes)
                        return false;
                    const unsigned char ch = static_cast<unsigned char>(text_[position_]);
                    if (escaping)
                    {
                        if (ch == 'u')
                        {
                            if (position_ + 4u >= text_.size())
                                return false;
                            for (std::size_t digit = 1u; digit <= 4u; ++digit)
                            {
                                const unsigned char hex =
                                    static_cast<unsigned char>(text_[position_ + digit]);
                                if (!((hex >= '0' && hex <= '9') || (hex >= 'a' && hex <= 'f') ||
                                      (hex >= 'A' && hex <= 'F')))
                                {
                                    return false;
                                }
                            }
                            position_ += 4u;
                        }
                        else if (ch != '"' && ch != '\\' && ch != '/' && ch != 'b' && ch != 'f' &&
                                 ch != 'n' && ch != 'r' && ch != 't')
                        {
                            return false;
                        }
                        escaping = false;
                    }
                    else if (ch == '\\')
                    {
                        escaping = true;
                    }
                    else if (ch == '"')
                    {
                        ++position_;
                        return true;
                    }
                    else if (ch < 0x20u)
                    {
                        return false;
                    }
                }
                return false;
            }

            [[nodiscard]] bool number() noexcept
            {
                const std::size_t start = position_;
                if (position_ < text_.size() && text_[position_] == '-')
                    ++position_;
                if (position_ >= text_.size())
                    return false;
                if (text_[position_] == '0')
                {
                    ++position_;
                }
                else
                {
                    if (text_[position_] < '1' || text_[position_] > '9')
                        return false;
                    while (position_ < text_.size() && text_[position_] >= '0' &&
                           text_[position_] <= '9')
                    {
                        ++position_;
                    }
                }
                if (position_ < text_.size() && text_[position_] == '.')
                {
                    ++position_;
                    const std::size_t fractional_start = position_;
                    while (position_ < text_.size() && text_[position_] >= '0' &&
                           text_[position_] <= '9')
                    {
                        ++position_;
                    }
                    if (position_ == fractional_start)
                        return false;
                }
                if (position_ < text_.size() &&
                    (text_[position_] == 'e' || text_[position_] == 'E'))
                {
                    ++position_;
                    if (position_ < text_.size() &&
                        (text_[position_] == '+' || text_[position_] == '-'))
                    {
                        ++position_;
                    }
                    const std::size_t exponent_start = position_;
                    while (position_ < text_.size() && text_[position_] >= '0' &&
                           text_[position_] <= '9')
                    {
                        ++position_;
                    }
                    if (position_ == exponent_start)
                        return false;
                }
                return position_ > start;
            }

            [[nodiscard]] bool object(const std::size_t depth) noexcept
            {
                if (depth > k_release_json_max_depth || position_ >= text_.size() ||
                    text_[position_] != '{')
                {
                    return false;
                }
                ++position_;
                skip_space();
                if (position_ < text_.size() && text_[position_] == '}')
                {
                    ++position_;
                    return true;
                }

                std::size_t fields = 0u;
                for (;;)
                {
                    if (++fields > k_release_json_max_object_fields ||
                        !string(k_release_json_max_field_bytes))
                    {
                        return false;
                    }
                    skip_space();
                    if (position_ >= text_.size() || text_[position_] != ':')
                        return false;
                    ++position_;
                    skip_space();
                    if (!value(depth))
                        return false;
                    skip_space();
                    if (position_ >= text_.size())
                        return false;
                    if (text_[position_] == '}')
                    {
                        ++position_;
                        return true;
                    }
                    if (text_[position_] != ',')
                        return false;
                    ++position_;
                    skip_space();
                }
            }

            [[nodiscard]] bool array(const std::size_t depth) noexcept
            {
                if (depth > k_release_json_max_depth || position_ >= text_.size() ||
                    text_[position_] != '[')
                {
                    return false;
                }
                ++position_;
                skip_space();
                if (position_ < text_.size() && text_[position_] == ']')
                {
                    ++position_;
                    return true;
                }

                std::size_t items = 0u;
                for (;;)
                {
                    if (++items > k_release_json_max_array_items || !value(depth))
                        return false;
                    skip_space();
                    if (position_ >= text_.size())
                        return false;
                    if (text_[position_] == ']')
                    {
                        ++position_;
                        return true;
                    }
                    if (text_[position_] != ',')
                        return false;
                    ++position_;
                    skip_space();
                }
            }

            [[nodiscard]] bool value(const std::size_t depth) noexcept
            {
                if (depth > k_release_json_max_depth || position_ >= text_.size())
                    return false;
                switch (text_[position_])
                {
                case '{':
                    return object(depth + 1u);
                case '[':
                    return array(depth + 1u);
                case '"':
                    return string(k_release_json_max_string_bytes);
                case 't':
                    return consume("true");
                case 'f':
                    return consume("false");
                case 'n':
                    return consume("null");
                default:
                    return number();
                }
            }
        };

        [[nodiscard]] inline bool
        json_document_within_release_limits(const std::string_view text) noexcept
        {
            return BoundedJsonReader{text}.document();
        }

        [[nodiscard]] inline std::optional<std::size_t>
        json_string_end(const std::string_view text, const std::size_t open,
                        const std::size_t maximum_bytes) noexcept
        {
            if (open >= text.size() || text[open] != '"')
                return std::nullopt;
            bool escaping = false;
            for (std::size_t cursor = open + 1u; cursor < text.size(); ++cursor)
            {
                if (cursor - open - 1u > maximum_bytes)
                    return std::nullopt;
                const char ch = text[cursor];
                if (escaping)
                    escaping = false;
                else if (ch == '\\')
                    escaping = true;
                else if (ch == '"')
                    return cursor + 1u;
            }
            return std::nullopt;
        }

        [[nodiscard]] inline std::optional<std::size_t>
        json_value_end(const std::string_view text, const std::size_t start) noexcept
        {
            if (start >= text.size())
                return std::nullopt;
            if (text[start] == '"')
                return json_string_end(text, start, k_release_json_max_string_bytes);
            if (text[start] != '{' && text[start] != '[')
            {
                std::size_t cursor = start;
                while (cursor < text.size() && text[cursor] != ',' && text[cursor] != '}' &&
                       text[cursor] != ']' && !json_space(text[cursor]))
                {
                    ++cursor;
                }
                return cursor == start ? std::nullopt : std::optional<std::size_t>{cursor};
            }

            std::array<char, k_release_json_max_depth> closing{};
            std::size_t depth = 0u;
            bool in_string = false;
            bool escaping = false;
            for (std::size_t cursor = start; cursor < text.size(); ++cursor)
            {
                const char ch = text[cursor];
                if (in_string)
                {
                    if (escaping)
                        escaping = false;
                    else if (ch == '\\')
                        escaping = true;
                    else if (ch == '"')
                        in_string = false;
                    continue;
                }
                if (ch == '"')
                {
                    in_string = true;
                    continue;
                }
                if (ch == '{' || ch == '[')
                {
                    if (depth >= closing.size())
                        return std::nullopt;
                    closing[depth++] = ch == '{' ? '}' : ']';
                    continue;
                }
                if (ch == '}' || ch == ']')
                {
                    if (depth == 0u || closing[depth - 1u] != ch)
                        return std::nullopt;
                    if (--depth == 0u)
                        return cursor + 1u;
                }
            }
            return std::nullopt;
        }

        struct JsonMemberSpan final
        {
            std::size_t start{};
            std::size_t end{};
        };

        [[nodiscard]] inline std::optional<JsonMemberSpan>
        json_object_member_span(const std::string_view object_text,
                                const std::string_view field_name) noexcept
        {
            if (field_name.empty() || field_name.size() > k_release_json_max_field_bytes ||
                !json_document_within_release_limits(object_text))
            {
                return std::nullopt;
            }
            std::size_t cursor = 0u;
            while (cursor < object_text.size() && json_space(object_text[cursor]))
                ++cursor;
            if (cursor >= object_text.size() || object_text[cursor] != '{')
                return std::nullopt;
            ++cursor;
            std::optional<JsonMemberSpan> found;
            std::size_t fields = 0u;
            for (;;)
            {
                while (cursor < object_text.size() && json_space(object_text[cursor]))
                    ++cursor;
                if (cursor >= object_text.size() || object_text[cursor] == '}')
                    return found;
                if (++fields > k_release_json_max_object_fields || object_text[cursor] != '"')
                    return std::nullopt;
                const auto key_end =
                    json_string_end(object_text, cursor, k_release_json_max_field_bytes);
                if (!key_end)
                    return std::nullopt;
                const std::string_view key =
                    object_text.substr(cursor + 1u, *key_end - cursor - 2u);
                cursor = *key_end;
                while (cursor < object_text.size() && json_space(object_text[cursor]))
                    ++cursor;
                if (cursor >= object_text.size() || object_text[cursor] != ':')
                    return std::nullopt;
                do
                    ++cursor;
                while (cursor < object_text.size() && json_space(object_text[cursor]));
                const auto value_end = json_value_end(object_text, cursor);
                if (!value_end)
                    return std::nullopt;
                if (key == field_name)
                {
                    if (found)
                        return std::nullopt;
                    found = JsonMemberSpan{cursor, *value_end};
                }
                cursor = *value_end;
                while (cursor < object_text.size() && json_space(object_text[cursor]))
                    ++cursor;
                if (cursor >= object_text.size())
                    return std::nullopt;
                if (object_text[cursor] == '}')
                    return found;
                if (object_text[cursor] != ',')
                    return std::nullopt;
                ++cursor;
            }
        }

        [[nodiscard]] inline std::vector<std::string>
        extract_json_objects(const std::string_view array_text)
        {
            std::vector<std::string> objects;
            if (!json_document_within_release_limits(array_text))
                return objects;
            std::size_t cursor = 0u;
            while (cursor < array_text.size() && json_space(array_text[cursor]))
                ++cursor;
            if (cursor >= array_text.size() || array_text[cursor] != '[')
                return objects;
            ++cursor;
            while (cursor < array_text.size())
            {
                while (cursor < array_text.size() && json_space(array_text[cursor]))
                    ++cursor;
                if (cursor < array_text.size() && array_text[cursor] == ']')
                    return objects;
                if (objects.size() >= k_release_json_max_array_items ||
                    cursor >= array_text.size() || array_text[cursor] != '{')
                {
                    return {};
                }
                const auto end = json_value_end(array_text, cursor);
                if (!end)
                    return {};
                objects.emplace_back(array_text.substr(cursor, *end - cursor));
                cursor = *end;
                while (cursor < array_text.size() && json_space(array_text[cursor]))
                    ++cursor;
                if (cursor >= array_text.size())
                    return {};
                if (array_text[cursor] == ']')
                    return objects;
                if (array_text[cursor] != ',')
                    return {};
                ++cursor;
            }
            return {};
        }

        [[nodiscard]] inline std::string
        extract_json_string_field(const std::string_view object_text,
                                  const std::string_view field_name)
        {
            const auto span = json_object_member_span(object_text, field_name);
            if (!span || object_text[span->start] != '"')
                return {};
            return unescape_json_string_basic(
                std::string{object_text.substr(span->start + 1u, span->end - span->start - 2u)});
        }

        [[nodiscard]] inline std::string extract_json_array_text(const std::string_view object_text,
                                                                 const std::string_view field_name)
        {
            const auto span = json_object_member_span(object_text, field_name);
            if (!span || object_text[span->start] != '[')
                return {};
            return std::string{object_text.substr(span->start, span->end - span->start)};
        }

        [[nodiscard]] inline std::string
        extract_json_object_text(const std::string_view object_text,
                                 const std::string_view field_name)
        {
            const auto span = json_object_member_span(object_text, field_name);
            if (!span || object_text[span->start] != '{')
                return {};
            return std::string{object_text.substr(span->start, span->end - span->start)};
        }

        [[nodiscard]] inline std::optional<std::vector<unsigned char>>
        decode_base64url(const std::string_view encoded)
        {
            std::vector<unsigned char> decoded;
            decoded.reserve((encoded.size() * 3u) / 4u + 1u);

            std::uint32_t accumulator = 0u;
            int bit_count = 0;
            for (const unsigned char ch : encoded)
            {
                int value = -1;
                if (ch >= 'A' && ch <= 'Z')
                    value = static_cast<int>(ch - 'A');
                else if (ch >= 'a' && ch <= 'z')
                    value = 26 + static_cast<int>(ch - 'a');
                else if (ch >= '0' && ch <= '9')
                    value = 52 + static_cast<int>(ch - '0');
                else if (ch == '-')
                    value = 62;
                else if (ch == '_')
                    value = 63;
                else
                    return std::nullopt;

                accumulator = (accumulator << 6u) | static_cast<std::uint32_t>(value);
                bit_count += 6;
                if (bit_count >= 8)
                {
                    bit_count -= 8;
                    decoded.push_back(static_cast<unsigned char>(
                        (accumulator >> static_cast<unsigned int>(bit_count)) & 0xFFu));
                    accumulator &= bit_count == 0
                        ? 0u
                        : ((1u << static_cast<unsigned int>(bit_count)) - 1u);
                }
            }

            if (bit_count != 0 && accumulator != 0u)
                return std::nullopt;

            return decoded;
        }

        [[nodiscard]] inline bool verify_ed25519(
            const std::string_view message,
            const std::string_view signature_base64url)
        {
            const auto public_key = decode_base64url(
                PROJECT_RELEASE_SIGNING_PUBLIC_KEY_BASE64URL);
            const auto signature = decode_base64url(signature_base64url);
            if (!public_key || public_key->size() != 32u
                || !signature || signature->size() != 64u)
            {
                return false;
            }

            EVP_PKEY* key = EVP_PKEY_new_raw_public_key(
                EVP_PKEY_ED25519,
                nullptr,
                public_key->data(),
                public_key->size());
            if (key == nullptr)
                return false;

            EVP_MD_CTX* context = EVP_MD_CTX_new();
            if (context == nullptr)
            {
                EVP_PKEY_free(key);
                return false;
            }

            const bool verified = EVP_DigestVerifyInit(
                    context,
                    nullptr,
                    nullptr,
                    nullptr,
                    key) == 1
                && EVP_DigestVerify(
                    context,
                    signature->data(),
                    signature->size(),
                    reinterpret_cast<const unsigned char*>(message.data()),
                    message.size()) == 1;

            EVP_MD_CTX_free(context);
            EVP_PKEY_free(key);
            return verified;
        }

        [[nodiscard]] inline std::optional<std::string> verified_release_integrity_payload(const std::string &document)
        {
            if (!json_document_within_release_limits(document))
            {
                log_error("Release-integrity metadata exceeds bounded JSON policy "
                          "or is malformed.");
                return std::nullopt;
            }
            const std::string canonical_payload = extract_json_string_field(document, "canonical_payload");
            const std::string signature_object = extract_json_object_text(document, "signature");
            const std::string algorithm = extract_json_string_field(signature_object, "algorithm");
            const std::string key_id = extract_json_string_field(signature_object, "key_id");
            const std::string signature = extract_json_string_field(signature_object, "value");

            if (canonical_payload.empty() ||
                !json_document_within_release_limits(canonical_payload) ||
                algorithm != PROJECT_RELEASE_SIGNING_ALGORITHM ||
                key_id != PROJECT_RELEASE_SIGNING_KEY_ID ||
                extract_json_string_field(canonical_payload, "schema") !=
                    PROJECT_RELEASE_INTEGRITY_SCHEMA ||
                extract_json_string_field(canonical_payload, "edition") != EDITION_ID)
            {
                log_error("Release-integrity metadata is incomplete or uses an "
                          "untrusted signing identity.");
                return std::nullopt;
            }

            if (!verify_ed25519(canonical_payload, signature))
            {
                log_error("Release-integrity Ed25519 signature verification failed.");
                return std::nullopt;
            }

            log_info("Release-integrity Ed25519 signature verified with pinned key " + key_id +
                     ".");
            return canonical_payload;
        }

        struct IntegrityAssetInfo
        {
            std::string name;
            std::string url;
            std::string checksum_url;
            std::string sha256;
        };

        struct IntegrityReleaseInfo
        {
            std::string version;
            std::string tag;
            std::vector<IntegrityAssetInfo> assets;
        };

        [[nodiscard]] inline std::vector<IntegrityReleaseInfo>
        parse_release_integrity_payload(const std::string& canonical_payload)
        {
            std::vector<IntegrityReleaseInfo> releases;
            const auto releases_array = extract_json_array_text(canonical_payload, "releases");
            if (releases_array.empty())
                return releases;

            for (const auto& release_object : extract_json_objects(releases_array))
            {
                IntegrityReleaseInfo release{};
                release.version = extract_json_string_field(release_object, "version");
                release.tag = extract_json_string_field(release_object, "tag");

                const auto assets_array = extract_json_array_text(release_object, "assets");
                for (const auto& asset_object : extract_json_objects(assets_array))
                {
                    IntegrityAssetInfo asset{};
                    asset.name = extract_json_string_field(asset_object, "name");
                    asset.url = extract_json_string_field(asset_object, "url");
                    asset.checksum_url =
                        extract_json_string_field(asset_object, "checksum_url");
                    asset.sha256 = extract_json_string_field(asset_object, "sha256");
                    if (!asset.name.empty() && !asset.url.empty() && !asset.sha256.empty())
                        release.assets.push_back(std::move(asset));
                }

                if (!release.version.empty() && !release.tag.empty())
                    releases.push_back(std::move(release));
            }

            return releases;
        }

        struct ResolvedPackagedRelease
        {
            bool found{ false };
            bool discovery_ok{ false };
            std::string tag_name;
            std::string binary_name;
            std::string remote_version;
            std::string latest_version;
            std::string binary_url;
            std::string checksum_url;
            std::string expected_sha256;
        };

        [[nodiscard]] inline std::optional<std::string> download_expected_sha256(
            const std::string& checksum_url)
        {
            if (checksum_url.empty())
                return std::nullopt;

            const auto checksum_path =
                make_temp_download_path("sha256").replace_extension(".txt");
            if (!download_file(checksum_url, checksum_path.string()))
                return std::nullopt;

            const auto expected = parse_sha256_text(read_text_file(checksum_path));
            std::error_code ec;
            std::filesystem::remove(checksum_path, ec);
            return expected;
        }

        [[nodiscard]] inline std::optional<std::string> resolve_expected_sha256(
            const std::string& checksum_url,
            const std::string_view signed_sha256)
        {
            if (!signed_sha256.empty())
            {
                const auto expected = parse_sha256_text(std::string{ signed_sha256 });
                if (expected)
                    log_info("Using SHA-256 authenticated by the signed release manifest.");
                return expected;
            }

            return download_expected_sha256(checksum_url);
        }

        [[nodiscard]] inline ResolvedPackagedRelease resolve_packaged_release()
        {
            ResolvedPackagedRelease resolved{};
            if (!PROJECT_PACKAGED_UPDATES_ENABLED)
                return resolved;
            const auto releases_json_path =
                make_temp_download_path("release_integrity").replace_extension(".json");

            if (!download_file(PROJECT_RELEASE_INTEGRITY_URL(), releases_json_path.string()))
                return resolved;

            const auto json = read_text_file(releases_json_path);
            std::error_code ec;
            std::filesystem::remove(releases_json_path, ec);
            if (json.empty())
                return resolved;

            const auto canonical_payload = verified_release_integrity_payload(json);
            if (!canonical_payload)
                return resolved;

            const auto releases = parse_release_integrity_payload(*canonical_payload);
            resolved.discovery_ok = true;
            for (const auto& release : releases)
            {
                const std::string version = extract_version_string(release.version);
                if (!version.empty()
                    && (resolved.latest_version.empty()
                        || compare_versions(resolved.latest_version, version) < 0))
                {
                    resolved.latest_version = version;
                }
            }
            const auto binary_prefix = PACKAGED_BINARY_ASSET_PREFIX();
            const auto binary_suffix = PACKAGED_BINARY_ASSET_SUFFIX();

            for (const auto& release : releases)
            {
                for (const auto& asset : release.assets)
                {
                    if (!starts_with_ascii(asset.name, binary_prefix)
                        || !ends_with_ascii(asset.name, binary_suffix))
                    {
                        continue;
                    }

                    const std::size_t version_offset = binary_prefix.size();
                    const std::size_t version_length =
                        asset.name.size() - binary_prefix.size() - binary_suffix.size();
                    const std::string remote_version =
                        extract_version_string(asset.name.substr(version_offset, version_length));
                    if (remote_version.empty())
                    {
                        continue;
                    }

                    const auto expected_digest = core::sha256::from_hex(asset.sha256);
                    if (!expected_digest
                        || extract_version_string(release.version) != remote_version
                        || release.tag != "v" + remote_version
                        || !PROJECT_RELEASE_DOWNLOAD_ALLOWED(asset.url)
                        || (!asset.checksum_url.empty()
                            && !PROJECT_RELEASE_DOWNLOAD_ALLOWED(asset.checksum_url)))
                    {
                        continue;
                    }

                    resolved.found = true;
                    resolved.tag_name = release.tag;
                    resolved.binary_name = asset.name;
                    resolved.remote_version = remote_version;
                    resolved.binary_url = asset.url;
                    resolved.checksum_url = asset.checksum_url;
                    resolved.expected_sha256 = core::sha256::hex(*expected_digest);
                    return resolved;
                }
            }

            return resolved;
        }

        [[nodiscard]] inline bool text_matches_ci_job(
            const std::string& value,
            const std::string& job_name)
        {
            if (value.empty() || job_name.empty())
                return false;

            const auto normalized_value = lower_ascii(value);
            const auto normalized_job = lower_ascii(job_name);
            return normalized_value == normalized_job
                || normalized_value.find(normalized_job) != std::string::npos;
        }

        [[nodiscard]] inline BuildStatusResult check_platform_build_status(
            const std::string& runs_api_url,
            const std::string& job_name)
        {
            BuildStatusResult result{};
            result.job_name = job_name;

            if (runs_api_url.empty() || job_name.empty())
            {
                result.reason = "No platform build gate is configured.";
                return result;
            }

            const auto runs_json_path =
                make_temp_download_path("actions_runs").replace_extension(".json");
            if (!download_file(runs_api_url, runs_json_path.string()))
            {
                result.reason = "Could not download hosted build run metadata.";
                return result;
            }

            const auto runs_json = read_text_file(runs_json_path);
            std::error_code ec;
            std::filesystem::remove(runs_json_path, ec);
            if (runs_json.empty())
            {
                result.reason = "Hosted build run metadata was empty.";
                return result;
            }

            const auto workflow_runs = extract_json_array_text(runs_json, "workflow_runs");
            if (workflow_runs.empty())
            {
                result.reason = "Hosted build response did not contain workflow runs.";
                return result;
            }

            bool attempted_job_metadata = false;
            bool parsed_job_metadata = false;
            for (const auto& run_object : extract_json_objects(workflow_runs))
            {
                const auto jobs_url = extract_json_string_field(run_object, "jobs_url");
                if (jobs_url.empty())
                    continue;

                attempted_job_metadata = true;
                const auto jobs_json_path =
                    make_temp_download_path("actions_jobs").replace_extension(".json");
                if (!download_file(jobs_url, jobs_json_path.string()))
                    continue;

                const auto jobs_json = read_text_file(jobs_json_path);
                std::filesystem::remove(jobs_json_path, ec);
                if (jobs_json.empty())
                    continue;

                const auto jobs_array = extract_json_array_text(jobs_json, "jobs");
                if (jobs_array.empty())
                    continue;

                parsed_job_metadata = true;
                for (const auto& job_object : extract_json_objects(jobs_array))
                {
                    const auto candidate_name = extract_json_string_field(job_object, "name");
                    if (!text_matches_ci_job(candidate_name, job_name))
                        continue;

                    result.checked = true;
                    result.job_name = candidate_name.empty() ? job_name : candidate_name;
                    result.status = extract_json_string_field(job_object, "status");
                    result.conclusion = extract_json_string_field(job_object, "conclusion");
                    result.url = extract_json_string_field(job_object, "html_url");

                    if (result.status != "completed")
                    {
                        result.pending = true;
                        result.reason = "Platform build is still " + result.status + ".";
                        return result;
                    }

                    result.ok = result.conclusion == "success";
                    if (result.ok)
                    {
                        result.reason = "Platform build succeeded.";
                    }
                    else if (!result.conclusion.empty())
                    {
                        result.reason = "Platform build concluded with " + result.conclusion + ".";
                    }
                    else
                    {
                        result.reason = "Platform build completed without a success conclusion.";
                    }

                    return result;
                }
            }

            if (attempted_job_metadata && !parsed_job_metadata)
            {
                result.reason = "Could not download hosted build job metadata.";
                return result;
            }

            result.reason = "No matching platform build job was found for '" + job_name + "'.";
            return result;
        }

        [[nodiscard]] inline bool build_status_failure_is_transient(const BuildStatusResult& result)
        {
            if (result.checked || result.pending || result.ok)
                return false;

            return result.reason.find("Could not download hosted build") != std::string::npos
                || result.reason.find("Hosted build run metadata was empty") != std::string::npos
                || result.reason.find("Hosted build response did not contain workflow runs") != std::string::npos;
        }

        [[nodiscard]] inline std::string capture_process_output(
            const std::filesystem::path& executable,
            const std::vector<std::string>& args,
            const std::filesystem::path& working_directory,
            int* const exit_code = nullptr)
        {
            const auto temp_output =
                make_temp_download_path("proc_capture").replace_extension(".log");

            int local_exit_code = -1;
            const bool launched = run_process_hidden(
                executable,
                args,
                working_directory,
                temp_output,
                true,
                &local_exit_code);

            auto output = trim_ascii(read_text_file(temp_output));
            std::error_code ec;
            std::filesystem::remove(temp_output, ec);

            if (exit_code != nullptr)
                *exit_code = launched ? local_exit_code : -1;

            return launched ? output : std::string{};
        }

        [[nodiscard]] inline std::string query_latest_github_asset_url(
            const std::string& api_url,
            const std::string_view include_fragment,
            const std::string_view include_suffix,
            const std::string_view excluded_fragment,
            const std::filesystem::path& log_path)
        {
            const auto json_path =
                make_temp_download_path("release_asset").replace_extension(".json");

            const bool downloaded = download_file(api_url, json_path.string());
            const auto json = downloaded ? read_text_file(json_path) : std::string{};

            std::error_code ec;
            std::filesystem::remove(json_path, ec);

            if (json.empty())
            {
                append_log_line(log_path, "[ERROR] Failed to query GitHub release metadata: " + api_url);
                return {};
            }

            static const std::regex k_asset_regex(
                R"REGEX("browser_download_url"\s*:\s*"([^"]+)")REGEX",
                std::regex::optimize);

            std::string fallback_match;
            for (auto it = std::sregex_iterator(json.begin(), json.end(), k_asset_regex);
                it != std::sregex_iterator();
                ++it)
            {
                std::string candidate = unescape_json_string_basic((*it)[1].str());
                if (!include_fragment.empty()
                    && candidate.find(include_fragment) == std::string::npos)
                {
                    continue;
                }

                if (!include_suffix.empty()
                    && !candidate.ends_with(std::string{ include_suffix }))
                {
                    continue;
                }

                if (excluded_fragment.empty()
                    || candidate.find(excluded_fragment) == std::string::npos)
                {
                    return candidate;
                }

                if (fallback_match.empty())
                    fallback_match = std::move(candidate);
            }

            if (fallback_match.empty())
            {
                append_log_line(log_path, "[ERROR] No matching GitHub release asset was found.");
                return {};
            }

            return fallback_match;
        }

