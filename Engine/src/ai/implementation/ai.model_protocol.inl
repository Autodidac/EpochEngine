        [[nodiscard]] static std::string_view model_reply_kind(std::string_view reply) noexcept
        {
            if (reply.empty()) return "unusable_reply";
            return "assistant_text";
        }

        [[nodiscard]] static InferenceBudget request_inference_budget(
            InferenceWorkload workload, std::string_view input) noexcept
        {
            auto budget = inference_budget(workload);
            // Every role uses the same loaded context policy. A provider or
            // explicit operator capacity can reduce the default; chat and
            // authoring must not silently pack against an unrelated old limit.
            {
                const auto native = native_runtime::selected();
                const std::size_t loadedCapacity = native
                    ? active_model_manifest().loaded_context_capacity_tokens
                    : selected_model_context_capacity();
                if (loadedCapacity != 0u && (native || loadedCapacity >= 8'192u))
                {
                    budget.context_tokens = loadedCapacity;
                    if (native)
                        budget.output_tokens=(std::min)(budget.output_tokens,
                            (std::max)(std::size_t{512u}, loadedCapacity / 4u));
                    else if (budget.output_tokens >= budget.context_tokens)
                        budget.output_tokens=(std::max)(std::size_t{2'048u}, budget.context_tokens / 8u);
                    budget.maximum_prompt_bytes = source_prompt_byte_budget(
                        budget.context_tokens, budget.output_tokens);
                }
            }
            return budget;
        }

        [[nodiscard]] static bool model_reply_has_visible_content(std::string_view reply)
        {
            return is_promotable_assistant_text(reply);
        }

        class StructuredJsonCursor final
        {
        public:
            explicit StructuredJsonCursor(std::string_view source) noexcept
                : source_(source)
            {
            }

            [[nodiscard]] bool consume(char expected) noexcept
            {
                skip_space();
                if (position_ >= source_.size()
                    || source_[position_] != expected)
                {
                    return false;
                }
                ++position_;
                return true;
            }

            [[nodiscard]] std::optional<std::string> string()
            {
                skip_space();
                if (position_ >= source_.size()
                    || source_[position_] != '"')
                {
                    return std::nullopt;
                }
                const std::size_t begin = ++position_;
                for (std::size_t index = begin; index < source_.size(); ++index)
                {
                    if (source_[index] != '"'
                        || json_character_is_escaped(source_, index, begin))
                    {
                        continue;
                    }
                    const std::string_view raw =
                        source_.substr(begin, index - begin);
                    std::string decoded = json_unescape(raw);
                    if (!raw.empty() && decoded.empty())
                        return std::nullopt;
                    position_ = index + 1u;
                    return decoded;
                }
                return std::nullopt;
            }

            [[nodiscard]] std::optional<std::uint32_t> bounded_integer(
                std::uint32_t maximum) noexcept
            {
                skip_space();
                const std::size_t begin = position_;
                std::uint32_t value{};
                while (position_ < source_.size()
                    && source_[position_] >= '0' && source_[position_] <= '9')
                {
                    const auto digit = static_cast<std::uint32_t>(
                        source_[position_] - '0');
                    if ((position_ > begin && source_[begin] == '0')
                        || digit > maximum || value > (maximum - digit) / 10u)
                        return std::nullopt;
                    value = value * 10u + digit;
                    ++position_;
                }
                if (position_ == begin)
                    return std::nullopt;
                return value;
            }

            [[nodiscard]] bool skip_value(unsigned depth = 0u)
            {
                if (depth > 32u) return false;
                skip_space();
                if (position_ >= source_.size()) return false;
                if (source_[position_] == '"') return string().has_value();
                if (consume('{'))
                {
                    if (consume('}')) return true;
                    do
                    {
                        if (!string() || !consume(':') || !skip_value(depth + 1u)) return false;
                        if (consume('}')) return true;
                    } while (consume(','));
                    return false;
                }
                if (consume('['))
                {
                    if (consume(']')) return true;
                    do
                    {
                        if (!skip_value(depth + 1u)) return false;
                        if (consume(']')) return true;
                    } while (consume(','));
                    return false;
                }
                for (const std::string_view literal : {"true", "false", "null"})
                {
                    if (source_.substr(position_).starts_with(literal))
                    {
                        position_ += literal.size();
                        return true;
                    }
                }
                const auto begin = position_;
                if (position_ < source_.size() && source_[position_] == '-') ++position_;
                const auto integer = position_;
                while (position_ < source_.size() && source_[position_] >= '0'
                    && source_[position_] <= '9') ++position_;
                if (position_ == integer || (position_ - integer > 1u && source_[integer] == '0')) return false;
                if (position_ < source_.size() && source_[position_] == '.')
                {
                    const auto fraction = ++position_;
                    while (position_ < source_.size() && source_[position_] >= '0'
                        && source_[position_] <= '9') ++position_;
                    if (position_ == fraction) return false;
                }
                if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E'))
                {
                    ++position_;
                    if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) ++position_;
                    const auto exponent = position_;
                    while (position_ < source_.size() && source_[position_] >= '0'
                        && source_[position_] <= '9') ++position_;
                    if (position_ == exponent) return false;
                }
                return position_ > begin;
            }

            [[nodiscard]] bool complete() noexcept
            {
                skip_space();
                return position_ == source_.size();
            }

            [[nodiscard]] std::optional<std::string_view> raw_value()
            {
                skip_space();
                const auto begin = position_;
                if (!skip_value()) return std::nullopt;
                return source_.substr(begin, position_ - begin);
            }

        private:
            void skip_space() noexcept
            {
                while (position_ < source_.size()
                    && std::isspace(static_cast<unsigned char>(
                        source_[position_])) != 0)
                {
                    ++position_;
                }
            }

            std::string_view source_{};
            std::size_t position_{};
        };

        using ModelJsonFields = std::vector<std::pair<std::string, std::string_view>>;

        [[noreturn]] static void invalid_model_stream()
        {
            throw std::runtime_error("Local model stream was malformed, incomplete, or outside the single-response contract. No partial result was admitted.");
        }

        static ModelJsonFields model_json_fields(std::string_view object)
        {
            StructuredJsonCursor cursor{object};
            ModelJsonFields fields;
            if (!cursor.consume('{')) invalid_model_stream();
            if (!cursor.consume('}'))
            {
                do
                {
                    auto key = cursor.string();
                    if (!key || !cursor.consume(':') || fields.size() >= 64u)
                        invalid_model_stream();
                    auto value = cursor.raw_value();
                    if (!value || std::ranges::any_of(fields, [&](const auto& field)
                        { return field.first == *key; })) invalid_model_stream();
                    fields.emplace_back(std::move(*key), *value);
                    if (cursor.consume('}')) break;
                    if (!cursor.consume(',')) invalid_model_stream();
                } while (true);
            }
            if (!cursor.complete()) invalid_model_stream();
            return fields;
        }

        static std::string_view model_json_field(const ModelJsonFields& fields,
            std::string_view key) noexcept
        {
            for (const auto& field : fields)
                if (field.first == key) return field.second;
            return {};
        }

        static std::string model_json_string(std::string_view value)
        {
            if (value.empty() || value.compare("null") == 0) return {};
            StructuredJsonCursor cursor{value};
            auto text = cursor.string();
            if (!text || !cursor.complete()) invalid_model_stream();
            return std::move(*text);
        }

        static std::optional<std::string_view> model_json_single_item(
            std::string_view array)
        {
            StructuredJsonCursor cursor{array};
            if (!cursor.consume('[')) invalid_model_stream();
            if (cursor.consume(']'))
            {
                if (!cursor.complete()) invalid_model_stream();
                return std::nullopt;
            }
            auto value = cursor.raw_value();
            if (!value || !cursor.consume(']') || !cursor.complete())
                invalid_model_stream();
            return value;
        }

        // Only the separate reasoning channel is a repetition candidate.
        // Metadata uses conservative encoded-length bounds, not repetition.
        // Source bytes and ordinary content can legitimately repeat.
        static void check_model_reasoning_tail(std::string& tail, std::string_view chunk,
            std::size_t& uncheckedBytes)
        {
            constexpr std::size_t tailLimit = 4096u;
            if (chunk.size() >= tailLimit) tail.assign(chunk.substr(chunk.size() - tailLimit));
            else
            {
                tail.append(chunk);
                if (tail.size() > tailLimit) tail.erase(0u, tail.size() - tailLimit);
            }
            // Token-sized deltas must not rescan the entire tail per byte.
            uncheckedBytes += chunk.size();
            if (uncheckedBytes < 96u) return;
            uncheckedBytes = 0u;
            for (std::size_t period = 96u; period * 4u <= tail.size(); ++period)
            {
                const std::string_view view{tail};
                const auto block = view.substr(view.size() - period);
                if (std::ranges::any_of(block, [](char byte)
                        { return !std::isspace(static_cast<unsigned char>(byte)); })
                    && block == view.substr(view.size() - period * 2u, period)
                    && block == view.substr(view.size() - period * 3u, period)
                    && block == view.substr(view.size() - period * 4u, period))
                    throw std::runtime_error("Local model repeated the same reasoning without completing its action. The request was retired; no partial result was admitted.");
            }
        }

        class ModelActionMetadataGuard final
        {
        public:
            void feed(std::string_view chunk)
            {
                for (const char byte : chunk)
                {
                    if (inString_)
                    {
                        if (!escaped_ && byte == '"')
                        {
                            inString_ = false;
                            if (isKey_) key_ = json_unescape(raw_);
                            limit_ = 0u;
                            continue;
                        }
                        ++rawSize_;
                        if (isKey_ && raw_.size() < 256u) raw_.push_back(byte);
                        if (limit_ && rawSize_ > limit_ * 12u)
                            throw std::runtime_error("Local model exceeded compact action metadata bounds while generating unfinished arguments. No partial source was admitted.");
                        if (escaped_) escaped_ = false;
                        else if (byte == '\\') escaped_ = true;
                    }
                    else if (byte == '"')
                    {
                        inString_ = true;
                        isKey_ = !afterColon_;
                        afterColon_ = false;
                        raw_.clear();
                        rawSize_ = 0u;
                        limit_ = isKey_ ? 0u : metadata_limit(key_);
                    }
                    else if (byte == ':') afterColon_ = true;
                    else if (byte == ',' || byte == '{' || byte == '['
                        || byte == '}' || byte == ']') afterColon_ = false;
                }
            }
        private:
            static std::size_t metadata_limit(std::string_view key) noexcept
            {
                if (key.compare("title") == 0) return 160u;
                if (key.compare("rationale") == 0) return 1024u;
                if (key.compare("summary") == 0 || key.compare("reason") == 0) return 512u;
                if (key.compare("query") == 0) return 256u;
                return 0u;
            }
            std::string raw_, key_;
            std::size_t rawSize_{}, limit_{};
            bool inString_{}, escaped_{}, isKey_{}, afterColon_{};
        };

        class ModelOutputLimit final : public std::runtime_error
        {
        public:
            ModelOutputLimit() : std::runtime_error{
                "The provider ended generation at its output-token limit (finish_reason=length). Reasoning can consume this limit before final content. No partial answer, plan or edit was admitted; the same generation was not automatically restarted. Check the provider's output limit or reasoning setting, then use Retry/resume from the retained checkpoint."} {}
        };

        // Worker-owned SSE assembly. Deltas are data, never executable actions.
        // Publication still uses the existing full-message phase/schema checks.
        [[nodiscard]] std::uint64_t reported_input_image_tokens(std::string_view encoded) noexcept
        {
            try
            {
                if (encoded.empty() || encoded.compare("null") == 0) return 0u;
                const auto usage = model_json_fields(encoded);
                for (const auto* name : {"prompt_tokens_details", "input_tokens_details"})
                {
                    const auto details = model_json_field(usage, name);
                    if (details.empty() || details.compare("null") == 0) continue;
                    const auto fields = model_json_fields(details);
                    const auto count = model_json_field(fields, "image_tokens");
                    if (count.empty()) continue;
                    std::uint64_t tokens{};
                    const auto [end, error] = std::from_chars(count.data(), count.data() + count.size(), tokens);
                    if (error == std::errc{} && end == count.data() + count.size()) return tokens;
                }
            }
            catch (...) {} // Optional metadata cannot change action admission.
            return 0u;
        }

        class ModelReplyStream final
        {
        public:
            explicit ModelReplyStream(bool sourceAction) noexcept : sourceAction_(sourceAction) {}

            void update_progress(ModelRequestProgress& progress) const noexcept
            {
                progress.streaming_received = modeChosen_ && !jsonFallback_;
                progress.response_bytes = wireBytes_;
                progress.response_events = events_;
                progress.content_bytes = content_.size();
                progress.tool_argument_bytes = arguments_.size();
                progress.reasoning_bytes = reasoningBytes_;
                progress.completion_tokens = completionTokens_;
                progress.token_usage_reported = tokensReported_;
                progress.input_image_tokens = imageTokens_;
                if (providerGenerationMs_ > 0.0)
                {
                    progress.generation_ms = providerGenerationMs_;
                    progress.provider_generation_timing = true;
                }
            }

            void feed(std::string_view chunk)
            {
                if (chunk.size() > kMaximumModelHttpEnvelopeBytes - wireBytes_)
                    invalid_model_stream();
                wireBytes_ += chunk.size();
                for (const char byte : chunk)
                {
                    if (!modeChosen_)
                    {
                        if (std::isspace(static_cast<unsigned char>(byte))) continue;
                        modeChosen_ = true;
                        jsonFallback_ = byte == '{';
                    }
                    if (jsonFallback_) continue;
                    if (byte == '\n')
                    {
                        if (!line_.empty() && line_.back() == '\r') line_.pop_back();
                        line(line_);
                        line_.clear();
                    }
                    else
                    {
                        if (line_.size() >= kMaximumModelHttpEnvelopeBytes) invalid_model_stream();
                        line_.push_back(byte);
                    }
                }
            }

            std::string finish(std::string_view response)
            {
                if (!modeChosen_ || response.size() > kMaximumModelHttpEnvelopeBytes)
                    invalid_model_stream();
                if (jsonFallback_)
                {
                    const auto fields = model_json_fields(response);
                    accept_usage(model_json_field(fields, "usage"));
                    accept_timings(model_json_field(fields, "timings"));
                    if (const auto choices = model_json_field(fields, "choices"); !choices.empty())
                    {
                        if (const auto choice = model_json_single_item(choices))
                        {
                            const auto item = model_json_fields(*choice);
                            const auto reason = model_json_string(model_json_field(item, "finish_reason"));
                            if (reason == "length") throw ModelOutputLimit{};
                            if (!reason.empty() && reason != "stop" && reason != "tool_calls")
                                invalid_model_stream();
                        }
                    }
                    return std::string{response}; // Explicit complete mode or server fallback, same request.
                }
                if (!line_.empty())
                {
                    if (line_.back() == '\r') line_.pop_back();
                    line(line_);
                    line_.clear();
                }
                if (!event_.empty()) event();
                if (!done_ || !finished_) invalid_model_stream();
                std::string result = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\""
                    + json_escape(content_) + "\"";
                if (toolSeen_)
                {
                    if (name_.empty() || arguments_.empty()) invalid_model_stream();
                    result += ",\"tool_calls\":[{\"id\":\"" + json_escape(id_)
                        + "\",\"type\":\"function\",\"function\":{\"name\":\""
                        + json_escape(name_) + "\",\"arguments\":\""
                        + json_escape(arguments_) + "\"}}]";
                }
                result += "},\"finish_reason\":\"" + finishReason_ + "\"}]}";
                if (result.size() > kMaximumModelHttpEnvelopeBytes) invalid_model_stream();
                return result;
            }

        private:
            static void append(std::string& destination, std::string_view chunk)
            {
                if (chunk.size() > 1024u * 1024u - destination.size()) invalid_model_stream();
                destination.append(chunk);
            }

            void accept_usage(std::string_view encoded)
            {
                if (encoded.empty() || encoded.compare("null") == 0) return;
                const auto fields = model_json_fields(encoded);
                imageTokens_ = (std::max)(imageTokens_, reported_input_image_tokens(encoded));
                for (const auto* name : {"completion_tokens", "output_tokens"})
                {
                    const auto count = model_json_field(fields, name);
                    if (count.empty()) continue;
                    std::uint64_t parsed{};
                    const auto [end, error] = std::from_chars(
                        count.data(), count.data() + count.size(), parsed);
                    if (error == std::errc{} && end == count.data() + count.size())
                    {
                        completionTokens_ = parsed;
                        tokensReported_ = true;
                        return;
                    }
                }
            }

            void accept_timings(std::string_view encoded)
            {
                if (encoded.empty() || encoded.compare("null") == 0) return;
                // Timing metadata is optional; malformed metadata must not
                // turn a successful model response into a failed action.
                try
                {
                    const auto fields = model_json_fields(encoded);
                    const auto raw = model_json_field(fields, "predicted_ms");
                    if (raw.empty()) return;
                    double ms{};
                    const auto [end, error] = std::from_chars(
                        raw.data(), raw.data() + raw.size(), ms);
                    if (error == std::errc{} && end == raw.data() + raw.size()
                        && std::isfinite(ms) && ms > 0.0)
                        providerGenerationMs_ = ms;
                }
                catch (const std::exception&) { /* supplemental telemetry */ }
            }

            void line(std::string_view text)
            {
                if (text.empty()) { if (!event_.empty()) event(); return; }
                if (text.starts_with(':')) return;
                const auto colon = text.find(':');
                const auto field = text.substr(0u, colon);
                auto value = colon == std::string_view::npos ? std::string_view{} : text.substr(colon + 1u);
                if (value.starts_with(' ')) value.remove_prefix(1u);
                if (field.compare("data") == 0)
                {
                    if (done_) invalid_model_stream();
                    if (!event_.empty()) append(event_, "\n");
                    append(event_, value);
                }
                else if (field.compare("event") != 0 && field.compare("id") != 0
                    && field.compare("retry") != 0) invalid_model_stream();
            }

            void event()
            {
                ++events_;
                const std::string data = std::move(event_);
                event_.clear();
                if (data == "[DONE]")
                {
                    if (!finished_ || done_) invalid_model_stream();
                    done_ = true;
                    return;
                }
                const auto fields = model_json_fields(data);
                accept_timings(model_json_field(fields, "timings"));
                if (!model_json_field(fields, "error").empty()) invalid_model_stream();
                const auto choices = model_json_field(fields, "choices");
                if (choices.empty()) invalid_model_stream();
                const auto choice = model_json_single_item(choices);
                if (!choice)
                {
                    // The terminal usage-only frame is the authoritative
                    // source of output token counts in streamed OpenAI APIs.
                    accept_usage(model_json_field(fields, "usage"));
                    return;
                }
                if (finished_) invalid_model_stream();
                const auto item = model_json_fields(*choice);
                if (model_json_field(item, "index").compare("0") != 0) invalid_model_stream();
                const auto delta = model_json_field(item, "delta");
                if (delta.empty()) invalid_model_stream();
                const auto parts = model_json_fields(delta);
                const auto role = model_json_string(model_json_field(parts, "role"));
                if (!role.empty() && role != "assistant") invalid_model_stream();
                append(content_, model_json_string(model_json_field(parts, "content")));
                const auto reasoning = model_json_string(model_json_field(parts, "reasoning_content"));
                reasoningBytes_ += reasoning.size();
                check_model_reasoning_tail(reasoningTail_, reasoning, uncheckedReasoningBytes_);
                const auto tools = model_json_field(parts, "tool_calls");
                if (!tools.empty() && tools.compare("null") != 0)
                {
                    if (const auto tool = model_json_single_item(tools))
                    {
                        const auto call = model_json_fields(*tool);
                        if (model_json_field(call, "index").compare("0") != 0) invalid_model_stream();
                        toolSeen_ = true;
                        const auto id = model_json_string(model_json_field(call, "id"));
                        if (!id.empty())
                        {
                            if (!id_.empty() && id != id_) invalid_model_stream();
                            id_ = id;
                        }
                        const auto type = model_json_string(model_json_field(call, "type"));
                        if (!type.empty() && type != "function") invalid_model_stream();
                        const auto function = model_json_field(call, "function");
                        if (function.empty()) invalid_model_stream();
                        const auto action = model_json_fields(function);
                        append(name_, model_json_string(model_json_field(action, "name")));
                        if (name_.size() > 128u) invalid_model_stream();
                        const auto arguments = model_json_string(model_json_field(action, "arguments"));
                        if (sourceAction_) metadata_.feed(arguments);
                        append(arguments_, arguments);
                    }
                }
                const auto reason = model_json_string(model_json_field(item, "finish_reason"));
                if (!reason.empty())
                {
                    if (reason == "length") throw ModelOutputLimit{};
                    if (reason != "stop" && reason != "tool_calls") invalid_model_stream();
                    if ((reason == "tool_calls") != toolSeen_) invalid_model_stream();
                    finished_ = true;
                    finishReason_ = reason;
                }
            }

            std::string line_, event_, content_, reasoningTail_, id_, name_, arguments_, finishReason_;
            ModelActionMetadataGuard metadata_;
            std::size_t wireBytes_{}, uncheckedReasoningBytes_{}, reasoningBytes_{}, events_{};
            std::uint64_t completionTokens_{};
            std::uint64_t imageTokens_{};
            double providerGenerationMs_{};
            bool tokensReported_{};
            bool sourceAction_{}, modeChosen_{}, jsonFallback_{}, toolSeen_{}, finished_{}, done_{};
        };

        static std::vector<std::string> extract_native_model_keys(std::string_view response)
        {
            StructuredJsonCursor cursor{response};
            std::vector<std::string> keys;
            const auto invalid = []() -> void
            { throw std::runtime_error("LM Studio native model inventory has an invalid response format."); };
            if (!cursor.consume('{') || cursor.consume('}')) invalid();
            bool modelsSeen{};
            do
            {
                const auto name = cursor.string();
                if (!name || !cursor.consume(':')) invalid();
                if (*name != "models")
                {
                    if (!cursor.skip_value()) invalid();
                }
                else
                {
                    if (modelsSeen || !cursor.consume('[')) invalid();
                    modelsSeen = true;
                    std::size_t count{};
                    if (!cursor.consume(']'))
                    {
                        do
                        {
                            if (++count > 8192u || !cursor.consume('{') || cursor.consume('}')) invalid();
                            std::optional<std::string> key, type;
                            do
                            {
                                const auto field = cursor.string();
                                if (!field || !cursor.consume(':')) invalid();
                                if (*field == "key" || *field == "type")
                                {
                                    auto& target = *field == "key" ? key : type;
                                    if (target) invalid();
                                    target = cursor.string();
                                    if (!target) invalid();
                                }
                                else if (!cursor.skip_value()) invalid();
                                if (cursor.consume('}')) break;
                                if (!cursor.consume(',')) invalid();
                            } while (true);
                            if (!key || !type || !valid_preferred_model(*key)) invalid();
                            if (*type == "llm" && std::find(keys.begin(), keys.end(), *key) == keys.end())
                                keys.push_back(*key);
                            if (cursor.consume(']')) break;
                            if (!cursor.consume(',')) invalid();
                        } while (true);
                    }
                }
                if (cursor.consume('}')) break;
                if (!cursor.consume(',')) invalid();
            } while (true);
            if (!modelsSeen || !cursor.complete()) invalid();
            return keys;
        }

        struct StructuredPatchOperation final
        {
            std::string path{};
            std::string summary{};
            std::string search{};
            std::string replacement{};
        };

