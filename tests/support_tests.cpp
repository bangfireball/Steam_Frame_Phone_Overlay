#include "phonecast/platform/network/LanDiscovery.h"
#include "phonecast/platform/support/SupportDiagnostics.h"
#include "phonecast/vr/overlay/SettingsMenuController.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
void CheckAt(bool value, int line) { if (!value) throw std::runtime_error("support test failed at line " + std::to_string(line)); }
#define Check(value) CheckAt((value), __LINE__)
void Env(const char* key, const std::string& value) {
#ifdef _WIN32
    _putenv_s(key, value.c_str());
#else
    setenv(key, value.c_str(), 1);
#endif
}
int main() {
    try {
        using namespace phonecast::platform;
        const std::string nonce = "0123456789abcdef0123456789abcdef";
        const auto query = "PCVR_DISCOVER 1 " + nonce;
        Check(network::DiscoveryReply(query,49321,"Frame","0.9.0") == "PCVR_RECEIVER 1 " + nonce + " 49321 0.9.0 Frame");
        Check(network::DiscoveryReply(query + "x",49321,"Frame","0.9.0").empty());
        Check(network::DiscoveryReply("PCVR_DISCOVER 1 " + std::string(32,'z'),49321,"Frame","0.9.0").empty());
        Check(network::DiscoveryReply(query,0,"Frame","0.9.0").empty());
        Check(network::DiscoveryReply(query,49321,"Frame\nsecret","0.9.0").find('\n') == std::string::npos);
        Check(support::SafeDiagnosticLine("[network] Rejected a client with an invalid pairing code").size() > 0);
        Check(support::SafeDiagnosticLine("[network] code=123456").empty());
        Check(support::SafeDiagnosticLine("[notification] private text").empty());
        Check(support::SafeDiagnosticLine("[diagnostics] connected=yes rx-fps=30 dropped=0").size() > 0);
        Check(support::SafeDiagnosticLine("[diagnostics] connected=yes pair-code=123456").empty());
        Check(support::SafeDiagnosticLine("[diagnostics] connected=yes working-set-mb=105.2 audio-opens=1 vr-dropped=0").size() > 0);
        Check(support::SafeDiagnosticLine("[diagnostics] audio-secret=123456").empty());
        Check(support::SafeDiagnosticLine("[diagnostics] rx-fps=private").empty());
        phonecast::vr::SettingsMenuController menu;
        using phonecast::vr::SettingsMenuCommand;
        menu.Open({});
        for (int i=0;i<10;++i) menu.Handle(SettingsMenuCommand::NextItem);
        menu.Handle(SettingsMenuCommand::Activate);
        Check(menu.View().title == "ABOUT PHONECAST");
        Check(menu.View().values[0] == PHONECAST_VERSION);
        menu.Handle(SettingsMenuCommand::NextItem); menu.Handle(SettingsMenuCommand::NextItem);
        Check(menu.Handle(SettingsMenuCommand::Activate) == phonecast::vr::SettingsMenuResult::None);
        Check(menu.View().values[2] == "CONFIRM EXPORT");
        Check(menu.Handle(SettingsMenuCommand::Activate) == phonecast::vr::SettingsMenuResult::ExportRequested);
        Check(menu.Handle(SettingsMenuCommand::Activate) == phonecast::vr::SettingsMenuResult::None);
        menu.SetExportStatus("SAVED TO DOWNLOADS");
        menu.Handle(SettingsMenuCommand::Activate);
        menu.Handle(SettingsMenuCommand::NextItem);
        menu.Handle(SettingsMenuCommand::PreviousItem);
        Check(menu.Handle(SettingsMenuCommand::Activate) == phonecast::vr::SettingsMenuResult::None);

        const auto root = std::filesystem::temp_directory_path() / ("phonecast-support-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root / "state/phonecast-vr");
        std::filesystem::create_directories(root / "config");
#ifdef _WIN32
        Env("USERPROFILE", root.string()); Env("LOCALAPPDATA", (root / "state").string());
        std::filesystem::create_directories(root / "state/PhoneCastVR");
        auto logPath = root / "state/PhoneCastVR/receiver.log";
#else
        Env("HOME", root.string()); Env("XDG_STATE_HOME", (root / "state").string()); Env("XDG_CONFIG_HOME", (root / "config").string());
        auto logPath = root / "state/phonecast-vr/receiver.log";
#endif
        { std::ofstream log(logPath); log << "private 123456\n[diagnostics] connected=yes rx-fps=30 dropped=0\n[network] Rejected a client with an invalid pairing code\n"; }
        std::string filename,error;
        Check(support::ExportDiagnostics(filename,error));
        Check(filename.rfind("phonecast-headset-", 0) == 0);
        std::ifstream exported(root / "Downloads" / filename);
        std::string report((std::istreambuf_iterator<char>(exported)), {});
        Check(report.find("123456") == std::string::npos);
        Check(report.find("rx-fps=30") != std::string::npos);
        Check(report.find("invalid pairing code") != std::string::npos);
        exported.close();
        { std::ofstream log(logPath); for (int i=0;i<12000;++i) log << "[diagnostics] connected=yes rx-fps=30 working-set-mb=105 dropped=0\n"; }
        Check(support::ExportDiagnostics(filename,error));
        Check(std::filesystem::file_size(root / "Downloads" / filename) < 70000);
#ifndef _WIN32
        { std::ofstream dirs(root / "config/user-dirs.dirs"); dirs << "XDG_DOWNLOAD_DIR=\"$HOME/LocalizedDownloads\"\n"; }
        Check(support::ExportDiagnostics(filename,error));
        Check(std::filesystem::exists(root / "LocalizedDownloads" / filename));
        std::filesystem::remove(root / "config/user-dirs.dirs");
#endif
        std::filesystem::remove_all(root / "Downloads");
        { std::ofstream blocked(root / "Downloads"); blocked << "not a directory"; }
        Check(!support::ExportDiagnostics(filename,error));
        Check(!error.empty());
        std::filesystem::remove_all(root);

        network::LanDiscovery responder; Check(responder.Start(49321));
        auto socket = ::socket(AF_INET,SOCK_DGRAM,0);
#ifdef _WIN32
        DWORD timeout = 2000; setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
#else
        timeval timeout{2,0}; setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
#endif
        sockaddr_in address{}; address.sin_family=AF_INET; address.sin_port=htons(49322); address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        sendto(socket,query.data(),static_cast<int>(query.size()),0,reinterpret_cast<sockaddr*>(&address),sizeof(address));
        char reply[256]{};
        const auto received=recv(socket,reply,sizeof(reply),0);
        Check(received>0);
        Check(std::string(reply,static_cast<std::size_t>(received)).rfind("PCVR_RECEIVER 1 " + nonce,0)==0);
#ifdef _WIN32
        closesocket(socket);
#else
        close(socket);
#endif
        responder.Stop(); Check(responder.Start(49321)); responder.Stop();
        std::cout << "Support/discovery tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return EXIT_FAILURE; }
}
