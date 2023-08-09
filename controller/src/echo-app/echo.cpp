#include "echo.hpp"

echo::app::app(boost::asio::io_context& ioc, short port)
    : s_(ioc, port),
      echo_(
        [&](echo::app::coro_t::pull_type& source){
            for( auto rcvdmsg: source){
                sctp::sndinfo sndinfo = {
                    .snd_sid = rcvdmsg.rmt_endpt.rcvinfo.rcv_sid,
                    .snd_flags = 0,
                    .snd_ppid = rcvdmsg.rmt_endpt.rcvinfo.rcv_ppid,
                    .snd_context = rcvdmsg.rmt_endpt.rcvinfo.rcv_context,
                    .snd_assoc_id = rcvdmsg.rmt_endpt.rcvinfo.rcv_assoc_id
                };

                sctp::sctp_rmt_endpt dst_endpt = {
                    .endpt = rcvdmsg.rmt_endpt.endpt,
                    .sndinfo = sndinfo
                };

                sctp::sctp_message snd_msg = {
                    .rmt_endpt = dst_endpt,
                    .payload = rcvdmsg.payload
                };
                s_.do_write(snd_msg);      
            }
        }
      )
{
    loop();
}

void echo::app::loop(){
    while(true){
        sctp::sctp_message rcvdmsg = s_.do_read();
        echo_(rcvdmsg);
    }
}