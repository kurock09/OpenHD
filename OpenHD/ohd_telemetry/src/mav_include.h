//
// Created by consti10 on 13.04.22.
//

#ifndef XMAVLINKSERVICE_MAV_INCLUDE_H
#define XMAVLINKSERVICE_MAV_INCLUDE_H

extern "C" {
//NOTE: Make sure to include the openhd mavlink flavour, otherwise the custom messages won't bw parsed.
#include <openhd/mavlink.h>
}

#include <vector>
#include <functional>
#include <chrono>
#include <memory>

// OpenHD mavlink sys IDs
// Any mavlink message generated by openhd on the ground unit uses this sys id
static constexpr auto OHD_SYS_ID_GROUND = 100;
// Any mavlink message generated by openhd on the air unit uses this sys id
static constexpr auto OHD_SYS_ID_AIR = 101;
static_assert(OHD_SYS_ID_GROUND != OHD_SYS_ID_AIR);
// Sys id of QOpenHD or any other gcs connected to the ground unit that talks mavlink
static constexpr auto QOPENHD_SYS_ID=255;

// dirty (hard coded for now). Pretty much all FCs default to a sys id of 1 - this works as long as long as the user doesn't change the sys id
static constexpr auto OHD_SYS_ID_FC=1;

static constexpr auto OHD_GROUND_CLIENT_UDP_PORT_OUT = 14550;
static constexpr auto OHD_GROUND_CLIENT_UDP_PORT_IN = 14551;

struct MavlinkMessage {
  mavlink_message_t m{};
  // how often this packet should be injected (increase reliability)
  int recommended_n_injections =1;
  [[nodiscard]] std::vector<uint8_t> pack() const {
	std::vector<uint8_t> buf(MAVLINK_MAX_PACKET_LEN);
	auto size = mavlink_msg_to_send_buffer(buf.data(), &m);
	buf.resize(size);
	return buf;
  }
};

// It is more efficient to aggregate / keep mavlink messages in chunks instead of using a wb packet for each of them.
struct AggregatedMavlinkPacket{
  std::shared_ptr<std::vector<uint8_t>> data;
  int recommended_n_retransmissions=1;
};
static std::vector<AggregatedMavlinkPacket> aggregate_pack_messages(const std::vector<MavlinkMessage>& messages,uint32_t max_mtu=1024){
  std::vector<AggregatedMavlinkPacket> ret;
  auto buff=std::make_shared<std::vector<uint8_t>>();;
  int recommended_n_retransmissions=1;
  buff->reserve(max_mtu);
  for(const auto& msg:messages){
    auto data=msg.pack();
    if(buff->size()+data.size()<=max_mtu){
      // we haven't reached MTU yet
      buff->insert(buff->end(), data.begin(), data.end());
      if(msg.recommended_n_injections>recommended_n_retransmissions){
        recommended_n_retransmissions=msg.recommended_n_injections;
      }
    }else{
      // MTU is reached or we need to allocate a new buffer
      if(!buff->empty()){
        ret.push_back({buff,recommended_n_retransmissions});
        buff=std::make_shared<std::vector<uint8_t>>();
        buff->reserve(max_mtu);
        recommended_n_retransmissions=1;
      }
      buff->insert(buff->end(), data.begin(), data.end());
      if(msg.recommended_n_injections>recommended_n_retransmissions){
        recommended_n_retransmissions=msg.recommended_n_injections;
      }
    }
  }
  if(!buff->empty()){
    ret.push_back({buff,recommended_n_retransmissions});
  }
  return ret;
}

static int get_size(const std::vector<MavlinkMessage>& messages){
  int ret=0;
  for(const auto& message:messages){
    ret+=message.pack().size();
  }
  return ret;
}

// For registering a callback that is called every time component X receives one or more mavlink messages
typedef std::function<void(const std::vector<MavlinkMessage> messages)> MAV_MSG_CALLBACK;

static int64_t get_time_microseconds(){
  const auto time=std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(time).count();
}

#endif //XMAVLINKSERVICE_MAV_INCLUDE_H
