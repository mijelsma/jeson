#include <jeson/jeson.hpp>

#include <string>

struct DeviceMessage {
  jeson::Field<"name", std::string> name;
  jeson::Field<"reading", int> reading = 0;
};

void setup() {
  Serial.begin(115200);

  DeviceMessage message;
  const auto errors = jeson::parse(R"({"name":"sensor-01","reading":42})", message);
  if (!errors) {
    Serial.println(errors.toString().c_str());
    return;
  }

  Serial.println(jeson::toJson(message).c_str());
}

void loop() {}

