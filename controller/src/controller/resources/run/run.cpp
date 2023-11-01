#include "run.hpp"
#include "../../app/action-relation.hpp"
#include "../../app/execution-context.hpp"
#include <csignal>
#include <boost/asio.hpp>
#include <poll.h>
#include <sys/eventfd.h>
#include <cstdint>

namespace controller{
namespace resources{
namespace run{
    Request::Request( boost::json::object& obj )
    {
        /*{"value":{"execution_context":{"uuid":"a70ea480860c45e19a5385c68188d1ff","idx":0,"peers":[],"value":{}}},
        "namespace":"guest","action_name":"test","api_host":"localhost","api_key":"akey","activation_id":"activation","transaction_id":"transaction","deadline":123456789}*/
        boost::json::object& value = obj.at("value").as_object();
        if (!value.contains("execution_context")){
            value_ = boost::json::object(value);
        } else {
            boost::json::object& context = value.at("execution_context").as_object();
            std::stringstream uuid(std::string(context.at("uuid").as_string()));
            uuid >> execution_context_id_;
            boost::json::value& idx = context.at("idx");
            if (idx.is_int64()){
                execution_context_idx_ = idx.get_int64();
            } else if (idx.is_uint64()){
                execution_context_idx_ = idx.get_uint64();
            } else {
                std::cerr << "run.cpp:31:execution context index is too large." << std::endl;
                throw "execution context index is too large.";
            }
            boost::json::array& peers = context.at("peers").as_array();
            for(auto& peer: peers){
                peers_.emplace_back(peer.as_string());
            }
            value_ = boost::json::object(context.at("value").as_object());
        }
        for ( auto& kvp: obj ){
            std::string key(kvp.key());
            if ( key != "value" ){
                std::string val;
                if ( kvp.value().is_string() ){
                    val = std::string( kvp.value().get_string() );
                } else if ( kvp.value().is_int64() ){
                    val = std::to_string( kvp.value().get_int64() );
                } else if ( kvp.value().is_uint64() ){
                    val = std::to_string( kvp.value().get_uint64() );
                } else if ( kvp.value().is_double() ){
                    val = std::to_string( kvp.value().get_double() );
                }
                std::string envkey("__OW_");
                std::transform(key.cbegin(), key.cend(), key.begin(), []( unsigned char c ) { return std::toupper(c); });
                envkey.append(key);
                env_.emplace(envkey, val);
            }
        }
    }

    std::shared_ptr<controller::app::ExecutionContext> handle(Request& req, std::vector<std::shared_ptr<controller::app::ExecutionContext> >& ctx_ptrs){
        std::shared_ptr<controller::app::ExecutionContext> ctx_ptr;
        if(req.execution_context_id() != UUID::Uuid()){
            auto it = std::find_if(ctx_ptrs.begin(), ctx_ptrs.end(), [&](auto ctx_ptr){
                return (ctx_ptr->execution_context_id() == req.execution_context_id());
            });
            if( it != ctx_ptrs.end()){
                (*it)->execution_context_idx_array().push_back(req.idx());
                (*it)->push_execution_idx(req.idx());
                return std::shared_ptr<controller::app::ExecutionContext>(*it);
            }else{
                ctx_ptr = std::make_shared<controller::app::ExecutionContext>(controller::app::ExecutionContext::run, req.execution_context_id(), req.idx(), req.peers());
            }
        } else {
            ctx_ptr = std::make_shared<controller::app::ExecutionContext>(controller::app::ExecutionContext::run);
        }
        for (auto& relation: ctx_ptr->manifest()){
            ctx_ptr->thread_controls().emplace_back();
            auto& thread_control = ctx_ptr->thread_controls().back();
            thread_control.relation = relation;
            thread_control.env = req.env();
            thread_control.params = req.value();
        }
        return ctx_ptr;
    }
}//namespace run
}//namespace resources
}//namespace controller