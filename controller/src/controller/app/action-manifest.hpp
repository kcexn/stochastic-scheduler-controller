#ifndef ACTION_MANIFEST_HPP
#define ACTION_MANIFEST_HPP
#include <string>
#include <memory>
#include <vector>

/* Forward Declarations */
namespace boost{
namespace json{
    class object;
}
}
namespace controller{
namespace app{
    class Relation;
}
}
namespace http{
    class HttpSession;
}

namespace controller{
namespace app{
    class ActionManifest
    {
    public:
        ActionManifest();
        void emplace(const std::string& key, const boost::json::object& manifest);
        std::shared_ptr<Relation> next(const std::string& key, const std::size_t& idx);
        std::size_t& concurrency(){ return concurrency_; }
        const std::vector<std::shared_ptr<Relation> >& index(){ return index_; }

        // Reexport the std::vector interface.
        std::vector<std::shared_ptr<Relation> >::iterator begin();
        std::vector<std::shared_ptr<Relation> >::iterator end();
        std::vector<std::shared_ptr<Relation> >::const_iterator cbegin();
        std::vector<std::shared_ptr<Relation> >::const_iterator cend();
        std::vector<std::shared_ptr<Relation> >::reverse_iterator rbegin();
        std::vector<std::shared_ptr<Relation> >::reverse_iterator rend();
        std::vector<std::shared_ptr<Relation> >::const_reverse_iterator crbegin();
        std::vector<std::shared_ptr<Relation> >::const_reverse_iterator crend();

        std::shared_ptr<Relation>& operator[](std::vector<std::shared_ptr<Relation> >::size_type pos);
        
        void push_back(const std::shared_ptr<Relation>& relation) { index_.push_back(relation); return; }
        void push_back(std::shared_ptr<Relation>&& relation) { index_.push_back(relation); return; }

        std::vector<std::shared_ptr<Relation> >::size_type size(){ return index_.size(); }
    private:
        std::size_t concurrency_;
        std::vector<std::shared_ptr<Relation> > index_;
    };
}//namespace app
}//namespace controller
#endif