#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace car_terminal {

enum class CanFrameFormat {
    Standard,
    Extended,
};

struct RawCanFrame {
    std::uint32_t identifier{};
    CanFrameFormat format{CanFrameFormat::Standard};
    bool remoteTransmissionRequest{};
    bool errorFrame{};
    std::uint8_t dataLength{};
    std::array<std::uint8_t, 8> data{};
};

struct CanFilterRule {
    std::uint32_t identifier{};
    std::uint32_t mask{};
    CanFrameFormat format{CanFrameFormat::Standard};
};

enum class VehicleSignal {
    SpeedKmh,
    TemperatureC,
    VoltageV,
};

enum class CanByteOrder {
    LittleEndian,
    BigEndian,
};

enum class CanValueType {
    Unsigned,
    Signed,
};

struct CanSignalMapping {
    VehicleSignal signal{VehicleSignal::SpeedKmh};
    std::uint32_t identifier{};
    CanFrameFormat format{CanFrameFormat::Standard};
    std::size_t byteOffset{};
    std::size_t byteLength{};
    CanByteOrder byteOrder{CanByteOrder::LittleEndian};
    CanValueType valueType{CanValueType::Unsigned};
    double scale{1.0};
    double valueOffset{};
};

}  // namespace car_terminal
