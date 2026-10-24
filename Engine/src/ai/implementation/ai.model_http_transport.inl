        [[nodiscard]] static int model_http_timeout_milliseconds(
            std::uint32_t timeoutSeconds) noexcept
        {
            const std::uint64_t milliseconds =
                // Respect the remaining *shared* stage deadline on retries.
                // A historical 180-second floor could turn a final 30-second
                // recovery window into another three-minute request.
                (std::max)(std::uint64_t{1u},
                    static_cast<std::uint64_t>(timeoutSeconds)) * 1'000u;
            return static_cast<int>((std::min)(
                milliseconds,
                static_cast<std::uint64_t>(
                    (std::numeric_limits<int>::max)())));
        }

#if defined(_WIN32)
        struct WinHttpUrl
        {
            std::wstring host;
            INTERNET_PORT port = 0;
            std::wstring path;
            bool secure = false;
        };

        static WinHttpUrl crack_url(const std::string& url_utf8)
        {
            // Convert to UTF-16
            int wlen = MultiByteToWideChar(CP_UTF8, 0, url_utf8.c_str(), (int)url_utf8.size(), nullptr, 0);
            if (wlen <= 0) throw std::runtime_error("WinHTTP: url UTF-8->UTF-16 failed");
            std::wstring wurl((std::size_t)wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, url_utf8.c_str(), (int)url_utf8.size(), wurl.data(), wlen);

            URL_COMPONENTS uc{};
            uc.dwStructSize = sizeof(uc);
            uc.dwSchemeLength = (DWORD)-1;
            uc.dwHostNameLength = (DWORD)-1;
            uc.dwUrlPathLength = (DWORD)-1;
            uc.dwExtraInfoLength = (DWORD)-1;

            if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc))
                throw std::runtime_error("WinHTTP: WinHttpCrackUrl failed");

            WinHttpUrl out{};
            out.secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
            out.port = uc.nPort;

            out.host.assign(uc.lpszHostName, uc.dwHostNameLength);

            std::wstring full_path;
            if (uc.dwUrlPathLength && uc.lpszUrlPath)
                full_path.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
            if (uc.dwExtraInfoLength && uc.lpszExtraInfo)
                full_path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

            if (full_path.empty()) full_path = L"/";
            out.path = std::move(full_path);
            return out;
        }

        struct AsyncModelHttpState final : std::enable_shared_from_this<AsyncModelHttpState>
        {
            std::mutex mutex{};
            std::condition_variable_any changed{};
            std::array<char, 16u * 1024u> readBuffer{};
            std::string requestBody{};
            HINTERNET session{};
            HINTERNET connection{};
            HINTERNET request{};
            std::shared_ptr<AsyncModelHttpState> callbackLifetime{};
            DWORD completion{};
            DWORD error{};
            DWORD bytesRead{};
            DWORD bytesAvailable{};
            DWORD bytesWritten{};
            DWORD lastNotification{};
            std::uint64_t notificationCount{};
            std::array<DWORD, 16u> notificationTrace{};
            std::uint64_t sentBytes{};
            std::atomic_uint64_t enteredCallbacks{0u};
            std::atomic_uint64_t returnedCallbacks{0u};
            std::atomic_uint64_t failedCallbacks{0u};
            std::atomic<DWORD> lastEnteredStatus{0u};
            bool callbackRegistered{};
            bool closing{};
            bool closed{};
            bool settlementWaited{};

            ~AsyncModelHttpState()
            {
                // A registered request keeps itself alive until HANDLE_CLOSING.
                // Unregistered handles have never begun asynchronous operations.
                if (request && !callbackRegistered)
                    (void)WinHttpCloseHandle(request);
                if (connection)
                    (void)WinHttpCloseHandle(connection);
                if (session)
                    (void)WinHttpCloseHandle(session);
            }

            static void CALLBACK callback(HINTERNET, DWORD_PTR context,
                DWORD status, LPVOID information, DWORD informationSize) noexcept
            {
                if (context == 0u)
                {
                    g_modelHttpNullContextCallbacks.fetch_add(1u, std::memory_order_relaxed);
                    return;
                }
                auto* const owner = reinterpret_cast<AsyncModelHttpState*>(context);
                owner->enteredCallbacks.fetch_add(1u, std::memory_order_relaxed);
                owner->lastEnteredStatus.store(status, std::memory_order_relaxed);
                try
                {
                    // The close notification can wake the owner before this
                    // callback returns; its own shared lease prevents a dangling
                    // mutex, condition variable, body or read buffer.
                    auto state = reinterpret_cast<AsyncModelHttpState*>(context)->shared_from_this();
                    {
                        std::scoped_lock lock{state->mutex};
                        state->lastNotification = status;
                        if (state->notificationCount < state->notificationTrace.size())
                            state->notificationTrace[state->notificationCount] = status;
                        ++state->notificationCount;
                        if (status == WINHTTP_CALLBACK_STATUS_REQUEST_SENT && information
                            && informationSize >= sizeof(DWORD))
                            state->sentBytes += *static_cast<DWORD*>(information);
                        if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)
                        {
                            state->closed = true;
                            state->callbackLifetime.reset();
                        }
                        else if (status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)
                        {
                            state->error = information
                                && informationSize >= sizeof(WINHTTP_ASYNC_RESULT)
                                ? static_cast<WINHTTP_ASYNC_RESULT*>(information)->dwError
                                : ERROR_WINHTTP_INTERNAL_ERROR;
                        }
                        else if (status == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE
                            || status == WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE
                            || status == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE
                            || status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE
                            || status == WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE)
                        {
                            state->completion = status;
                            if (status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE)
                                state->bytesRead = informationSize;
                            if (status == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE)
                            {
                                if (information && informationSize >= sizeof(DWORD))
                                    state->bytesAvailable = *static_cast<DWORD*>(information);
                                else state->error = ERROR_WINHTTP_INTERNAL_ERROR;
                            }
                            if (status == WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE)
                            {
                                if (information && informationSize >= sizeof(DWORD))
                                    state->bytesWritten = *static_cast<DWORD*>(information);
                                else
                                    state->error = ERROR_WINHTTP_INTERNAL_ERROR;
                            }
                        }
                    }
                    state->changed.notify_all();
                    state->returnedCallbacks.fetch_add(1u, std::memory_order_relaxed);
                }
                catch (...)
                {
                    // Exceptions must never cross the system callback ABI.
                    owner->failedCallbacks.fetch_add(1u, std::memory_order_relaxed);
                }
            }

            void prepare_operation()
            {
                std::scoped_lock lock{mutex};
                completion = 0u;
                error = 0u;
                bytesRead = 0u;
                bytesAvailable = 0u;
                bytesWritten = 0u;
            }

            void wait_operation(DWORD expected, std::stop_token cancellation,
                std::chrono::steady_clock::time_point deadline)
            {
                std::unique_lock lock{mutex};
                const bool completed = changed.wait_until(lock, cancellation, deadline,
                    [&] { return completion == expected || error != 0u || closed; });
                if (cancellation.stop_requested())
                {
                    std::string diagnostic = "Model HTTP cancellation during status wait: expected="
                        + std::to_string(expected) + ", last_callback="
                        + std::to_string(lastNotification) + ", callbacks="
                        + std::to_string(notificationCount) + ", completion="
                        + std::to_string(completion) + ", error=" + std::to_string(error)
                        + ", entered=" + std::to_string(enteredCallbacks.load())
                        + ", returned=" + std::to_string(returnedCallbacks.load())
                        + ", last_entered=" + std::to_string(lastEnteredStatus.load())
                        + ", failed_callbacks=" + std::to_string(failedCallbacks.load())
                        + ", null_context=" + std::to_string(g_modelHttpNullContextCallbacks.load())
                        + ", body_bytes=" + std::to_string(requestBody.size())
                        + ", sent_bytes=" + std::to_string(sentBytes) + ", trace=";
                    for (std::size_t index = 0u;
                        index < (std::min)(notificationCount, notificationTrace.size()); ++index)
                        diagnostic += std::to_string(notificationTrace[index]) + ":";
                    lock.unlock();
                    core::log::info("ai", epochengine::string_view{diagnostic.data(), diagnostic.size()});
                    throw std::runtime_error("WinHTTP: local-model request cancelled");
                }
                if (!completed)
                    throw ModelTransportTimeout(true, "WinHTTP: local-model request deadline exceeded");
                if (error == ERROR_WINHTTP_TIMEOUT)
                    throw ModelTransportTimeout(std::chrono::steady_clock::now()
                            + std::chrono::milliseconds{999} >= deadline,
                        "WinHTTP: asynchronous model transport operation timed out");
                if (error != 0u || closed)
                    throw std::runtime_error("WinHTTP: asynchronous model request failed (error "
                        + std::to_string(error) + ")");
            }

            [[nodiscard]] bool close_and_settle() noexcept
            {
                if (!callbackRegistered)
                    return true;
                HINTERNET toClose{};
                {
                    std::scoped_lock lock{mutex};
                    if (closed)
                        return true;
                    if (!closing)
                    {
                        closing = true;
                        toClose = std::exchange(request, nullptr);
                    }
                }
                // Only the request's worker calls this, after each initiating
                // WinHTTP function has returned. Stop callbacks only wake its
                // wait; they never close a handle concurrently with an API call.
                if (toClose && !WinHttpCloseHandle(toClose))
                    return false;
                std::unique_lock lock{mutex};
                if (settlementWaited)
                    return closed;
                settlementWaited = true;
                return changed.wait_for(lock, std::chrono::seconds{5},
                    [this] { return closed; });
            }
        };

        struct ScopedAsyncModelHttp final
        {
            std::shared_ptr<AsyncModelHttpState> state;
            ~ScopedAsyncModelHttp()
            {
                if (!state->close_and_settle())
                    core::log::error("ai",
                        "Model HTTP handle closure did not settle; callback-owned request state remains retained.");
            }
        };

        static std::string winhttp_post_json(const std::string& url,
            const std::string& body_utf8,
            const std::vector<std::pair<std::string, std::string>>& headers,
            std::uint32_t timeoutSeconds,
            std::stop_token cancellation,
            const ModelRequestObserver& observer,
            const ModelReplyChunkConsumer& consumeChunk = {})
        {
            if (g_modelHttpRetirementUncertain.load(std::memory_order_acquire))
                throw ModelTransportRetirementFailure(
                    "Model HTTP retirement was not confirmed. Restart the engine before another HTTP request.");
            if (cancellation.stop_requested())
                throw std::runtime_error("WinHTTP: local-model request cancelled before dispatch");
            if (body_utf8.size() > kMaximumModelHttpEnvelopeBytes)
                throw std::runtime_error("WinHTTP: model request exceeded the bounded envelope size");
            const auto u = crack_url(url);
            auto state = std::make_shared<AsyncModelHttpState>();
            const ScopedAsyncModelHttp cleanup{state};
            try
            {
            state->requestBody = body_utf8;
            state->session = WinHttpOpen(L"EpochEngine/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
            if (!state->session) throw std::runtime_error("WinHTTP: WinHttpOpen failed");
            const int receiveTimeout =
                model_http_timeout_milliseconds(timeoutSeconds);
            if (!WinHttpSetTimeouts(
                    state->session, 10000, 10000, 180000, receiveTimeout))
                throw std::runtime_error("WinHTTP: model request timeouts could not be set");
            const auto deadline = std::chrono::steady_clock::now()
                + std::chrono::milliseconds{receiveTimeout};

            state->connection = WinHttpConnect(state->session, u.host.c_str(), u.port, 0);
            if (!state->connection)
                throw std::runtime_error("WinHTTP: WinHttpConnect failed");

            DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;
            state->request = WinHttpOpenRequest(state->connection, L"POST", u.path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (!state->request)
                throw std::runtime_error("WinHTTP: WinHttpOpenRequest failed");
            // A model request belongs to the selected endpoint. Do not replay
            // its source body to a redirect or negotiate ambient credentials.
            DWORD disabledFeatures = WINHTTP_DISABLE_REDIRECTS
                | WINHTTP_DISABLE_AUTHENTICATION | WINHTTP_DISABLE_COOKIES;
            if (!WinHttpSetOption(state->request, WINHTTP_OPTION_DISABLE_FEATURE,
                    &disabledFeatures, sizeof(disabledFeatures)))
                throw std::runtime_error("WinHTTP: model endpoint policy could not be set");
            DWORD_PTR callbackContext = reinterpret_cast<DWORD_PTR>(state.get());
            if (!WinHttpSetOption(state->request, WINHTTP_OPTION_CONTEXT_VALUE,
                    &callbackContext, sizeof(callbackContext)))
                throw std::runtime_error("WinHTTP: request context registration failed");
            if (WinHttpSetStatusCallback(state->request, AsyncModelHttpState::callback,
                    WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0u)
                == WINHTTP_INVALID_STATUS_CALLBACK)
                throw std::runtime_error("WinHTTP: asynchronous callback registration failed");
            state->callbackRegistered = true;
            state->callbackLifetime = state;

            // Default headers
            std::wstring hdr = L"Content-Type: application/json\r\nAccept: application/json, text/event-stream\r\n";
            for (const auto& [k, v] : headers)
            {
                int wk = MultiByteToWideChar(CP_UTF8, 0, k.c_str(), (int)k.size(), nullptr, 0);
                int wv = MultiByteToWideChar(CP_UTF8, 0, v.c_str(), (int)v.size(), nullptr, 0);
                if (wk <= 0 || wv <= 0) continue;
                std::wstring ws_k((std::size_t)wk, L'\0');
                std::wstring ws_v((std::size_t)wv, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, k.c_str(), (int)k.size(), ws_k.data(), wk);
                MultiByteToWideChar(CP_UTF8, 0, v.c_str(), (int)v.size(), ws_v.data(), wv);
                hdr += ws_k;
                hdr += L": ";
                hdr += ws_v;
                hdr += L"\r\n";
            }

            if (!WinHttpAddRequestHeaders(state->request, hdr.c_str(), (DWORD)hdr.size(),
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
                throw std::runtime_error("WinHTTP: model request headers could not be set");

            notify_model_stage(observer, ModelRequestStage::sending);
            if (cancellation.stop_requested())
                throw std::runtime_error("WinHTTP: local-model request cancelled before dispatch");
            state->prepare_operation();
            BOOL ok = WinHttpSendRequest(state->request,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0u,
                static_cast<DWORD>(state->requestBody.size()), callbackContext);

            if (!ok)
                throw std::runtime_error("WinHTTP: WinHttpSendRequest failed");
            state->wait_operation(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE,
                cancellation, deadline);
            // Keep header and body completion distinct. A REQUEST_SENT progress
            // callback can describe only the headers; it is not body evidence.
            for (std::size_t offset = 0u; offset < state->requestBody.size();)
            {
                if (cancellation.stop_requested())
                    throw std::runtime_error("WinHTTP: local-model request cancelled during upload");
                const DWORD chunk = static_cast<DWORD>((std::min)(
                    state->requestBody.size() - offset, state->readBuffer.size()));
                state->prepare_operation();
                if (!WinHttpWriteData(state->request,
                        state->requestBody.data() + offset, chunk, nullptr))
                    throw std::runtime_error("WinHTTP: model request body write failed");
                state->wait_operation(WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE,
                    cancellation, deadline);
                const DWORD written = state->bytesWritten;
                if (written == 0u || written > chunk)
                    throw std::runtime_error("WinHTTP: invalid model request body write completion");
                offset += written;
            }
            notify_model_stage(observer, ModelRequestStage::request_sent);

            state->prepare_operation();
            if (cancellation.stop_requested())
                throw std::runtime_error("WinHTTP: local-model request cancelled");
            if (!WinHttpReceiveResponse(state->request, nullptr))
                throw std::runtime_error("WinHTTP: WinHttpReceiveResponse failed");
            notify_model_stage(observer, ModelRequestStage::awaiting_response);
            state->wait_operation(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE,
                cancellation, deadline);
            DWORD httpStatus{};
            DWORD httpStatusBytes = sizeof(httpStatus);
            if (!WinHttpQueryHeaders(state->request,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &httpStatus, &httpStatusBytes,
                    WINHTTP_NO_HEADER_INDEX))
                throw std::runtime_error("WinHTTP: response status could not be read");

            std::string resp;
            for (;;)
            {
                if (cancellation.stop_requested())
                    throw std::runtime_error("WinHTTP: local-model request cancelled");
                state->prepare_operation();
                // Read exactly the currently available stream data. A large
                // fixed-length read can delay publication of small SSE frames.
                if (!WinHttpQueryDataAvailable(state->request, nullptr))
                    throw std::runtime_error("WinHTTP: stream availability query failed");
                state->wait_operation(WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE,
                    cancellation, deadline);
                const auto available = state->bytesAvailable;
                if (available == 0u) break;
                state->prepare_operation();
                if (!WinHttpReadData(state->request, state->readBuffer.data(),
                        (std::min)(available, static_cast<DWORD>(state->readBuffer.size())), nullptr))
                    throw std::runtime_error("WinHTTP: WinHttpReadData failed");
                state->wait_operation(WINHTTP_CALLBACK_STATUS_READ_COMPLETE,
                    cancellation, deadline);
                const DWORD read = state->bytesRead;
                if (read == 0u)
                    break;
                if (read > state->readBuffer.size()
                    || resp.size() > kMaximumModelHttpEnvelopeBytes - read)
                    throw std::runtime_error("WinHTTP: model response exceeded the bounded envelope size");
                resp.append(state->readBuffer.data(), read);
                notify_model_stage(observer, ModelRequestStage::receiving);
                if (consumeChunk && httpStatus >= 200u && httpStatus < 300u)
                    consumeChunk(std::string_view{state->readBuffer.data(), read});
            }
            if (!state->close_and_settle())
            {
                g_modelHttpRetirementUncertain.store(true, std::memory_order_release);
                throw ModelTransportRetirementFailure("WinHTTP: model request closure did not settle; this request will not be retried");
            }
            if (cancellation.stop_requested())
                throw std::runtime_error("WinHTTP: local-model request cancelled");
            if (httpStatus < 200u || httpStatus >= 300u)
                throw ModelHttpFailure(httpStatus, resp);
            return resp;
            }
            catch (...)
            {
                if (!state->close_and_settle())
                {
                    g_modelHttpRetirementUncertain.store(true, std::memory_order_release);
                    throw ModelTransportRetirementFailure(
                        "WinHTTP: model request closure did not settle; this request will not be retried and its callback lifetime remains retained");
                }
                throw;
            }
        }

        static std::string winhttp_get_json(const std::string& url, const std::vector<std::pair<std::string, std::string>>& headers, int timeoutMs = 180000)
        {
            const auto u = crack_url(url);

            HINTERNET hSession = WinHttpOpen(L"EpochEngine/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) throw std::runtime_error("WinHTTP: WinHttpOpen failed");
            (void)WinHttpSetTimeouts(hSession, (std::min)(10000, timeoutMs), (std::min)(10000, timeoutMs), timeoutMs, timeoutMs);

            HINTERNET hConnect = WinHttpConnect(hSession, u.host.c_str(), u.port, 0);
            if (!hConnect)
            {
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpConnect failed");
            }

            DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", u.path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (!hRequest)
            {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpOpenRequest failed");
            }

            DWORD disabledFeatures = WINHTTP_DISABLE_REDIRECTS
                | WINHTTP_DISABLE_AUTHENTICATION | WINHTTP_DISABLE_COOKIES;
            if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE,
                    &disabledFeatures, sizeof(disabledFeatures)))
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: model inventory endpoint policy could not be set");
            }
            std::wstring hdr = L"Accept: application/json\r\n";
            for (const auto& [k, v] : headers)
            {
                int wk = MultiByteToWideChar(CP_UTF8, 0, k.c_str(), (int)k.size(), nullptr, 0);
                int wv = MultiByteToWideChar(CP_UTF8, 0, v.c_str(), (int)v.size(), nullptr, 0);
                if (wk <= 0 || wv <= 0) continue;
                std::wstring ws_k((std::size_t)wk, L'\0');
                std::wstring ws_v((std::size_t)wv, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, k.c_str(), (int)k.size(), ws_k.data(), wk);
                MultiByteToWideChar(CP_UTF8, 0, v.c_str(), (int)v.size(), ws_v.data(), wv);
                hdr += ws_k;
                hdr += L": ";
                hdr += ws_v;
                hdr += L"\r\n";
            }

            (void)WinHttpAddRequestHeaders(hRequest, hdr.c_str(), (DWORD)hdr.size(),
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

            BOOL ok = WinHttpSendRequest(hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0,
                0, 0);

            if (!ok)
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpSendRequest failed");
            }

            if (!WinHttpReceiveResponse(hRequest, nullptr))
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpReceiveResponse failed");
            }

            DWORD httpStatus{};
            DWORD statusSize = sizeof(httpStatus);
            const bool statusRead = WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &httpStatus, &statusSize,
                WINHTTP_NO_HEADER_INDEX) != FALSE;
            if (!statusRead || httpStatus < 200u || httpStatus >= 300u)
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error(statusRead ? model_http_status_message(httpStatus)
                    : "Model inventory HTTP status could not be read.");
            }
            std::string resp;
            for (;;)
            {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &avail))
                {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    throw std::runtime_error("WinHTTP: WinHttpQueryDataAvailable failed");
                }
                if (avail == 0) break;
                // An endpoint discovered on the loopback interface is still
                // untrusted input. Reject oversized model inventories before
                // allocating any buffers, including when WinHTTP advertises
                // an unexpectedly large available-data window.
                if (avail > kMaximumModelHttpEnvelopeBytes
                    || resp.size() > kMaximumModelHttpEnvelopeBytes - avail)
                {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    throw std::runtime_error("Model inventory response exceeds the 8 MiB safety limit.");
                }

                std::string buf(avail, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(hRequest, buf.data(), avail, &read))
                {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    throw std::runtime_error("WinHTTP: WinHttpReadData failed");
                }
                if (read == 0u)
                {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    throw std::runtime_error("Model inventory endpoint stalled during response reading.");
                }
                buf.resize(read);
                resp += buf;
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return resp;
        }
#endif

#if !defined(_WIN32) && defined(EPOCH_HAS_CURL)
        static std::size_t curl_write_cb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata)
        {
            if (size != 0u && nmemb > (std::numeric_limits<std::size_t>::max)() / size)
                return 0u;
            const std::size_t bytes = size * nmemb;
            if (userdata && ptr && bytes)
            {
                auto* out = static_cast<std::string*>(userdata);
                if (bytes > kMaximumModelHttpEnvelopeBytes
                    || out->size() > kMaximumModelHttpEnvelopeBytes - bytes)
                    return 0u;
                try { out->append(ptr, bytes); }
                catch (...) { return 0u; }
            }
            return bytes;
        }

        static int curl_ai_request_progress(
            void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) noexcept
        {
            return context && static_cast<std::stop_token*>(context)->stop_requested() ? 1 : 0;
        }

        struct CurlModelReply final
        {
            std::string& response;
            const ModelReplyChunkConsumer& consume;
            const ModelRequestObserver& observer;
            std::exception_ptr failure{};
        };

        static std::size_t curl_model_reply_cb(char* data, std::size_t size,
            std::size_t count, void* context) noexcept
        {
            if (!context || (size && count > kMaximumModelHttpEnvelopeBytes / size)) return 0u;
            const auto bytes = size * count;
            if (bytes == 0u) return 0u;
            if (!data) return 0u;
            auto& reply = *static_cast<CurlModelReply*>(context);
            if (bytes > kMaximumModelHttpEnvelopeBytes
                || reply.response.size() > kMaximumModelHttpEnvelopeBytes - bytes) return 0u;
            try
            {
                reply.response.append(data, bytes);
                if (bytes)
                {
                    notify_model_stage(reply.observer, ModelRequestStage::receiving);
                    if (reply.consume) reply.consume(std::string_view{data, bytes});
                }
                return bytes;
            }
            catch (...) { reply.failure = std::current_exception(); return 0u; }
        }

        static std::string http_post_json(const std::string& url,
            const std::string& body_utf8,
            const std::vector<std::pair<std::string, std::string>>& headers,
            std::uint32_t timeoutSeconds,
            std::stop_token cancellation,
            const ModelRequestObserver& observer,
            const ModelReplyChunkConsumer& consumeChunk = {})
        {
            if (cancellation.stop_requested())
                throw std::runtime_error("CURL: local-model request cancelled before dispatch");
            struct CurlRequestOwner final
            {
                CURL* handle{curl_easy_init()};
                curl_slist* headers{};
                ~CurlRequestOwner()
                {
                    curl_slist_free_all(headers);
                    if (handle) curl_easy_cleanup(handle);
                }
                void append_header(const char* value)
                {
                    auto* next = curl_slist_append(headers, value);
                    if (!next)
                        throw std::runtime_error("CURL: request header allocation failed");
                    headers = next;
                }
            } owner{};
            CURL* curl = owner.handle;
            if (!curl)
                throw std::runtime_error("CURL: curl_easy_init failed");

            std::string resp;
            CurlModelReply reply{resp, consumeChunk, observer};
            owner.append_header("Content-Type: application/json");
            owner.append_header("Accept: application/json, text/event-stream");

            for (const auto& [k, v] : headers)
            {
                std::string hdr = k;
                hdr += ": ";
                hdr += v;
                owner.append_header(hdr.c_str());
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_utf8.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_utf8.size()));
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, owner.headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_model_reply_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
            curl_easy_setopt(
                curl,
                CURLOPT_TIMEOUT_MS,
                static_cast<long>(
                    model_http_timeout_milliseconds(timeoutSeconds)));
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_ai_request_progress);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancellation);

            notify_model_stage(observer, ModelRequestStage::sending);
            if (cancellation.stop_requested())
                throw std::runtime_error("CURL: local-model request cancelled before dispatch");
            const auto attemptStarted = std::chrono::steady_clock::now();
            const CURLcode code = curl_easy_perform(curl);
            if (reply.failure && !cancellation.stop_requested())
                std::rethrow_exception(reply.failure);
            if (code != CURLE_OK || cancellation.stop_requested())
            {
                if (code == CURLE_OPERATION_TIMEDOUT && !cancellation.stop_requested())
                {
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - attemptStarted).count();
                    throw ModelTransportTimeout(model_total_budget_elapsed(
                        static_cast<std::uint64_t>((std::max)(elapsed, decltype(elapsed){0})), timeoutSeconds),
                        "CURL: model transport operation timed out");
                }
                const std::string err = cancellation.stop_requested() || code == CURLE_ABORTED_BY_CALLBACK
                    ? "CURL: local-model request cancelled"
                    : std::string("CURL: curl_easy_perform failed: ") + curl_easy_strerror(code);
                throw std::runtime_error(err);
            }

            notify_model_stage(observer, ModelRequestStage::receiving);
            if (cancellation.stop_requested())
                throw std::runtime_error("CURL: local-model request cancelled");
            long http_status = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);

            if (http_status < 200 || http_status >= 300)
            {
                throw ModelHttpFailure(static_cast<unsigned>(http_status), resp);
            }

            return resp;
        }

        static std::string http_get_json(const std::string& url,
            const std::vector<std::pair<std::string, std::string>>& headers, int timeoutMs = 180000)
        {
            CURL* curl = curl_easy_init();
            if (!curl)
                throw std::runtime_error("CURL: curl_easy_init failed");

            std::string resp;
            struct curl_slist* request_headers = nullptr;
            request_headers = curl_slist_append(request_headers, "Accept: application/json");

            for (const auto& [k, v] : headers)
            {
                std::string hdr = k;
                hdr += ": ";
                hdr += v;
                request_headers = curl_slist_append(request_headers, hdr.c_str());
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>((std::min)(10000, timeoutMs)));
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeoutMs));

            const CURLcode code = curl_easy_perform(curl);
            if (code != CURLE_OK)
            {
                const std::string err = std::string("CURL: curl_easy_perform failed: ") + curl_easy_strerror(code);
                curl_slist_free_all(request_headers);
                curl_easy_cleanup(curl);
                throw std::runtime_error(err);
            }

            long http_status = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);

            curl_slist_free_all(request_headers);
            curl_easy_cleanup(curl);

            if (http_status < 200 || http_status >= 300)
            {
                std::ostringstream oss;
                oss << model_http_status_message(static_cast<unsigned>(http_status));
                throw std::runtime_error(oss.str());
            }

            return resp;
        }
#endif

