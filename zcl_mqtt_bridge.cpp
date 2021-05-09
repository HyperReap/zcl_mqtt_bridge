#include "zcl_mqtt_bridge.h"

namespace esphome
{
  namespace zcl_mqtt
  {
    const char *TAG = "uart_CC2530";
    const char *MQTT_RECEIVE_TAG = "MQTT_uart_CC2530";
    const char *UART_RECEIVE_TAG = "CC2530 - received";
    const char *UART_WRITE_TAG = "CC2530 - write";
    const char *BOOT_COMMANDS_TAG = "BOOT Commands";
    const char *INIT_COMMANDS_TAG = "INIT Commands";
    const char *DEVICE_JOIN_TAG = "INIT Commands";
    const char *DEVICE_LEAVE_TAG = "INIT Commands";
    const unsigned CMD0_POS = 2;
    const unsigned CMD1_POS = 2;
    const unsigned DELAY = 500000;


    void ZclMqttBridge::on_message(const std::string &topic, const std::string &payload)
    {

      auto pos_2nd_backslash = topic.find_first_of('/', 14);
      pos_2nd_backslash + this->application_name.length();
      std::string dst = topic.substr(++pos_2nd_backslash, 4);

      uint8_t cmd = 0x0;
      if (payload == "OFF")
        cmd = 0x0;
      else if (payload == "ON")
        cmd = 0x1;
      else if (payload == "TOGGLE")
        cmd = 0x2;
      ZCLHelper::ClusterIds cluster_id = ZCLHelper::ClusterIds::on_off_cluster;
      bool isOk = send_AF_DATA_REQUEST(dst, cluster_id, this->transaction_number, cmd);
    }

    void ZclMqttBridge::on_json_message(const std::string &topic, JsonObject &payload)
    {
      ESP_LOGD(MQTT_RECEIVE_TAG, "Got to MQTT message: JSON");
      auto pos_2nd_backslash = topic.find_first_of('/', 14);
      pos_2nd_backslash + this->application_name.length();
      std::string dst = topic.substr(++pos_2nd_backslash, 4);

      ZCLHelper::ClusterIds cluster_id;
      uint8_t cmd = 0x0;
      uint8_t brightness = 0x0;
      int clr = 0x0;

      std::string js = "";

      if (payload["state"] == "OFF")
        cmd = 0x0;
      else if (payload["state"] == "ON")
        cmd = 0x1;
      else if (payload["state"] == "TOGGLE")
        cmd = 0x2;

      brightness = payload["brightness"];
      clr = payload["color_temp"];

      cluster_id = ZCLHelper::ClusterIds::on_off_cluster;
      if (brightness > 0)
        cluster_id = ZCLHelper::ClusterIds::level_control;
      if (clr > 0)
        cluster_id = ZCLHelper::ClusterIds::color_control;

      send_AF_DATA_REQUEST(dst, cluster_id, this->transaction_number, cmd, brightness, clr);
    }

    void ZclMqttBridge::receive()
    {
      uint8_t byte;
      std::vector<uint8_t> response;
      while (this->available())
      {
        byte = this->read();
        response.push_back(byte);
      }

      if (response.empty())
        return;

      string resp = "";
      for (auto i : response)
        resp.append(ZCLHelper::n2hexstr(i));

      ESP_LOGD(UART_RECEIVE_TAG, "received bytes : 0x%s", resp.c_str());

      if (check_AF_INCOMING(response)) //most common incoming messages so its first for optimalization
        return; 
      if (check_join_device(response))
        return;
      if (check_leave_device(response))
        return;
    }

    void ZclMqttBridge::boot()
    {
      std::vector<uint8_t> response;

      ZNPCommand bdbTCRequireKeyExchange({0xFE, 0x01, 0x2F, 0x09, 0x00, 0x27}, 6, {0xfe, 0x01, 0x6f, 0x09, 0x00, 0x67}, 6);

      ESP_LOGD(BOOT_COMMANDS_TAG, "BDB NOT-RequireTCKeyExchange");
      send_cmd_and_wait_for_response(bdbTCRequireKeyExchange, response);

      ESP_LOGD(BOOT_COMMANDS_TAG, "ZDO startUpFromApp");
      ZNPCommand startUpFromApp({0xFE, 0x01, 0x25, 0x40, 0x00, 0x64}, 6, {0xfe, 0x01, 0x65, 0x40, 0x00, 0x24}, 6);
      send_cmd_and_wait_for_response(startUpFromApp, response);

      ESP_LOGD(BOOT_COMMANDS_TAG, "Not permit Join");
      ZNPCommand notPermitJoin({0xFE, 0x03, 0x26, 0x08, 0xFC, 0xFF, 0x00, 0x2E}, 8, {0xFE, 0x01, 0x66, 0x08, 0x00, 0x6F}, 6);
      send_cmd_and_wait_for_response(notPermitJoin, response);

      std::vector<Endpoint> endpoints;
      endpoints.push_back(Endpoint(1, 0x104));
      endpoints.push_back(Endpoint(2, 0x101));
      endpoints.push_back(Endpoint(3, 0x105));
      endpoints.push_back(Endpoint(4, 0x107));
      endpoints.push_back(Endpoint(5, 0x108));
      endpoints.push_back(Endpoint(6, 0x109));
      endpoints.push_back(Endpoint(8, 0x104));
      endpoints.push_back(Endpoint(10, 0x104));
      endpoints.push_back(Endpoint(0x6E, 0x104));
      endpoints.push_back(Endpoint(12, 0xc05e));
      endpoints.push_back(Endpoint(47, 0x104));
      endpoints.push_back(Endpoint(242, 0xa1e0));

      // Endpoint bonus1(11, 0x104);
      // bonus1.AppDeviceId = 0x0400;
      // bonus1.num_outclusters = 2;
      // bonus1.outcluster_list = {0x0, 0x0, 0x0, 0x0}; //todo ssiasZone ssiasWD
      // bonus1.number_inclusters = 1;
      // bonus1.incluster_list = {0x0, 0x0}; // todo ssiasAce

      // Endpoint bonus2(13, 0x104);
      // bonus2.number_inclusters = 1;
      // bonus2.incluster_list = {0x0}; //TODO genOTA

      // endpoints.push_back(bonus1);
      // endpoints.push_back(bonus2);

      ESP_LOGD(BOOT_COMMANDS_TAG, "Registering Endpoints");
      for (auto ep : endpoints)
      {
        uint8_t fcs = 0x0;

        auto profile = static_cast<uint8_t *>(static_cast<void *>(&ep.profile_id));
        auto device = static_cast<uint8_t *>(static_cast<void *>(&ep.AppDeviceId));
        uint8_t len = 9 + ep.number_inclusters + ep.num_outclusters;

        std::vector<uint8_t> cmd = {
            0xfe,
            static_cast<uint8_t>(len),
            0x24, //cmd0
            0x00, //cmd1
            static_cast<uint8_t>(ep.endpoint_id),
            profile[0],
            profile[1],
            device[0],
            device[1],
            static_cast<uint8_t>(ep.App_dev_version),
            static_cast<uint8_t>(ep.latency_required),
        };
        cmd.push_back(static_cast<uint8_t>(ep.number_inclusters));
        for (auto cluster : ep.incluster_list)
          cmd.push_back(cluster);

        cmd.push_back(static_cast<uint8_t>(ep.num_outclusters));
        for (auto cluster : ep.outcluster_list)
          cmd.push_back(cluster);

        fcs = ZCLHelper::count_fcs(cmd);
        // for (short i = 1; i < len; i++)
        // {
        //   fcs = fcs ^ cmd[i];
        // }

        cmd.push_back(fcs);

        ZNPCommand afRegister(cmd, len + 5, {0xfe, 0x01, 0x64, 0x00, 0x00, 0x65}, 6);
        send_cmd_and_wait_for_response(afRegister, response);
      }
    }

    void ZclMqttBridge::zcl_mqtt_bridge_init()
    {

      // 39 30 65 63 6e 61 69 6c 6c 41 65 65 42 67 69 5a
      std::vector<uint8_t> Nwkkey = {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39};        //ZigBeeAlliance09
      std::vector<uint8_t> NwkkeyReverse = {0x39, 0x30, 0x65, 0x63, 0x6e, 0x61, 0x69, 0x6c, 0x6c, 0x41, 0x65, 0x65, 0x42, 0x67, 0x69, 0x5a}; //ZigBeeAlliance09
      //ID 257 for Zstack 1.2
      //ID 273 for < Zstack 3.0.x
      //ID 4 for > Zstack 3.0.x
      std::vector<uint8_t> TcLinkkey = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x5a, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6c,
                                        0x6c, 0x69, 0x61, 0x6e, 0x63, 0x65, 0x30, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //ZigBeeAlliance09

      std::vector<uint8_t> response = {};
      std::vector<uint8_t> writeConfigExpRsp = {0xFE, 0x01, 0x66, 0x09, 0x00, 0x6E};

      //check GET_NV_INFo if key is the same as NwkKey, if so, skip init
      ZNPCommand getNVInfo({0xfe, 0x00, 0x27, 0x01, 0x26}, 5, {}, 0);
      write_command(getNVInfo.command, getNVInfo.command_length);
      delayMicroseconds(DELAY * 10);

      uint8_t byte;
      while (this->available())
      {
        byte = this->read();
        response.push_back(byte);
      }

      std::vector<uint8_t> keyFromNvInfo(response.begin() + 20, response.end() - 1);
      if (keyFromNvInfo == Nwkkey)
      {
        ESP_LOGD(INIT_COMMANDS_TAG, "Key are equal");
        return;
      }

      ZNPCommand reset({0xfe, 0x01, 0x41, 0x00, 0x00, 0x40}, 6, {0xfe, 0x06, 0x41, 0x80, 0x02, 0x02, 0x00, 0x02, 0x05, 0x00, 0xC0}, 11);

      ZNPCommand confStartUpDeleteNVOpt({0xfe, 0x05, 0x21, 0x09, 0x00, 0x03, 0x00, 0x01, 0x03, 0x2c}, 10, writeConfigExpRsp, 6);
      ZNPCommand confStartUpOpt({0xfe, 0x05, 0x21, 0x09, 0x00, 0x03, 0x00, 0x01, 0x02, 0x2d}, 10, writeConfigExpRsp, 6);
      ZNPCommand confLogType({0xfe, 0x05, 0x21, 0x09, 0x00, 0x87, 0x00, 0x01, 0x00, 0xab}, 10, writeConfigExpRsp, 6);
      ZNPCommand confNwkKeyDistrib({0xfe, 0x05, 0x21, 0x09, 0x00, 0x63, 0x00, 0x01, 0x01, 0x4e}, 10, writeConfigExpRsp, 6);
      ZNPCommand confDirectCB({0xfe, 0x05, 0x21, 0x09, 0x00, 0x8f, 0x00, 0x01, 0x01, 0xa2}, 10, writeConfigExpRsp, 6);
      // ZNPCommand confChannel(           {0xfe, 0x08, 0x21, 0x09, 0x00, 0x84, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0xa8}, 13, writeConfigExpRsp, 6);
      ZNPCommand confPanId({0xfe, 0x06, 0x21, 0x09, 0x00, 0x83, 0x00, 0x02, 0x01, 0x01, 0xaf}, 11, writeConfigExpRsp, 6);
      ZNPCommand extConfPanId({0xfe, 0x0C, 0x21, 0x09, 0x00, 0x2d, 0x00, 0x08, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, 17, writeConfigExpRsp, 6);
      // ZNPCommand confNwkKey(            {0xfe, 0x14, 0x21, 0x09, 0x00, 0x62, 0x00, 0x10, /*startOf the key LSB*/ 0x39, 0x30, 0x65, 0x63, 0x6e, 0x61, 0x69, 0x6c, 0x6c, 0x41, 0x65, 0x65, 0x42, 0x67, 0x69, 0x5a /*end of key*/, 0x70}, 25, writeConfigExpRsp, 6);
      ZNPCommand confNwkKey({0xfe, 0x10, 0x27, 0x05, /*startOf the key LSB*/ 0x39, 0x30, 0x65, 0x63, 0x6e, 0x61, 0x69, 0x6c, 0x6c, 0x41, 0x65, 0x65, 0x42, 0x67, 0x69, 0x5a /*end of key*/, 0x0C}, 25, {0xfe, 0x01, 0x67, 0x05, 0x00, 0x63}, 6);

      ZNPCommand bdbSetChannelNonPrimary({0xFE, 0x05, 0x2F, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22}, 10, {0xfe, 0x01, 0x6f, 0x08, 0x00, 0x66}, 6);
      ZNPCommand bdbSetChannelPrimary({0xFE, 0x05, 0x2F, 0x08, 0x01, 0x00, 0x08, 0x00, 0x00, 0x2b}, 10, {0xfe, 0x01, 0x6f, 0x08, 0x00, 0x66}, 6);
      ZNPCommand bdbStartCommissioning4({0xFE, 0x01, 0x2F, 0x05, 0x04, 0x2f}, 6, {0xfe, 0x01, 0x6f, 0x05, 0x00, 0x6b}, 6);
      ZNPCommand bdbStartCommissioning2({0xFE, 0x01, 0x2F, 0x05, 0x02, 0x29}, 6, {0xfe, 0x01, 0x6f, 0x05, 0x00, 0x6b}, 6);

      //TC link key should be ok with ZNP 3.x.x version - should be 'ZigBeeAlliance09'
      ESP_LOGD(INIT_COMMANDS_TAG, "reset-Soft");
      send_cmd_and_wait_for_response(reset, response);
      // ESP_LOGD(INIT_COMMANDS_TAG, "StartUp Option delete NV");
      // send_cmd_and_wait_for_response(confStartUpDeleteNVOpt, response);
      ESP_LOGD(INIT_COMMANDS_TAG, "StartUp Option");
      send_cmd_and_wait_for_response(confStartUpOpt, response);
      ESP_LOGD(INIT_COMMANDS_TAG, "reset-Soft");
      send_cmd_and_wait_for_response(reset, response);
      ESP_LOGD(INIT_COMMANDS_TAG, "LogType");
      send_cmd_and_wait_for_response(confLogType, response);
      ESP_LOGD(INIT_COMMANDS_TAG, "NWK Key distrib");
      send_cmd_and_wait_for_response(confNwkKeyDistrib, response);
      ESP_LOGD(INIT_COMMANDS_TAG, "Direct CB");
      send_cmd_and_wait_for_response(confDirectCB, response);
      ESP_LOGD(INIT_COMMANDS_TAG, "Config Channel");
      // send_cmd_and_wait_for_response(confChannel, response);
      ESP_LOGD(INIT_COMMANDS_TAG, "Config PanId");
      send_cmd_and_wait_for_response(confPanId, response);
      ESP_LOGD(INIT_COMMANDS_TAG, "Config Extended PanId");
      send_cmd_and_wait_for_response(extConfPanId, response);
      ESP_LOGD(INIT_COMMANDS_TAG, "Config Nwk Key");
      send_cmd_and_wait_for_response(confNwkKey, response);

      ESP_LOGD(INIT_COMMANDS_TAG, "BDB primaryChannel");
      send_cmd_and_wait_for_response(bdbSetChannelPrimary, response);
      ESP_LOGD(INIT_COMMANDS_TAG, "BDB non-primaryChannel");
      send_cmd_and_wait_for_response(bdbSetChannelNonPrimary, response);

      ESP_LOGD(INIT_COMMANDS_TAG, "BDB startCommissioning 0x04");
      send_cmd_and_wait_for_response(bdbStartCommissioning4, response);
      delayMicroseconds(DELAY);
      receive();
      ESP_LOGD(INIT_COMMANDS_TAG, "BDB startCommissioning 0x02");
      send_cmd_and_wait_for_response(bdbStartCommissioning2, response);
    }

    void ZclMqttBridge::send_cmd_and_wait_for_response(ZNPCommand cmd, std::vector<uint8_t> response)
    {
      bool waitForResponse = write_command(cmd.command, cmd.command_length);
      delayMicroseconds(DELAY);

      while (waitForResponse)
      {
        waitForResponse = receive_response(response, cmd.response_length);
      }
    }

    bool ZclMqttBridge::write_command(std::vector<uint8_t> cmd, int length)
    {
      bool waitForResponse = true;
      if (!cmd.empty())
      {
        string r = "";
        for (auto i : cmd)
          r.append(ZCLHelper::n2hexstr(i));

        ESP_LOGD(UART_WRITE_TAG, "write : 0x%s", r.c_str());
      }

      this->write_array(&cmd[0], sizeof(uint8_t) * length);
      return waitForResponse;
    }

    //TODO::
    bool ZclMqttBridge::receive_response(std::vector<uint8_t> expectedResponse, int length)
    {
      bool waitForResponse = false;
      receive();
      delayMicroseconds(DELAY / 2);
      return waitForResponse;
      // while (available())
      // {
      //   ESP_LOGI(UART_RECEIVE_TAG, "RESPONSE");
      //   this->read_array(&expectedResponse[0], sizeof(uint8_t) * length);
      //   return false; //no nned to wait anymore
      // }
    }

    bool ZclMqttBridge::check_join_device(std::vector<uint8_t> response)
    {

      if (response.size() == 16) // 0xFE, LEN(ofPayload), CMD0 CMD1,Payload(min 0x0B), FCS
      {
        if (response[CMD0_POS] == 0x45 && response[CMD1_POS] == 0xC1) //ZDO_END_DEVICE_ANNCE
        {
          std::__cxx11::string nwkAddress = to_string(response[5]); //MSB
          nwkAddress += to_string(response[4]);
          ESP_LOGD(DEVICE_JOIN_TAG, "new Device address: %s", nwkAddress.c_str());
        }
      }
    }

    bool ZclMqttBridge::check_leave_device(std::vector<uint8_t> response)
    {
      if (response.size() == 18) // 0xFE, LEN(ofPayload), CMD0 CMD1,Payload(min 0x0D), FCS
      {
        if (response[CMD0_POS] == 0x45 && response[CMD1_POS] == 0xC9) //ZDO_LEAVE_IND
        {
          std::__cxx11::string nwkAddress = ZCLHelper::n2hexstr(response[5]); //MSB
          nwkAddress += ZCLHelper::n2hexstr(response[4]);
          ESP_LOGD(DEVICE_LEAVE_TAG, "LEAVE - Device address: %s", nwkAddress.c_str());

          //TODO LifeHack - needs better solution
          this->publish("homeassistant/switch/" + App.get_name() + nwkAddress + "/config", "");
          this->publish("homeassistant/light/" + App.get_name() + nwkAddress + "/config", "");
          this->publish("homeassistant/binary_sensor/" + App.get_name() + nwkAddress + "/config", "");
          this->publish("homeassistant/sensor/" + App.get_name() + nwkAddress + "t/config", "");
          this->publish("homeassistant/sensor/" + App.get_name() + nwkAddress + "h/config", "");
          this->publish("homeassistant/sensor/" + App.get_name() + nwkAddress + "p/config", "");
        }
      }
    }

    bool ZclMqttBridge::check_AF_INCOMING(std::vector<uint8_t> response)
    {
      ZCluster zcl;

      check_ZCL_cmd(&zcl, response);
      if (zcl.attributes.empty() || !this->is_connected())
      {
        return false;
      }
      Attribute a0 = zcl.attributes[0];

      if (a0.id == ZCLHelper::Attributes::model_identifier && zcl.id == ZCLHelper::ClusterIds::basic)
      {
        ESP_LOGD(MQTT_RECEIVE_TAG, "model identifier");
        publish_component_config(a0.value, this->application_name + zcl.src_address);
        return true;
      }

      if (zcl.id == ZCLHelper::ClusterIds::basic)
      {
      }
      if (zcl.id == ZCLHelper::ClusterIds::door_lock)
      {
        std::string state = "";
        if (std::strtoul(a0.value.c_str(), nullptr, 16) < 1)
          state = "OF";
        else
          state = "ON";

        this->publish_json(
            "homeassistant/binary_sensor/" + this->application_name + zcl.src_address + "/state",
            [=](JsonObject &r) {
              r["state"] = state;
            },
            0, true);

        return true;
      }
      if (zcl.id == ZCLHelper::ClusterIds::multistate_input)
      {
        auto val = std::strtoul(a0.value.c_str(), nullptr, 16);
        std::string set = "";
        if (val == 1)
          set = "ON"; //single click
        else if (val == 2)
          set = "OFF"; //double click
        else if (val == 0)
          set = "OFF"; //long click
        this->publish("homeassistant/switch/" + this->application_name + zcl.src_address + "/set", set, 0, true);

        return true;
      }

      if (zcl.id == ZCLHelper::ClusterIds::occupancy_sensing)
      {
        std::string state = "";
        if (std::strtoul(a0.value.c_str(), nullptr, 16) < 1)
          state = "OF";
        else
          state = "ON";

        this->publish_json(
            "homeassistant/binary_sensor/" + this->application_name + zcl.src_address + "/state",
            [=](JsonObject &r) {
              r["state"] = state;
            },
            0, true);

        return true;
      }

      if (zcl.id == ZCLHelper::ClusterIds::on_off_cluster)
      {
        //TODO Lifehack 2, do it better
        std::string value = "";
        if (std::strtoul(a0.value.c_str(), nullptr, 16) == 1)
          value = "ON";
        else
          value = "OFF";

        this->publish_json(
            "homeassistant/light/" + this->application_name + zcl.src_address + "/state",
            [=](JsonObject &r) {
              r["state"] = value;
            },
            0, true);
        this->publish_json(
            "homeassistant/switch/" + this->application_name + zcl.src_address + "/state",
            [=](JsonObject &r) {
              r["state"] = value;
            },
            0, true);
        this->publish_json(
            "homeassistant/binary_sensor/" + this->application_name + zcl.src_address + "/state",
            [=](JsonObject &r) {
              r["state"] = value;
            },
            0, true);

        return true;
      }

      if (zcl.id == ZCLHelper::ClusterIds::pressure)
      {
        // Attribute a1 = zcl.attributes[1];
        // Attribute a2 = zcl.attributes[2];

        auto value = to_string(std::strtoul(a0.value.c_str(), nullptr, 16));
        value.insert(2, 1, ',');

        this->publish_json(
            "homeassistant/sensor/" + this->application_name + zcl.src_address + "p/state",
            [=](JsonObject &r) {
              r["pressure"] = value;
            },
            0, true);

        return true;
      }

      if (zcl.id == ZCLHelper::ClusterIds::relative_humidity)
      {
        auto value = to_string(std::strtoul(a0.value.c_str(), nullptr, 16));

        value.insert(2, 1, ',');

        this->publish_json(
            "homeassistant/sensor/" + this->application_name + zcl.src_address + "h/state",
            [=](JsonObject &r) {
              r["humidity"] = value;
            },
            0, true);

        return true;
      }

      if (zcl.id == ZCLHelper::ClusterIds::temperature)
      {
        auto value = to_string(std::strtoul(a0.value.c_str(), nullptr, 16));
        value.insert(2, 1, ',');

        this->publish_json(
            "homeassistant/sensor/" + this->application_name + zcl.src_address + "t/state",
            [=](JsonObject &r) {
              r["temperature"] = value;
            },
            0, true);

        return true;
      }

      return false;
    }

    bool ZclMqttBridge::send_AF_DATA_REQUEST(string dstAddr, int clusterId, uint8_t transNumber, uint8_t action, uint8_t brightness, int color_temp)
    {

      this->transaction_number++;
      uint8_t lsb;
      uint8_t msb;
      std::vector<uint8_t> cmd = {0xfe};
      int datalen = 0;
      if (clusterId == ZCLHelper::ClusterIds::on_off_cluster)
        datalen = 3;
      if (clusterId == ZCLHelper::ClusterIds::level_control)
        datalen = 6;
      if (clusterId == ZCLHelper::ClusterIds::color_control)
        datalen = 10;

      cmd.push_back(10 + datalen);
      cmd.push_back(0x24); //cmd0
      cmd.push_back(0x01); //cmd1
      ulong addr = std::strtoul(dstAddr.c_str(), nullptr, 16);
      lsb = addr;
      msb = addr >> 8;
      ESP_LOGD(MQTT_RECEIVE_TAG, "addr LSB: %02x MSB: %02x ", lsb, msb);
      cmd.push_back(lsb);
      cmd.push_back(msb);
      cmd.push_back(1); //dst Endpoint
      cmd.push_back(1); //src Endpoint
      int cl = clusterId;

      lsb = cl;
      msb = cl >> 8;

      ESP_LOGD(MQTT_RECEIVE_TAG, "cluster LSB: %02x MSB: %02x ", lsb, msb);
      cmd.push_back(lsb); //cluster 1
      cmd.push_back(msb); //cluster 0

      cmd.push_back(0);  //transId
      cmd.push_back(0);  //options
      cmd.push_back(30); //Radius - max hops

      cmd.push_back(datalen); //datalen
      if (clusterId == ZCLHelper::ClusterIds::on_off_cluster)
      {

        ESP_LOGD(MQTT_RECEIVE_TAG, "ON/OFF action %d", action);
        cmd.push_back(0x01);        //FrameControl
        cmd.push_back(transNumber); //Trans
        cmd.push_back(action);      //  0x00 OFF, 0x01 ON, 0x02 Toggle
      }
      if (clusterId == ZCLHelper::ClusterIds::level_control)
      {
        ESP_LOGD(MQTT_RECEIVE_TAG, "level action %d", brightness);
        cmd.push_back(0x01);        //FrameControl
        cmd.push_back(transNumber); //Trans
        cmd.push_back(0x00);        //move to level command - 0x00

        cmd.push_back(brightness); //level
        cmd.push_back(0x3c);       //Transition time 6 seconds LSB
        cmd.push_back(0x00);       //Transition time  in 10th of seconds MSB
      }
      if (clusterId == ZCLHelper::ClusterIds::color_control)
      {
        cmd.push_back(0x01);        //FrameControl
        cmd.push_back(transNumber); //Trans
        cmd.push_back(0x4b);        //Move Color Temperature

        cmd.push_back(0x03); //move mode - 0x03 DOWN
        cmd.push_back(0x05); //rate
        cmd.push_back(0x00); //rate

        lsb = color_temp;
        msb = color_temp >> 8;
        ESP_LOGD(MQTT_RECEIVE_TAG, "color action %d %d", lsb, msb);
        //max 65535 Mired => 15K
        //min 0 Mired
        cmd.push_back(lsb); //Color Temperature-Minimum Mireds
        cmd.push_back(msb); //Color Temperature-Minimum Mireds
        cmd.push_back(lsb); //Color Temperature-Maximum Mireds
        cmd.push_back(msb); //Color Temperature-Maximum Mireds
      }

      uint8_t fcs = ZCLHelper::count_fcs(cmd);
      cmd.push_back(fcs);

      ZNPCommand dataReq(cmd, 15 + datalen, {0xfe, 0x01, 0x64, 0x01, 0x00, 0x64}, 6);
      send_cmd_and_wait_for_response(dataReq, {});

      ESP_LOGD(MQTT_RECEIVE_TAG, "sending data req: ");
      ESP_LOGD(MQTT_RECEIVE_TAG, "to: %s, action: %d ", dstAddr.c_str(), action);
    }

    void ZclMqttBridge::check_ZCL_cmd(ZCluster *cluster, std::vector<uint8_t> response)
    {
      Command cmd;
      Attribute attrib;
      uint8_t datalen = 0;

      if (response.size() > 21) // 0xFE, LEN(ofPayload), CMD0 CMD1,Payload(min 0x11), FCS
      {

        if (response[CMD0_POS] == 0x44 && response[CMD1_POS] == 0x81)
        {
          datalen = response[20];
          std::vector<uint8_t> data(response.begin() + 21, response.begin() + 21 + datalen);
          cluster->src_address = (ZCLHelper::n2hexstr(response[9]) + ZCLHelper::n2hexstr(response[8])).c_str(); //MSB
          uint8_t srcEP = response[10];                                                                        //difers left or right btn

          if (response[6] == 0x12 && response[7] == 0x00) // MultiState Input cluster
          {
            cluster->id = ZCLHelper::ClusterIds::multistate_input;

            if (data[0] == 0x18) //profileWide
            {
              if (data[2] == 0x0A) //ReportAttributes
              {
                cmd.id = ZCLHelper::Commands::report_attributes;
                cluster->commands.push_back(cmd);

                if (data[3] == 0x55 && data[4] == 0x00) // 0x0055 Attribute present Value
                {
                  attrib.id = ZCLHelper::Attributes::present_value;

                  if (data[5] == 0x21) //datatype = UnsignedInteger 16-bit
                  {
                    attrib.type = ZCLHelper::DataTypes::uint16;
                    attrib.value = (ZCLHelper::n2hexstr(data[7]) + ZCLHelper::n2hexstr(data[6]));
                    ESP_LOGD(UART_RECEIVE_TAG, "Attrib value: %s", attrib.value.c_str());
                    cluster->attributes.push_back(attrib);
                  }
                }
              }
            }
          }

          if (response[6] == 0x00 && response[7] == 0x00) // basic cluster
          {
            ESP_LOGD(MQTT_RECEIVE_TAG, "basic cluster: ");
            cluster->id = ZCLHelper::ClusterIds::basic;
            if (data[0] == 0x18) //profileWide
            {
              if (data[3] == 0x05 && data[4] == 0x00) // 0x0005 Model Identifier
              {
                attrib.id = ZCLHelper::Attributes::model_identifier;
                attrib.type = static_cast<ZCLHelper::DataTypes>(response[5]);
                std::vector<uint8_t> name;
                int i = 6;
                while (data[i] != 0x01)
                {
                  name.push_back(data[i]);
                  i++;
                }
                attrib.value = std::string(name.begin(), name.end());
                ESP_LOGD(MQTT_RECEIVE_TAG, "Attrib dev name: %s", attrib.value.c_str());
                cluster->attributes.push_back(attrib);

                if (data[i + 1] == 0x00 && data[i] == 0x01) //version
                {
                  Attribute a2; //version
                  a2.id = ZCLHelper::Attributes::app_version;
                  a2.type = static_cast<ZCLHelper::DataTypes>(data[i + 2]);
                  a2.value = ZCLHelper::n2hexstr(data[i + 3]);
                  ESP_LOGD(MQTT_RECEIVE_TAG, "Attrib version: %s", a2.value.c_str());

                  cluster->attributes.push_back(a2);
                }
              }
            }
          }

          if (response[6] == 0x06 && response[7] == 0x00) // on off cluster
          {
            cluster->id = ZCLHelper::ClusterIds::on_off_cluster;
            if (data[0] == 0x18) //profileWide
            {
              if (data[2] == 0x0a) // ReportAttributes Command
              {
                if (data[3] == 0x00 && data[4] == 0x00)
                {
                  attrib.id = ZCLHelper::Attributes::on_off_attribute;
                  attrib.type = static_cast<ZCLHelper::DataTypes>(data[5]);
                  attrib.value = ZCLHelper::n2hexstr(data[6]);
                  cluster->attributes.push_back(attrib);
                }
              }
            }
          }

          if (response[6] == 0x08 && response[7] == 0x00) // level control cluster
          {
            cluster->id = ZCLHelper::ClusterIds::level_control;
            if (data[0] == 0x18) //profileWide
            {
              if (data[2] == 0x0a) // ReportAttributes Command
              {
              }
            }
          }

          if (response[6] == 0x01 && response[7] == 0x01) // door lock cluster
          {
            cluster->id = ZCLHelper::ClusterIds::door_lock;
            if (data[0] == 0x18) //profileWide
            {
              if (data[2] == 0x0a) // ReportAttributes Command
              {
                if (data[3] == 0x55 && data[4] == 0x00) // 0x0055 Attribute present Value
                {
                  attrib.id = ZCLHelper::Attributes::present_value;
                  attrib.type = static_cast<ZCLHelper::DataTypes>(data[5]);
                  attrib.value = ZCLHelper::n2hexstr(data[7]);
                  attrib.value += ZCLHelper::n2hexstr(data[6]);
                }
                else if (data[3] == 0x08 && data[4] == 0x05) //unknown
                {
                  attrib.id = ZCLHelper::Attributes::unknown0508;
                  attrib.type = static_cast<ZCLHelper::DataTypes>(data[5]);
                  attrib.value = ZCLHelper::n2hexstr(data[11]);
                  attrib.value += ZCLHelper::n2hexstr(data[10]);
                  attrib.value += ZCLHelper::n2hexstr(data[9]);
                  attrib.value += ZCLHelper::n2hexstr(data[8]);
                  attrib.value += ZCLHelper::n2hexstr(data[7]);
                  attrib.value += ZCLHelper::n2hexstr(data[6]);
                }
              }
            }
          }

          if (response[6] == 0x00 && response[7] == 0x03) // color control cluster
          {
            cluster->id = ZCLHelper::ClusterIds::color_control;
            if (data[0] == 0x18) //profileWide
            {
              if (data[2] == 0x0a) // ReportAttributes Command
              {
              }
            }
          }

          if (response[6] == 0x02 && response[7] == 0x04) // temperature cluster
          {
            cluster->id = ZCLHelper::ClusterIds::temperature;
            if (data[0] == 0x18) //profileWide
            {
              if (data[2] == 0x0a) // ReportAttributes Command
              {
                if (data[3] == 0x00 && data[4] == 0x00)
                {
                  attrib.id = ZCLHelper::Attributes::on_off_attribute;
                  attrib.type = static_cast<ZCLHelper::DataTypes>(data[5]);
                  attrib.value = ZCLHelper::n2hexstr(data[7]);
                  attrib.value += ZCLHelper::n2hexstr(data[6]);
                  cluster->attributes.push_back(attrib);
                }
              }
            }
          }
          if (response[6] == 0x05 && response[7] == 0x04) // humidity cluster
          {
            cluster->id = ZCLHelper::ClusterIds::relative_humidity;
            if (data[0] == 0x18) //profileWide
            {
              if (data[2] == 0x0a) // ReportAttributes Command
              {
                if (data[3] == 0x00 && data[4] == 0x00)
                {
                  attrib.id = ZCLHelper::Attributes::on_off_attribute;
                  attrib.type = static_cast<ZCLHelper::DataTypes>(data[5]);
                  attrib.value = ZCLHelper::n2hexstr(data[7]);
                  attrib.value += ZCLHelper::n2hexstr(data[6]);
                  cluster->attributes.push_back(attrib);
                }
              }
            }
          }

          if (response[6] == 0x03 && response[7] == 0x04) // pressure cluster
          {
            cluster->id = ZCLHelper::ClusterIds::pressure;
            if (data[0] == 0x18) //profileWide
            {
              if (data[2] == 0x0a) // ReportAttributes Command
              {
                if (data[3] == 0x00 && data[4] == 0x00)
                {
                  attrib.id = ZCLHelper::Attributes::on_off_attribute;
                  attrib.type = static_cast<ZCLHelper::DataTypes>(data[5]);
                  attrib.value = ZCLHelper::n2hexstr(data[7]);
                  attrib.value += ZCLHelper::n2hexstr(data[6]);
                  cluster->attributes.push_back(attrib);
                }
                if (data[9] == 0x00 && data[8] == 0x20)
                {
                  Attribute a2;
                  a2.id = ZCLHelper::Attributes::scale;
                  a2.type = static_cast<ZCLHelper::DataTypes>(data[10]);
                  a2.value = ZCLHelper::n2hexstr(data[11]);
                  cluster->attributes.push_back(a2);
                }
                if (data[13] == 0x00 && data[12] == 0x16)
                {
                  Attribute a3;
                  a3.id = ZCLHelper::Attributes::scaled_value;
                  a3.type = static_cast<ZCLHelper::DataTypes>(data[14]);
                  a3.value = ZCLHelper::n2hexstr(data[16]);
                  a3.value += ZCLHelper::n2hexstr(data[15]);
                  cluster->attributes.push_back(a3);
                }
              }
            }
          }

          if (response[6] == 0x06 && response[7] == 0x04) // occupancy cluster
          {
            cluster->id = ZCLHelper::ClusterIds::occupancy_sensing;
            if (data[0] == 0x18) //profileWide
            {
              if (data[2] == 0x0A) //ReportAttributes
              {
                if (data[3] == 0x00 && data[4] == 0x00)
                {
                  attrib.id = ZCLHelper::Attributes::on_off_attribute; //0 = occupancy
                  attrib.type = static_cast<ZCLHelper::DataTypes>(data[5]);
                  attrib.value = ZCLHelper::n2hexstr(data[6]);
                  cluster->attributes.push_back(attrib);
                }
              }
            }
          }

        } //cmd is not AF INC
      }   //size >21
    }

    void ZclMqttBridge::publish_component_config(std::string modelName, std::string srcAddress)
    {
      std::string component = "";
      if (modelName.find("plug") != std::string::npos)
      {
        component = "switch";
        std::string cmdTopic = "homeassistant/" + component + "/" + srcAddress + "/set";
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress;
                             //  r["device_class"] = "outlet";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "/state";
                             r["command_topic"] = cmdTopic;
                             r["unique_id"] = srcAddress;
                             r["opt"] = true;
                           });

        this->subscribe(cmdTopic, &ZclMqttBridge::on_message);
      }
      else if (modelName.find("light") != std::string::npos)
      {
        component = "light";
        std::string cmdTopic = "homeassistant/" + component + "/" + srcAddress + "/set";
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress;
                             r["schema"] = "json";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "/state";
                             r["command_topic"] = cmdTopic;
                             r["brightness"] = true;
                             r["color_temp"] = true;
                             r["opt"] = true;
                             r["unique_id"] = srcAddress;
                           });
        ///example
        // {
        //   "~": "homeassistant/light/aaa",
        //   "name": "aaa",
        //   "unique_id": "aaa",
        //   "cmd_t": "~/set",
        //   "stat_t": "~/state",
        //   "schema": "json",
        //   "brightness": true,
        //   "color_temp": true,
        //   "opt": true
        // }
        this->subscribe_json(cmdTopic, &ZclMqttBridge::on_json_message);
      }
      else if (modelName.find("cube") != std::string::npos)
      {
        component = "switch";
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress;
                             r["command_topic"] = "homeassistant/" + component + "/" + srcAddress + "/set";
                             r["unique_id"] = srcAddress;
                           });
      }
      else if (modelName.find("remote.b1") != std::string::npos)
      {
        component = "switch";
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress;
                             r["command_topic"] = "homeassistant/" + component + "/" + srcAddress + "/set";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "/state";
                             r["val_tpl"] = "{{ value_json.state}}";
                             r["opt"] = true;
                             r["unique_id"] = srcAddress;
                           });
      }
      else if (modelName.find("remote.b2") != std::string::npos)
      {
        component = "switch";
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress;
                             r["command_topic"] = "homeassistant/" + component + "/" + srcAddress + "/set";
                             r["unique_id"] = srcAddress;
                           });
      }

      else if (modelName.find("switch") != std::string::npos)
      {
        component = "switch";
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress;
                             r["command_topic"] = "homeassistant/" + component + "/" + srcAddress + "/set";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "/state";
                             r["val_tpl"] = "{{ value_json.state}}";
                             r["opt"] = true;
                             r["unique_id"] = srcAddress;
                           });
      }
      else if (modelName.find("sensor_magnet") != std::string::npos)
      {
        component = "binary_sensor";

        this->publish_json("homeassistant/" + component + "/" + srcAddress + "/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress;
                             r["device_class"] = "door";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "/state";
                             r["val_tpl"] = "{{ value_json.state}}";
                             r["unique_id"] = srcAddress;
                           });
      }
      else if (modelName.find("sensor_motion") != std::string::npos)
      {
        component = "binary_sensor";

        this->publish_json("homeassistant/" + component + "/" + srcAddress + "/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress;
                             r["device_class"] = "motion";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "/state";
                             r["val_tpl"] = "{{ value_json.state}}";
                             r["unique_id"] = srcAddress;
                           });
      }
      else if (modelName.find("sensor_wleak") != std::string::npos)
      {
        component = "binary_sensor";
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress;
                             r["device_class"] = "moisture";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "/state";
                             r["val_tpl"] = "{{ value_json.state}}";
                             r["unique_id"] = srcAddress;
                           });
      }
      else if (modelName.find("vibration") != std::string::npos)
      {
        component = "binary_sensor";
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress;
                             r["device_class"] = "vibration";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "/state";
                             r["val_tpl"] = "{{ value_json.state}}";
                             r["unique_id"] = srcAddress;
                           });
      }
      else if (modelName.find("weather") != std::string::npos)
      {
        component = "sensor";
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "p/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress + "p";
                             r["device_class"] = "pressure";
                             r["unit_of_measurement"] = "kPa";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "p/state";
                             r["val_tpl"] = "{{ value_json.pressure}}";
                             r["unique_id"] = srcAddress + "p";
                           });
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "h/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress + "h";
                             r["device_class"] = "humidity";
                             r["unit_of_measurement"] = "%";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "h/state";
                             r["val_tpl"] = "{{ value_json.humidity}}";
                             r["unique_id"] = srcAddress + "h";
                           });
        this->publish_json("homeassistant/" + component + "/" + srcAddress + "t/config",
                           [=](JsonObject &r) {
                             r["name"] = srcAddress + "t";
                             r["device_class"] = "temperature";
                             r["unit_of_measurement"] = "°C";
                             r["value_template"] = "{{ value_json.temperature}}";
                             r["unique_id"] = srcAddress + "t";
                             r["state_topic"] = "homeassistant/" + component + "/" + srcAddress + "t/state";
                           });
      }
      ESP_LOGD(MQTT_RECEIVE_TAG, "homeassistant/%s/%s/config", component.c_str(), srcAddress.c_str());
    }
  } // namespace zcl_mqtt
} // namespace esphome
