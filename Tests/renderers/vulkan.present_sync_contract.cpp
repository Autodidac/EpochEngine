// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// CPU fixture; production functions are inserted by test_vulkan_present_sync.py.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define CHECK(...) do { if(!(__VA_ARGS__)) return __LINE__; } while(false)
constexpr bool VK_TRUE=true;
namespace mock {
inline int next=100,allocations{},failAt=-1,waits{},resets{},submits{},presents{},recreates{};
inline int signal{},presentWait{},submittedFence{},waitedFence{},acquireSemaphore{};
inline unsigned image=2;
inline bool invalidReuse{},idle{};
inline std::unordered_set<int> live,pending;
inline std::unordered_map<unsigned,int> imageWait;
struct Delete {void operator()(int* p)const {if(p){live.erase(*p);delete p;}}};
}
namespace vk {
using Semaphore=int;using Fence=int;using CommandBuffer=int;using SwapchainKHR=int;
using UniqueSemaphore=std::unique_ptr<int,mock::Delete>;
enum class Result {eSuccess,eSuboptimalKHR,eErrorOutOfDateKHR,eErrorSurfaceLostKHR,eErrorDeviceLost,eErrorUnknown};
struct SemaphoreCreateInfo{};
enum class PipelineStageFlagBits {eColorAttachmentOutput};
using PipelineStageFlags=PipelineStageFlagBits;
struct SubmitInfo {unsigned waitSemaphoreCount{},commandBufferCount{},signalSemaphoreCount{};
const Semaphore* pWaitSemaphores{};const PipelineStageFlags* pWaitDstStageMask{};
const CommandBuffer* pCommandBuffers{};const Semaphore* pSignalSemaphores{};};
struct PresentInfoKHR {unsigned waitSemaphoreCount{},swapchainCount{};const Semaphore* pWaitSemaphores{};
const SwapchainKHR* pSwapchains{};const unsigned* pImageIndices{};};
}
namespace mock {inline vk::Result acquireResult=vk::Result::eSuccess,presentResult=vk::Result::eSuccess;}
struct Device {
 auto createSemaphoreUnique(vk::SemaphoreCreateInfo){
  if(mock::allocations++==mock::failAt)return std::pair{vk::Result::eErrorUnknown,vk::UniqueSemaphore{}};
  const int n=mock::next++;mock::live.insert(n);
  return std::pair{vk::Result::eSuccess,vk::UniqueSemaphore{new int(n)}};
 }
 vk::Result waitForFences(int,const int* f,bool,std::uint64_t){++mock::waits;mock::waitedFence=*f;return vk::Result::eSuccess;}
 vk::Result resetFences(int,const int*){++mock::resets;return vk::Result::eSuccess;}
 vk::Result acquireNextImageKHR(int,std::uint64_t,int semaphore,int,unsigned* image){
  mock::acquireSemaphore=semaphore;*image=mock::image;
  if(mock::acquireResult==vk::Result::eSuccess||mock::acquireResult==vk::Result::eSuboptimalKHR)
   mock::pending.erase(mock::imageWait[*image]);
  return mock::acquireResult;
 }
 int resetCommandPool(int){return 0;}
};
struct Queue {
 vk::Result submit(int,const vk::SubmitInfo* s,int fence){
  ++mock::submits;mock::signal=*s->pSignalSemaphores;mock::submittedFence=fence;
  if(mock::pending.contains(mock::signal))mock::invalidReuse=true;
  return vk::Result::eSuccess;
 }
 vk::Result presentKHR(const vk::PresentInfoKHR& p){
  ++mock::presents;mock::presentWait=*p.pWaitSemaphores;
  mock::pending.insert(mock::presentWait);mock::imageWait[*p.pImageIndices]=mock::presentWait;
  return mock::presentResult;
 }
};
struct Resource {void reset(){};void clear(){};};
struct Application {
 static constexpr unsigned kMaxFramesInFlight=2;
 Device backing;Device* device=&backing;Queue graphicsQueue,presentQueue;
 std::vector<int> swapChainImages{1,2,3};std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
 std::vector<std::unique_ptr<int>> imageAvailableSemaphores,inFlightFences,commandBuffers;
 std::unique_ptr<int> swapChain=std::make_unique<int>(42),commandPool;
 bool presentModeReady=true,resize{},stopped{};unsigned currentFrame{},nextImageCount=2;int cam{};
 std::unordered_map<int,int> guiContexts;
 Resource framebuffers,depthImageView,depthImage,depthImageMemory,arcadeScreenPipeline,
 solidGraphicsPipeline,graphicsPipeline,pipelineLayout,renderPass,swapChainImageViews,
 descriptorPool,descriptorSets,uniformBuffers,uniformBuffersMemory,uniformBuffersMapped;
 Application(){for(int i=0;i<2;++i){imageAvailableSemaphores.push_back(std::make_unique<int>(10+i));
 inFlightFences.push_back(std::make_unique<int>(20+i));}for(int i=0;i<3;++i)commandBuffers.push_back(std::make_unique<int>(30+i));}
 void assert_thread_affinity(){} bool consume_framebuffer_resize_intent(){return std::exchange(resize,false);}
 bool prepareCanvas2D(){return true;}void set_framebuffer_resize_intent(bool v){resize=v;}
 void request_render_stop(){stopped=true;}void updateUniformBuffer(unsigned,int){}
 void updateGuiUniformBuffer(unsigned){}void recordCommandBuffer(unsigned){}
 void destroyArcadeRenderTarget(){}void reset_gui_swapchain_state(int){}
 void createPresentSyncObjects();void cleanupSwapChain();void drawFrame();
 // Fake swapchain/device boundary; production cleanup and allocator run unchanged.
 void recreateSwapChain(){++mock::recreates;mock::idle=true;mock::pending.clear();
 cleanupSwapChain();swapChain=std::make_unique<int>(43);swapChainImages.resize(nextImageCount);
 createPresentSyncObjects();}
};
ALLOCATOR
CLEANUP
DRAW_FRAME
int main(){
 Application app;app.createPresentSyncObjects();
 CHECK(app.renderFinishedSemaphores.size()==3&&mock::live.size()==3);
 const int original0=*app.renderFinishedSemaphores[0];
 mock::failAt=mock::allocations+1;bool threw=false;
 try{app.createPresentSyncObjects();}catch(const std::runtime_error&){threw=true;}
 CHECK(threw&&mock::live.size()==3&&*app.renderFinishedSemaphores[0]==original0);
 mock::failAt=-1;
 // A graphics fence retires a frame slot, NOT presentation's semaphore wait.
 for(unsigned image: {2u,0u,1u,2u}){
  const auto frame=app.currentFrame;mock::image=image;
  app.drawFrame();CHECK(mock::signal==*app.renderFinishedSemaphores[image]);
  CHECK(mock::presentWait==mock::signal&&mock::submittedFence==20+static_cast<int>(frame));
  CHECK(mock::waitedFence==mock::submittedFence&&mock::acquireSemaphore==10+static_cast<int>(frame));
  CHECK(!mock::invalidReuse);
 }
 const auto oldPool=mock::live;const int oldSubmits=mock::submits,oldResets=mock::resets;
 const unsigned oldFrame=app.currentFrame;
 mock::acquireResult=vk::Result::eErrorOutOfDateKHR;mock::image=99;app.drawFrame();
 CHECK(mock::submits==oldSubmits&&mock::resets==oldResets&&app.currentFrame==oldFrame);
 CHECK(mock::recreates==1&&mock::idle&&app.renderFinishedSemaphores.size()==2&&mock::live.size()==2);
 for(int old:oldPool)CHECK(!mock::live.contains(old));
 mock::acquireResult=vk::Result::eSuccess;mock::image=1;app.drawFrame();
 CHECK(mock::signal==*app.renderFinishedSemaphores[1]&&!mock::invalidReuse);
 const int beforeLost=mock::submits;mock::acquireResult=vk::Result::eErrorSurfaceLostKHR;app.drawFrame();
 CHECK(app.stopped&&app.resize&&mock::submits==beforeLost);
 app.swapChainImages.clear();threw=false;
 try{app.createPresentSyncObjects();}catch(const std::runtime_error&){threw=true;}
 CHECK(threw&&app.renderFinishedSemaphores.size()==2);
 return 0;
}
