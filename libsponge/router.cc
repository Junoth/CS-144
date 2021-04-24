#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

bool Router::is_prefix_match(const RouterInfo& rt_info, uint32_t dst_ip) {
    uint32_t rt_prefix = rt_info.route_prefix;
    uint8_t rt_prefix_len = rt_info.prefix_length;
    int i;

    for (i = 1; i <= rt_prefix_len; ++i) {
        uint32_t mask = 1U << (32 - i);
        if ((mask & rt_prefix) != (mask & dst_ip)) {
            return false;
        }
    }

    return true;
}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {

    RouterInfo rt_info = {route_prefix, prefix_length, next_hop, interface_num};
    _routers.push_back(rt_info);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    uint32_t dst_ip = dgram.header().dst;
    optional<RouterInfo> router_cand;

    if (dgram.header().ttl <= 1) {
        return;
    }

    for (auto it : _routers) {
        if (is_prefix_match(it, dst_ip)) {
            if (router_cand.has_value() && router_cand.value().prefix_length < it.prefix_length) {
                // router with larger prefix length
                router_cand = {it};
            } else {
                // no candidate now
                router_cand = {it};
            }
        }
    }

    if (router_cand.has_value()) {
        RouterInfo rt_info = router_cand.value();
        // send dgram to according network interface
        dgram.header().ttl--;
        Address next_hop = rt_info.next_hop.has_value()? rt_info.next_hop.value() : Address::from_ipv4_numeric(dst_ip);
        interface(router_cand.value().interface_num).send_datagram(dgram, next_hop);
    }   
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
