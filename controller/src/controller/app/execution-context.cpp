#include "execution-context.hpp"
#include <filesystem>
#include <fstream>
#include <boost/json.hpp>
#include "action-relation.hpp"

namespace controller{
namespace app{
    // Execution Context
    ExecutionContext::ExecutionContext(ExecutionContext::Init init)
      : execution_context_id_(UUID::Uuid(UUID::Uuid::v4)),
        execution_context_idx_stack_{0},
        route_{controller::resources::Routes::INIT}
    {}

    ExecutionContext::ExecutionContext(ExecutionContext::Run run)
      : execution_context_id_(UUID::Uuid(UUID::Uuid::v4)),
        execution_context_idx_stack_{0},
        route_{controller::resources::Routes::RUN}
    {
        const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
        if ( __OW_ACTIONS == nullptr ){
            throw "Environment variable __OW_ACTIONS not defined!";
        }
        std::filesystem::path action_path(__OW_ACTIONS);
        std::filesystem::path manifest_path(action_path / "action-manifest.json");
        if (std::filesystem::exists(manifest_path)){
            std::fstream f(manifest_path, std::ios_base::in);
            boost::json::error_code ec;
            boost::json::value tmp = boost::json::parse(f,ec);
            boost::json::object manifest(tmp.as_object());
            if (ec){
                throw "boost json parse failed.";
            }
            // If the manifest is empty throw an exception.
            if(manifest.empty()){
                throw "action-manifest.json can't be empty.";
            }
            // Loop through manifest.json until manifest_ contains the same number of keys.
            while(manifest_.size() < manifest.size()){
                // Find a key that isn't in the manifest_ yet.
                auto it = std::find_if(manifest.begin(), manifest.end(), [&](auto& kvp){
                    auto tmp = std::find_if(manifest_.begin(), manifest_.end(), [&](auto& rel){
                        return rel->key() == kvp.key();
                    });
                    return (tmp != manifest_.end()) ? false : true;
                });
                // Insert it into the manifest, using a recursive tree traversal method.
                std::string key(it->key());
                manifest_.emplace(key, manifest); 
            }
            // Reverse lexicographically sort the manifest.
            std::sort(manifest_.begin(), manifest_.end(), [&](std::shared_ptr<Relation> a, std::shared_ptr<Relation> b){
                return a->depth() > b->depth();
            });
        } else {
            const char* __OW_ACTION_EXT = getenv("__OW_ACTION_EXT");
            if ( __OW_ACTION_EXT == nullptr ){
                throw "Environment variable __OW_ACTION_EXT is not defined!";
            }
            // By default the file is called "main" + __OW_ACTION_EXT.
            // e.g. "main.lua", or "main.py", or "main.js".
            std::string filename("main");
            filename.append(".");
            filename.append(__OW_ACTION_EXT);
            std::filesystem::path fn_path(action_path / filename);

            const char* __OW_ACTION_ENTRY_POINT = getenv("__OW_ACTION_ENTRY_POINT");
            std::string entrypoint;
            if ( __OW_ACTION_ENTRY_POINT == nullptr ){
                // By default, the entry point is called main.
                entrypoint = std::string("main");
            } else {
                entrypoint = std::string(__OW_ACTION_ENTRY_POINT);
            }
            // By default, the entry point has no dependencies.
            manifest_.push_back( std::make_shared<Relation>(std::move(entrypoint), std::move(fn_path), std::vector<std::shared_ptr<Relation> >()));
        }
    }

    ExecutionContext::ExecutionContext(ExecutionContext::Run run, const UUID::Uuid& uuid)
      : execution_context_id_(uuid),
        execution_context_idx_stack_{0},
        route_{controller::resources::Routes::RUN}
    {
        const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
        if ( __OW_ACTIONS == nullptr ){
            throw "Environment variable __OW_ACTIONS not defined!";
        }
        std::filesystem::path action_path(__OW_ACTIONS);
        std::filesystem::path manifest_path(action_path / "action-manifest.json");
        if (std::filesystem::exists(manifest_path)){
            std::fstream f(manifest_path, std::ios_base::in);
            boost::json::error_code ec;
            boost::json::value tmp = boost::json::parse(f,ec);
            boost::json::object manifest(tmp.as_object());
            if (ec){
                throw "boost json parse failed.";
            }
            // If the manifest is empty throw an exception.
            if(manifest.empty()){
                throw "action-manifest.json can't be empty.";
            }
            // Loop through manifest.json until manifest_ contains the same number of keys.
            while(manifest_.size() < manifest.size()){
                // Find a key that isn't in the manifest_ yet.
                auto it = std::find_if(manifest.begin(), manifest.end(), [&](auto& kvp){
                    auto tmp = std::find_if(manifest_.begin(), manifest_.end(), [&](auto& rel){
                        return rel->key() == kvp.key();
                    });
                    return (tmp != manifest_.end()) ? false : true;
                });
                // Insert it into the manifest, using a recursive tree traversal method.
                std::string key(it->key());
                manifest_.emplace(key, manifest); 
            }
            // Reverse lexicographically sort the manifest.
            std::sort(manifest_.begin(), manifest_.end(), [&](std::shared_ptr<Relation> a, std::shared_ptr<Relation> b){
                return a->depth() > b->depth();
            });
        } else {
            const char* __OW_ACTION_EXT = getenv("__OW_ACTION_EXT");
            if ( __OW_ACTION_EXT == nullptr ){
                throw "Environment variable __OW_ACTION_EXT is not defined!";
            }
            // By default the file is called "main" + __OW_ACTION_EXT.
            // e.g. "main.lua", or "main.py", or "main.js".
            std::string filename("main");
            filename.append(".");
            filename.append(__OW_ACTION_EXT);
            std::filesystem::path fn_path(action_path / filename);

            const char* __OW_ACTION_ENTRY_POINT = getenv("__OW_ACTION_ENTRY_POINT");
            std::string entrypoint;
            if ( __OW_ACTION_ENTRY_POINT == nullptr ){
                // By default, the entry point is called main.
                entrypoint = std::string("main");
            } else {
                entrypoint = std::string(__OW_ACTION_ENTRY_POINT);
            }
            // By default, the entry point has no dependencies.
            manifest_.push_back( std::make_shared<Relation>(std::move(entrypoint), std::move(fn_path), std::vector<std::shared_ptr<Relation> >()));
        }
    }

    bool operator==(const ExecutionContext& lhs, const ExecutionContext& rhs){
        return lhs.execution_context_id() == rhs.execution_context_id();
    }

    bool ExecutionContext::is_stopped() {
        for(auto& thread_control: thread_controls_){
            if (!(thread_control.is_stopped())){
                return false;
            }
        }
        return true;
    }

    std::size_t ExecutionContext::pop_execution_idx() {
        if (execution_context_idx_stack_.empty()){
            throw "Execution Context idx stack shouldn't be empty when calling pop.";
        }
        std::size_t idx = execution_context_idx_stack_.back();
        execution_context_idx_stack_.pop_back();
        return idx;
    }

    void ExecutionContext::push_execution_idx(std::size_t idx){
        execution_context_idx_stack_.push_back(idx);
        return;
    }
} //namespace app
} //namespace controller