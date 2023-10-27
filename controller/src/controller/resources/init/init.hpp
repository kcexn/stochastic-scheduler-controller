#ifndef CONTROLLER_RESOURCES_INIT_HPP
#define CONTROLLER_RESOURCES_INIT_HPP
#include <boost/json.hpp>
#include <map>
#include <memory>

/* Forward Declarations */
namespace controller{
namespace app{
    class ExecutionContext;
}
}

namespace controller{
namespace resources{
namespace init{
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

        const std::string& name() const { return name_; }
        const std::string& main() const { return main_; }
        const std::string& code() const { return code_; }
        const bool& binary() const { return binary_; }
        void env_emplace(std::string lhs, std::string rhs) { env_.emplace(lhs, rhs); }
        const std::map<std::string, std::string>& env() const { return env_; }
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
        const InitValue& value() const { return value_; }
    private:
        InitValue value_;
    };

    std::shared_ptr<controller::app::ExecutionContext> handle( Request& req);
    void base64extract(const std::string& filename, int pipefd_down[2], int pipefd_up[2], const Request& req);
    void tar_extract(const std::string& filename);
}// init namespace
}// resources namespace
}// controller namespace
#endif
