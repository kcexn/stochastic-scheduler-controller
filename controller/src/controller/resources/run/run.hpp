#ifndef CONTROLLER_RUN_HPP
#define CONTROLLER_RUN_HPP
#include <cstdint>
#include <boost/json.hpp>
#include <boost/context/fiber.hpp>
#include <string>
#include "../../app/controller-app.hpp"

namespace controller{
namespace resources{
namespace run{

    struct Response{
        // Status can take values:
        // "success" or "application error" or "action developer error" or "whisk internal error".
        boost::json::string status;
        // status code can take values:
        // 0 : success
        // 1 : application error
        // 2 : action developer error
        // 3 : whisk internal error
        std::int64_t status_code;
        // success is true if and only if status is success.
        bool success;
        // result: is a json object holding the results.
        boost::json::object result;
    };

    struct ActivationRecord{
        boost::json::string activation_id;
        boost::json::string name_space;
        boost::json::string action_name;
        std::int64_t start_time;
        std::int64_t end_time;
        boost::json::array logs;
        boost::json::array annotations;
        boost::json::object response;
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

    // boost::context::fiber handle( Request& req, std::shared_ptr<controller::app::AppMbox>& mbox_ptr );
    std::shared_ptr<controller::app::ExecutionContext> handle( Request& req );

}//namespace run
}//namespace resources
}//namespace controller
#endif