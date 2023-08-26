#ifndef CONTROLLER_INIT_HPP
#define CONTROLLER_INIT_HPP
#include <cstdint>
#include <string>
#include <boost/json.hpp>
#include <memory>
#include <map>
#include "../../app/controller-app.hpp"


namespace controller{
namespace resources{
namespace init{

    // struct EnvironmentVariables
    // {
    //     boost::json::string ow_api_key;
    //     boost::json::string ow_namespace;
    //     boost::json::string ow_action_name;
    //     boost::json::string ow_action_version;
    //     boost::json::string ow_activation_id;
    //     std::int64_t ow_deadline;
    // };

    class InitValue
    {
    public:
        InitValue(
            boost::json::string name,
            boost::json::string main,
            boost::json::string code,
            bool binary
        ) : name_(name),
            main_(main),
            code_(code),
            binary_(binary)
        {}

        std::string& name() { return name_; }
        std::string& main() { return main_; }
        std::string& code() { return code_; }
        bool& binary() { return binary_; }
        std::map<std::string, std::string>& env() { return env_; }
    private:
        std::string name_;
        std::string main_;
        std::string code_;
        bool binary_;
        std::map<std::string, std::string> env_;
    };

    class Request
    {
    public:
        Request( boost::json::object obj );
        InitValue& value() { return value_; }
    private:
        InitValue value_;
    };

    Http::Response handle( Request& req );

}// init namespace
}// resources namespace
}// controller namespace
#endif