"""Compile the actual native-present owner against CPU-only GL/NV/COM ports.

Only module declarations, native includes and the platform selector are adapted.
No production resource, presentation or state-restoration algorithm is copied.
This proves owner control flow, not a driver, window or compositor integration.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest

from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
OWNER = "Engine/src/renderers/opengl/opengl.native_present.cpp"
INTERFACE = "Engine/modules/opengl.native_present.ixx"


def actual_owner(recording):
    interface = (ROOT / INTERFACE).read_text(encoding="utf-8")
    implementation = (ROOT / OWNER).read_text(encoding="utf-8")
    interface = interface.replace("export module opengl.native_present;", "")
    interface = interface.replace("export namespace", "namespace")
    implementation = re.sub(
        r"#if defined\(_WIN32\) && \([^\n]+\\\n[^\n]+\\\n[^\n]+",
        "#if " + ("1" if recording else "0"), implementation)
    lines = []
    for line in implementation.splitlines():
        if line in ("module;", "module opengl.native_present;", "import core.env;", "import core.logger;", "import opengl.platform;"):
            continue
        if line.startswith('#include "') or line in (
            "#include <include/engine.config.hpp>", "#include <d3d11.h>", "#include <glad/glad.h>"
        ):
            continue
        if line == "#if defined(_WIN32) && defined(EPOCH_USING_OPENGL) && EPOCH_USING_OPENGL == 1":
            line = "#if " + ("1" if recording else "0")
        lines.append(line)
    return interface + "\n" + "\n".join(lines)


def body_of(source, start):
    begin = source.index("{", source.index(start))
    depth, end = 1, begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin + 1:end - 1]


COMMON = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr,"CHECK line %d: %s\n",__LINE__,#value); std::exit(1); } } while(false)
namespace core::env {
std::optional<std::string> value; unsigned queries{};
auto get(const char*) {++queries;return value;}
}
namespace logger {
enum class LogLevel {Error,INFO,WARN};
unsigned reports{};
struct Log {
 template<class... T> void logf(T&&...){++reports;}
 template<class... T> void log(T&&...){++reports;}
};
Log& get(const char*){static Log result;return result;}
}
'''


PORTS = r'''
#define WINAPI
#define __uuidof(T) iid<T>()
using HWND=void*;using HDC=void*;using HGLRC=void*;using HANDLE=void*;using PROC=void(*)();
using BOOL=int;using UINT=unsigned;using DWORD=std::uint32_t;using HRESULT=std::int32_t;
using GLuint=unsigned;using GLenum=unsigned;using GLint=int;using GLboolean=unsigned char;
using D3D_FEATURE_LEVEL=unsigned;
constexpr BOOL TRUE=1;
constexpr HRESULT S_OK=0,DXGI_STATUS_OCCLUDED=0x087A0001,E_INVALIDARG=-2147024809;
constexpr HRESULT E_FAIL=-2147467259;
bool FAILED(HRESULT value){return value<0;}bool SUCCEEDED(HRESULT value){return value>=0;}
constexpr unsigned CP_UTF8=65001,D3D11_USAGE_DEFAULT=0,DXGI_FORMAT_R8G8B8A8_UNORM=28;
constexpr unsigned DXGI_USAGE_RENDER_TARGET_OUTPUT=32,DXGI_SWAP_EFFECT_DISCARD=0;
constexpr unsigned D3D11_CREATE_DEVICE_BGRA_SUPPORT=32,D3D11_CREATE_DEVICE_SINGLETHREADED=1;
constexpr unsigned D3D_FEATURE_LEVEL_11_1=0xb100,D3D_FEATURE_LEVEL_11_0=0xb000;
constexpr unsigned D3D_FEATURE_LEVEL_10_1=0xa100,D3D_FEATURE_LEVEL_10_0=0xa000;
constexpr unsigned D3D_DRIVER_TYPE_HARDWARE=1,D3D11_SDK_VERSION=7;
constexpr GLenum GL_NO_ERROR=0,GL_BACK=0x405,GL_FRONT=0x404,GL_TRUE=1;
constexpr GLenum GL_READ_FRAMEBUFFER_BINDING=1,GL_DRAW_FRAMEBUFFER_BINDING=2,GL_READ_BUFFER=3;
constexpr GLenum GL_SCISSOR_TEST=4,GL_FRAMEBUFFER_SRGB=5,GL_READ_FRAMEBUFFER=6,GL_DRAW_FRAMEBUFFER=7;
constexpr GLenum GL_RENDERBUFFER=8,GL_COLOR_ATTACHMENT0=9,GL_FRAMEBUFFER_COMPLETE=10;
constexpr GLenum GL_VENDOR=11,GL_MAJOR_VERSION=12,GL_MINOR_VERSION=13,GL_DOUBLEBUFFER=14;
constexpr GLenum GL_VERSION=15;
constexpr GLenum GL_COLOR_BUFFER_BIT=16,GL_NEAREST=17,GL_SAMPLE_BUFFERS=18,GL_BACK_LEFT=19;
constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE=30,GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING=31;
constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE=32,GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE=33;
constexpr GLenum GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE=34,GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE=35;
constexpr GLenum GL_UNSIGNED_NORMALIZED=36,GL_LINEAR=37;
struct D3D11_TEXTURE2D_DESC {UINT Width{},Height{},Format{},Usage{};struct{UINT Count{},Quality{};} SampleDesc;};
struct DXGI_SWAP_CHAIN_DESC {
 struct {UINT Width{},Height{};struct{UINT Numerator{},Denominator{};} RefreshRate;UINT Format{};} BufferDesc;
 struct{UINT Count{},Quality{};} SampleDesc;
 UINT BufferUsage{},BufferCount{};HWND OutputWindow{};BOOL Windowed{};UINT SwapEffect{};
};
struct DXGI_ADAPTER_DESC {wchar_t Description[128]{};UINT VendorId{0x10DE},DeviceId{0x2C02};
 struct {std::uint32_t LowPart{7};std::int32_t HighPart{};} AdapterLuid;};
struct IUnknown;struct IDXGIDevice;struct IDXGIAdapter;struct ID3D11Texture2D;
struct ID3D11Device;struct ID3D11DeviceContext;struct IDXGISwapChain;
template<class T> constexpr int iid(){
 if constexpr(std::is_same_v<T,IUnknown>)return 1;
 if constexpr(std::is_same_v<T,IDXGIDevice>)return 2;
 if constexpr(std::is_same_v<T,ID3D11Texture2D>)return 3;
 return 0;
}
namespace mock {
std::vector<std::string> events;
HDC dc=reinterpret_cast<void*>(2);HGLRC context=reinterpret_cast<void*>(3);
bool multipleOwners{};unsigned owner(){return context==reinterpret_cast<void*>(33)?1u:0u;}
bool registeredOwners[2]{},lockedOwners[2]{};unsigned ownerPresents[2]{},ownerBlits[2]{};
bool registered{},locked{},unlockFails{},unregisterFails{},closeFails{},lockFails{},identityChanges{};
bool& registered_current(){return multipleOwners?registeredOwners[owner()]:registered;}
bool& locked_current(){return multipleOwners?lockedOwners[owner()]:locked;}
bool badDescriptor{},missingExtension{},copyFails{};unsigned presents{},blits{},resizes{},glCalls{},nvOpens{};
const char* vendor="NVIDIA recording";const char* missingProcedure{};
const char* version="4.6 recording";unsigned majorQueries{};
int majorVersion=4,minorVersion=6;bool doubleBuffered=true,defaultComplete=true;
int sampleBuffers=0,colorBits=8;unsigned attachmentQueries{},procQueries{};
GLenum componentType=GL_UNSIGNED_NORMALIZED,colorEncoding=GL_LINEAR;
unsigned invalidCalls{},nextGlName{50};HRESULT presentResult=S_OK,resizeResult=S_OK;
unsigned lastInterval{};
GLint read=21,draw=31;std::array<GLenum,256> readSelector{},drawSelector{};
bool scissor=true,srgb=true;GLenum glError{};UINT width=640,height=480;
void event(const char* name){events.emplace_back(name);}
}
struct IUnknown {
 unsigned refs{},owner{};const char* name;
 explicit IUnknown(const char* value):name(value){}
 void AddRef(){++refs;}
 unsigned Release(){CHECK(!mock::multipleOwners||owner==mock::owner());mock::events.emplace_back(std::string(name)+".release");CHECK(refs);return --refs;}
 HRESULT QueryInterface(int,void** out){*out=this;AddRef();return S_OK;}
};
struct ID3D11Texture2D:IUnknown {
 ID3D11Texture2D():IUnknown("buffer"){}
 void GetDesc(D3D11_TEXTURE2D_DESC* out){*out={mock::width,mock::height,DXGI_FORMAT_R8G8B8A8_UNORM,
  mock::badDescriptor?1u:D3D11_USAGE_DEFAULT,{1,0}};}
};
struct ID3D11DeviceContext:IUnknown {ID3D11DeviceContext():IUnknown("immediate"){} };
struct IDXGIAdapter:IUnknown {
 IDXGIAdapter():IUnknown("adapter"){}
 HRESULT GetDesc(DXGI_ADAPTER_DESC* out){*out={};return S_OK;}
};
struct IDXGIDevice:IUnknown {IDXGIDevice():IUnknown("dxgiDevice"){} HRESULT GetAdapter(IDXGIAdapter**);};
struct ID3D11Device:IUnknown {ID3D11Device():IUnknown("device"){} HRESULT QueryInterface(int,void**);};
struct IDXGISwapChain:IUnknown {
 IDXGISwapChain():IUnknown("chain"){}
 HRESULT GetBuffer(UINT index,int,void**);
 HRESULT ResizeBuffers(UINT count,UINT width,UINT height,UINT format,UINT flags){
  mock::event("resize");++mock::resizes;CHECK(!mock::registered_current()&&!mock::locked_current());
  CHECK(count==2&&format==DXGI_FORMAT_R8G8B8A8_UNORM&&flags==0);
  if(FAILED(mock::resizeResult))return mock::resizeResult;
  mock::width=width;mock::height=height;return S_OK;
 }
 HRESULT Present(UINT interval,UINT flags){CHECK(!mock::multipleOwners||owner==mock::owner());++mock::ownerPresents[owner];mock::event("present");++mock::presents;
  CHECK(!mock::locked_current()&&interval<=1&&flags==0);mock::lastInterval=interval;return mock::presentResult;}
};
namespace mock {
ID3D11Texture2D buffer,otherBuffer;ID3D11Device device;ID3D11DeviceContext immediate;
IDXGISwapChain chain;IDXGIDevice dxgiDevice;IDXGIAdapter adapter;
ID3D11Texture2D buffer2,otherBuffer2;ID3D11Device device2;ID3D11DeviceContext immediate2;
IDXGISwapChain chain2;IDXGIDevice dxgiDevice2;IDXGIAdapter adapter2;
unsigned com_refs(){return buffer.refs+otherBuffer.refs+device.refs+immediate.refs+chain.refs+dxgiDevice.refs+adapter.refs
 +buffer2.refs+otherBuffer2.refs+device2.refs+immediate2.refs+chain2.refs+dxgiDevice2.refs+adapter2.refs;}
}
HRESULT ID3D11Device::QueryInterface(int id,void** out){CHECK(id==iid<IDXGIDevice>());
 auto& result=owner?mock::dxgiDevice2:mock::dxgiDevice;*out=&result;result.AddRef();return S_OK;}
HRESULT IDXGIDevice::GetAdapter(IDXGIAdapter** out){auto& result=owner?mock::adapter2:mock::adapter;*out=&result;result.AddRef();return S_OK;}
HRESULT IDXGISwapChain::GetBuffer(UINT index,int id,void** out){
 CHECK(index==0&&id==iid<ID3D11Texture2D>());mock::event("getBuffer");
 CHECK(!mock::multipleOwners||owner==mock::owner());
 auto* buffer=owner?&mock::buffer2:&mock::buffer;
 if(mock::identityChanges&&mock::presents)buffer=owner?&mock::otherBuffer2:&mock::otherBuffer;
 *out=buffer;buffer->AddRef();return S_OK;
}
HRESULT D3D11CreateDeviceAndSwapChain(void*,unsigned type,void*,UINT flags,const D3D_FEATURE_LEVEL*,UINT,
 UINT version,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** chain,ID3D11Device** device,D3D_FEATURE_LEVEL*,
 ID3D11DeviceContext** immediate){
 mock::event("create");CHECK(type==D3D_DRIVER_TYPE_HARDWARE&&flags==D3D11_CREATE_DEVICE_BGRA_SUPPORT);
 CHECK(version==D3D11_SDK_VERSION&&desc->BufferCount==2&&desc->SwapEffect==DXGI_SWAP_EFFECT_DISCARD);
 CHECK(desc->Windowed&&desc->SampleDesc.Count==1&&desc->SampleDesc.Quality==0);
 const unsigned owner=mock::multipleOwners?mock::owner():0u;
 CHECK(desc->BufferUsage==DXGI_USAGE_RENDER_TARGET_OUTPUT&&desc->OutputWindow==reinterpret_cast<void*>(static_cast<std::uintptr_t>(1+30*owner)));
 mock::width=desc->BufferDesc.Width;mock::height=desc->BufferDesc.Height;
 *chain=owner?&mock::chain2:&mock::chain;(*chain)->AddRef();
 *device=owner?&mock::device2:&mock::device;(*device)->AddRef();
 *immediate=owner?&mock::immediate2:&mock::immediate;(*immediate)->AddRef();return S_OK;
}
DWORD GetLastError(){return 77;}
int WideCharToMultiByte(unsigned,int,const wchar_t*,int,char* out,int,void*,void*){
 std::memcpy(out,"NVIDIA test",12);return 12;
}
HDC wglGetCurrentDC(){return mock::dc;}HGLRC wglGetCurrentContext(){return mock::context;}
HANDLE owner_handle(unsigned base){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(base+10*(mock::multipleOwners?mock::owner():0u)));}
HANDLE nvOpen(void* device){
 CHECK(device==(mock::multipleOwners&&mock::owner()?&mock::device2:&mock::device));
 mock::event("open");++mock::nvOpens;return owner_handle(4);
}
BOOL nvClose(HANDLE device){CHECK(device==owner_handle(4));mock::event("close");
 CHECK(!mock::registered_current()&&!mock::locked_current());return !mock::closeFails;}
HANDLE nvRegister(HANDLE device,void* buffer,GLuint,GLenum target,GLenum access){
 CHECK(device==owner_handle(4)&&buffer==(mock::multipleOwners&&mock::owner()?&mock::buffer2:&mock::buffer));
 mock::event("register");CHECK(!mock::registered_current()&&target==GL_RENDERBUFFER&&access==2);
 mock::registered_current()=true;return owner_handle(5);
}
BOOL nvUnregister(HANDLE device,HANDLE object){CHECK(device==owner_handle(4)&&object==owner_handle(5));
 mock::event("unregister");CHECK(mock::registered_current()&&!mock::locked_current());
 if(mock::unregisterFails)return 0;mock::registered_current()=false;return 1;
}
BOOL nvLock(HANDLE device,GLint count,HANDLE* object){CHECK(device==owner_handle(4)&&*object==owner_handle(5));
 mock::event("lock");CHECK(mock::registered_current()&&!mock::locked_current()&&count==1);
 if(mock::lockFails)return 0;mock::locked_current()=true;return 1;
}
BOOL nvUnlock(HANDLE device,GLint count,HANDLE* object){CHECK(device==owner_handle(4)&&*object==owner_handle(5));
 mock::event("unlock");CHECK(mock::registered_current()&&mock::locked_current()&&count==1);
 if(mock::unlockFails)return 0;mock::locked_current()=false;return 1;
}
const char* extensions(HDC){return mock::missingExtension?"WGL_NV_DX_interop":"WGL_NV_DX_interop WGL_NV_DX_interop2";}
PROC wglGetProcAddress(const char* name){
 if(mock::missingProcedure&&std::strcmp(name,mock::missingProcedure)==0)return nullptr;
 if(std::strcmp(name,"wglGetExtensionsStringARB")==0)return reinterpret_cast<PROC>(&extensions);
 if(std::strcmp(name,"wglDXOpenDeviceNV")==0)return reinterpret_cast<PROC>(&nvOpen);
 if(std::strcmp(name,"wglDXCloseDeviceNV")==0)return reinterpret_cast<PROC>(&nvClose);
 if(std::strcmp(name,"wglDXRegisterObjectNV")==0)return reinterpret_cast<PROC>(&nvRegister);
 if(std::strcmp(name,"wglDXUnregisterObjectNV")==0)return reinterpret_cast<PROC>(&nvUnregister);
 if(std::strcmp(name,"wglDXLockObjectsNV")==0)return reinterpret_cast<PROC>(&nvLock);
 if(std::strcmp(name,"wglDXUnlockObjectsNV")==0)return reinterpret_cast<PROC>(&nvUnlock);
 return nullptr;
}
GLenum glGetError(){return std::exchange(mock::glError,GL_NO_ERROR);}
const unsigned char* glGetString(GLenum value){++mock::glCalls;return reinterpret_cast<const unsigned char*>(value==GL_VERSION?mock::version:mock::vendor);}
void glGetIntegerv(GLenum value,GLint* out){++mock::glCalls;switch(value){
 case GL_READ_FRAMEBUFFER_BINDING:*out=mock::read;break;case GL_DRAW_FRAMEBUFFER_BINDING:*out=mock::draw;break;
 case GL_READ_BUFFER:*out=static_cast<GLint>(mock::readSelector[mock::read]);break;
 case GL_SAMPLE_BUFFERS:CHECK(mock::draw==0);*out=mock::sampleBuffers;break;
 case GL_MAJOR_VERSION:++mock::majorQueries;if(mock::majorVersion<3){mock::glError=0x500;*out=0;}else *out=mock::majorVersion;break;
 case GL_MINOR_VERSION:*out=mock::minorVersion;break;default:++mock::invalidCalls;
}}
GLboolean glIsEnabled(GLenum value){++mock::glCalls;return value==GL_SCISSOR_TEST?mock::scissor:mock::srgb;}
void glEnable(GLenum value){++mock::glCalls;if(value==GL_SCISSOR_TEST)mock::scissor=true;else mock::srgb=true;}
void glDisable(GLenum value){++mock::glCalls;if(value==GL_SCISSOR_TEST)mock::scissor=false;else mock::srgb=false;}
void bindFramebuffer(GLenum target,GLuint id){++mock::glCalls;
 if(target==GL_READ_FRAMEBUFFER)mock::read=static_cast<GLint>(id);
 else if(target==GL_DRAW_FRAMEBUFFER)mock::draw=static_cast<GLint>(id);else ++mock::invalidCalls;
}
auto glBindFramebuffer=&bindFramebuffer;
void glReadBuffer(GLenum value){++mock::glCalls;mock::readSelector[mock::read]=value;}
void glDrawBuffer(GLenum value){++mock::glCalls;mock::drawSelector[mock::draw]=value;}
void genNames(GLint count,GLuint* out){++mock::glCalls;CHECK(count==1);*out=mock::nextGlName++;}
auto glGenFramebuffers=&genNames;auto glGenRenderbuffers=&genNames;
void deleteFramebuffer(GLint,const GLuint*){mock::event("deleteFbo");CHECK(!mock::registered_current());}
void deleteRenderbuffer(GLint,const GLuint*){mock::event("deleteRbo");CHECK(!mock::registered_current());}
auto glDeleteFramebuffers=&deleteFramebuffer;auto glDeleteRenderbuffers=&deleteRenderbuffer;
void attach(GLenum target,GLenum attachment,GLenum type,GLuint){++mock::glCalls;
 CHECK(mock::locked_current()&&target==GL_DRAW_FRAMEBUFFER&&attachment==GL_COLOR_ATTACHMENT0&&type==GL_RENDERBUFFER);}
auto glFramebufferRenderbuffer=&attach;
GLenum complete(GLenum target){++mock::glCalls;
 if(target==GL_READ_FRAMEBUFFER){CHECK(mock::read==0);return mock::defaultComplete?GL_FRAMEBUFFER_COMPLETE:0;}
 CHECK(target==GL_DRAW_FRAMEBUFFER);return GL_FRAMEBUFFER_COMPLETE;}
auto glCheckFramebufferStatus=&complete;
void glGetBooleanv(GLenum value,GLboolean* out){++mock::glCalls;CHECK(value==GL_DOUBLEBUFFER&&mock::draw==0);*out=mock::doubleBuffered;}
void blit(GLint x0,GLint y0,GLint x1,GLint y1,GLint dx0,GLint dy0,GLint dx1,GLint dy1,GLenum mask,GLenum filter){
 mock::event("blit");++mock::blits;
 CHECK(mock::locked_current()&&mock::read==0&&mock::readSelector[0]==GL_BACK&&mock::draw>=50);
 CHECK(!mock::scissor&&!mock::srgb&&x0==0&&y0==0&&dx0==0&&dy0==static_cast<int>(mock::height));
 CHECK(x1==static_cast<int>(mock::width)&&y1==static_cast<int>(mock::height)&&dx1==x1&&dy1==0);
 CHECK(mask==GL_COLOR_BUFFER_BIT&&filter==GL_NEAREST);
 if(mock::copyFails)mock::glError=0x502;
}
auto glBlitFramebuffer=&blit;
template<unsigned Owner> void ownedBlit(GLint x0,GLint y0,GLint x1,GLint y1,GLint dx0,GLint dy0,GLint dx1,GLint dy1,GLenum mask,GLenum filter){
 CHECK(mock::owner()==Owner);++mock::ownerBlits[Owner];blit(x0,y0,x1,y1,dx0,dy0,dx1,dy1,mask,filter);
}


void glGetFramebufferAttachmentParameteriv(GLenum target,GLenum attachment,GLenum property,GLint* out){
 ++mock::glCalls;++mock::attachmentQueries;CHECK(target==GL_READ_FRAMEBUFFER&&mock::read==0&&attachment==GL_BACK_LEFT);
 if(property==GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE)*out=mock::componentType;
 else if(property==GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING)*out=mock::colorEncoding;
 else {CHECK(property>=GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE&&property<=GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE);*out=mock::colorBits;}
}
using PFNGLGETSTRINGPROC=decltype(&glGetString);
using PFNGLGETERRORPROC=decltype(&glGetError);
using PFNGLGETINTEGERVPROC=decltype(&glGetIntegerv);
using PFNGLGETBOOLEANVPROC=decltype(&glGetBooleanv);
using PFNGLISENABLEDPROC=decltype(&glIsEnabled);
using PFNGLENABLEPROC=decltype(&glEnable);
using PFNGLDISABLEPROC=decltype(&glDisable);
using PFNGLBINDFRAMEBUFFERPROC=decltype(glBindFramebuffer);
using PFNGLREADBUFFERPROC=decltype(&glReadBuffer);
using PFNGLDRAWBUFFERPROC=decltype(&glDrawBuffer);
using PFNGLDELETEFRAMEBUFFERSPROC=decltype(glDeleteFramebuffers);
using PFNGLDELETERENDERBUFFERSPROC=decltype(glDeleteRenderbuffers);
using PFNGLGENRENDERBUFFERSPROC=decltype(glGenRenderbuffers);
using PFNGLGENFRAMEBUFFERSPROC=decltype(glGenFramebuffers);
using PFNGLFRAMEBUFFERRENDERBUFFERPROC=decltype(glFramebufferRenderbuffer);
using PFNGLCHECKFRAMEBUFFERSTATUSPROC=decltype(glCheckFramebufferStatus);
using PFNGLBLITFRAMEBUFFERPROC=decltype(glBlitFramebuffer);
using PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC=decltype(&glGetFramebufferAttachmentParameteriv);
namespace epochengine::openglcontext::PlatformGL {
void* get_proc_address(const char* name){
 ++mock::procQueries;
 if(mock::missingProcedure&&std::strcmp(name,mock::missingProcedure)==0)return nullptr;
 if(std::strcmp(name,"glGetString")==0)return reinterpret_cast<void*>(&glGetString);
 if(std::strcmp(name,"glGetError")==0)return reinterpret_cast<void*>(&glGetError);
 if(std::strcmp(name,"glGetIntegerv")==0)return reinterpret_cast<void*>(&glGetIntegerv);
 if(std::strcmp(name,"glGetBooleanv")==0)return reinterpret_cast<void*>(&glGetBooleanv);
 if(std::strcmp(name,"glIsEnabled")==0)return reinterpret_cast<void*>(&glIsEnabled);
 if(std::strcmp(name,"glEnable")==0)return reinterpret_cast<void*>(&glEnable);
 if(std::strcmp(name,"glDisable")==0)return reinterpret_cast<void*>(&glDisable);
 if(std::strcmp(name,"glBindFramebuffer")==0)return reinterpret_cast<void*>(glBindFramebuffer);
 if(std::strcmp(name,"glReadBuffer")==0)return reinterpret_cast<void*>(&glReadBuffer);
 if(std::strcmp(name,"glDrawBuffer")==0)return reinterpret_cast<void*>(&glDrawBuffer);
 if(std::strcmp(name,"glDeleteFramebuffers")==0)return reinterpret_cast<void*>(glDeleteFramebuffers);
 if(std::strcmp(name,"glDeleteRenderbuffers")==0)return reinterpret_cast<void*>(glDeleteRenderbuffers);
 if(std::strcmp(name,"glGenRenderbuffers")==0)return reinterpret_cast<void*>(glGenRenderbuffers);
 if(std::strcmp(name,"glGenFramebuffers")==0)return reinterpret_cast<void*>(glGenFramebuffers);
 if(std::strcmp(name,"glFramebufferRenderbuffer")==0)return reinterpret_cast<void*>(glFramebufferRenderbuffer);
 if(std::strcmp(name,"glCheckFramebufferStatus")==0)return reinterpret_cast<void*>(glCheckFramebufferStatus);
 if(std::strcmp(name,"glBlitFramebuffer")==0){
  if(mock::multipleOwners)return reinterpret_cast<void*>(mock::owner()?&ownedBlit<1>:&ownedBlit<0>);
  return reinterpret_cast<void*>(glBlitFramebuffer);
 }
 if(std::strcmp(name,"glGetFramebufferAttachmentParameteriv")==0)return reinterpret_cast<void*>(&glGetFramebufferAttachmentParameteriv);
 return reinterpret_cast<void*>(wglGetProcAddress(name));
}
}
'''


CASES = r'''
namespace actual=epochengine::openglcontext::native_present;
void reset(){
 CHECK(mock::com_refs()==0&&!mock::registered&&!mock::locked);
 mock::sampleBuffers=0;mock::colorBits=8;mock::defaultComplete=true;mock::componentType=GL_UNSIGNED_NORMALIZED;mock::colorEncoding=GL_LINEAR;
 mock::events.clear();mock::unlockFails=mock::unregisterFails=mock::closeFails=mock::lockFails=false;
 mock::identityChanges=mock::badDescriptor=mock::missingExtension=mock::copyFails=false;
 mock::presentResult=mock::resizeResult=S_OK;mock::presents=mock::blits=mock::resizes=mock::invalidCalls=0;
 mock::dc=reinterpret_cast<void*>(2);mock::context=reinterpret_cast<void*>(3);
 mock::read=21;mock::draw=31;mock::readSelector.fill(0);mock::drawSelector.fill(0);
 mock::readSelector[0]=GL_FRONT;mock::readSelector[21]=GL_COLOR_ATTACHMENT0+7;
 mock::drawSelector[31]=GL_COLOR_ATTACHMENT0+8;mock::scissor=mock::srgb=true;
 actual::retirementFailed=false;
}
void initialize(actual::State& state){
 CHECK(actual::initialize(state,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
 CHECK(state.resource&&!state.failed&&mock::registered&&!mock::locked);
 CHECK(state.selectionResolved&&state.useDxgi&&state.selectionContext==mock::context&&state.selectionDc==mock::dc);
 CHECK(mock::read==21&&mock::draw==31&&mock::readSelector[0]==GL_FRONT);
 mock::events.clear();
}
void restored(){
 CHECK(mock::read==21&&mock::draw==31&&mock::readSelector[0]==GL_FRONT);
 CHECK(mock::readSelector[21]==GL_COLOR_ATTACHMENT0+7&&mock::drawSelector[31]==GL_COLOR_ATTACHMENT0+8);
 CHECK(mock::scissor&&mock::srgb&&mock::invalidCalls==0);
}
std::size_t event(const char* name){
 auto at=std::find(mock::events.begin(),mock::events.end(),name);CHECK(at!=mock::events.end());
 return static_cast<std::size_t>(at-mock::events.begin());
}
int policy_case(const std::string& policy,const std::string& scenario){
 reset();
 if(policy!="absent")core::env::value=policy;
 const bool automatic=policy=="absent"||policy.empty()||policy=="auto";
 const bool forced=policy=="dxgi",legacy=policy=="wgl";
 const bool valid=automatic||forced||legacy;
 if(scenario=="missing-extension")mock::missingExtension=true;
 if(scenario=="non-nvidia")mock::vendor="AMD recording";
 if(scenario=="old-gl"||scenario=="gl2-stale-error"){
  mock::majorVersion=2;mock::minorVersion=1;mock::version="2.1 recording";
  if(scenario=="gl2-stale-error")mock::glError=0x502;
 }
 if(scenario=="gl3"){mock::majorVersion=3;mock::minorVersion=0;mock::version="3.0 recording";}
 if(scenario=="missing-nv-proc")mock::missingProcedure="wglDXUnlockObjectsNV";
 if(scenario=="no-double-buffer")mock::doubleBuffered=false;
 if(scenario=="missing-gl-proc")mock::missingProcedure="glBlitFramebuffer";
 if(scenario=="incomplete-default")mock::defaultComplete=false;
 if(scenario=="rgba8-msaa"||scenario=="incompatible-msaa")mock::sampleBuffers=1;
 if(scenario=="incompatible-msaa")mock::colorBits=10;
 if(scenario=="gl-error")mock::glError=0x502;
 if(scenario=="multi-context"){
  actual::State unsupported,supported;
  mock::context=reinterpret_cast<void*>(10);mock::dc=reinterpret_cast<void*>(11);mock::vendor="AMD recording";
  CHECK(!actual::selected(unsupported)&&unsupported.selectionResolved&&!unsupported.failed);
  CHECK(!unsupported.useDxgi&&unsupported.selectionContext==mock::context&&unsupported.selectionDc==mock::dc);
  const auto unsupportedQueries=mock::glCalls;
  CHECK(!actual::selected(unsupported)&&mock::glCalls==unsupportedQueries);
  mock::context=reinterpret_cast<void*>(12);mock::dc=reinterpret_cast<void*>(13);mock::vendor="NVIDIA recording";
  CHECK(actual::selected(supported)&&supported.selectionResolved&&!supported.failed);
  const auto supportedQueries=mock::glCalls;
  CHECK(actual::selected(supported)&&mock::glCalls==supportedQueries);
  mock::context=reinterpret_cast<void*>(10);mock::dc=reinterpret_cast<void*>(11);
  CHECK(!actual::selected(unsupported)&&!unsupported.failed&&mock::glCalls==supportedQueries);
  mock::context=reinterpret_cast<void*>(12);mock::dc=reinterpret_cast<void*>(13);
  CHECK(actual::selected(supported)&&!supported.failed&&mock::glCalls==supportedQueries);
  CHECK(core::env::queries==1&&mock::nvOpens==0&&mock::com_refs()==0);return 0;
 }
 actual::State state;
 if(scenario=="no-current"){
  mock::context=nullptr;
  const bool initial=actual::selected(state);
  CHECK(!initial&&mock::glCalls==0);
  if(automatic||forced)CHECK(!state.failed&&!state.selectionResolved);
  mock::context=reinterpret_cast<void*>(3);
 }
 const bool unsupported=scenario=="missing-extension"||scenario=="non-nvidia"||scenario=="old-gl"||scenario=="gl2-stale-error"
  ||scenario=="missing-nv-proc"||scenario=="no-double-buffer"||scenario=="missing-gl-proc"
  ||scenario=="incomplete-default"||scenario=="incompatible-msaa";
 const bool failed=!valid||(!legacy&&(scenario=="gl-error"||(forced&&unsupported)));
 const bool chosen=valid&&!legacy&&!failed&&!unsupported;
 CHECK(actual::selected(state)==chosen&&state.failed==failed);restored();
 if(scenario=="rgba8-msaa"&&!legacy)CHECK(mock::attachmentQueries==6);
 if(scenario=="supported"&&!legacy&&valid)CHECK(mock::attachmentQueries==0);
 if(!failed){
  CHECK(state.selectionResolved&&state.useDxgi==chosen);
  if(!legacy)CHECK(state.selectionContext==mock::context&&state.selectionDc==mock::dc);
 }
 const auto calls=mock::glCalls;
 core::env::value=legacy?"dxgi":"wgl";
 CHECK(actual::selected(state)==chosen&&state.failed==failed&&mock::glCalls==calls);
 CHECK(core::env::queries==1&&mock::nvOpens==0&&mock::com_refs()==0);
 CHECK(mock::majorQueries==0);
 if(scenario=="gl2-stale-error")CHECK(mock::glError==0x502);
 if(legacy||!valid)CHECK(mock::glCalls==0&&mock::procQueries==0);
 if(scenario=="cached-context-mismatch"){
  mock::context=reinterpret_cast<void*>(99);
  CHECK(!actual::selected(state)&&state.failed&&mock::glCalls==calls);
 }
 if(failed){
  CHECK(!actual::initialize(state,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
  CHECK(!state.resource&&state.failed&&mock::nvOpens==0);
 }
 return 0;
}
int live_resources(){
 reset();core::env::value="dxgi";mock::multipleOwners=true;
 for(auto* object:std::array<IUnknown*,7>{&mock::buffer2,&mock::otherBuffer2,&mock::device2,&mock::immediate2,&mock::chain2,&mock::dxgiDevice2,&mock::adapter2})object->owner=1;
 actual::State first,second;
 CHECK(actual::initialize(first,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));restored();
 mock::context=reinterpret_cast<void*>(33);mock::dc=reinterpret_cast<void*>(32);
 CHECK(actual::initialize(second,reinterpret_cast<void*>(31),mock::dc,mock::context,640,480));restored();
 CHECK(first.resource!=second.resource&&first.resource->device!=second.resource->device);
 CHECK(first.resource->chain!=second.resource->chain&&first.resource->buffer!=second.resource->buffer);
 CHECK(first.resource->interopDevice!=second.resource->interopDevice&&first.resource->interopObject!=second.resource->interopObject);
 CHECK(first.resource->gl.blitFramebuffer!=second.resource->gl.blitFramebuffer);
 const auto resolutions=mock::procQueries;
 for(unsigned turn=0;turn<2;++turn){
  mock::context=reinterpret_cast<void*>(3);mock::dc=reinterpret_cast<void*>(2);
  CHECK(actual::selected(first));CHECK(actual::present(first,640,480,0)==actual::Result::presented);restored();
  mock::context=reinterpret_cast<void*>(33);mock::dc=reinterpret_cast<void*>(32);
  CHECK(actual::selected(second));CHECK(actual::present(second,640,480,1)==actual::Result::presented);restored();
 }
 CHECK(mock::procQueries==resolutions&&mock::ownerPresents[0]==2&&mock::ownerPresents[1]==2);
 CHECK(mock::ownerBlits[0]==2&&mock::ownerBlits[1]==2);
 mock::context=reinterpret_cast<void*>(3);mock::dc=reinterpret_cast<void*>(2);
 CHECK(actual::release(first));CHECK(!mock::registeredOwners[0]&&mock::registeredOwners[1]);
 CHECK(mock::device.refs==0&&mock::chain.refs==0&&mock::buffer.refs==0);
 CHECK(mock::device2.refs==1&&mock::chain2.refs==1&&mock::buffer2.refs==1);
 mock::context=reinterpret_cast<void*>(33);mock::dc=reinterpret_cast<void*>(32);
 CHECK(actual::release(second));CHECK(!mock::registeredOwners[1]&&mock::com_refs()==0);
 CHECK(!mock::lockedOwners[0]&&!mock::lockedOwners[1]&&!actual::admission_failed());return 0;
}
int main(int argc,char** argv){
 if(argc>1&&std::string_view{argv[1]}=="live-resources")return live_resources();
 if(argc>1)return policy_case(argv[1],argc>2?argv[2]:"supported");
 core::env::value="dxgi";
 static_assert(!std::is_copy_constructible_v<actual::State>);
 static_assert(!std::is_copy_assignable_v<actual::State>);
 static_assert(std::is_nothrow_move_constructible_v<actual::State>);
 reset();{
  actual::State state;initialize(state);
  CHECK(actual::present(state,640,480,0)==actual::Result::presented);restored();
  CHECK(event("lock")<event("blit")&&event("blit")<event("unlock")&&event("unlock")<event("present"));
  CHECK(state.resource->identityChecked&&mock::presents==1&&mock::blits==1&&mock::lastInterval==0);
  mock::events.clear();CHECK(actual::present(state,800,600,1)==actual::Result::presented);restored();
  CHECK(event("unregister")<event("buffer.release")&&event("buffer.release")<event("resize"));
  CHECK(event("resize")<event("register")&&event("register")<event("present"));
  CHECK(mock::resizes==1&&mock::width==800&&mock::height==600&&mock::lastInterval==1);
  CHECK(actual::release(state)&&!state.resource&&mock::com_refs()==0);
 }
 reset();{
  actual::State state;initialize(state);mock::presentResult=DXGI_STATUS_OCCLUDED;
  CHECK(actual::present(state,640,480,0)==actual::Result::occluded);restored();
  CHECK(!state.failed&&!state.resource->identityChecked&&mock::presents==1);
  mock::presentResult=S_OK;CHECK(actual::present(state,640,480,0)==actual::Result::presented);
  CHECK(state.resource->identityChecked);CHECK(actual::release(state));
 }
 reset();{
  actual::State state;initialize(state);mock::unlockFails=true;
  CHECK(actual::present(state,640,480,0)==actual::Result::failed);restored();
  CHECK(mock::presents==0&&mock::locked&&state.failed);
  const auto refs=mock::com_refs();CHECK(!actual::release(state));
  CHECK(actual::admission_failed()&&state.resource&&mock::com_refs()==refs);
  mock::unlockFails=false;CHECK(actual::release(state));
 }
 reset();{
  actual::State state;initialize(state);mock::unregisterFails=true;
  auto* retained=state.resource;const auto refs=mock::com_refs();
  CHECK(!actual::release(state)&&state.resource==retained&&mock::com_refs()==refs);
  CHECK(actual::admission_failed()&&state.failed&&mock::registered);
  CHECK(std::find(mock::events.begin(),mock::events.end(),"buffer.release")==mock::events.end());
  actual::State blocked;const auto opened=mock::nvOpens;
  CHECK(!actual::initialize(blocked,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
  CHECK(!blocked.resource&&mock::nvOpens==opened);
  mock::unregisterFails=false;CHECK(actual::release(state));
 }
 reset();{
  actual::State state;initialize(state);mock::unregisterFails=true;const auto refs=mock::com_refs();
  CHECK(actual::present(state,800,600,0)==actual::Result::failed);
  CHECK(mock::resizes==0&&mock::presents==0&&mock::com_refs()==refs&&actual::admission_failed());
  CHECK(std::find(mock::events.begin(),mock::events.end(),"buffer.release")==mock::events.end());
  mock::unregisterFails=false;CHECK(actual::release(state));
 }
 reset();{
  actual::State state;initialize(state);const auto refs=mock::com_refs();mock::context=nullptr;
  CHECK(!actual::release(state)&&state.resource&&mock::com_refs()==refs&&actual::admission_failed());
  CHECK(mock::events.empty());mock::context=reinterpret_cast<void*>(3);CHECK(actual::release(state));
 }
 reset();{
  actual::State state;initialize(state);mock::closeFails=true;
  CHECK(!actual::release(state)&&state.resource&&actual::admission_failed());
  CHECK(!mock::registered&&mock::buffer.refs==0&&mock::device.refs==1&&mock::chain.refs==1);
  mock::closeFails=false;CHECK(actual::release(state));
 }
 reset();{
  actual::State state;initialize(state);mock::identityChanges=true;
  CHECK(actual::present(state,640,480,0)==actual::Result::failed&&state.failed);
  CHECK(mock::otherBuffer.refs==0&&mock::buffer.refs==1);CHECK(actual::release(state));
 }
 reset();{
  actual::State state;initialize(state);mock::copyFails=true;
  CHECK(actual::present(state,640,480,0)==actual::Result::failed);restored();
  CHECK(mock::presents==0&&mock::locked);CHECK(actual::release(state));
  CHECK(state.failed&&!state.resource);mock::copyFails=false;
  CHECK(!actual::initialize(state,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
 }
 reset();{
  actual::State state;initialize(state);auto* resource=state.resource;
  actual::State moved{std::move(state)};CHECK(!state.resource&&moved.resource==resource);
  CHECK(moved.selectionResolved&&moved.useDxgi&&moved.selectionContext==mock::context&&moved.selectionDc==mock::dc);
  CHECK(!state.selectionResolved&&!state.useDxgi&&!state.selectionContext&&!state.selectionDc);
  actual::State assigned;assigned=std::move(moved);CHECK(!moved.resource&&assigned.resource==resource);
  CHECK(assigned.selectionResolved&&assigned.useDxgi&&assigned.selectionContext==mock::context&&assigned.selectionDc==mock::dc);
  CHECK(!moved.selectionResolved&&!moved.useDxgi&&!moved.selectionContext&&!moved.selectionDc);
  actual::State donor;donor.resource=new actual::Resource;auto* donorResource=donor.resource;
  mock::unregisterFails=true;assigned=std::move(donor);
  CHECK(assigned.resource==resource&&assigned.failed&&donor.resource==donorResource&&actual::admission_failed());
  mock::unregisterFails=false;CHECK(actual::release(assigned));
  CHECK(actual::release(donor));
 }
 reset();{
  actual::State state;mock::badDescriptor=true;
  CHECK(!actual::initialize(state,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
  CHECK(state.failed&&!state.resource&&mock::com_refs()==0);
 }
 reset();{
  actual::State state;mock::missingExtension=true;
  CHECK(!actual::initialize(state,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
  CHECK(state.failed&&!state.resource&&mock::com_refs()==0);
 }
 reset();return 0;
}
'''


class OpenGLNativePresent(unittest.TestCase):
    def test_actual_resource_owner_with_recording_gl_nv_and_com(self):
        report = ROOT / "Artifacts/Reports/opengl-native-production-oct30/gl-family-dispatch/cpu"
        report.mkdir(parents=True, exist_ok=True)
        source = report / "native_present_owner_contract.cpp"
        source.write_text(COMMON + PORTS + actual_owner(True) + CASES, encoding="utf-8")
        results = compile_and_run(self, source, report, windows_flags=("/WX",))
        extra = []
        env = dict(os.environ, TEMP=str(report / "tmp"), TMP=str(report / "tmp"), TMPDIR=str(report / "tmp"))
        policy_cases = [(value, "supported") for value in ("absent", "", "auto", "dxgi", "wgl", "DXGI", " dxgi", "dxgi ", "unknown")]
        policy_cases += [(policy, scenario) for policy in ("auto", "dxgi")
                         for scenario in ("missing-extension", "non-nvidia", "old-gl", "gl2-stale-error", "gl3",
                                          "missing-nv-proc", "no-double-buffer", "no-current",
                                          "missing-gl-proc", "incomplete-default", "rgba8-msaa", "incompatible-msaa",
                                          "gl-error", "cached-context-mismatch")]
        policy_cases += [("auto", "multi-context"), ("wgl", "gl-error"), ("wgl", "missing-extension"), ("live-resources", "two-contexts")]
        for configuration in ("Debug", "Release"):
            executable = report / (configuration + (".exe" if os.name == "nt" else ""))
            for policy, scenario in policy_cases:
                ran = subprocess.run([str(executable), policy, scenario], cwd=report, env=env,
                                     capture_output=True, text=True, timeout=15)
                self.assertEqual(ran.returncode, 0, f"{configuration}/{policy!r}/{scenario}: {ran.returncode}\n{ran.stdout}{ran.stderr}")
                extra.append(dict(configuration=configuration, policy=policy, scenario=scenario, exit=ran.returncode))
        (report / "result.json").write_text(json.dumps(dict(
            production={path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in (OWNER, INTERFACE)},
            configurations=results, selection_processes=extra,
            scope="Actual owner with CPU recording ports; no native GL/D3D/window/driver execution."
        ), indent=2) + "\n", encoding="utf-8")

    def test_actual_resolver_initializes_fallback_once_concurrently(self):
        path = "Engine/modules/opengl.platform.ixx"
        owner = (ROOT / path).read_text(encoding="utf-8")
        resolver = body_of(owner, "inline void* get_proc_address(const char* name) noexcept")
        resolver = resolver.replace("#if defined(_WIN32)", "#if 1")
        code = COMMON + r'''
#include <barrier>
#include <bit>
#include <ranges>
#include <thread>
using HMODULE=void*;using PROC=void(*)();
std::atomic<unsigned> moduleQueries{},loads{},coreQueries{},wglQueries{};
thread_local std::uintptr_t wglValue{};
PROC wglGetProcAddress(const char*){++wglQueries;return reinterpret_cast<PROC>(wglValue);}
HMODULE GetModuleHandleW(const wchar_t*){++moduleQueries;return nullptr;}
HMODULE LoadLibraryW(const wchar_t*){++loads;std::this_thread::yield();return reinterpret_cast<void*>(0x1000);}
PROC GetProcAddress(HMODULE module,const char*){CHECK(module==reinterpret_cast<void*>(0x1000));++coreQueries;return reinterpret_cast<PROC>(0x2000);}
void* actual_resolve(const char* name) noexcept {
''' + resolver + r'''
}
int main(){
 CHECK(!actual_resolve(nullptr)&&!actual_resolve(""));CHECK(wglQueries==0&&loads==0);
 wglValue=0x3000;CHECK(actual_resolve("extension")==reinterpret_cast<void*>(0x3000));
 CHECK(moduleQueries==0&&loads==0&&coreQueries==0);
 std::barrier start{9};std::vector<std::jthread> workers;
 for(unsigned worker=0;worker<8;++worker)workers.emplace_back([worker,&start]{
  constexpr std::uintptr_t sentinels[]{0,1,2,3,static_cast<std::uintptr_t>(-1)};
  wglValue=sentinels[worker%5];start.arrive_and_wait();
  for(unsigned call=0;call<16;++call)CHECK(actual_resolve("core")==reinterpret_cast<void*>(0x2000));
 });
 start.arrive_and_wait();workers.clear();
 CHECK(moduleQueries==1&&loads==1&&coreQueries==128&&wglQueries==129);
 return 0;
}
'''
        report = ROOT / "Artifacts/Reports/opengl-native-production-oct30/gl-family-dispatch/resolver-cpu"
        report.mkdir(parents=True, exist_ok=True)
        source = report / "native_present_resolver_contract.cpp"
        source.write_text(code, encoding="utf-8")
        results = compile_and_run(self, source, report, windows_flags=("/WX",))
        (report / "result.json").write_text(json.dumps(dict(
            production={path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()},
            configurations=results, scope="Actual existing resolver body, eight CPU threads, recording Win32 ports only."
        ), indent=2) + "\n", encoding="utf-8")

    def test_actual_unsupported_branch_and_interface_ownership(self):
        report = ROOT / "Artifacts/Reports/opengl-native-production-oct30/gl-family-dispatch/unsupported-cpu"
        report.mkdir(parents=True, exist_ok=True)
        source = report / "native_present_unsupported_contract.cpp"
        source.write_text(COMMON + actual_owner(False) + r'''
int main(int argc,char** argv){
 namespace actual=epochengine::openglcontext::native_present;
 const std::string policy=argc>1?argv[1]:"auto";core::env::value=policy;
 const bool failed=policy!="auto"&&policy!=""&&policy!="wgl";
 actual::State state;
 CHECK(!actual::selected(state)&&state.selectionResolved&&state.failed==failed&&!state.useDxgi);
 CHECK(!actual::initialize(state,nullptr,nullptr,nullptr,640,480));
 CHECK(state.failed==failed&&!state.resource&&actual::present(state,640,480,0)==actual::Result::unsupported);
 CHECK(actual::release(state)&&!actual::admission_failed());
 actual::State moved{std::move(state)};CHECK(moved.failed==failed&&!state.failed&&moved.selectionResolved&&!state.selectionResolved);
 actual::State assigned;assigned=std::move(moved);CHECK(assigned.failed==failed&&!moved.failed&&assigned.selectionResolved&&!moved.selectionResolved);
 assigned=actual::State{};CHECK(!assigned.failed&&!assigned.selectionResolved&&!assigned.useDxgi);
 CHECK(core::env::queries==1);
 return 0;
}
''', encoding="utf-8")
        results = compile_and_run(self, source, report, windows_flags=("/WX",))
        extra = []
        env = dict(os.environ, TEMP=str(report / "tmp"), TMP=str(report / "tmp"), TMPDIR=str(report / "tmp"))
        for configuration in ("Debug", "Release"):
            executable = report / (configuration + (".exe" if os.name == "nt" else ""))
            for policy in ("", "auto", "wgl", "dxgi", "unknown"):
                ran = subprocess.run([str(executable), policy], cwd=report, env=env,
                                     capture_output=True, text=True, timeout=15)
                self.assertEqual(ran.returncode, 0, f"{configuration}/{policy!r}: {ran.returncode}\n{ran.stdout}{ran.stderr}")
                extra.append(dict(configuration=configuration, policy=policy, exit=ran.returncode))
        (report / "result.json").write_text(json.dumps(dict(configurations=results, policy_processes=extra), indent=2) + "\n", encoding="utf-8")

    def test_actual_pacing_completion_and_failed_retirement_hooks(self):
        paths = ["Engine/src/renderers/opengl/opengl.backend.cpp",
                 "Engine/src/renderers/opengl/opengl.context_process_impl.cpp",
                 "Engine/src/renderers/opengl/opengl.context_cleanup.cpp"]
        backend, process, cleanup = [(ROOT / path).read_text(encoding="utf-8") for path in paths]
        pacing = body_of(backend, "        ctx->apply_frame_pacing = [](")
        start = process.index("        if (nativeOccluded)")
        completion = process[start:process.index("\n    }\n}", start)]
        start = cleanup.index("        if (!nativePresentationRetired)")
        retirement = cleanup[start:cleanup.index("\n#elif defined(__linux__)", start)]
        selection = process.index("        const bool dxgi = native_present::selected(nativeState.nativePresent);")
        rejection = process.index("        if (nativeState.nativePresent.failed || !nativeState.nativePresent.selectionResolved)", selection)
        legacy = process.index("            nativeSucceeded = PlatformGL::swap_buffers(guard.target());", selection)
        self.assertLess(selection, rejection)
        self.assertLess(rejection, legacy)
        code = COMMON + PORTS + actual_owner(True) + r'''
namespace native_present=epochengine::openglcontext::native_present;
using HDC=void*;using HGLRC=void*;using HWND=void*;
constexpr int FALSE=0,RDW_INVALIDATE=1,RDW_ALLCHILDREN=2;
unsigned redraws{},contextDeletes{},dcReleases{},windowDestroys{};
int IsWindow(HWND handle){return handle!=nullptr;}
void RedrawWindow(HWND,void*,void*,int){++redraws;}
void wglDeleteContext(HGLRC){++contextDeletes;}
void ReleaseDC(HWND,HDC){++dcReleases;}
void DestroyWindow(HWND){++windowDestroys;}
namespace perf {
enum class frame_pacing_mode{uncapped,capped,vsync};
struct native_frame_pacing_result{
 bool configured{},native_pacing_active{};
 frame_pacing_mode effective_mode{frame_pacing_mode::uncapped};double hz{};
};
const char* to_string(frame_pacing_mode){return "recording";}
}
namespace core {
enum class BackendLifecycleState{ready,failed};
struct Window {
 bool ownsNativeGlContext=true,ownsNativeDc=true;
 BackendLifecycleState lifecycle=BackendLifecycleState::ready;
 std::atomic<bool> firstPresentComplete{};HWND hwnd=reinterpret_cast<void*>(1);
 void set_backend_lifecycle(BackendLifecycleState value){lifecycle=value;}
};
}
struct Context{bool init_failed{};core::Window* windowData{};};
struct State {
 native_present::State nativePresent;
 unsigned nativePresentSyncInterval{};unsigned frameCount{};
 bool ownsContext=true,ownsDc=true,ownsWindow=true;
 HGLRC hglrc=reinterpret_cast<void*>(3);HDC hdc=reinterpret_cast<void*>(2);
 HWND hwnd=reinterpret_cast<void*>(1),parent=reinterpret_cast<void*>(4);
};
namespace openglcontext {
namespace native_present {
bool selection{},rejected{},unresolved{};
bool selected(epochengine::openglcontext::native_present::State& state){
 state.failed=rejected;state.selectionResolved=!unresolved;state.useDxgi=selection&&!rejected&&!unresolved;
 return state.useDxgi;
}}
namespace PlatformGL {
struct Native {bool available=true;bool valid()const{return available;}};
Native native;unsigned sets{},queries{};int requested{};bool requestSucceeds=true;
struct SwapStatus {bool available=true;int interval{};} status;
Native get_current(){return native;}
bool set_swap_interval(int value){++sets;requested=value;return requestSucceeds;}
SwapStatus query_swap_interval(){++queries;return status;}
}
}
namespace opengltextures {
struct Backend{};State state;
Backend& get_opengl_backend(){static Backend value;return value;}
State& state_for_platform_context(Backend&,openglcontext::PlatformGL::Native,const void*){return state;}
}
perf::native_frame_pacing_result apply(perf::frame_pacing_mode mode,double hz){
''' + pacing + r'''
}
bool complete(Context* ctx,State& glState,bool nativeSucceeded,bool nativeOccluded){
''' + completion + r'''
}
void retire(Context* ctx,State& glState,bool nativePresentationRetired){
''' + retirement + r'''
}
int main(){
 using Mode=perf::frame_pacing_mode;
 openglcontext::native_present::selection=true;
 auto result=apply(Mode::vsync,60);
 CHECK(result.configured&&result.native_pacing_active&&result.effective_mode==Mode::vsync);
 CHECK(opengltextures::state.nativePresentSyncInterval==1);
 result=apply(Mode::capped,120);
 CHECK(result.configured&&!result.native_pacing_active&&result.effective_mode==Mode::uncapped);
 CHECK(opengltextures::state.nativePresentSyncInterval==0);
 CHECK(openglcontext::PlatformGL::sets==0&&openglcontext::PlatformGL::queries==0);
 openglcontext::PlatformGL::native.available=false;
 result=apply(Mode::vsync,60);CHECK(!result.configured&&!result.native_pacing_active);
 CHECK(opengltextures::state.nativePresentSyncInterval==0);
 openglcontext::PlatformGL::native.available=true;
 openglcontext::native_present::rejected=true;
 result=apply(Mode::capped,120);CHECK(!result.configured&&!result.native_pacing_active);
 CHECK(openglcontext::PlatformGL::sets==0&&openglcontext::PlatformGL::queries==0);
 openglcontext::native_present::rejected=false;openglcontext::native_present::unresolved=true;
 result=apply(Mode::capped,120);CHECK(!result.configured&&!result.native_pacing_active);
 CHECK(openglcontext::PlatformGL::sets==0&&openglcontext::PlatformGL::queries==0);
 openglcontext::native_present::unresolved=false;
 openglcontext::native_present::selection=false;
 openglcontext::PlatformGL::status.interval=1;
 result=apply(Mode::vsync,60);CHECK(result.configured&&result.native_pacing_active);
 CHECK(openglcontext::PlatformGL::sets==1&&openglcontext::PlatformGL::queries==1);
 CHECK(openglcontext::PlatformGL::requested==1);
 openglcontext::PlatformGL::status.interval=0;
 result=apply(Mode::capped,120);CHECK(result.configured&&!result.native_pacing_active);
 CHECK(openglcontext::PlatformGL::requested==0);
 core::Window window;Context ctx;ctx.windowData=&window;State state;
 CHECK(complete(&ctx,state,false,true));CHECK(state.frameCount==0&&!window.firstPresentComplete&&redraws==0);
 CHECK(!complete(&ctx,state,false,false));CHECK(state.frameCount==0&&!window.firstPresentComplete&&redraws==0);
 CHECK(complete(&ctx,state,true,false));CHECK(state.frameCount==1&&window.firstPresentComplete);
 CHECK(complete(&ctx,state,true,false));CHECK(state.frameCount==2);
#if defined(_WIN32)
 CHECK(redraws==1);
#endif
 // A successful generic release leaves local failure sticky until the actual
 // cleanup boundary, including reuse of the same State and numeric HGLRC.
 core::env::value="dxgi";State success;success.nativePresentSyncInterval=1;
 mock::badDescriptor=true;
 CHECK(!native_present::initialize(success.nativePresent,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
 CHECK(success.nativePresent.failed&&!success.nativePresent.resource&&!native_present::admission_failed());
 mock::badDescriptor=false;
 CHECK(!native_present::initialize(success.nativePresent,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
 retire(&ctx,success,true);
 CHECK(!success.nativePresent.failed&&!success.nativePresent.resource&&success.nativePresentSyncInterval==0);
 CHECK(!success.nativePresent.selectionResolved&&!success.nativePresent.useDxgi&&!success.nativePresent.selectionContext&&!success.nativePresent.selectionDc);
 CHECK(!native_present::admission_failed());
 CHECK(contextDeletes==1&&dcReleases==1&&windowDestroys==1);
 CHECK(!success.ownsContext&&!success.ownsDc&&!success.ownsWindow&&!success.hglrc&&!success.hdc&&!success.hwnd);
 CHECK(native_present::initialize(success.nativePresent,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
 CHECK(native_present::release(success.nativePresent));
 // A failed release must preserve this context's owner and its sync interval,
 // and cleanup must never reset the process-wide failed-retirement admission.
 state.nativePresentSyncInterval=1;
 CHECK(native_present::initialize(state.nativePresent,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
 auto* retained=state.nativePresent.resource;const auto refs=mock::com_refs();
 mock::unregisterFails=true;const bool retired=native_present::release(state.nativePresent);
 CHECK(!retired&&state.nativePresent.failed&&native_present::admission_failed());
 retire(&ctx,state,retired);
 CHECK(ctx.init_failed&&!window.ownsNativeGlContext&&!window.ownsNativeDc);
 CHECK(window.lifecycle==core::BackendLifecycleState::failed);
 CHECK(contextDeletes==1&&dcReleases==1&&windowDestroys==1);
 CHECK(state.ownsContext&&state.ownsDc&&state.ownsWindow&&state.hglrc&&state.hdc&&state.hwnd);
 CHECK(state.nativePresent.resource==retained&&state.nativePresent.failed&&state.nativePresentSyncInterval==1);
 CHECK(state.nativePresent.selectionResolved&&state.nativePresent.useDxgi&&state.nativePresent.selectionContext==mock::context&&state.nativePresent.selectionDc==mock::dc);
 CHECK(mock::com_refs()==refs&&native_present::admission_failed());
 mock::unregisterFails=false;CHECK(native_present::release(state.nativePresent));
 CHECK(state.nativePresent.failed&&!state.nativePresent.resource&&native_present::admission_failed());
 retire(&ctx,state,true);
 CHECK(!state.nativePresent.failed&&!state.nativePresent.resource&&state.nativePresentSyncInterval==0);
 CHECK(native_present::admission_failed()&&mock::com_refs()==0);
 CHECK(!native_present::initialize(state.nativePresent,reinterpret_cast<void*>(1),mock::dc,mock::context,640,480));
 return 0;
}
'''
        report = ROOT / "Artifacts/Reports/opengl-native-production-oct30/gl-family-dispatch/hooks-cpu"
        report.mkdir(parents=True, exist_ok=True)
        source = report / "native_present_hooks_contract.cpp"
        source.write_text(code, encoding="utf-8")
        results = compile_and_run(self, source, report, windows_flags=("/WX",))
        (report / "result.json").write_text(json.dumps(dict(
            configurations=results,
            production={path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in paths},
            scope="Actual callback/completion/cleanup blocks with CPU ports; host HWND retirement remains separately audited."
        ), indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
