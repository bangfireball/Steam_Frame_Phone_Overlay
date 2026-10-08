#pragma once
#include <string>
namespace phonecast::platform::support {
// Whitelist, not a best-effort credential regex: arbitrary log text is excluded.
std::string SafeDiagnosticLine(const std::string& line);
bool ExportDiagnostics(std::string& filename, std::string& error);
}
