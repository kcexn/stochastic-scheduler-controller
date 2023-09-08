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
        Request(boost::json::object& obj);
        const boost::json::object& value() const { return value_; }
        const std::map<std::string, std::string>& env() const { return env_; }
        const UUID::Uuid& execution_context_id() const { return execution_context_id_; }
        const std::size_t idx() const { return execution_context_idx_; }
    private:
        UUID::Uuid execution_context_id_;
        std::size_t execution_context_idx_;
        std::vector<std::string> peers_;
        boost::json::object value_;
        std::map<std::string, std::string> env_;
    };
    std::shared_ptr<controller::app::ExecutionContext> handle(Request& req, std::vector<std::shared_ptr<controller::app::ExecutionContext> >& ctx_ptrs);
}//namespace run
}//namespace resources
}//namespace controller
#endif