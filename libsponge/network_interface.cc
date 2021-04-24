#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void NetworkInterface::push_ip_datagram(const InternetDatagram &dgram, const EthernetAddress& dst_addr) {
    // initialize the header(dst, src, type)
    EthernetHeader header = {dst_addr, _ethernet_address, EthernetHeader::TYPE_IPv4};
    // initialize the frame(header, payload)
    EthernetFrame frame(header, dgram.serialize());

    _frames_out.push(frame);
}

void NetworkInterface::push_arp_reply(const ARPMessage &arp_msg) {
    // initialize the arp reply
    ARPMessage reply_msg(ARPMessage::OPCODE_REPLY, _ethernet_address, _ip_address.ipv4_numeric(), arp_msg.sender_ethernet_address, arp_msg.sender_ip_address);
    // initialize the header(dst, src, type)
    EthernetHeader header = {arp_msg.sender_ethernet_address, _ethernet_address, EthernetHeader::TYPE_ARP};
    // initialize the frame(header, payload)
    EthernetFrame frame(header, BufferList(reply_msg.serialize()));

    _frames_out.push(frame);
}

void NetworkInterface::broadcast(const uint32_t ip_addr) {
    // initialize the arp broadcast request
    ARPMessage arp_msg(ARPMessage::OPCODE_REQUEST, _ethernet_address, _ip_address.ipv4_numeric(), ip_addr);
    // initialize the header(dst, src, type)
    EthernetHeader header = {ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};
    // initialize the frame(header, payload)
    EthernetFrame frame(header, BufferList(arp_msg.serialize()));

    _frames_out.push(frame);
}

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    if (ip2Ethernet.find(next_hop_ip) != ip2Ethernet.end()) {
        // the address is in cache, send frame directly
        push_ip_datagram(dgram,ip2Ethernet[next_hop_ip].second);
    } else if (ip2queue.find(next_hop_ip) == ip2queue.end()) {
        // we haven't tried to send ARP request for this ip, try to search it
        broadcast(next_hop_ip);

        // put the datagram into queue map
        queue<InternetDatagram> dgram_queue;
        dgram_queue.push(dgram);
        ip2queue[next_hop_ip] = make_pair(0, dgram_queue);
    } else {
        // we have tried to send ARP request for this ip, check the interval
        const uint64_t interval = ip2queue[next_hop_ip].first;
        if (interval >= _arp_resend_interval) {
            // interval exceeds, resend broadcast and reset time
            broadcast(next_hop_ip);
            ip2queue[next_hop_ip].first = 0;
        }

        // add datagram to the queue
        ip2queue[next_hop_ip].second.push(dgram);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    EthernetHeader header = frame.header();
    if (ethernet_addr_equals(header.dst, ETHERNET_BROADCAST) || ethernet_addr_equals(header.dst, _ethernet_address)) {
        // dst ethernet address should be either broadcast arp request or hardware address of the interface
        if (header.type == EthernetHeader::TYPE_IPv4) {
            InternetDatagram dgram;
            if ((dgram.parse(frame.payload())) == ParseResult::NoError) {
                // parse success, return the datagram
                return {dgram};
            }
        } else if (header.type == EthernetHeader::TYPE_ARP) {
            // cerr<<"DEBUG: Type ARP "<<to_string(header.dst)<<endl;
            ARPMessage arp_msg;
            if ((arp_msg.parse(frame.payload())) == ParseResult::NoError) {
                // parse success
                // cerr<<"DEBUG: Parse succss "<<to_string(header.dst)<<" opcode: "<<arp_msg.opcode<<endl;

                // update our ip2Ethernet map
                if (ip2Ethernet.find(arp_msg.sender_ip_address) == ip2Ethernet.end()) {
                    // not in our map, store it
                    ip2Ethernet[arp_msg.sender_ip_address] = make_pair(0, arp_msg.sender_ethernet_address);
                } else {
                    // reset the time
                    ip2Ethernet[arp_msg.sender_ip_address].first = 0;
                }

                if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST) {
                    // ARP request, if it's asking for our address, then we should return an approriate ARP reply
                    // cerr<<"DEBUG: push reply "<<to_string(header.dst)<<" opcode: "<<arp_msg.opcode<<endl;
                    if (arp_msg.target_ip_address == _ip_address.ipv4_numeric()) {
                        push_arp_reply(arp_msg);
                    }
                } else {
                    // ARP reply, we should check our queue, send those stored datagrams
                    if (ip2queue.find(arp_msg.sender_ip_address) != ip2queue.end()) {
                        queue<InternetDatagram> stored_queue = ip2queue[arp_msg.sender_ip_address].second;
                        while (!stored_queue.empty()) {
                            InternetDatagram dgram = stored_queue.front();
                            push_ip_datagram(dgram, arp_msg.sender_ethernet_address);
                            stored_queue.pop();
                        }
                        ip2queue.erase(arp_msg.sender_ip_address);
                    }
                }
            }
        } else {
            // another unsupported type
            cerr<<"The type of header is "<<header.type<<" which is not supported"<<endl;
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    unordered_map<uint32_t, std::pair<uint64_t, EthernetAddress>>::iterator mapping_it = ip2Ethernet.begin();
    std::unordered_map<uint32_t, std::pair<uint64_t, std::queue<InternetDatagram>>>::iterator queue_it = ip2queue.begin();

    // check our ip2ethernet map, clear outdated data
    while (mapping_it != ip2Ethernet.end()) {
        mapping_it->second.first += ms_since_last_tick;
        if (mapping_it->second.first > _ip_to_ethernet_mapping_outdate_interval) {
            // outdated data, erase from map
            mapping_it = ip2Ethernet.erase(mapping_it);
        } else {
            ++mapping_it;
        }
    }

    // check our queue map, add interval
    while (queue_it != ip2queue.end()) {
        queue_it->second.first += ms_since_last_tick;
        ++queue_it;
    }
 }
