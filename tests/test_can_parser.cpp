#include "car_terminal/can_parser.h"
#include "test_support.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <vector>

namespace {

car_terminal::RawCanFrame makeFrame(
    std::uint32_t identifier,
    car_terminal::CanFrameFormat format,
    std::initializer_list<std::uint8_t> bytes) {
    car_terminal::RawCanFrame frame;
    frame.identifier = identifier;
    frame.format = format;
    frame.dataLength = static_cast<std::uint8_t>(bytes.size());
    std::copy(bytes.begin(), bytes.end(), frame.data.begin());
    return frame;
}

car_terminal::CanSignalMapping makeMapping(
    car_terminal::VehicleSignal signal,
    std::size_t offset,
    std::size_t length) {
    return {signal,
            0x123,
            car_terminal::CanFrameFormat::Standard,
            offset,
            length,
            car_terminal::CanByteOrder::LittleEndian,
            car_terminal::CanValueType::Unsigned,
            1.0,
            0.0};
}

std::vector<car_terminal::CanSignalMapping> completeMappings() {
    auto speed = makeMapping(car_terminal::VehicleSignal::SpeedKmh, 0, 2);
    speed.scale = 0.01;
    auto temperature =
        makeMapping(car_terminal::VehicleSignal::TemperatureC, 2, 2);
    temperature.valueType = car_terminal::CanValueType::Signed;
    temperature.scale = 0.1;
    auto voltage = makeMapping(car_terminal::VehicleSignal::VoltageV, 4, 2);
    voltage.byteOrder = car_terminal::CanByteOrder::BigEndian;
    voltage.scale = 0.01;
    return {speed, temperature, voltage};
}

}  // namespace

void runCanParserTests(test_support::TestSuite& suite) {
    using namespace car_terminal;
    using namespace test_support;

    suite.run("CAN filter validates standard and extended identifiers", [] {
        validateCanFilter({0x7FF, 0x7FF, CanFrameFormat::Standard});
        validateCanFilter(
            {0x1FFFFFFF, 0x1FFFFFFF, CanFrameFormat::Extended});
        requireThrows<CanParseError>(
            [] {
                validateCanFilter(
                    {0x800, 0x7FF, CanFrameFormat::Standard});
            },
            "identifier");
        requireThrows<CanParseError>(
            [] {
                validateCanFilter(
                    {0x123, 0, CanFrameFormat::Standard});
            },
            "mask");
        requireThrows<CanParseError>(
            [] {
                validateCanFilter(
                    {0x123, 0x800, CanFrameFormat::Standard});
            },
            "mask");
    });

    suite.run("CAN little-endian unsigned scaling and offset", [] {
        auto mapping = makeMapping(VehicleSignal::SpeedKmh, 0, 2);
        mapping.scale = 0.1;
        mapping.valueOffset = -2.0;
        const auto frame = makeFrame(
            0x123, CanFrameFormat::Standard, {0x34, 0x12});
        requireNear(decodeCanSignal(frame, mapping), 464.0, 1e-9,
                    "little-endian scaled value");
    });

    suite.run("CAN big-endian signed scaling and offset", [] {
        auto mapping = makeMapping(VehicleSignal::TemperatureC, 0, 2);
        mapping.byteOrder = CanByteOrder::BigEndian;
        mapping.valueType = CanValueType::Signed;
        mapping.scale = 0.1;
        mapping.valueOffset = 40.0;
        const auto frame = makeFrame(
            0x123, CanFrameFormat::Standard, {0xFF, 0x9C});
        requireNear(decodeCanSignal(frame, mapping), 30.0, 1e-9,
                    "big-endian signed value");
    });

    suite.run("CAN extended mapping does not accept standard frame", [] {
        auto mapping = makeMapping(VehicleSignal::VoltageV, 0, 1);
        mapping.identifier = 0x18FF50E5;
        mapping.format = CanFrameFormat::Extended;
        const auto extended = makeFrame(
            0x18FF50E5, CanFrameFormat::Extended, {0x7F});
        requireNear(decodeCanSignal(extended, mapping), 127.0, 1e-9,
                    "extended frame value");
        const auto standard =
            makeFrame(0x6E5, CanFrameFormat::Standard, {0x7F});
        requireThrows<CanParseError>(
            [&] { static_cast<void>(decodeCanSignal(standard, mapping)); },
            "does not match");
    });

    suite.run("CAN DLC and payload bounds are enforced", [] {
        auto mapping = makeMapping(VehicleSignal::SpeedKmh, 6, 2);
        const auto full = makeFrame(0x123, CanFrameFormat::Standard,
                                    {0, 0, 0, 0, 0, 0, 1, 2});
        requireNear(decodeCanSignal(full, mapping), 513.0, 1e-9,
                    "mapping may end at byte eight");
        const auto shortFrame = makeFrame(
            0x123, CanFrameFormat::Standard, {0, 0, 0, 0, 0, 0, 1});
        requireThrows<CanParseError>(
            [&] {
                static_cast<void>(decodeCanSignal(shortFrame, mapping));
            },
            "shorter");
        auto invalidFrame = full;
        invalidFrame.dataLength = 9;
        requireThrows<CanParseError>(
            [&] {
                static_cast<void>(decodeCanSignal(invalidFrame, mapping));
            },
            "exceeds 8");
    });

    suite.run("invalid CAN mappings are rejected", [] {
        auto zeroLength = makeMapping(VehicleSignal::SpeedKmh, 0, 0);
        requireThrows<CanParseError>(
            [&] { validateCanSignalMapping(zeroLength); }, "byte length");
        auto overflow = makeMapping(VehicleSignal::SpeedKmh, 7, 2);
        requireThrows<CanParseError>(
            [&] { validateCanSignalMapping(overflow); }, "exceeds");
        auto zeroScale = makeMapping(VehicleSignal::SpeedKmh, 0, 1);
        zeroScale.scale = 0.0;
        requireThrows<CanParseError>(
            [&] { validateCanSignalMapping(zeroScale); }, "scale");
        auto invalidOffset = makeMapping(VehicleSignal::SpeedKmh, 0, 1);
        invalidOffset.valueOffset =
            std::numeric_limits<double>::infinity();
        requireThrows<CanParseError>(
            [&] { validateCanSignalMapping(invalidOffset); }, "offset");
    });

    suite.run("CAN parser rejects duplicate and incomplete mappings", [] {
        auto mappings = completeMappings();
        mappings[1].signal = VehicleSignal::SpeedKmh;
        requireThrows<CanParseError>(
            [&] { CanVehicleDataParser parser(mappings); }, "Duplicate");
        mappings = completeMappings();
        mappings.pop_back();
        requireThrows<CanParseError>(
            [&] { CanVehicleDataParser parser(mappings); }, "must define");
    });

    suite.run("CAN parser builds one sample from a complete frame", [] {
        CanVehicleDataParser parser(completeMappings());
        const auto frame = makeFrame(0x123, CanFrameFormat::Standard,
                                     {0xD2, 0x04, 0xFA, 0x00,
                                      0x05, 0x00});
        const auto data = parser.consume(frame);
        require(data.has_value(), "complete frame should produce data");
        requireEqual(data->sampleNumber, std::size_t{1}, "sample number");
        requireNear(data->speedKmh, 12.34, 1e-9, "decoded speed");
        requireNear(data->temperatureC, 25.0, 1e-9,
                    "decoded temperature");
        requireNear(data->voltageV, 12.8, 1e-9, "decoded voltage");
    });

    suite.run("CAN parser waits for fresh values from multiple frames", [] {
        auto mappings = completeMappings();
        mappings[0].identifier = 0x100;
        mappings[1].identifier = 0x101;
        mappings[2].identifier = 0x102;
        CanVehicleDataParser parser(mappings);
        require(!parser.consume(makeFrame(
                     0x100, CanFrameFormat::Standard, {0x64, 0x00}))
                     .has_value(),
                "speed alone is incomplete");
        require(!parser.consume(makeFrame(
                     0x101, CanFrameFormat::Standard,
                     {0, 0, 0xFA, 0x00}))
                     .has_value(),
                "speed and temperature are incomplete");
        require(parser.consume(makeFrame(
                    0x102, CanFrameFormat::Standard,
                    {0, 0, 0, 0, 0x05, 0x00}))
                    .has_value(),
                "all three fresh values should produce a sample");
        require(!parser.consume(makeFrame(
                     0x100, CanFrameFormat::Standard, {0x65, 0x00}))
                     .has_value(),
                "a new cycle requires all mappings to refresh");
    });

    suite.run("CAN parser handles error and remote frames explicitly", [] {
        CanVehicleDataParser parser(completeMappings());
        auto errorFrame = makeFrame(0x1, CanFrameFormat::Standard, {});
        errorFrame.errorFrame = true;
        requireThrows<CanParseError>(
            [&] { static_cast<void>(parser.consume(errorFrame)); },
            "error frame");
        auto remoteFrame = makeFrame(0x123, CanFrameFormat::Standard, {});
        remoteFrame.remoteTransmissionRequest = true;
        requireThrows<CanParseError>(
            [&] { static_cast<void>(parser.consume(remoteFrame)); },
            "remote frame");
    });

    suite.run("CAN scaled result overflow is rejected", [] {
        auto mapping = makeMapping(VehicleSignal::SpeedKmh, 0, 1);
        mapping.scale = std::numeric_limits<double>::max();
        const auto frame =
            makeFrame(0x123, CanFrameFormat::Standard, {0x02});
        requireThrows<CanParseError>(
            [&] { static_cast<void>(decodeCanSignal(frame, mapping)); },
            "finite double");
    });
}
