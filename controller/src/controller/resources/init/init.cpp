#include "init.hpp"
#include <boost/context/fiber.hpp>
#include <fstream>

namespace controller{
namespace resources{
namespace init{
    void handle( Request& req ){
        std::string code(req.value().code());
        std::size_t pos = 0;
        while ( (pos = code.find("\\n", pos)) != std::string::npos ){
            code.replace(pos, 2, "\n");
            ++pos;
        }

        std::string filename("/workspaces/whisk-controller-dev/action-runtimes/python3/functions/fn_001.py");
        std::ofstream file(filename);
        file << code;
        return;
    }
}// namespace init
}// namespace resources
}// namespace controller

