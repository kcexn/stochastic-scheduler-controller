#include "uuid-tests.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace tests{
    Uuid::Uuid()
      : passed_{false},
        uuid_()
    {
        for(int i=0; i < (UUID::Uuid::size); ++i){
            if(uuid_.bytes[i] != 0){
                return;
            }
        }
        passed_ = true;
        return;
    }
}