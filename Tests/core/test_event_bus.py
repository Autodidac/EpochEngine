"""Instantiate the production bus; no Engine, renderer, model or worker starts."""
from pathlib import Path
import json
import os
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]

SOURCE = r'''
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
import events.bus;
bool deny_allocations{};unsigned blocked{},callbacks{};
void* operator new(std::size_t count){if(deny_allocations){++blocked;throw std::bad_alloc{};}
 if(auto* value=std::malloc(count?count:1))return value;throw std::bad_alloc{};}
void* operator new[](std::size_t count){return ::operator new(count);}
void operator delete(void* value)noexcept{std::free(value);}void operator delete[](void* value)noexcept{std::free(value);}
void operator delete(void* value,std::size_t)noexcept{std::free(value);}void operator delete[](void* value,std::size_t)noexcept{std::free(value);}
struct Change {int amount{};int depth{};};struct Other {int amount{};};
using Bus=epochengine::events::bus;
void add(void* owner,const Change& event)noexcept{++callbacks;*static_cast<int*>(owner)+=event.amount;}
void twice(void* owner,const Change& event)noexcept{++callbacks;*static_cast<int*>(owner)+=event.amount*2;}
void other(void* owner,const Other& event)noexcept{++callbacks;*static_cast<int*>(owner)+=event.amount;}
struct Action {Bus* bus{};int calls{},secondary{};bool error{},registered{};Bus* source{};};
void remove_self(void* owner,const Change&)noexcept{
 auto& state=*static_cast<Action*>(owner);++state.calls;
 if(state.bus->unsubscribe<Change>(owner,&remove_self)!=1)state.error=true;
 if(state.bus->handler_count()!=1)state.error=true;
}
void remove_later(void* owner,const Change&)noexcept{
 auto& state=*static_cast<Action*>(owner);++state.calls;
 if(state.bus->unsubscribe<Change>(&state.secondary,&add)!=1)state.error=true;
}
void clear_all(void* owner,const Change&)noexcept{
 auto& state=*static_cast<Action*>(owner);++state.calls;state.bus->clear();
 if(state.bus->handler_count()!=0)state.error=true;
}
void nested(void* owner,const Change& event)noexcept{
 auto& state=*static_cast<Action*>(owner);++state.calls;
 if(event.depth==0)state.bus->emit(Change{event.amount,1});
}
void grow(void* owner,const Change&)noexcept{
 auto& state=*static_cast<Action*>(owner);++state.calls;
 if(!state.registered){state.registered=true;for(unsigned i=0;i<64;++i)state.bus->subscribe<Change>(&state.secondary,&add);}
}
void assign(void* owner,const Change&)noexcept{
 auto& state=*static_cast<Action*>(owner);++state.calls;*state.bus=*state.source;
}
#define CHECK(value) do{if(!(value))return __LINE__;}while(false)
int main(){
 {Bus bus;int a{},b{},c{};bus.subscribe<Change>(&a,&add);bus.subscribe<Change>(&b,&twice);bus.subscribe<Other>(&c,&other);
  CHECK(bus.handler_count()==3);deny_allocations=true;bus.emit(Change{3});bus.emit(Other{7});deny_allocations=false;
  CHECK(a==3&&b==6&&c==7&&blocked==0);
  auto copy=bus;CHECK(copy.unsubscribe<Change>(&a,&add)==1);copy.emit(Change{1});
  CHECK(a==3&&b==8&&bus.handler_count()==3&&copy.handler_count()==2);
  deny_allocations=true;CHECK(bus.unsubscribe<Change>(&b,&twice)==1);bus.emit(Change{1});bus.clear();deny_allocations=false;
  CHECK(a==4&&b==8&&bus.handler_count()==0&&blocked==0);}
 {Bus bus;int value{};bus.subscribe<const Change>(&value,&add);
  CHECK(bus.unsubscribe<Change>(&value,&add)==1);bus.subscribe<Change>(&value,nullptr);CHECK(bus.handler_count()==0);
  bus.subscribe<Change>(&value,&add);bus.subscribe<Change>(&value,&add);
  CHECK(bus.unsubscribe<Change>(&value,&add)==2&&bus.handler_count()==0);}
 {Bus bus;Action state{&bus};bus.subscribe<Change>(&state,&remove_self);bus.subscribe<Change>(&state.secondary,&add);
  deny_allocations=true;bus.emit(Change{2});bus.emit(Change{3});deny_allocations=false;
  CHECK(!state.error&&state.calls==1&&state.secondary==5&&bus.handler_count()==1&&blocked==0);}
 {Bus bus;Action state{&bus};bus.subscribe<Change>(&state,&remove_later);bus.subscribe<Change>(&state.secondary,&add);
  deny_allocations=true;bus.emit(Change{2});deny_allocations=false;
  CHECK(!state.error&&state.calls==1&&state.secondary==0&&bus.handler_count()==1&&blocked==0);}
 {Bus bus;Action state{&bus};bus.subscribe<Change>(&state,&clear_all);bus.subscribe<Change>(&state.secondary,&add);
  deny_allocations=true;bus.emit(Change{2});bus.emit(Change{3});deny_allocations=false;
  CHECK(!state.error&&state.calls==1&&state.secondary==0&&bus.handler_count()==0&&blocked==0);}
 {Bus bus;Action state{&bus};bus.subscribe<Change>(&state,&nested);bus.subscribe<Change>(&state.secondary,&add);
  deny_allocations=true;bus.emit(Change{2});deny_allocations=false;
  CHECK(state.calls==2&&state.secondary==4&&blocked==0);}
 {Bus bus;Action state{&bus};bus.subscribe<Change>(&state,&grow);bus.emit(Change{1});
  CHECK(state.calls==1&&state.secondary==0&&bus.handler_count()==65);
  deny_allocations=true;bus.emit(Change{1});deny_allocations=false;
  CHECK(state.calls==2&&state.secondary==64&&blocked==0);}
 {Bus bus;auto owner=std::make_unique<int>(0);auto* value=owner.get();bus.subscribe<Change>(value,&add);bus.emit(Change{1});
  CHECK(*value==1);CHECK(bus.unsubscribe<Change>(value,&add)==1);owner.reset();const auto before=callbacks;
  deny_allocations=true;bus.emit(Change{2});deny_allocations=false;CHECK(callbacks==before&&blocked==0);}
 {Bus bus,source;int value{};source.subscribe<Change>(&value,&add);Action state{&bus};state.source=&source;
  bus.subscribe<Change>(&state,&assign);bus.subscribe<Change>(&state.secondary,&add);bus.emit(Change{1});
  CHECK(state.calls==1&&state.secondary==0&&value==0&&bus.handler_count()==1);
  deny_allocations=true;bus.emit(Change{3});deny_allocations=false;CHECK(value==3&&blocked==0);}
 return 0;
}
'''


class EventBus(unittest.TestCase):
    def test_real_subscriptions_removal_reentrancy_and_no_emit_allocations(self):
        proof = ROOT / "build/validation/events-bus-repair-oct27"
        proof.mkdir(parents=True, exist_ok=True)
        (proof / "events.bus_contract.cpp").write_text(SOURCE, encoding="utf-8")
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(EventBusContract LANGUAGES CXX)
add_executable(event_contract events.bus_contract.cpp)
target_compile_features(event_contract PRIVATE cxx_std_23)
target_sources(event_contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES "{(ROOT/'Engine/modules/events.bus.ixx').as_posix()}")
target_include_directories(event_contract PRIVATE "{(ROOT/'Engine/include').as_posix()}")
if(MSVC)
 target_compile_options(event_contract PRIVATE /utf-8 /EHsc /permissive-)
endif()
enable_testing()
add_test(NAME actual_event_bus COMMAND event_contract)
set_tests_properties(actual_event_bus PROPERTIES TIMEOUT 15)
''', encoding="utf-8")
        configure = ["cmake", "-S", str(proof), "-B", str(proof / "cmake")]
        configure += ["-G", "Visual Studio 17 2022", "-A", "x64"] if os.name == "nt" else ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"]
        commands = [("configure", configure)]
        for config in (("Debug", "Release") if os.name == "nt" else ("Release",)):
            build = ["cmake", "--build", str(proof / "cmake"), "--config", config, "--parallel", "1"]
            if os.name == "nt":
                build += ["--", "/nr:false", "/p:UseMultiToolTask=false", "/p:CL_MPCount=1"]
            commands += [(config + "-build", build), (config + "-contract", ["ctest", "--test-dir",
                str(proof / "cmake"), "-C", config, "--output-on-failure"])]
        completed = []
        for label, command in commands:
            result = subprocess.run(command, cwd=ROOT, env=dict(os.environ, MSBUILDDISABLENODEREUSE="1"),
                capture_output=True, text=True, timeout=120)
            (proof / (label + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
            completed.append({"phase": label, "exit": result.returncode, "command": command})
            (proof / "result.json").write_text(json.dumps({"completed": completed,
                "scope": "owner-thread notifications; no transaction/history/thread-safety claim",
                "emit_allocation_denial": True}, indent=2) + "\n", encoding="utf-8")
            self.assertEqual(result.returncode, 0, (result.stdout + result.stderr)[-10000:])


if __name__ == "__main__":
    unittest.main()
