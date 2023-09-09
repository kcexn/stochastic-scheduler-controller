#include "../tests/uuid/uuid-tests.hpp"
#include <boost/asio.hpp>

int main(int argc, char* argv[]){
    tests::Uuid default_uuid;
    if (default_uuid){
        std::cout << "default uuid tests passed." << std::endl;
    } else {
        std::cerr << "Default uuid tests failed." << std::endl;
    }
    

    // UUID::Uuid uuid(UUID::Uuid::v4);
    // std::stringstream ss;
    // ss << uuid;
    // std::cout << uuid << std::endl;

    // UUID::Uuid uuid1;
    // ss >> uuid1;
    // std::cout << uuid1 << std::endl;

    // std::cout << std::boolalpha << (uuid == uuid1) << std::endl;
    // UUID::Uuid uuid2(UUID::Uuid::v4);
    // std::cout << std::boolalpha << (uuid1 == uuid2) << std::endl;

    return 0;
}