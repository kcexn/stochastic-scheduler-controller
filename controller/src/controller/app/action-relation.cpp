#include "action-relation.hpp"

namespace controller{
namespace app{
    Relation::Relation()
      : kvp_(),
        dependencies_(),
        depth_{0},
        path_()
    {}


    Relation::Relation(const std::string& key, const std::filesystem::path& path, const std::vector<std::shared_ptr<Relation> >& dependencies)
      : kvp_(std::string(key), std::string() ),
        dependencies_(dependencies),
        depth_{1},
        path_(path)
    {
        for ( auto dependency: dependencies_ ){
            if ( dependency->depth() >= depth_ ){
                depth_ = dependency->depth() + 1;
            }
        }
    }

    Relation::Relation(std::string&& key, std::filesystem::path&& path, std::vector<std::shared_ptr<Relation> >&& dependencies)
      : kvp_(std::string(key), std::string() ),
        dependencies_(dependencies),
        depth_{1},
        path_(path)
    {
        for ( auto dependency: dependencies_ ){
            if ( dependency->depth() >= depth_ ){
                depth_ = dependency->depth() + 1;
            }
        }
    }

    Relation::Relation(const std::string& key, const std::string& value, const std::filesystem::path& path, const std::vector<std::shared_ptr<Relation> >& dependencies)
      : kvp_(std::string(key), std::string(value)),
        dependencies_(dependencies),
        depth_{1},
        path_(path)
    {
        for ( auto dependency: dependencies_ ){
            if ( dependency->depth() >= depth_ ){
                depth_ = dependency->depth() + 1;
            }
        }
    }

    Relation::Relation(std::string&& key, std::string&& value, std::filesystem::path&& path, std::vector<std::shared_ptr<Relation> >&& dependencies)
      : kvp_(std::string(key), std::string(value)),
        dependencies_(dependencies),
        depth_{1},
        path_(path)
    {
        for ( auto dependency: dependencies_ ){
            if ( dependency->depth() >= depth_ ){
                depth_ = dependency->depth() + 1;
            }
        }
    }
}//namespace app
}//namespace controller