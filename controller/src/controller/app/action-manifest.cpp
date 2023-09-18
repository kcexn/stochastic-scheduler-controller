#include "action-manifest.hpp"
#include <boost/json.hpp>
#include "action-relation.hpp"

#ifdef DEBUG
#include <iostream>
#endif

namespace controller{
namespace app{
    ActionManifest::ActionManifest()
      : index_()
    {}

    void ActionManifest::emplace(const std::string& key, const boost::json::object& manifest){
        // Search for key in index_
        auto it = std::find_if(index_.begin(), index_.end(),[&](auto& rel){
            return rel->key() == key;
        });
        // If the key isn't in the manifest, loop through
        // all of the dependencies, and recursively insert them.
        if ( it == index_.end() ){
            boost::json::array deps(manifest.at(key).as_object().at("depends").as_array());
            std::vector<std::shared_ptr<Relation> > dependencies;
            for (auto& dep: deps){
                std::string dep_key(dep.as_string());
                emplace(dep_key, manifest);
                // Find the emplaced key in the index.
                auto tmp = std::find_if(index_.begin(), index_.end(), [&](auto& rel){
                    return rel->key() == dep_key;
                });
                // Create a copy of the std::shared_ptr<Relation> and emplace 
                // it into the dependencies vector.
                dependencies.emplace_back(*tmp);
            }
            // This is the second recursive base case.
            // All of the dependencies have been emplaced, so this element can be constructed and emplaced now.
            std::string fname(manifest.at(key).as_object().at("file").as_string());
            const char* __OW_ACTIONS = getenv("__OW_ACTIONS");
            std::filesystem::path path(__OW_ACTIONS);
            path /= fname;
            
            std::shared_ptr<Relation> rel = std::make_shared<Relation>(key, path, dependencies);
            index_.push_back(std::move(rel));
            return;
        } else {
            // This is one of the recursive base cases. The key is already in the 
            // index, so don't do anything.
            return;
        }
    }

    std::shared_ptr<Relation> ActionManifest::next(const std::string& key, const std::size_t& idx){
        // Return the next task that needs to be completed in the list of dependences for 
        // "key".

        //Defensive programming, first check to see that there are still relations that need to finish executing.
        auto tmp = std::find_if(index_.begin(), index_.end(), [&](auto& rel){
            std::string value = rel->acquire_value();
            rel->release_value();
            if( value.empty() ){
                return true;
            } else {
                return false;
            }
        });
        if (tmp == index_.end()){
            // if there are no more relations to complete, return a default constructed relation.
            return std::make_shared<Relation>();
        }

        // Search for "key" in index.
        auto it = std::find_if(index_.begin(), index_.end(),[&](auto& rel){
            return rel->key() == key;
        });
        // If key isn't in index. Throw an exception, this shouldn't be possible.
        if (it == index_.end() ){
            throw "Key not in index!";
        }

        // First check to see if the relation at key has already been completed.
        std::string& value = (*it)->acquire_value();
        (*it)->release_value();
        if( !(value.empty())){
            // If the relation has already been completed, then recursively call next with 
            // the next relation modulo the index size.
            std::ptrdiff_t i = it - index_.begin();
            std::string k(index_[(++i)%index_.size()]->key());
            return next(k, idx);
        }
        // Otherwise the relation still needs to complete.

        // Iterate through all of the dependencies starting at
        // the index: idx (mod dependencies.size()).
        std::size_t num_deps = (*it)->size();
        if (num_deps == 0){
            // The first recursive base case.
            // The number of dependencies the current relation has is 0.
            // Therefore we can just directly return the relation.
            return std::shared_ptr<Relation>(*it);
        }
        std::size_t start = idx % num_deps;
        for (std::size_t offset=0; offset < num_deps; ++offset){
            std::size_t select = (start + offset)%num_deps;
            // If the dependency value is empty that means the dependency
            // has not finished computing yet. Call next recursively.
            if( (**it)[select]->acquire_value().empty() ){
                (**it)[select]->release_value();
                std::string dep_key((**it)[select]->key());
                return next(dep_key, idx);
            }
            (**it)[select]->release_value();
        }
        // The second recursive base case.
        // All of the dependencies have computed values. Therefore the next thing to do is compute the current relation.
        return std::shared_ptr<Relation>(*it);
    }

    std::vector<std::shared_ptr<Relation> >::iterator ActionManifest::begin() { return index_.begin(); }
    std::vector<std::shared_ptr<Relation> >::iterator ActionManifest::end() { return index_.end(); }
    std::vector<std::shared_ptr<Relation> >::const_iterator ActionManifest::cbegin() { return index_.cbegin(); }
    std::vector<std::shared_ptr<Relation> >::const_iterator ActionManifest::cend() { return index_.cend(); }
    std::vector<std::shared_ptr<Relation> >::reverse_iterator ActionManifest::rbegin() { return index_.rbegin(); }
    std::vector<std::shared_ptr<Relation> >::reverse_iterator ActionManifest::rend() { return index_.rend(); }
    std::vector<std::shared_ptr<Relation> >::const_reverse_iterator ActionManifest::crbegin() { return index_.crbegin(); }
    std::vector<std::shared_ptr<Relation> >::const_reverse_iterator ActionManifest::crend() { return index_.crend(); }
    std::shared_ptr<Relation>& ActionManifest::operator[](std::vector<std::shared_ptr<Relation> >::size_type pos){ return index_[pos]; }
}// namespace app
}// namespace controller