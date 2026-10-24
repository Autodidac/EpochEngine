// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Windows speech backend. Platform headers stay in this owned implementation.
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#include "ai.voice_runtime.hpp"
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sapi.h>
#include <wrl/client.h>
#endif

namespace epochengine::ai::voice_runtime {
namespace {
struct Service final {
    std::mutex mutex{};
    Snapshot state{.status="Voice providers have not been checked"};
    std::deque<Transcript> transcripts{};
    std::uint64_t sequence{};
    std::jthread discovery{},capture{},playback{};
    std::atomic_bool discovering{},capturing{},playing{};
    ~Service()
    {
        discovery.request_stop();
        capture.request_stop();
        playback.request_stop();
        if (discovery.joinable()) discovery.join();
        if (capture.joinable()) capture.join();
        if (playback.joinable()) playback.join();
    }
};
Service& service(){static Service value;return value;}
#ifdef _WIN32
void status(Service& s, std::string text)
{
    std::scoped_lock lock{s.mutex};
    s.state.status = std::move(text);
}
using Microsoft::WRL::ComPtr;
struct Apartment final {
    HRESULT result{CoInitializeEx(nullptr,COINIT_MULTITHREADED)};
    ~Apartment(){if(SUCCEEDED(result))CoUninitialize();}
};
std::string utf8(const wchar_t* text){
    if(!text)return {};
    const auto count=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);
    if(count<=1)return {};
    std::string value(static_cast<std::size_t>(count),'\0');
    if(!WideCharToMultiByte(CP_UTF8,0,text,-1,value.data(),count,nullptr,nullptr))return {};
    value.pop_back();return value;
}
std::wstring wide(const std::string& text){
    const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if(count<=0)return {};
    std::wstring value(static_cast<std::size_t>(count),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),value.data(),count);return value;
}
bool tokens(const wchar_t* category){
    ComPtr<ISpObjectTokenCategory> source;ComPtr<IEnumSpObjectTokens> items;ULONG count{};
    return SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(source.GetAddressOf())))
        &&SUCCEEDED(source->SetId(category,FALSE))&&SUCCEEDED(source->EnumTokens(nullptr,nullptr,items.GetAddressOf()))
        &&SUCCEEDED(items->GetCount(&count))&&count!=0u;
}
void clear_event(SPEVENT& event){
    if(event.elParamType==SPET_LPARAM_IS_OBJECT||event.elParamType==SPET_LPARAM_IS_TOKEN){
        if(event.lParam)reinterpret_cast<IUnknown*>(event.lParam)->Release();
    }else if(event.elParamType==SPET_LPARAM_IS_POINTER||event.elParamType==SPET_LPARAM_IS_STRING)
        CoTaskMemFree(reinterpret_cast<void*>(event.lParam));
    event={};
}
#endif
}
Snapshot snapshot(){auto& s=service();std::scoped_lock lock{s.mutex};return s.state;}
void probe(){
    auto& s=service();if(s.discovering.exchange(true))return;
    s.discovery=std::jthread([&s](std::stop_token stop){
#ifdef _WIN32
        Apartment apartment;
        const bool recognition=!stop.stop_requested()&&SUCCEEDED(apartment.result)&&tokens(SPCAT_RECOGNIZERS)&&tokens(SPCAT_AUDIOIN);
        const bool speech=!stop.stop_requested()&&SUCCEEDED(apartment.result)&&tokens(SPCAT_VOICES);
        {std::scoped_lock lock{s.mutex};s.state.probed=true;s.state.available=recognition;s.state.speech_available=speech;
            s.state.status=recognition?"Windows speech recognition is ready. Dictation is editable and may need correction.":"No working Windows SAPI recognizer and microphone were detected. Check speech language and the default microphone.";}
#else
        (void)stop;{std::scoped_lock lock{s.mutex};s.state.probed=true;s.state.status="No native speech provider is connected on this platform";}
#endif
        s.discovering.store(false);
    });
}
bool start(std::uint64_t owner){
    auto& s=service();const auto before=snapshot();
    if(owner==0u||!before.available||s.capturing.exchange(true))return false;
    {std::scoped_lock lock{s.mutex};s.state.owner=owner;++s.state.generation;s.state.listening=false;s.state.status="Opening microphone";}
    const auto generation=snapshot().generation;
    s.capture=std::jthread([&s,owner,generation](std::stop_token stop){
#ifdef _WIN32
        Apartment apartment;ComPtr<ISpRecognizer> recognizer;ComPtr<ISpRecoContext> context;ComPtr<ISpRecoGrammar> grammar;
        auto result=apartment.result;
        if(SUCCEEDED(result))result=CoCreateInstance(CLSID_SpInprocRecognizer,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(recognizer.GetAddressOf()));
        ComPtr<ISpObjectTokenCategory> audioCategory;ComPtr<ISpObjectToken> audioToken;wchar_t* tokenId{};
        if(SUCCEEDED(result))result=CoCreateInstance(CLSID_SpObjectTokenCategory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(audioCategory.GetAddressOf()));
        if(SUCCEEDED(result))result=audioCategory->SetId(SPCAT_AUDIOIN,FALSE);
        if(SUCCEEDED(result))result=audioCategory->GetDefaultTokenId(&tokenId);
        if(SUCCEEDED(result))result=CoCreateInstance(CLSID_SpObjectToken,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(audioToken.GetAddressOf()));
        if(SUCCEEDED(result))result=audioToken->SetId(nullptr,tokenId,FALSE);
        CoTaskMemFree(tokenId);
        if(SUCCEEDED(result))result=recognizer->SetInput(audioToken.Get(),TRUE);
        if(SUCCEEDED(result))result=recognizer->CreateRecoContext(context.GetAddressOf());
        if(SUCCEEDED(result))result=context->SetNotifyWin32Event();
        if(SUCCEEDED(result))result=context->SetInterest(SPFEI(SPEI_RECOGNITION),SPFEI(SPEI_RECOGNITION));
        if(SUCCEEDED(result))result=context->CreateGrammar(1u,grammar.GetAddressOf());
        if(SUCCEEDED(result))result=grammar->LoadDictation(nullptr,SPLO_STATIC);
        if(SUCCEEDED(result))result=grammar->SetDictationState(SPRS_ACTIVE);
        if(FAILED(result)){status(s,"Speech input could not start (Windows error "+std::to_string(static_cast<long>(result))+")");s.capturing.store(false);return;}
        {std::scoped_lock lock{s.mutex};s.state.listening=true;s.state.status="Listening · transcript stays editable in the composer";}
        bool speechPaused{};
        while(!stop.stop_requested()){
            const bool pause=s.playing.load();
            if(pause!=speechPaused){(void)grammar->SetDictationState(pause?SPRS_INACTIVE:SPRS_ACTIVE);speechPaused=pause;}
            if(WaitForSingleObject(context->GetNotifyEventHandle(),40u)!=WAIT_OBJECT_0)continue;
            SPEVENT event{};ULONG fetched{};
            while(SUCCEEDED(context->GetEvents(1u,&event,&fetched))&&fetched!=0u){
                if(event.eEventId==SPEI_RECOGNITION&&event.lParam&&!stop.stop_requested()&&!speechPaused){
                    auto* recognition=reinterpret_cast<ISpRecoResult*>(event.lParam);wchar_t* text{};
                    if(SUCCEEDED(recognition->GetText(SP_GETWHOLEPHRASE,SP_GETWHOLEPHRASE,TRUE,&text,nullptr))){
                        auto transcript=utf8(text);CoTaskMemFree(text);
                        if(!transcript.empty()&&transcript.size()<=32u*1024u){std::scoped_lock lock{s.mutex};
                            if(s.state.owner==owner&&s.state.generation==generation)s.transcripts.push_back({++s.sequence,owner,generation,std::move(transcript)});
                            if(s.transcripts.size()>64u)s.transcripts.pop_front();}
                    }
                }clear_event(event);
            }
        }
        (void)grammar->SetDictationState(SPRS_INACTIVE);(void)recognizer->SetRecoState(SPRST_INACTIVE_WITH_PURGE);
#else
        (void)stop;(void)owner;(void)generation;
#endif
        {std::scoped_lock lock{s.mutex};s.state.listening=false;if(s.state.owner==owner)s.state.status="Microphone stopped";}
        s.capturing.store(false);
    });return true;
}
void stop(std::uint64_t owner){
    auto& s=service();{std::scoped_lock lock{s.mutex};if(s.state.owner!=owner)return;s.state.owner=0u;s.state.status="Stopping voice session";}
    s.capture.request_stop();s.playback.request_stop();
}
bool speak(std::uint64_t owner,std::string text){
    auto& s=service();const auto current=snapshot();
    if(owner==0u||current.owner!=owner||!current.speech_available||text.empty()||text.size()>64u*1024u||s.playing.exchange(true))return false;
    s.playback=std::jthread([&s,text=std::move(text)](std::stop_token stop){
#ifdef _WIN32
        Apartment apartment;ComPtr<ISpVoice> voice;const auto content=wide(text);
        if(SUCCEEDED(apartment.result)&&!content.empty()&&SUCCEEDED(CoCreateInstance(CLSID_SpVoice,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(voice.GetAddressOf())))
            &&SUCCEEDED(voice->Speak(content.c_str(),SPF_ASYNC|SPF_IS_NOT_XML,nullptr))){
            {std::scoped_lock lock{s.mutex};s.state.speaking=true;}
            while(!stop.stop_requested()&&voice->WaitUntilDone(100u)==S_FALSE){}
            if(stop.stop_requested())(void)voice->Speak(nullptr,SPF_ASYNC|SPF_PURGEBEFORESPEAK,nullptr);
        }else status(s,"Speech playback could not start");
#else
        (void)stop;(void)text;
#endif
        {std::scoped_lock lock{s.mutex};s.state.speaking=false;}s.playing.store(false);
    });return true;
}
std::vector<Transcript> transcripts_since(std::uint64_t sequence,std::uint64_t owner){
    auto& s=service();std::scoped_lock lock{s.mutex};std::vector<Transcript> values;
    for (const auto& entry : s.transcripts)
        if (entry.sequence > sequence && entry.owner == owner)
            values.push_back(entry);
    return values;
}
}
