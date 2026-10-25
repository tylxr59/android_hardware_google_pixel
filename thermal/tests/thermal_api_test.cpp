/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
#include <utility>

#include "Thermal.h"
#include "mock_thermal_helper.h"

namespace aidl::android::hardware::thermal::implementation {
using testing::Return;
using testing::ReturnRef;

class ThermalApiTest : public testing::Test {
  protected:
    void SetUp() override {
        ON_CALL(*helper, isInitializedOk()).WillByDefault(Return(true));
        ON_CALL(*helper, GetSensorInfoMap()).WillByDefault(ReturnRef(sensors));
        ON_CALL(*helper, GetCdevInfoMap()).WillByDefault(ReturnRef(devices));
        SensorInfo cpu{};
        cpu.type = TemperatureType::CPU;
        cpu.is_hidden = false;
        sensors.emplace("cpu", std::move(cpu));
    }

    std::unordered_map<std::string, SensorInfo> sensors;
    std::unordered_map<std::string, CdevInfo> devices;
    std::shared_ptr<testing::NiceMock<MockThermalHelper>> helper =
            std::make_shared<testing::NiceMock<MockThermalHelper>>();
    std::shared_ptr<Thermal> thermal = ndk::SharedRefBase::make<Thermal>(helper);
};

TEST_F(ThermalApiTest, MissingSkinThresholdsReturnEmptySuccess) {
    EXPECT_CALL(*helper, fillTemperatureThresholds).Times(0);
    std::vector<TemperatureThreshold> values(1);
    ASSERT_TRUE(thermal->getTemperatureThresholdsWithType(TemperatureType::SKIN, &values).isOk());
    EXPECT_TRUE(values.empty());
}

TEST_F(ThermalApiTest, MissingSkinTemperaturesReturnEmptySuccess) {
    EXPECT_CALL(*helper, fillCurrentTemperatures).Times(0);
    std::vector<Temperature> values(1);
    ASSERT_TRUE(thermal->getTemperaturesWithType(TemperatureType::SKIN, &values).isOk());
    EXPECT_TRUE(values.empty());
}

TEST_F(ThermalApiTest, HiddenSensorsDoNotMakeATypeAvailable) {
    SensorInfo skin{};
    skin.type = TemperatureType::SKIN;
    skin.is_hidden = true;
    sensors.emplace("hidden-skin", std::move(skin));
    EXPECT_CALL(*helper, fillTemperatureThresholds).Times(0);
    std::vector<TemperatureThreshold> values;
    EXPECT_TRUE(thermal->getTemperatureThresholdsWithType(TemperatureType::SKIN, &values).isOk());
    EXPECT_TRUE(values.empty());
}

TEST_F(ThermalApiTest, NoCoolingDevicesReturnEmptySuccess) {
    EXPECT_CALL(*helper, fillCurrentCoolingDevices).Times(0);
    std::vector<CoolingDevice> values(1);
    EXPECT_TRUE(thermal->getCoolingDevices(&values).isOk());
    EXPECT_TRUE(values.empty());
    values.resize(1);
    EXPECT_TRUE(thermal->getCoolingDevicesWithType(CoolingType::BATTERY, &values).isOk());
    EXPECT_TRUE(values.empty());
}

TEST_F(ThermalApiTest, EmptySensorConfigurationReturnsEmptySuccess) {
    sensors.clear();
    EXPECT_CALL(*helper, fillCurrentTemperatures).Times(0);
    EXPECT_CALL(*helper, fillTemperatureThresholds).Times(0);
    std::vector<Temperature> temperatures;
    std::vector<TemperatureThreshold> thresholds;
    EXPECT_TRUE(thermal->getTemperatures(&temperatures).isOk());
    EXPECT_TRUE(thermal->getTemperatureThresholds(&thresholds).isOk());
    EXPECT_TRUE(temperatures.empty());
    EXPECT_TRUE(thresholds.empty());
}

TEST_F(ThermalApiTest, ConfiguredTemperatureReadFailureStillReturnsError) {
    EXPECT_CALL(*helper, fillCurrentTemperatures).WillOnce(Return(false));
    std::vector<Temperature> values;
    auto status = thermal->getTemperaturesWithType(TemperatureType::CPU, &values);
    EXPECT_EQ(status.getExceptionCode(), EX_ILLEGAL_STATE);
}

TEST_F(ThermalApiTest, ConfiguredThresholdReadFailureStillReturnsError) {
    EXPECT_CALL(*helper, fillTemperatureThresholds).WillOnce(Return(false));
    std::vector<TemperatureThreshold> values;
    auto status = thermal->getTemperatureThresholdsWithType(TemperatureType::CPU, &values);
    EXPECT_EQ(status.getExceptionCode(), EX_ILLEGAL_STATE);
}

TEST_F(ThermalApiTest, ConfiguredCoolingReadFailureStillReturnsError) {
    CdevInfo device{};
    device.type = CoolingType::BATTERY;
    devices.emplace("battery", device);
    EXPECT_CALL(*helper, fillCurrentCoolingDevices).WillOnce(Return(false));
    std::vector<CoolingDevice> values;
    auto status = thermal->getCoolingDevices(&values);
    EXPECT_EQ(status.getExceptionCode(), EX_ILLEGAL_STATE);
}

TEST_F(ThermalApiTest, InitializationFailureStillReturnsErrorForMissingTypes) {
    ON_CALL(*helper, isInitializedOk()).WillByDefault(Return(false));
    EXPECT_CALL(*helper, GetSensorInfoMap()).Times(0);
    EXPECT_CALL(*helper, GetCdevInfoMap()).Times(0);
    std::vector<Temperature> temperatures;
    std::vector<TemperatureThreshold> thresholds;
    std::vector<CoolingDevice> cooling;
    EXPECT_EQ(thermal->getTemperaturesWithType(TemperatureType::SKIN, &temperatures)
                      .getExceptionCode(), EX_ILLEGAL_STATE);
    EXPECT_EQ(thermal->getTemperatureThresholdsWithType(TemperatureType::SKIN, &thresholds)
                      .getExceptionCode(), EX_ILLEGAL_STATE);
    EXPECT_EQ(thermal->getCoolingDevices(&cooling).getExceptionCode(), EX_ILLEGAL_STATE);
}

TEST_F(ThermalApiTest, ConfiguredSensorStillReturnsItsRealReading) {
    EXPECT_CALL(*helper, fillCurrentTemperatures)
            .WillOnce([](bool, bool, TemperatureType, std::vector<Temperature> *values) {
                Temperature reading{};
                reading.type = TemperatureType::CPU;
                reading.name = "cpu";
                reading.value = 37.5f;
                values->push_back(reading);
                return true;
            });
    std::vector<Temperature> values;
    ASSERT_TRUE(thermal->getTemperaturesWithType(TemperatureType::CPU, &values).isOk());
    ASSERT_EQ(values.size(), 1u);
    EXPECT_FLOAT_EQ(values.front().value, 37.5f);
    EXPECT_EQ(values.front().name, "cpu");
}
}  // namespace aidl::android::hardware::thermal::implementation
