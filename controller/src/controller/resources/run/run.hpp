#ifndef CONTROLLER_RUN_HPP
#define CONTROLLER_RUN_HPP
#include <boost/json.hpp>
#include <string>
#include <map>
#include "../../app/controller-app.hpp"

namespace controller{
namespace resources{
namespace run{
    class Request{
    public:
        Request(boost::json::object obj);
        const boost::json::object& value() const { return value_; }
        const std::map<std::string, std::string>& env() const { return env_; }
    private:
        boost::json::object value_;
        std::map<std::string, std::string> env_;
    };
    std::shared_ptr<controller::app::ExecutionContext> handle( Request& req);
}//namespace run
}//namespace resources
}//namespace controller
#endif