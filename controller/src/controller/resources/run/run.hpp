#ifndef CONTROLLER_RUN_HPP
#define CONTROLLER_RUN_HPP
#include <cstdint>
#include <boost/json.hpp>
#include <boost/context/fiber.hpp>
#include <vector>
#include <string>
#include <functional>

namespace controller{
namespace resources{
namespace run{

    class Response
    {
    public:
        Response(boost::json::object obj)
          : status_(obj["status"].as_string()),
            status_code_(obj["status_code"].as_int64()),
            success_(obj["success"].as_bool()),
            result_(obj["result"].as_object())
        {}
    private:
        std::string status_;
        std::int64_t status_code_;
        bool success_;
        boost::json::object result_;
    };

    class ActivationRecord{
    public:
        ActivationRecord(boost::json::object obj)
          : activation_id_(obj["activation_id"].as_string()),
            ns_(obj["namespace"].as_string()),
            action_name_(obj["name"].as_string()),
            start_time_(obj["start"].as_int64()),
            end_time_(obj["end"].as_int64()),
            logs_(obj["logs"].as_array()),
            annotations_(obj["annotations"].as_array()),
            response_(obj["response"].as_object())
        {}

    private:
        std::string activation_id_;
        std::string ns_;
        std::string action_name_;
        std::int64_t start_time_;
        std::int64_t end_time_;
        boost::json::array logs_;
        boost::json::array annotations_;
        Response response_;
    };

    class Request{
    public:
        Request(boost::json::object obj)
          : value_(obj["value"].as_object()),
            ns_(obj["namespace"].as_string()),
            action_name_(obj["action_name"].as_string()),
            api_host_(obj["api_host"].as_string()),
            api_key_(obj["api_key"].as_string()),
            activation_id_(obj["activation_id"].as_string()),
            transaction_id_(obj["transaction_id"].as_string()),
            deadline_(obj["deadline"].as_int64())
        {}

        const boost::json::object& value() const noexcept { return value_; }
        const std::string& name_space() const noexcept { return ns_; }
        const std::string& action_name() const noexcept { return action_name_; }
        const std::string& api_host() const noexcept { return api_host_; }
        const std::string& api_key()const noexcept { return api_key_; }
        const std::string& activation_id() const noexcept { return activation_id_; }
        const std::string& transaction_id() const noexcept { return transaction_id_; }
        const std::int64_t& deadline() const noexcept { return deadline_; }
    private:
        boost::json::object value_;
        std::string ns_;
        std::string action_name_;
        std::string api_host_;
        std::string api_key_;
        std::string activation_id_;
        std::string transaction_id_;
        std::int64_t deadline_;
    };

    boost::context::fiber handle( Request& req );

}//namespace run
}//namespace resources
}//namespace controller
#endif