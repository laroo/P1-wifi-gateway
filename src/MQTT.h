/*
 * Copyright (c) 2022 Ronald Leenes
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * @file MQTT.ino
 * @author Ronald Leenes
 * @date 28.12.2022
 *
 * @brief This file contains the MQTT functions
 *
 * @see http://esp8266thingies.nl
 */

/* MQTT part based on
 https://github.com/daniel-jong/esp8266_p1meter/blob/master/esp8266_p1meter/esp8266_p1meter.ino

*/

// **********************************
// * MQTT                           *
// **********************************

// void doMQTT(){
//   mqtt_connect();
//   if (MqttConnected) MQTT_reporter();
// }

// void mqtt_connect(){
//   if (!mqtt_client.connected()) {
//      MqttConnected = false;
//      Log.verboseln("Reconnecting to mqtt broker …");
//      mqtt_reconnect();
//   } else MqttConnected = true;
// }

// void mqtt_reconnect(){
//   if (millis() > nextMQTTreconnectAttempt) {
//     Log.verboseln("Attempting MQTT connection...");
//     statusMsg = "Attempting MQTT connection...";
//     // Create a random client ID
//     String clientId = "P1 Smart Meter – ";
//     clientId += String(random(0xffff), HEX);
//     // Attempt to connect
//     if (mqtt_client.connect(HOSTNAME, config_data.mqttUser,
//     config_data.mqttPass)){
//       Log.verboseln("   connected to broker");
//       statusMsg += "   connected to broker";
//       // Once connected, publish an announcement...
//       mqtt_client.publish("outTopic", "p1 gateway running");
//       // ... and resubscribe
//       mqtt_client.subscribe("inTopic");
//       MqttConnected = true;
//     } else {
//       Log.verbose("failed, rc=");
//       statusMsg += "failed, rc=";
//       Log.verbose(mqtt_client.state());
//       Log.verboseln(" trying again later (non blocking)");
//       statusMsg += " trying again later (non blocking)";
//       nextMQTTreconnectAttempt = millis() + 15000; // try again in 15 seconds
//       MqttConnected = false;
//     }
//   }
// }

void doMQTT() {
  bool response = mqtt_connect();
  if (response) {
    PHASE(23);
    MQTT_reporter();
  }
  mqtt_client.loop();
}

bool mqtt_connect() {
  mqtt_client.setCallback(callback);
  if (mqtt_client.connected())
    return true;
  if (millis() < nextMQTTreconnectAttempt)
    return false;

  Log.verboseln("Attempting MQTT connection");
  PHASE(21);
  boolean ret = mqtt_client.connect(HOSTNAME, config_data.mqttUser,
                                    config_data.mqttPass);
  PHASE(22);
  if (ret) {
    Log.verboseln("Connected to MQTT");
    MqttConnected = true;
    mqtt_client.publish("outTopic", "P1 gateway");
    mqtt_client.subscribe("inTopic");
    PHASE(24);
    return true;
  }
  MqttConnected = false;
  nextMQTTreconnectAttempt = millis() + 30000; // non blocking retry in 30s
  return false;
}

void callback(char *topic, byte *payload, unsigned int length) {
  Log.verbose("Message arrived [");
  Log.verbose(topic);
  Log.verbose("] ");
  for (unsigned int i = 0; i < length; i++) {
    Log.verbose("%s", (char)payload[i]);
  }
  Log.verboseln("");
}

// * Send a message to a broker topic
void send_mqtt_message(const char *topic, char *payload) {
  if (payload[0] == 0)
    return; // nothing to report
  Log.verbose("MQTT Outgoing on > ");
  Log.verbose(topic);
  Log.verbose(" > ");
  Log.verboseln(payload);
  bool result = mqtt_client.publish(topic, payload, false);

  if (!result) {
    Log.verbose("MQTT publish to topic: ");
    Log.verbose(topic);
    Log.verboseln(" failed.");
  }
}

void send_metric(const String &name, float metric) {
  char value[20];
  dtostrf(metric, 3, 3, value);

  mtopic = String(config_data.mqttTopic) + "/" + name;
  send_mqtt_message(mtopic.c_str(), value);
}

void send_metric(const String &name, double metric) {
  char value[20];
  dtostrf(metric, 3, 3, value);

  mtopic = String(config_data.mqttTopic) + "/" + name;
  send_mqtt_message(mtopic.c_str(), value);
}

void send_metric(const String &name, uint32_t metric) {
  char value[12];
  sprintf(value, "%u", metric);
  mtopic = String(config_data.mqttTopic) + "/" + name;
  send_mqtt_message(mtopic.c_str(), value);
}

void send_metric(const String &name, const char *metric) {
  mtopic = String(config_data.mqttTopic) + "/" + name;
  send_mqtt_message(mtopic.c_str(), (char *)metric);
}

void send_metric(const String &name, const String &metric) {
  send_metric(name, metric.c_str());
}

void send_metric(const String &name, FixedValue metric) {
  send_metric(name, String(metric.val()));
}

void MQTT_reporter() {
#ifdef NEDERLANDS
  Log.verbose("MQTT reporter: ");
  Log.verboseln("%F", dsmrData.power_returned.val());
  if (mqtt_dsmr) {
    send_metric("equipmentID", dsmrData.equipment_id);

    send_metric("reading/electricity_delivered_1", dsmrData.energy_delivered_tariff1);
    send_metric("reading/electricity_delivered_2", dsmrData.energy_delivered_tariff2);
    send_metric("reading/electricity_returned_1", dsmrData.energy_returned_tariff1);
    send_metric("reading/electricity_returned_2", dsmrData.energy_returned_tariff2);
    send_metric("reading/electricity_currently_delivered", dsmrData.power_delivered);
    send_metric("reading/electricity_currently_returned", dsmrData.power_returned);

    send_metric("reading/phase_currently_delivered_l1", dsmrData.power_delivered_l1);
    send_metric("reading/phase_currently_delivered_l2", dsmrData.power_delivered_l2);
    send_metric("reading/phase_currently_delivered_l3", dsmrData.power_delivered_l3);
    send_metric("reading/phase_currently_returned_l1", dsmrData.power_returned_l1);
    send_metric("reading/phase_currently_returned_l2", dsmrData.power_returned_l2);
    send_metric("reading/phase_currently_returned_l3", dsmrData.power_returned_l3);
    send_metric("reading/phase_voltage_l1", dsmrData.voltage_l1);
    send_metric("reading/phase_voltage_l2", dsmrData.voltage_l2);
    send_metric("reading/phase_voltage_l3", dsmrData.voltage_l3);

    send_metric("consumption/gas/delivered", dsmrData.gas_delivered);

    send_metric("meter-stats/actual_tarif_group", dsmrData.electricity_tariff);
    send_metric("meter-stats/power_failure_count", dsmrData.electricity_failures);
    send_metric("meter-stats/long_power_failure_count", dsmrData.electricity_long_failures);
    send_metric("meter-stats/short_power_drops", dsmrData.electricity_sags_l1);
    send_metric("meter-stats/short_power_peaks", dsmrData.electricity_swells_l1);

    send_metric("day-consumption/electricity1",
                (dsmrData.energy_delivered_tariff1.val() - atof(log_data.dayE1)));
    send_metric("day-consumption/electricity2",
                (dsmrData.energy_delivered_tariff2.val() - atof(log_data.dayE2)));
    send_metric("day-consumption/electricity1_returned",
                (dsmrData.energy_returned_tariff1.val() - atof(log_data.dayR1)));
    send_metric("day-consumption/electricity2_returned",
                (dsmrData.energy_returned_tariff2.val() - atof(log_data.dayR2)));

    send_metric("day-consumption/electricity_merged",
                ((dsmrData.energy_delivered_tariff1.val() - atof(log_data.dayE1)) +
                 (dsmrData.energy_delivered_tariff2.val() - atof(log_data.dayE2))));
    send_metric("day-consumption/electricity_returned_merged",
                ((dsmrData.energy_returned_tariff1.val() - atof(log_data.dayR1)) +
                 (dsmrData.energy_returned_tariff2.val() - atof(log_data.dayR2))));
    send_metric("day-consumption/gas",
                (dsmrData.gas_delivered.toFloat() - atof(log_data.dayG)));

    send_metric("current-month/electricity1",
                (dsmrData.energy_delivered_tariff1.val() - atof(log_data.monthE1)));
    send_metric("current-month/electricity2",
                (dsmrData.energy_delivered_tariff2.val() - atof(log_data.monthE2)));
    send_metric("current-month/electricity1_returned",
                (dsmrData.energy_returned_tariff1.val() - atof(log_data.monthR1)));
    send_metric("current-month/electricity2_returned",
                (dsmrData.energy_returned_tariff2.val() - atof(log_data.monthR2)));

    send_metric("current-month/electricity_merged",
                ((dsmrData.energy_delivered_tariff1.val() - atof(log_data.monthE1)) +
                 (dsmrData.energy_delivered_tariff2.val() - atof(log_data.monthE2))));
    send_metric("current-month/electricity_returned_merged",
                ((dsmrData.energy_returned_tariff1.val() - atof(log_data.monthR1)) +
                 (dsmrData.energy_returned_tariff2.val() - atof(log_data.monthR2))));
    send_metric("current-month/gas",
                (dsmrData.gas_delivered.toFloat() - atof(log_data.monthG)));
  } else {
    send_metric("equipmentID", dsmrData.equipment_id);

    send_metric("consumption_low_tarif", dsmrData.energy_delivered_tariff1);
    send_metric("consumption_high_tarif", dsmrData.energy_delivered_tariff2);
    send_metric("returndelivery_low_tarif", dsmrData.energy_returned_tariff1);
    send_metric("returndelivery_high_tarif", dsmrData.energy_returned_tariff2);
    send_metric("actual_consumption", dsmrData.power_delivered);
    send_metric("actual_returndelivery", dsmrData.power_returned);

    send_metric("l1_instant_power_usage", dsmrData.power_delivered_l1);
    send_metric("l2_instant_power_usage", dsmrData.power_delivered_l2);
    send_metric("l3_instant_power_usage", dsmrData.power_delivered_l3);
    send_metric("l1_instant_power_current", dsmrData.current_l1);
    send_metric("l2_instant_power_current", dsmrData.current_l2);
    send_metric("l3_instant_power_current", dsmrData.current_l3);
    send_metric("l1_voltage", dsmrData.voltage_l1);
    send_metric("l2_voltage", dsmrData.voltage_l2);
    send_metric("l3_voltage", dsmrData.voltage_l3);
    send_metric("l1_instant_power_return", dsmrData.power_returned_l1);
    send_metric("l2_instant_power_return", dsmrData.power_returned_l2);
    send_metric("l3_instant_power_return", dsmrData.power_returned_l3);

    send_metric("gas_meter_m3", dsmrData.gas_delivered);

    send_metric("actual_tarif_group", dsmrData.electricity_tariff);
    send_metric("short_power_outages", dsmrData.electricity_failures);
    send_metric("long_power_outages", dsmrData.electricity_long_failures);
    send_metric("short_power_drops", dsmrData.electricity_sags_l1);
    send_metric("short_power_peaks", dsmrData.electricity_swells_l1);
    LastReportinMillis = millis();
    LastMReport = timestampkaal();
    MqttDelivered = true;
    return;
  }
#endif
  LastMReport = timestampkaal();
  MqttDelivered = true;
  LastReportinMillis = millis();
}

void MQTT_Debug() {
  char outstr[16];

  dtostrf(millis(), 10, 0, outstr);
  send_mqtt_message("p1wifi/P1now", outstr);

  dtostrf(nextUpdateTime, 10, 0, outstr);
  send_mqtt_message("p1wifi/P1next", outstr);

  LastMReport.toCharArray(outstr, 10);
  send_mqtt_message("p1wifi/P1time_LastMReport", outstr);

  LastTReport.toCharArray(outstr, 10);
  send_mqtt_message("p1wifi/P1time_LastTReport", outstr);


  //     dtostrf(time_to_wake, 10,0, outstr);
  //     send_mqtt_message("p1wifi/P1time_toWake", outstr);

  //    dtostrf(ESP.getFreeHeap(), 10,0, outstr);
  //     send_mqtt_message("p1wifi/P1Heap", outstr);

  //     dtostrf(ESP.getHeapFragmentation(), 10,0, outstr);
  //     send_mqtt_message("p1wifi/P1HeapFrag", outstr);

  //     dtostrf(WiFi.RSSI(), 10,0, outstr);
  //     send_mqtt_message("p1wifi/RSSI", outstr);
}
