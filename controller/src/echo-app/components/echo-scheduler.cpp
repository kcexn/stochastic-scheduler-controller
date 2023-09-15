#include "echo-scheduler.hpp"
#include <boost/asio.hpp>
#include "../utils/common.hpp"
#include "../../controller/io/controller-io.hpp"
#include "../../controller/app/controller-app.hpp"
#include <filesystem>

#ifdef DEBUG
#include <iostream>
#endif

namespace echo{
    Scheduler::Scheduler(
        boost::asio::io_context& ioc, 
        std::shared_ptr<std::mutex> signal_mtx_ptr,
        std::shared_ptr<std::atomic<int> > signal_ptr,
        std::shared_ptr<std::condition_variable> signal_cv_ptr
    ) : signal_mtx_ptr_(signal_mtx_ptr),
        signal_ptr_(signal_ptr),
        signal_cv_ptr_(signal_cv_ptr),
        controller_mbox_ptr_(std::make_shared<MailBox>()),
        controller_ptr_(std::make_shared<controller::app::Controller>(
            controller_mbox_ptr_,
            ioc
        )),
        ioc_(ioc) 
    {
        #ifdef DEBUG
        std::cout << "Scheduler Constructor!" << std::endl;
        #endif
        controller_mbox_ptr_->sched_signal_mtx_ptr = signal_mtx_ptr_;
        controller_mbox_ptr_->sched_signal_ptr = signal_ptr_;
        controller_mbox_ptr_->sched_signal_cv_ptr = signal_cv_ptr_;
    }

    void Scheduler::start(){
        while(!(signal_ptr_->load() & Signals::TERMINATE)){
            std::unique_lock<std::mutex> lk(*signal_mtx_ptr_);
            signal_cv_ptr_->wait(lk, [&]{ return signal_ptr_->load() != 0; });
            lk.unlock();
        }
    }



    #ifdef DEBUG
    Scheduler::~Scheduler(){
        std::cout << "Scheduler Destructor!" << std::endl;
    }
    #endif
}//namespace echo.