#pragma once
#include "esphome.h"
#include <cstdlib>
#include <string>
#include <list>
#include <vector>

using namespace std;
namespace esphome
{
    namespace zcl_mqtt
    {

        class ZCLHelper
        {
        public:
            enum DataTypes
            {
                noData = 0,
                data8 = 8,
                data16 = 9,
                data24 = 10,
                data32 = 11,
                data40 = 12,
                data48 = 13,
                data56 = 14,
                data64 = 15,
                boolean_ = 16,
                bitmap8 = 24,
                bitmap16 = 25,
                bitmap24 = 26,
                bitmap32 = 27,
                bitmap40 = 28,
                bitmap48 = 29,
                bitmap56 = 30,
                bitmap64 = 31,
                uint8 = 32,
                uint16 = 33,
                uint24 = 34,
                uint32 = 35,
                uint40 = 36,
                uint48 = 37,
                uint56 = 38,
                uint64 = 39,
                int8 = 40,
                int16 = 41,
                int24 = 42,
                int32 = 43,
                enum8 = 48,
                enum16 = 49,
                single_prec = 57,
                double_prec = 58,
                octet_str = 65,
                char_str = 66,
                long_octet_str = 67,
                long_char_str = 68,
                array = 72,
                _struct = 76,
                set = 80,
                bag = 81,
                tod = 224,
                date = 225,
                utc = 226,
                cluster_id = 232,
                attr_id = 233,
                bac_o_id = 234,
                ieee_addr = 240,
                sec_key = 241,

                unknown = 255,
            };

            enum ClusterIds
            {
                basic = 0,
                multistate_input = 0x12, //18
                on_off_cluster = 0x06,
                level_control = 0x08,
                door_lock = 0x101,
                color_control = 0x300,
                temperature = 0x402,
                pressure = 0x403,
                relative_humidity = 0x405,
                occupancy_sensing = 0x406,
            };

            enum Attributes
            {
                on_off_attribute = 0x00, //occupancy , level -current level,color - current Hue, temperature - measuredValue
                app_version = 0x01,      // basic version, color-currentSaturation
                model_identifier = 0x05,
                scale = 0x14,
                scaled_value = 0x10,
                present_value = 0x55, //85
                unknown0508 = 0x508,
                unknown3 = 0x505,
                on_level = 0x11, //levelcontrol
            };

            enum Commands
            {
                report_attributes = 10,
                move_to_level = 0x4, //level control

            };

            // https://stackoverflow.com/questions/5100718/integer-to-hex-string-in-c
            template <typename I>
            static std::string n2hexstr(I w, size_t hex_len = sizeof(I) << 1)
            {
                static const char *digits = "0123456789abcdef";
                std::string rc(hex_len, '0');
                for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
                    rc[i] = digits[(w >> j) & 0x0f];
                return rc;
            }

            /*
                * @brief Counts FCS from LEN byte to last DATA byte from vector<uintr_t>
                * starting from position 1, because XOR is counted from LEN to last DATA
                * 0 - 0xFE, 1 - LEN,  ending at end of cmd - cmd does not have FCS yet
            */
            static uint8_t CountFCS(std::vector<uint8_t> cmd)
            {
                uint8_t fcs = 0x0;
                for (short i = 1; i < cmd.size(); i++)
                {
                    fcs = fcs ^ cmd[i];
                }
                return fcs;
            }

            /*
            * @brief gets HASS name of component based on Cluster Id in C string fromat
            */
            static void GetComponentName(std::string modelName, std::string componentName, std::string devClass)
            {
                devClass = "";
                componentName = "";
                if (modelName.find("plug") != std::string::npos) //aqara smart plug
                {
                    componentName = "switch";
                }
                else if (modelName.find("light") != std::string::npos) //aqara light .compare("lumi.light.aqcn02")
                {
                    componentName = "light";
                }
                else if (modelName.find("cube") != std::string::npos) //aqara cube .compare("lumi.sensor_cube.aqgl01")
                {
                    componentName = "switch";
                }
                else if (modelName.find("remote") != std::string::npos) //switch .compare("lumi.remote.b186acn01")
                {
                    componentName = "switch";
                }
                // else if (modelName.compare("lumi.remote.b286acn01"))//switch
                // {
                //     componentName = "switch";
                // }
                else if (modelName.find("switch") != std::string::npos) //switch .compare("lumi.sensor_switch.aq2")
                {
                    componentName = "switch";
                }
                else if (modelName.find("sensor") != std::string::npos) //sesor
                {
                    componentName = "binary_sensor";

                    if (modelName.find("sensor_motion") != std::string::npos) //motion sensor .compare("lumi.sensor_motion")
                    {
                        devClass = "motion";
                    }
                    else if (modelName.find("sensor_wleak") != std::string::npos) //leak sensor
                    {
                        devClass = "moisture";
                    }

                    else if (modelName.find("door") != std::string::npos) //motion sensor .compare("lumi.sensor_motion")
                    {
                        devClass = "door";
                    }
                }

                else if (modelName.find("vibration") != std::string::npos) //vibration sensor .compare("lumi.vibration.aq1")
                {
                    componentName = "binary_sensor";
                    devClass = "vibration";
                }

                else if (modelName.find("door") != std::string::npos) //motion sensor .compare("lumi.sensor_motion")
                {
                    componentName = "binary_sensor";
                    devClass = "door";
                }

                else if (modelName.find("weather") != std::string::npos) //weather sensor    .compare("lumi.weather")
                {
                    componentName = "sensor";
                    devClass = "weather";
                }
            }
        };

        struct Parameter
        {
        public:
            string name;
            ZCLHelper::DataTypes type;
        };

        struct Attribute
        {
        public:
            ZCLHelper::Attributes id;  //2bytes
            ZCLHelper::DataTypes type; //1byte
            string value;
        };
        struct Command
        {
        public:
            ZCLHelper::Commands id; //1byte
            vector<Parameter> parameters;
        };

        struct ZCluster
        {
        public:
            ZCLHelper::ClusterIds id; //2 bytes
            std::string srcAddress;   //2bytes
            vector<Attribute> attributes;
            vector<Command> commands;
            std::string name;
        };

        /*
      SOF (Start of Frame): This is a one byte field with value equal to 0xFE that defines the
    start of each general serial packet.
      MT CMD (Monitor Test Command): This contains 1 byte for the length of the actual
    data, 2 bytes for the MT command Id, and the data ranging from 0-250 bytes. Check
    2.1.2 for more details.
      FCS (Frame Check Sequence): This is a one byte field that is used to ensure packet
    integrity. This field is computed as an XOR of all the bytes in the message starting with
    LEN field and through the last byte of data. The receiver XORs all the received data
    bytes as indicated above and then XORs the received FCS field. If the sum is not equal to
    zero, the received packet is in error

    Fields that are multi-byte fields are transmitted Least Significant byte first (LSB). There is no
provision for retransmission of 
    */
        class ZNPCommand
        {
        private:
        public:
            /*0xFE, LEN, CMD ID0, CMD ID1, PAYLOAD, FCS */
            std::vector<uint8_t> Command;
            int CommandLength;
            std::vector<uint8_t> ExpectedResponse;
            int ResponseLength;

            ZNPCommand(/* args */) {}
            ZNPCommand(std::vector<uint8_t> fullCommand, int commandLength, std::vector<uint8_t> expectedResponse, int responseLength)
            {
                this->Command = fullCommand;
                this->CommandLength = commandLength;
                this->ExpectedResponse = expectedResponse;
                this->ResponseLength = responseLength;
            }
            ~ZNPCommand() {}
        };

        struct Endpoint
        {
            int EndpointId;
            int ProfileId;
            int AppDeviceId;
            int AppDevVer;
            int NumInClusters;
            /// 2 bytes make single cluster
            std::vector<uint8_t> InClusterList;
            int NumOutClusters;
            /// 2 bytes make single cluster
            std::vector<uint8_t> OutClusterList;
            int LatencyReq;

            /*
      basic endpoint with default values
      */
            Endpoint(int endpointId, int profileId)
            {
                EndpointId = endpointId;
                ProfileId = profileId;
                AppDeviceId = 0x0005;
                AppDevVer = 0;
                NumInClusters = 0;
                NumOutClusters = 0;
                LatencyReq = 0; //NoLattencyReq
            }
        };

        class zcl_mqtt_bridge : public Component, public uart::UARTDevice, public mqtt::CustomMQTTDevice
        {
        public:
            zcl_mqtt_bridge(uart::UARTComponent *parent)
            {
                set_uart_parent(parent);
                set_setup_priority(setup_priority::LATE);
            }
            ~zcl_mqtt_bridge() {}

            void setup() override
            {

                ESP_LOGI("zcl_mqtt_bridge", "Setup of zcl_mqtt_bridge", "");
                this->transaction_number = 1; //first transaction

                while (millis() < 7500) //time for letting cc2530 init
                    NOP();

                zcl_mqtt_bridge_init();
                boot();

                //used for naming components, so with more bridges it would be still unique
                //if only using 1 coordinator, can be commented down
                this->application_name = App.get_name();
            }

            void loop() override
            {
                receive();
            }

        private:
            uint8_t transaction_number; //overflow doesnt matter
            std::string application_name = "";

            /* data */
            void zcl_mqtt_bridge_init();
            void boot();

            void receive();

            void send_cmd_and_wait_for_response(ZNPCommand cmd, std::vector<uint8_t> response);
            bool write_command(std::vector<uint8_t> cmd, int length);
            bool receive_response(std::vector<uint8_t> expectedResponse, int expectedLength);

            bool check_leave_device(std::vector<uint8_t> response);
            bool check_join_device(std::vector<uint8_t> response);
            bool check_AF_INCOMING(std::vector<uint8_t> response);
            void check_ZCL_cmd(ZCluster *cluster, std::vector<uint8_t> response);

            bool send_AF_DATA_REQUEST(string dstAddr, int clusterId, uint8_t transNumber, uint8_t action = 0x0, uint8_t brightness = 0x0, int color_temp = 0);

            void publish_component_config(std::string modelName, std::string srcAddress);
            void on_json_message(const std::string &topic, JsonObject &payload);
            void on_message(const std::string &topic, const std::string &payload);
        };

    }

}