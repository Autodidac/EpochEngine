"""CPU-only classification of the existing isolation fixture's network evidence."""
from pathlib import Path
import json
import tempfile
import unittest

from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import definition

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / 'Engine/src/platform/platform.child_process_contract.cpp'


class IsolationNetworkEvidence(unittest.TestCase):
    def test_actual_catalog_report_with_simulated_registry(self):
        owner = OWNER.read_text(encoding='utf-8')
        report_function = definition(owner, '    [[nodiscard]] std::string network_catalog_access_report(')
        source = r'''
#include <cassert>
#include <string>
#include <string_view>
using DWORD=unsigned long; using LSTATUS=long; using HKEY=int;
constexpr HKEY HKEY_LOCAL_MACHINE=1;
constexpr DWORD KEY_QUERY_VALUE=1,KEY_ENUMERATE_SUB_KEYS=8;
constexpr LSTATUS ERROR_SUCCESS=0;
int opens{},queries{},closes{},queryOpens{},combinedOpens{}; LSTATUS openResult{},combinedResult{},queryResult{};
LSTATUS RegOpenKeyExW(HKEY hive,const wchar_t* path,DWORD options,DWORD access,HKEY* key){
 assert(hive==HKEY_LOCAL_MACHINE && options==0);
 assert(access==KEY_QUERY_VALUE || access==(KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS));
 if(access==KEY_QUERY_VALUE)++queryOpens;else ++combinedOpens;
 std::wstring_view p{path};
 assert(p==L"SYSTEM\\CurrentControlSet\\Services\\WinSock2\\Parameters"
 ||p==L"SYSTEM\\CurrentControlSet\\Services\\WinSock2\\Parameters\\Protocol_Catalog9"
 ||p==L"SYSTEM\\CurrentControlSet\\Services\\WinSock2\\Parameters\\NameSpace_Catalog5");
 ++opens; *key=42;return access==KEY_QUERY_VALUE?openResult:combinedResult;
}
LSTATUS RegQueryInfoKeyW(HKEY key,void*,void*,void*,DWORD* sub,void*,void*,DWORD* val,void*,void*,void*,void*){
 assert(key==42);++queries;*sub=7;*val=3;return queryResult;
}
LSTATUS RegCloseKey(HKEY key){assert(key==42);++closes;return 0;}
'''
        source += report_function
        source += r'''
int main(){
 auto report=network_catalog_access_report();
 assert(opens==6 && queries==6 && closes==6 && queryOpens==3 && combinedOpens==3);
 for(auto label:{"parameters","protocol","namespace"})for(auto access:{"query-only","query-enumerate"}){
  auto prefix=std::string{"catalog-"}+label+"-"+access;
  assert(report.find(prefix+"-subkey-count=7")!=std::string::npos);
  assert(report.find(prefix+"-value-count=3")!=std::string::npos);
 }
 opens=queries=closes=0;openResult=combinedResult=5;
 report=network_catalog_access_report();assert(opens==6 && queries==0 && closes==0);
 assert(report.find("-open-status=5")!=std::string::npos && report.find("count=")==std::string::npos);
 opens=queries=closes=0;openResult=combinedResult=0;queryResult=5;
 report=network_catalog_access_report();assert(opens==6 && queries==6 && closes==6);
 assert(report.find("-query-status=5")!=std::string::npos && report.find("count=")==std::string::npos);
 // Distinguish denial caused only by the additional enumeration permission.
 opens=queries=closes=0;queryResult=openResult=0;combinedResult=5;
 report=network_catalog_access_report();assert(opens==6 && queries==3 && closes==3);
 for(auto label:{"parameters","protocol","namespace"}){
  auto prefix=std::string{"catalog-"}+label;
  assert(report.find(prefix+"-query-only-open-status=0")!=std::string::npos);
  assert(report.find(prefix+"-query-only-subkey-count=7")!=std::string::npos);
  assert(report.find(prefix+"-query-enumerate-open-status=5")!=std::string::npos);
  assert(report.find(prefix+"-query-enumerate-query-status=")==std::string::npos);
 }
 opens=queries=closes=0;openResult=5;combinedResult=0;
 report=network_catalog_access_report();assert(opens==6 && queries==3 && closes==3);
 assert(report.find("catalog-protocol-query-only-open-status=5")!=std::string::npos);
 assert(report.find("catalog-protocol-query-enumerate-open-status=0")!=std::string::npos);
 assert(report.find("network-denial-check=passed")==std::string::npos);
}
'''
        parent = ROOT / 'Artifacts/Reports/isolation-network-evidence'
        parent.mkdir(parents=True, exist_ok=True)
        report = Path(tempfile.mkdtemp(prefix='catalog-cpu-', dir=parent))
        cpp = report / 'catalog.cpp'
        cpp.write_text(source, encoding='utf-8')
        commands = compile_and_run(self, cpp, report)
        (report / 'result.json').write_text(json.dumps({'commands': commands,
            'scope': 'Actual catalog report with simulated registry; no native registry or network.'}, indent=2), encoding='utf-8')

    def test_actual_result_classification_without_network_or_process_launch(self):
        owner = OWNER.read_text(encoding='utf-8')
        enum = owner.split('    enum class IsolationNetworkStage ', 1)[1].split(';', 1)[0]
        source = '#include <cassert>\n#include <string>\n#include <optional>\nconstexpr int WSAEACCES=10013;\n'
        source += 'enum class IsolationNetworkStage ' + enum + ';\n'
        source += definition(owner, '    [[nodiscard]] bool isolation_network_denied(')
        source += definition(owner, '    [[nodiscard]] std::string isolation_network_report(')
        source += r'''
using SOCKET=int;
struct WSADATA {};
constexpr int INVALID_SOCKET=-1,AF_INET=2,SOCK_STREAM=1,IPPROTO_TCP=6;
constexpr int MAKEWORD(int a,int b){return a|(b<<8);}
int startupFailure{},socketFailure{},closeFailure{},cleanupFailure{},lastError{};
int starts{},creates{},closes{},cleanups{};
int WSAStartup(int,WSADATA*){++starts;return startupFailure;}
SOCKET socket(int,int,int){++creates;lastError=socketFailure;return socketFailure?INVALID_SOCKET:42;}
int closesocket(SOCKET s){assert(s==42);++closes;lastError=closeFailure;return closeFailure?-1:0;}
int WSACleanup(){++cleanups;lastError=cleanupFailure;return cleanupFailure?-1:0;}
int WSAGetLastError(){return lastError;}
void reset(){startupFailure=socketFailure=closeFailure=cleanupFailure=lastError=0;starts=creates=closes=cleanups=0;}
'''
        control = owner.split('    struct IsolationNetworkControl final', 1)[1].split('};\n\n', 1)[0]
        source += 'struct IsolationNetworkControl final' + control + '};\n'
        source += definition(owner, '    [[nodiscard]] IsolationNetworkControl network_initialization_control(')
        source += definition(owner, '    [[nodiscard]] std::string isolation_network_control_report(')
        source += r'''
int main() {
    reset();startupFailure=10107;
    auto control=network_initialization_control();
    assert(control.startup_error==10107 && !control.socket_error && !control.cleanup_error);
    assert(starts==1 && creates==0 && closes==0 && cleanups==0);
    assert(isolation_network_control_report(control).find("ordinary-control-socket-error=not-exercised")!=std::string::npos);
    reset();socketFailure=10013;
    control=network_initialization_control();
    assert(control.socket_error==10013 && !control.close_error && control.cleanup_error==0);
    assert(creates==1 && closes==0 && cleanups==1);
    reset();control=network_initialization_control();
    assert(control.startup_error==0 && control.socket_error==0 && control.close_error==0 && control.cleanup_error==0);
    assert(starts==1 && creates==1 && closes==1 && cleanups==1);
    assert(isolation_network_control_report(control).find("ordinary-control-connection-attempted=0")!=std::string::npos);
    reset();closeFailure=10038;cleanupFailure=10091;
    control=network_initialization_control();
    assert(control.close_error==10038 && control.cleanup_error==10091 && cleanups==1);
    auto matched=isolation_network_control_report(control,"child-control");
    assert(matched.find("child-control-connection-attempted=0")!=std::string::npos);
    assert(matched.find("ordinary-control")==std::string::npos);
    using Stage = IsolationNetworkStage;
    for (const auto stage : {Stage::startup, Stage::socket_creation, Stage::nonblocking,
            Stage::connection, Stage::wait, Stage::status_query, Stage::completion}) {
        for (int error : {0, 10107, 10061, 10060, 10035}) {
            assert(!isolation_network_denied(stage,error));
            assert(isolation_network_report(stage,error).find("network-denial-check=not-proven")!=std::string::npos);
        }
        bool denial = stage==Stage::socket_creation || stage==Stage::connection || stage==Stage::completion;
        assert(isolation_network_denied(stage,WSAEACCES)==denial);
    }
    auto startup=isolation_network_report(Stage::startup,10107);
    assert(startup.find("network-stage=winsock-startup")!=std::string::npos);
    assert(startup.find("network-error=10107")!=std::string::npos);
    assert(startup.find("connection-denial-check=not-exercised")!=std::string::npos);
    auto socket=isolation_network_report(Stage::socket_creation,WSAEACCES);
    assert(socket.find("network-denial-check=passed")!=std::string::npos);
    assert(socket.find("connection-denial-check=not-exercised")!=std::string::npos);
    auto connection=isolation_network_report(Stage::completion,WSAEACCES);
    assert(connection.find("connection-denial-check=passed")!=std::string::npos);
    assert(isolation_network_report(Stage::wait,WSAEACCES).find("connection-denial-check=not-proven")!=std::string::npos);
}
'''
        parent = ROOT / 'Artifacts/Reports/isolation-network-evidence'
        parent.mkdir(parents=True, exist_ok=True)
        report = Path(tempfile.mkdtemp(prefix='classification-', dir=parent))
        cpp = report / 'classification.cpp'
        cpp.write_text(source, encoding='utf-8')
        commands = compile_and_run(self, cpp, report)
        (report / 'result.json').write_text(json.dumps({
            'scope': 'Actual classification/report and control flow, with simulated Winsock calls; no native networking, isolation or child application launch.',
            'commands': commands,
        }, indent=2), encoding='utf-8')

    def test_matched_controls_preserve_launch_inputs_and_no_connect(self):
        owner = OWNER.read_text(encoding='utf-8')
        child = definition(owner, '    [[nodiscard]] int run_network_initialization_child(')
        initialization = definition(owner, '    [[nodiscard]] IsolationNetworkControl network_initialization_control(')
        for forbidden in ('::connect(', '::bind(', '::listen(', '::send('):
            self.assertNotIn(forbidden, child + initialization)
        self.assertIn('network_initialization_control()', child)
        self.assertLess(child.index('network_initialization_control()'), child.index('network_catalog_access_report()'))
        self.assertIn('"child-control"', child)
        self.assertIn('? 0 : 159', child)
        self.assertIn('::OpenEventW(EVENT_MODIFY_STATE, FALSE, eventPath.c_str())', child)
        self.assertNotIn('::SetEvent(', child)
        controls = owner.split('        const auto initializationControl = ', 1)[1].split(
            '        const auto launch = child_process::launch_or_focus(request);', 1)[0]
        self.assertIn('auto control = request;', controls)
        self.assertIn('control.arguments = {"--epoch-network-initialization-child", eventName};', controls)
        self.assertIn('if (!restricted) control.isolation.reset();', controls)
        for field in ('executable', 'expected_executable', 'working_directory', 'environment'):
            self.assertNotIn('control.' + field + ' =', controls)
        self.assertIn('initializationControl(false) || !initializationControl(true)', controls)
        for required in ('child_process::wait(', 'child_process::release(', 'active_in_group(',
                         'restricted_token_verified', 'isolation_retired', 'file_dacl(code)',
                         'completed.process->exit_code == 159', 'char bytes[4096]'):
            self.assertIn(required, controls)
        self.assertNotIn('network-denial-check=passed', controls)
        self.assertIn('eventOpenReturned(ERROR_ACCESS_DENIED) || eventOpenReturned(ERROR_FILE_NOT_FOUND)', controls)
        self.assertIn('eventOpenReturned(ERROR_SUCCESS)', controls)
        self.assertIn('eventBoundaryPassed && eventUnchanged', controls)
        self.assertIn('return run_network_initialization_child(argv[2]);', owner)
        self.assertIn('"--epoch-network-initialization-pair-only"', owner)
        self.assertIn('return run_isolation_contract(std::filesystem::absolute(argv[0]), true);', owner)
        self.assertIn('if (initializationOnly)', controls)
        self.assertIn('return fixture.write("isolation.evidence.txt", controlEvidence', controls)
        self.assertIn('connection-attempted=0', controls)
        self.assertIn('network-denial-check=not-proven', controls)
        contract = definition(owner, '    [[nodiscard]] int run_isolation_contract(')
        self.assertLess(contract.index('if (!initializationOnly)'), contract.index('null-dacl-canary.txt'))
        self.assertLess(contract.index('if (initializationOnly)'),
                        contract.index('        const auto launch = child_process::launch_or_focus(request);'))


if __name__ == '__main__':
    unittest.main()
