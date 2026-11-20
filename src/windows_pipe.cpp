#include "windows_pipe.hpp"

#include <sddl.h>
#include <array>
#include <system_error>

namespace cyhub {
namespace {
constexpr wchar_t pipe_name[] = L"\\\\.\\pipe\\CyHub.Updater.v1";
constexpr DWORD timeout = 3000;
constexpr DWORD max_message = 4096;

struct Handle {
    HANDLE value = nullptr;
    ~Handle() {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
};

[[noreturn]] void fail(const char* operation, DWORD error = GetLastError()) {
    throw std::system_error(error, std::system_category(), operation);
}

DWORD service_pid() {
    const auto manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!manager)
        return 0;
    const auto service = OpenServiceW(manager, service_name, SERVICE_QUERY_STATUS);
    CloseServiceHandle(manager);
    if (!service)
        return 0;
    SERVICE_STATUS_PROCESS status{};
    DWORD needed = 0;
    const auto ok = QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
        reinterpret_cast<BYTE*>(&status), sizeof(status), &needed);
    CloseServiceHandle(service);
    return ok && status.dwCurrentState == SERVICE_RUNNING ? status.dwProcessId : 0;
}

template <typename Io>
bool transfer(HANDLE pipe, HANDLE stop, DWORD wait, DWORD& transferred, Io operation) {
    Handle event{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    if (!event.value)
        fail("Creating the service event");
    OVERLAPPED state{};
    state.hEvent = event.value;
    if (operation(state, transferred))
        return true;

    const auto error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED)
        return true;
    if (error != ERROR_IO_PENDING)
        return false;

    const HANDLE events[] = {event.value, stop};
    if (WaitForMultipleObjects(stop ? 2 : 1, events, FALSE, wait) == WAIT_OBJECT_0)
        return GetOverlappedResult(pipe, &state, &transferred, FALSE) != FALSE;

    CancelIoEx(pipe, &state);
    GetOverlappedResult(pipe, &state, &transferred, TRUE);
    return false;
}

bool write_message(HANDLE pipe, HANDLE stop, std::string_view message) {
    DWORD written = 0;
    return transfer(pipe, stop, timeout, written, [&](OVERLAPPED& operation, DWORD& count) {
        return WriteFile(pipe, message.data(), static_cast<DWORD>(message.size()), &count, &operation);
    }) && written == message.size();
}

std::string read_message(HANDLE pipe, HANDLE stop, DWORD capacity = max_message) {
    std::array<char, max_message> data{};
    DWORD received = 0;
    if (!transfer(pipe, stop, timeout, received, [&](OVERLAPPED& operation, DWORD& count) {
            return ReadFile(pipe, data.data(), capacity, &count, &operation);
        }))
        return {};
    return {data.data(), received};
}
}

bool updater_running() {
    return supports_packages() && service_pid() != 0;
}

void request_update(const Project& project, std::string_view tag) {
    const auto expected_pid = service_pid();
    if (!expected_pid)
        throw std::runtime_error("CyHub Updater is stopped or missing.");
    if (!WaitNamedPipeW(pipe_name, timeout))
        throw std::runtime_error("The service is temporarily unavailable.");
    Handle pipe{CreateFileW(pipe_name, FILE_GENERIC_READ | FILE_WRITE_DATA, 0, nullptr, OPEN_EXISTING,
                            FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr)};
    if (pipe.value == INVALID_HANDLE_VALUE)
        fail("Connecting to the service");
    ULONG pid = 0;
    if (!GetNamedPipeServerProcessId(pipe.value, &pid) || pid != expected_pid || pid != service_pid())
        throw std::runtime_error("Could not verify the service identity.");

    if (!write_message(pipe.value, nullptr, Json{{"project", project.id}, {"tag", tag}}.dump()))
        throw std::runtime_error("Could not send the request.");
    const auto response = read_message(pipe.value, nullptr);
    if (response.empty())
        throw std::runtime_error("The service did not respond.");
    write_message(pipe.value, nullptr, "1");
    const auto data = cymcp::parse_json(response, max_message);
    if (!data.value("accepted", false))
        throw std::runtime_error(data.value("detail", "The update was refused."));
}

void serve_updates(HANDLE stop, const std::function<Json(const Json&)>& handler,
                   const std::function<void()>& ready) {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;LS)(A;;0x12008B;;;IU)",
            SDDL_REVISION_1, &descriptor, nullptr))
        fail("Setting service permissions");
    const std::unique_ptr<void, decltype(&LocalFree)> security(descriptor, LocalFree);
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
    Handle pipe{CreateNamedPipeW(pipe_name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                                FILE_FLAG_FIRST_PIPE_INSTANCE,
                                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS,
                                1, max_message, max_message, 0, &attributes)};
    if (pipe.value == INVALID_HANDLE_VALUE)
        fail("Creating the service pipe");
    ready();
    while (WaitForSingleObject(stop, 0) == WAIT_TIMEOUT) {
        DWORD unused = 0;
        if (transfer(pipe.value, stop, INFINITE, unused, [&](OVERLAPPED& operation, DWORD&) {
                return ConnectNamedPipe(pipe.value, &operation);
            })) {
            const auto message = read_message(pipe.value, stop);
            if (!message.empty()) {
                Json response;
                try {
                    response = handler(cymcp::parse_json(message, max_message));
                } catch (const std::exception& error) {
                    response = {{"accepted", false}, {"detail", cymcp::redact_error_message(error.what())}};
                }
                if (write_message(pipe.value, stop, response.dump(-1, ' ', false, Json::error_handler_t::replace)))
                    read_message(pipe.value, stop, 1);
            }
        }
        DisconnectNamedPipe(pipe.value);
    }
}
}
