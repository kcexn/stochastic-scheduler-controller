#include "echo-application.hpp"
#include "echo-worker.hpp"

echo::App::App(
    std::vector<std::shared_ptr<MailBox> >& results,
    std::vector<echo::ExecutionContext>& context_table,
    std::shared_ptr<sctp_server::server> s_ptr
) : results_(results),
    context_table_(context_table),
    s_ptr_(s_ptr)
{
    #ifdef DEBUG
    std::cout << "Echo Application Constructor!" << std::endl;
    #endif
}

#ifdef DEBUG
echo::App::~App(){
    std::cout << "Echo Application Destructor!" << std::endl;
}
#endif

// Schedule an Echo Application.
sctp::sctp_message echo::App::schedule(sctp::sctp_message& rcvdmsg)
{
    #ifdef DEBUG
    std::cout << "Entered the Echo Scheduler." << std::endl;
    #endif

    // Begin processing the echo application requests.
    echo::ExecutionContext context(
        rcvdmsg.rmt_endpt.rcvinfo.rcv_assoc_id,
        rcvdmsg.rmt_endpt.rcvinfo.rcv_sid
    );
    std::size_t context_idx = 0;
    for(; context_idx < context_table_.size(); ++context_idx){
        if (context_table_[context_idx] == context){
            break;
        }
    }
    if (context_idx == context_table_.size()){
        // stream isn't in the table.
        context_table_.push_back(std::move(context));
        std::shared_ptr<echo::MailBox> mbox_ptr = std::make_shared<echo::MailBox>();
        results_.push_back(std::move(mbox_ptr));

        #ifdef DEBUG
        std::cout << "Initialize the echo input." << std::endl;
        #endif

        results_.back()->rcvdmsg = rcvdmsg;
        results_.back()->msg_flag.store(true);
        results_.back()->payload_buffer_ptr = std::make_shared<std::vector<char> >(rcvdmsg.payload.size());

        echo::Worker worker(results_.back());
        std::thread t(
            &echo::Worker::start, std::move(worker)
        );

        // retrieve sendmessage from thread.
        std::unique_lock<std::mutex> lk(results_.back()->mbx_mtx);
        results_.back()->mbx_cv.wait(lk, [&](){ return results_.back()->msg_flag.load() == false; });
        sctp::sctp_message sndmsg = results_.back()->sndmsg;
        lk.unlock();

        context_table_.back().set_tid(t.native_handle());
        t.detach();
        return sndmsg;
    } else {
        // stream is already in the stream table.
        std::unique_lock<std::mutex> lk(results_[context_idx]->mbx_mtx);
        results_[context_idx]->rcvdmsg = rcvdmsg;
        lk.unlock();
        results_[context_idx]->msg_flag.store(true);
        results_[context_idx]->mbx_cv.notify_all();

        // Acquire the condition variable and store the output.
        lk.lock();
        results_[context_idx]->mbx_cv.wait(lk, [&](){ return results_[context_idx]->msg_flag.load() == false; });
        sctp::sctp_message sndmsg = results_[context_idx]->sndmsg;
        lk.unlock();
        return sndmsg;
    }
}   

void echo::App::deschedule(echo::ExecutionContext context)
{
    auto ctx_it = std::find(context_table_.begin(), context_table_.end(), context);
    if ( ctx_it == context_table_.end() ){
        // The stream has already been descheduled.
    } else{
        sctp::assoc_t assoc_id = ctx_it->assoc_id();
        auto results_it = std::find_if(results_.begin(), results_.end(), [&](auto val){ return val->rcvdmsg.rmt_endpt.rcvinfo.rcv_assoc_id == assoc_id; });
        sctp::endpoint remote = (*results_it)->rcvdmsg.rmt_endpt.endpt;
        s_ptr_->shutdown_read(remote, assoc_id);
        (*results_it)->signal.store(echo::Signals::TERMINATE);
        (*results_it)->mbx_cv.notify_all();

        #ifdef DEBUG
        std::cout << "Removing the context from the context table." << std::endl;
        #endif
        results_.erase(results_it);
        context_table_.erase(ctx_it);
    }
}