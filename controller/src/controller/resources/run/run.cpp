#include "run.hpp"

#ifdef DEBUG
#include <iostream>
#endif

namespace controller{
namespace resources{
namespace run{
    boost::context::fiber handle(Request& req ){
        boost::context::fiber f{
            [&, req](boost::context::fiber&& g){
                boost::json::value jv(boost::json::object_kind);
                jv = req.value();
                std::string params = boost::json::serialize(jv);
                for(int i=0; i < 2; ++i){
                    g = std::move(g).resume();
                    #ifdef DEBUG
                    std::cout << params << " : " << i << std::endl;
                    #endif
                }
                return std::move(g);
            }
        };
        return f;
    }
}//namespace run
}//namespace resources
}//namespace controller