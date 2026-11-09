// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Actual module + maintained Scheduler CPU proof. No graphics or models.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <future>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <vector>
import planet.terrain;
import planet.streaming;
import editor.task_scheduler;
namespace core=epoch::extensions::planetary_terrain::core;
namespace stream=epoch::extensions::planetary_terrain::streaming;
namespace tasks=epochengine::editor_tasks;
using Clock=std::chrono::steady_clock;
static void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
static bool equal(const core::Mesh& a,const core::Mesh& b)
{
    if(a.indices!=b.indices||a.vertices.size()!=b.vertices.size()||a.anchor.x!=b.anchor.x
        ||a.anchor.y!=b.anchor.y||a.anchor.z!=b.anchor.z||a.bound_radius!=b.bound_radius)return false;
    for(std::size_t i=0;i<a.vertices.size();++i)
    {
        const auto& x=a.vertices[i];const auto& y=b.vertices[i];
        if(x.position.x!=y.position.x||x.position.y!=y.position.y||x.position.z!=y.position.z
            ||x.normal.x!=y.normal.x||x.normal.y!=y.normal.y||x.normal.z!=y.normal.z
            ||x.parent_delta.x!=y.parent_delta.x||x.parent_delta.y!=y.parent_delta.y||x.parent_delta.z!=y.parent_delta.z
            ||x.color!=y.color)return false;
    }
    return true;
}
static void bounded(const stream::State& state)
{
    const auto s=stream::stats(state);
    require(s.resident_bytes+s.reserved_bytes+stream::detail::overhead<=s.maximum_bytes,"aggregate reservation exceeded cap");
}
static stream::Request request(const stream::State& state)
{
    stream::Request r;r.camera={0,0,state.world.body(0).radius*8};r.lod={0,6,210,.15};
    r.cells_per_tile=16;r.maximum_in_flight=4;
    r.maximum_bytes=stream::detail::overhead+6*stream::detail::retained_bound(16)+4*stream::detail::scratch_bound(16);
    return r;
}
int main()
{
    try
    {
        stream::State state;auto r=request(state);
        require(stream::Request{}.maximum_in_flight==1,"legacy default claim cap changed");
        auto legacy=r;legacy.maximum_in_flight=1;
        require(stream::update(state,legacy)==stream::Status::ready,"legacy update");
        auto single=stream::next_work(state);require(single&&!stream::next_work(state),"default single claim changed");
        require(stream::abandon(state,*single)==stream::Status::cancelled,"single abandon");
        require(stream::update(state,r)==stream::Status::ready,"parallel update");
        for(const auto cap:{0u,5u}) {auto invalid=r;invalid.maximum_in_flight=cap;
            require(stream::update(state,invalid)==stream::Status::invalid_request,"invalid claim cap admitted");}
        std::vector<stream::Work> claims;
        for(unsigned i=0;i<4;++i) {const auto work=stream::next_work(state);require(work.has_value(),"missing concurrent claim");
            for(const auto old:claims)require(old.key!=work->key&&old.serial!=work->serial,"duplicate claim identity");
            claims.push_back(*work);bounded(state);}
        require(!stream::next_work(state)&&stream::stats(state).claimed_patches==4,"claim cap not enforced");
        const auto reserved=state.flight_reservation;
        const auto generation=state.generation;
        auto too_small=r;too_small.maximum_bytes=stream::detail::overhead+reserved-1;
        require(stream::update(state,too_small)==stream::Status::capacity_exceeded
            &&state.generation==generation&&state.flight_reservation==reserved
            &&state.in_flight.size()==4,"rejected budget update changed outstanding claims");
        auto forged=claims[0];++forged.cells_per_tile;
        require(stream::complete(state,forged,{})==stream::Status::stale&&state.flight_reservation==reserved,"forged completion released another claim");
        std::array<Clock::time_point,4> starts{},ends{};
        std::mutex mutex;std::condition_variable changed;unsigned entered{};
        std::array<tasks::Submission<core::Mesh>,4> jobs;
        {
            tasks::Scheduler scheduler{{.worker_count=4,.retained_finished_nodes=32}};
            for(unsigned i=0;i<4;++i)
            {
                const auto work=claims[i];const auto body=state.world.body(work.key.body);
                jobs[i]=scheduler.submit_cancellable("Actual planetary patch",[&,body,work,i](std::stop_token stop) {
                    {std::unique_lock lock(mutex);++entered;changed.notify_all();
                        if(!changed.wait_for(lock,std::chrono::seconds{3},[&]{return entered==4;}))
                            throw std::runtime_error("maintained scheduler did not run four jobs");}
                    starts[i]=Clock::now();auto mesh=stream::build(body,work,stop);ends[i]=Clock::now();return mesh;
                });
            }
            // Deliberately admit in reverse claim order. All independent outputs
            // must equal a synchronous build and retain the exact byte bounds.
            for(unsigned i=4;i-->0;)
            {
                auto mesh=jobs[i].completion.get();
                require(equal(mesh,stream::build(state.world.body(claims[i].key.body),claims[i])),"parallel mesh differs from serial");
                require(stream::complete(state,claims[i],std::move(mesh))==stream::Status::ready,"reordered completion rejected");bounded(state);
            }
            scheduler.graph().WaitAll();
            require(scheduler.activity().outstanding_tasks==0,"scheduler did not retire");
        }
        bool overlap=false;
        for(unsigned i=0;i<4;++i)for(unsigned j=i+1;j<4;++j)
            overlap|=starts[i]<ends[j]&&starts[j]<ends[i];
        require(overlap,"real patch-build intervals did not overlap");
        require(state.flight_reservation==0&&state.in_flight.empty(),"completion reservation leaked");
        require(stream::complete(state,claims[0],{})==stream::Status::stale,"duplicate completion admitted");
        auto failure=stream::next_work(state);require(failure.has_value(),"failure work missing");
        require(stream::complete(state,*failure,{})==stream::Status::invalid_mesh,"failed mesh admitted");
        require(state.flight_reservation==0,"failure reservation leaked");
        auto old=stream::next_work(state);require(old.has_value(),"stale work missing");
        const auto old_body=state.world.body(old->key.body);
        auto replacement=r;replacement.cells_per_tile=4;
        require(stream::update(state,replacement)==stream::Status::ready,"generation replacement refused");bounded(state);
        require(stream::complete(state,*old,stream::build(old_body,*old))==stream::Status::stale,"old generation admitted");
        require(state.flight_reservation==0,"stale reservation leaked");
        // Claimed budget remains reserved until both running and never-started
        // jobs have retired. Cancellation cannot cancel a different identity.
        std::vector<stream::Work> cancelled;
        for(unsigned i=0;i<3;++i) {auto work=stream::next_work(state);require(work.has_value(),"cancel claim missing");cancelled.push_back(*work);}
        std::atomic<bool> started{};std::array<tasks::Submission<core::Mesh>,3> cancel_jobs;
        {
            tasks::Scheduler scheduler{{.worker_count=1,.retained_finished_nodes=32}};
            for(unsigned i=0;i<3;++i)cancel_jobs[i]=scheduler.submit_cancellable("Cancelled planetary patch",
                [&,work=cancelled[i],body=state.world.body(0)](std::stop_token stop) {
                    started=true;while(!stop.stop_requested())std::this_thread::yield();return stream::build(body,work,stop);});
            const auto deadline=Clock::now()+std::chrono::seconds{3};
            while(!started&&Clock::now()<deadline)std::this_thread::yield();require(started,"running cancellation setup");
            require(scheduler.activity().running_tasks==1&&scheduler.activity().queued_tasks==2,"queued/running evidence mismatch");
            for(auto& job:cancel_jobs)(void)scheduler.cancel(job.cancellation);
            for(unsigned i=0;i<3;++i) {
                try {auto mesh=cancel_jobs[i].completion.get();require(mesh.vertices.empty(),"cancelled build allocated mesh");}
                catch(const tasks::TaskCancelled&) {}
                require(stream::abandon(state,cancelled[i])==stream::Status::cancelled,"cancel reservation identity lost");bounded(state);
            }
            scheduler.graph().WaitAll();require(scheduler.activity().outstanding_tasks==0,"cancelled worker not retired");
        }
        require(state.flight_reservation==0&&state.in_flight.empty(),"cancel reservation leaked");
        // Tight one-slot byte cap: a second claim must be refused despite the
        // request's larger cardinality. This white-box reduction isolates claim
        // admission arithmetic; public update refusal is checked above.
        stream::State tight;auto cap=request(tight);cap.cells_per_tile=64;
        cap.maximum_bytes=stream::detail::overhead+6*stream::detail::retained_bound(64)+4*stream::detail::scratch_bound(64);
        require(stream::update(tight,cap)==stream::Status::ready,"tight setup");
        const auto reserve=stream::detail::retained_bound(64)+stream::detail::scratch_bound(64);
        tight.request.maximum_bytes=stream::detail::overhead+reserve;
        const auto only=stream::next_work(tight);require(only&&!stream::next_work(tight),"outstanding reservation was not added to admission");bounded(tight);
        require(stream::abandon(tight,*only)==stream::Status::cancelled,"tight release");
        std::ofstream result{"parallel-streaming-result.json"};
        result<<"{\"actual_module\":true,\"scheduler_workers\":4,\"distinct_claims\":4,\"real_build_overlap\":true,"
            "\"reverse_completions\":true,\"serial_determinism\":true,\"cancel_running_and_queued\":true,"
            "\"stale_generation\":true,\"invalid_mesh_release\":true,\"aggregate_byte_budget\":true,\"legacy_single_api\":true,\"gpu\":false}\n";
        return result?0:2;
    }
    catch(const std::exception& error){std::ofstream failure{"parallel-streaming-failure.txt"};failure<<error.what();return 1;}
}
