#include "phonecast/platform/steamframe/StandaloneRuntime.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/stat.h>

int main() {
    namespace steamframe = phonecast::platform::steamframe;
    const auto root = std::filesystem::temp_directory_path() /
                      ("phonecast-standalone-test-" + std::to_string(std::rand()));
    std::filesystem::create_directories(root);
    setenv("XDG_RUNTIME_DIR", root.c_str(), 1);

    int failures = 0;
    const auto check = [&](bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    };

    std::string error;
    const auto credential = root / "config" / "pairing-code";
    std::string firstCode;
    check(steamframe::LoadOrCreatePairCode(credential, firstCode, error),
          "persistent pairing code is created");
    check(firstCode.size() == 6U, "pairing code has six digits");
    struct stat credentialStatus{};
    check(stat(credential.c_str(), &credentialStatus) == 0 &&
              (credentialStatus.st_mode & 0777) == 0600,
          "pairing credential is owner-only");
    std::string secondCode;
    check(steamframe::LoadOrCreatePairCode(credential, secondCode, error) &&
              secondCode == firstCode,
          "pairing code is stable across launches");

    steamframe::StandaloneRuntime primary;
    check(primary.Start(error) == steamframe::InstanceStartResult::Primary,
          "first receiver owns the per-user instance lock");
    steamframe::StandaloneRuntime secondary;
    check(secondary.Start(error) == steamframe::InstanceStartResult::ExistingSignaled,
          "second receiver signals the resident process");
    check(primary.TakeDashboardFocusRequest(),
          "resident process receives the dashboard focus request");
    check(!primary.TakeDashboardFocusRequest(),
          "focus request is consumed once");

    primary.Stop();
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    if (failures == 0) std::cout << "All standalone runtime tests passed.\n";
    return failures == 0 ? 0 : 1;
}
