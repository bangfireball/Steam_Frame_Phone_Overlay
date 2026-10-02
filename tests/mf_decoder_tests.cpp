#include "phonecast/platform/windows/MfH264Decoder.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

// Each access unit must be visible without a subsequent input or end-of-stream
// drain. Screen capture is sparse: the next frame might arrive seconds later.
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    std::vector<std::size_t> boundaries;
    for (std::size_t i = 0; i + 4 < bytes.size(); ++i) {
        if (bytes[i] == 0 && bytes[i + 1] == 0 && bytes[i + 2] == 0 &&
            bytes[i + 3] == 1 && (bytes[i + 4] & 0x1f) == 9) {
            boundaries.push_back(i); // Four-byte Annex B access-unit delimiter.
        }
    }
    if (boundaries.size() != 3 || boundaries.front() != 0) {
        std::cerr << "Invalid three-frame test fixture\n";
        return 1;
    }
    boundaries.push_back(bytes.size());
    phonecast::platform::windows::MfH264Decoder decoder;
    std::string error;
    if (!decoder.Start(64, 64, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    for (std::size_t i = 0; i + 1 < boundaries.size(); ++i) {
        const std::vector<std::uint8_t> unit(bytes.begin() + boundaries[i],
                                             bytes.begin() + boundaries[i + 1]);
        phonecast::core::VideoFrame frame;
        bool produced = false;
        if (!decoder.Submit(unit, i * 33333, frame, produced, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        if (!produced) {
            std::cerr << "Frame " << i << " buffered until future input (latency regression)\n";
            return 1;
        }
        if (frame.width != 64 || frame.height != 64 || frame.pixels.size() != 64 * 64 * 4 ||
            frame.pixels[0] < 240 || frame.pixels[1] > 10 || frame.pixels[2] > 10) {
            std::cerr << "Unexpected decoded red frame\n";
            return 1;
        }
    }
    std::cout << "IDR and sparse P-frames decoded without future input\n";
    return 0;
}
