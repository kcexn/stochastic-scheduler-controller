#ifndef CONTROLLER_INIT_HPP
#define CONTROLLER_INIT_HPP
#include <cstdint>
#include <string>
#include <boost/json.hpp>
#include <memory>
#include "../../app/controller-app.hpp"


namespace controller{
namespace resources{
namespace init{

    struct EnvironmentVariables
    {
        boost::json::string ow_api_key;
        boost::json::string ow_action_name;
        boost::json::string ow_action_version;
        boost::json::string ow_activation_id;
        std::int64_t ow_deadline;
    };

    class InitValue
    {
    public:
        InitValue(
            boost::json::string name,
            boost::json::string main,
            boost::json::string code,
            bool binary,
            EnvironmentVariables env
        ) : name_(name),
            main_(main),
            code_(code),
            binary_(binary),
            env_(env)
        {}

        boost::json::string& name() { return name_; }
        boost::json::string& main() { return main_; }
        boost::json::string& code() { return code_; }
        bool& binary() { return binary_; }
        EnvironmentVariables& env() { return env_; }
    private:
        boost::json::string name_;
        boost::json::string main_;
        boost::json::string code_;
        bool binary_;
        EnvironmentVariables env_;
    };

    class Request
    {
    public:
        Request( boost::json::object obj )
          : value_(
            obj["value"].as_object()["name"].as_string(),
            obj["value"].as_object()["main"].as_string(),
            obj["value"].as_object()["code"].as_string(),
            obj["value"].as_object()["binary"].as_bool(),
            EnvironmentVariables{
                .ow_api_key = obj["value"].as_object()["env"].as_object()["__OW_API_KEY"].as_string(),
                .ow_action_name = obj["value"].as_object()["env"].as_object()["__OW_ACTION_NAME"].as_string(),
                .ow_action_version = obj["value"].as_object()["env"].as_object()["__OW_ACTION_VERSION"].as_string(),
                .ow_activation_id = obj["value"].as_object()["env"].as_object()["__OW_ACTIVATION_ID"].as_string(),
                .ow_deadline = obj["value"].as_object()["env"].as_object()["__OW_DEADLINE"].as_int64()
            }
          )
        {}

        InitValue& value() { return value_; }
    private:
        InitValue value_;
    };

    void handle( Request& req );

}// init namespace
}// resources namespace
}// controller namespace
#endif