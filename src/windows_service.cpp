#include "packages.hpp"
#include "windows_pipe.hpp"

#include <condition_variable>
#include <mutex>
#include <system_error>

namespace {
using namespace cyhub;
SERVICE_STATUS_HANDLE status_handle = nullptr;
HANDLE stop_event = nullptr;
Cancellation cancellation;
std::mutex status_mutex;
DWORD service_state = SERVICE_START_PENDING;

void report(DWORD state, DWORD error = NO_ERROR) {
    std::lock_guard lock(status_mutex);
    if (service_state == SERVICE_STOP_PENDING && state == SERVICE_RUNNING)
        return;
    service_state = state;
    SERVICE_STATUS status{};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    status.dwWin32ExitCode = error;
    status.dwWaitHint = state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING ? 15000 : 0;
    SetServiceStatus(status_handle, &status);
}

void log_error(const std::exception& error) {
    const auto source = RegisterEventSourceW(nullptr, service_name);
    if (source) {
        const char* detail = error.what();
        ReportEventA(source, EVENTLOG_ERROR_TYPE, 0, 1, nullptr, 1, 0, &detail, nullptr);
        DeregisterEventSource(source);
    }
}

DWORD WINAPI control(DWORD command, DWORD, LPVOID, LPVOID) {
    if (command != SERVICE_CONTROL_STOP && command != SERVICE_CONTROL_SHUTDOWN)
        return ERROR_CALL_NOT_IMPLEMENTED;
    report(SERVICE_STOP_PENDING);
    cancellation.cancel();
    if (stop_event)
        SetEvent(stop_event);
    return NO_ERROR;
}

void run_service() {
    if (!supports_packages())
        throw std::runtime_error("CyHub Updater requires Windows x64.");
    const auto catalog = load_catalog(executable_directory() / "catalog.json");
    const auto root = managed_root();
    if (!fs::is_directory(root))
        throw std::runtime_error("Managed storage is missing. Run the CyHub installer.");

    for (const auto& project : catalog) {
        try {
            const auto job = update_job(root, project);
            if (job.phase == "queued" || job.phase == "checking" || job.phase == "downloading" || job.phase == "verifying")
                set_job(root, project, "error", "Mise à jour interrompue. Tu peux la relancer.");
        } catch (const std::exception& error) {
            log_error(error);
        }
    }

    struct Request {
        const Project* project;
        std::string tag;
    };
    std::mutex mutex;
    std::condition_variable_any wake;
    std::optional<Request> pending;
    bool busy = false;
    std::jthread worker([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            std::unique_lock lock(mutex);
            if (!wake.wait(lock, stop, [&] { return pending.has_value(); }) || stop.stop_requested())
                return;
            const auto job = std::move(*pending);
            pending.reset();
            lock.unlock();
            try {
                install_release(root, *job.project, job.tag, cancellation);
                set_job(root, *job.project, "done");
            } catch (const std::exception& error) {
                try {
                    set_job(root, *job.project, "error", cymcp::redact_error_message(error.what()));
                } catch (const std::exception& state_error) {
                    log_error(state_error);
                }
            }
            lock.lock();
            busy = false;
        }
    });

    try {
        serve_updates(stop_event, [&](const Json& input) -> Json {
            if (!input.is_object() || input.size() != 2)
                throw std::runtime_error("Demande invalide.");
            const auto* project = find_project(catalog, input.at("project").get<std::string>());
            const auto tag = input.at("tag").get<std::string>();
            if (!project || !project->windows_x64 || !newer_version("", tag))
                throw std::runtime_error("Aucun paquet correspondant dans le catalogue du service.");
            std::lock_guard lock(mutex);
            if (busy)
                throw std::runtime_error("Une installation est déjà en cours.");
            set_job(root, *project, "queued");
            pending = Request{project, tag};
            busy = true;
            wake.notify_one();
            return {{"accepted", true}};
        }, [] { report(SERVICE_RUNNING); });
    } catch (...) {
        cancellation.cancel();
        throw;
    }
    cancellation.cancel();
}

void WINAPI service_main(DWORD, LPWSTR*) {
    status_handle = RegisterServiceCtrlHandlerExW(service_name, control, nullptr);
    if (!status_handle)
        return;
    report(SERVICE_START_PENDING);
    stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DWORD error = NO_ERROR;
    try {
        if (!stop_event)
            throw std::system_error(GetLastError(), std::system_category());
        run_service();
    } catch (const std::exception& exception) {
        log_error(exception);
        error = ERROR_GEN_FAILURE;
    }
    if (stop_event)
        CloseHandle(stop_event);
    report(SERVICE_STOPPED, error);
}
}

int main() {
    const SERVICE_TABLE_ENTRYW entries[] = {
        {const_cast<wchar_t*>(cyhub::service_name), service_main}, {nullptr, nullptr}
    };
    return StartServiceCtrlDispatcherW(entries) ? 0 : static_cast<int>(GetLastError());
}
