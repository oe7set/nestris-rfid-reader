#include "hw/serial_link.h"

namespace hw {

void SerialLink::begin(uint32_t baud) {
    Serial.setRxBufferSize(1024);
    Serial.begin(baud);
    buffer_.reserve(core::kMaxLine + 1);
}

void SerialLink::send(const std::string& line) {
    // One write per line, so lines never interleave.
    std::string out = line;
    out += '\n';
    Serial.write(reinterpret_cast<const uint8_t*>(out.data()), out.size());
}

bool SerialLink::poll(std::string& line, bool& tooLong) {
    tooLong = false;
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c < 0) break;
        if (c == '\n') {
            if (overflow_) {
                overflow_ = false;
                buffer_.clear();
                tooLong = true;
                return false;
            }
            line = buffer_;
            buffer_.clear();
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            return true;
        }
        if (overflow_) continue;
        if (buffer_.size() >= core::kMaxLine) {
            overflow_ = true;  // drop until the end of this line
            continue;
        }
        buffer_ += static_cast<char>(c);
    }
    return false;
}

}  // namespace hw
