        class ByteWriter final
        {
        public:
            explicit ByteWriter(std::uint64_t limit) noexcept
                : limit_(limit)
            {
            }

            [[nodiscard]] bool good() const noexcept
            {
                return good_;
            }

            [[nodiscard]] std::vector<std::byte> take() noexcept
            {
                return std::move(bytes_);
            }

            void raw(std::span<const std::byte> value)
            {
                if (!reserve(value.size()))
                    return;
                bytes_.insert(bytes_.end(), value.begin(), value.end());
            }

            template<std::unsigned_integral Value>
            void integer(Value value)
            {
                if (!reserve(sizeof(Value)))
                    return;
                for (std::size_t byte = 0u; byte < sizeof(Value); ++byte)
                {
                    bytes_.push_back(static_cast<std::byte>(
                        (value >> (byte * 8u))
                        & static_cast<Value>(0xffu)));
                }
            }

            template<std::signed_integral Value>
            void integer(Value value)
            {
                integer(static_cast<std::make_unsigned_t<Value>>(value));
            }

            template<typename Enum>
                requires std::is_enum_v<Enum>
            void enumeration(Enum value)
            {
                integer(static_cast<std::underlying_type_t<Enum>>(value));
            }

            void boolean(bool value)
            {
                integer<std::uint8_t>(value ? 1u : 0u);
            }

            void string(std::string_view value)
            {
                if (value.size()
                    > (std::numeric_limits<std::uint32_t>::max)())
                {
                    good_ = false;
                    return;
                }
                integer(static_cast<std::uint32_t>(value.size()));
                raw(std::as_bytes(std::span{value.data(), value.size()}));
            }

            void hash(ContentHash value)
            {
                for (const std::uint64_t word : value.words)
                    integer(word);
            }

        private:
            [[nodiscard]] bool reserve(std::size_t additional)
            {
                if (!good_
                    || additional > limit_
                    || bytes_.size() > limit_ - additional)
                {
                    good_ = false;
                    return false;
                }
                bytes_.reserve(bytes_.size() + additional);
                return true;
            }

            std::vector<std::byte> bytes_{};
            std::uint64_t limit_{};
            bool good_{true};
        };

        class ByteReader final
        {
        public:
            explicit ByteReader(
                std::span<const std::byte> bytes) noexcept
                : bytes_(bytes)
            {
            }

            [[nodiscard]] bool good() const noexcept
            {
                return good_;
            }

            [[nodiscard]] bool done() const noexcept
            {
                return good_ && offset_ == bytes_.size();
            }

            void fail() noexcept
            {
                good_ = false;
            }

            [[nodiscard]] std::span<const std::byte> raw(
                std::size_t count)
            {
                if (!good_ || count > bytes_.size() - offset_)
                {
                    good_ = false;
                    return {};
                }
                const auto value = bytes_.subspan(offset_, count);
                offset_ += count;
                return value;
            }

            template<std::unsigned_integral Value>
            [[nodiscard]] Value integer()
            {
                const auto valueBytes = raw(sizeof(Value));
                if (!good_)
                    return {};
                Value value{};
                for (std::size_t byte = 0u; byte < sizeof(Value); ++byte)
                {
                    value |= static_cast<Value>(
                        std::to_integer<std::uint8_t>(valueBytes[byte]))
                        << (byte * 8u);
                }
                return value;
            }

            template<std::signed_integral Value>
            [[nodiscard]] Value signed_integer()
            {
                return static_cast<Value>(
                    integer<std::make_unsigned_t<Value>>());
            }

            template<typename Enum>
                requires std::is_enum_v<Enum>
            [[nodiscard]] Enum enumeration()
            {
                using Underlying = std::underlying_type_t<Enum>;
                if constexpr (std::is_signed_v<Underlying>)
                    return static_cast<Enum>(signed_integer<Underlying>());
                else
                    return static_cast<Enum>(integer<Underlying>());
            }

            [[nodiscard]] bool boolean()
            {
                const std::uint8_t value = integer<std::uint8_t>();
                if (value > 1u)
                    good_ = false;
                return value != 0u;
            }

            [[nodiscard]] std::string string(
                std::uint32_t maximumBytes)
            {
                const std::uint32_t size = integer<std::uint32_t>();
                if (!good_ || size > maximumBytes)
                {
                    good_ = false;
                    return {};
                }
                const auto bytes = raw(size);
                if (!good_)
                    return {};
                return std::string{
                    reinterpret_cast<const char*>(bytes.data()),
                    bytes.size()};
            }

            [[nodiscard]] ContentHash hash()
            {
                ContentHash value{};
                for (std::uint64_t& word : value.words)
                    word = integer<std::uint64_t>();
                return value;
            }

        private:
            std::span<const std::byte> bytes_{};
            std::size_t offset_{};
            bool good_{true};
        };
