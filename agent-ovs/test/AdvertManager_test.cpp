/*
 * Test suite for class AdvertManager
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>
#include <boost/asio/ip/address.hpp>

#include <netinet/icmp6.h>
#include <netinet/ip.h>

#include "AdvertManager.h"
#include "ModbFixture.h"
#include "MockSwitchConnection.h"
#include "MockPortMapper.h"
#include "FlowManager.h"
#include "arp.h"
#include "logging.h"

using boost::asio::io_service;
using boost::bind;
using boost::ref;
using boost::thread;
using boost::asio::ip::address_v4;
using boost::asio::ip::address_v6;

#define CONTAINS(x, y) (x.find(y) != x.end())

class AdvertManagerFixture : public ModbFixture {
public:
    AdvertManagerFixture()
        : ModbFixture(), flowManager(agent),
          advertManager(agent, flowManager),
          proto(ofputil_protocol_from_ofp_version(conn.GetProtocolVersion())) {
        flowManager.SetPortMapper(&portMapper);
        flowManager.SetEncapIface("br0_vxlan0");
        flowManager.SetEncapType(FlowManager::ENCAP_VXLAN);
        flowManager.SetTunnelRemoteIp("10.11.12.13");
        conn.connected = true;
        advertManager.registerConnection(&conn);
        advertManager.setPortMapper(&portMapper);
        advertManager.setIOService(&ioService);

        portMapper.ports["br0_vxlan0"] = 2048;
        portMapper.RPortMap[2048] = "br0_vxlan0";
        portMapper.ports[ep0->getInterfaceName().get()] = 80;
        portMapper.RPortMap[80] = ep0->getInterfaceName().get();

        Mutator mutator(framework, policyOwner);
        epg0->addGbpEpGroupToNetworkRSrc()
            ->setTargetSubnets(subnetsfd0->getURI());
        mutator.commit();

        WAIT_FOR(agent.getPolicyManager().getRDForGroup(epg0->getURI()), 1000);
        WAIT_FOR(countSns(epg0->getURI()) == 4, 1000);
        BOOST_CHECK_EQUAL(4, countSns(epg0->getURI()));
        WAIT_FOR(countEps() == 5, 1000);
        BOOST_CHECK_EQUAL(5, countEps());
    }

    int countSns(const URI& uri) {
        PolicyManager::subnet_vector_t subnets;
        agent.getPolicyManager().getSubnetsForGroup(uri, subnets);
        return subnets.size();
    }

    int countEps() {
        size_t i = 0;
        EndpointManager& epMgr = agent.getEndpointManager();
        PolicyManager& polMgr = agent.getPolicyManager();
        
        PolicyManager::uri_set_t epgURIs;
        polMgr.getGroups(epgURIs);
        BOOST_FOREACH(const URI& epg, epgURIs) {
            unordered_set<string> eps;
            epMgr.getEndpointsForGroup(epg, eps);
            i += eps.size();
        }
        return i;
    }

    void start() {
        flowManager.Start();
        advertManager.start();

        io_service_thread = new thread(bind(&io_service::run, ref(ioService)));
    }

    void stop() {
        flowManager.Stop();
        advertManager.stop();

        if (io_service_thread) {
            io_service_thread->join();
            delete io_service_thread;
        }
    }

    MockSwitchConnection conn;
    MockPortMapper portMapper;
    FlowManager flowManager;
    AdvertManager advertManager;
    ofputil_protocol proto;

    boost::thread* io_service_thread;
    boost::asio::io_service ioService;
};

class EpAdvertFixture : public AdvertManagerFixture {
public:
    EpAdvertFixture()
        : AdvertManagerFixture() {
        advertManager.enableEndpointAdv(true);
        start();
    }

    ~EpAdvertFixture() {
        stop();
    }
};

class RouterAdvertFixture : public AdvertManagerFixture {
public:
    RouterAdvertFixture()
        : AdvertManagerFixture() {
        advertManager.enableRouterAdv(true);
        start();
    }

    ~RouterAdvertFixture() {
        stop();
    }
};

BOOST_AUTO_TEST_SUITE(AdvertManager_test)

static void verify_epadv(ofpbuf* msg, unordered_set<string>& found) {
    using namespace arp;

    struct ofputil_packet_out po;
    uint64_t ofpacts_stub[1024 / 8];
    struct ofpbuf ofpact;
    ofpbuf_use_stub(&ofpact, ofpacts_stub, sizeof ofpacts_stub);
    ofputil_decode_packet_out(&po, 
                              (ofp_header*)ofpbuf_data(msg),
                              &ofpact);
    struct ofpbuf pkt;
    struct flow flow;
    ofpbuf_use_const(&pkt, po.packet, po.packet_len);
    flow_extract(&pkt, NULL, &flow);

    uint16_t dl_type = ntohs(flow.dl_type);

    if (dl_type == ETH_TYPE_ARP) {
        BOOST_REQUIRE_EQUAL(sizeof(struct eth_header) + 
                            sizeof(struct arp_hdr) + 
                            2 * ETH_ADDR_LEN + 2 * 4,
                            ofpbuf_size(&pkt));
        uint32_t ip = 
            ntohl(*(uint32_t*)((char*)ofpbuf_l2(&pkt) + 
                               sizeof(struct eth_header) + 
                               sizeof(struct arp_hdr) + ETH_ADDR_LEN));
        found.insert(address_v4(ip).to_string());
    } else if (dl_type == ETH_TYPE_IPV6) {
        size_t l4_size = ofpbuf_l4_size(&pkt);
        BOOST_REQUIRE(l4_size > sizeof(struct icmp6_hdr));
        struct ip6_hdr* ip6 = (struct ip6_hdr*) ofpbuf_l3(&pkt);
        address_v6::bytes_type bytes;
        memcpy(bytes.data(), &ip6->ip6_src, sizeof(struct in6_addr));
        found.insert(address_v6(bytes).to_string());
        BOOST_CHECK(0 == memcmp(&ip6->ip6_src, &ip6->ip6_dst,
                                sizeof(struct in6_addr)));
        struct icmp6_hdr* icmp = (struct icmp6_hdr*) ofpbuf_l4(&pkt);
        BOOST_CHECK_EQUAL(ND_NEIGHBOR_ADVERT, icmp->icmp6_type);
    } else {
        LOG(ERROR) << "Incorrect dl_type: " << std::hex << dl_type;
        BOOST_FAIL("Incorrect dl_type");
    }
}

BOOST_FIXTURE_TEST_CASE(endpointAdvert, EpAdvertFixture) {
    WAIT_FOR(conn.sentMsgs.size() == 7, 1000);
    BOOST_CHECK_EQUAL(7, conn.sentMsgs.size());
    unordered_set<string> found;
    BOOST_FOREACH(ofpbuf* msg, conn.sentMsgs) {
        verify_epadv(msg, found);
    }
    BOOST_CHECK(CONTAINS(found, "10.20.45.31"));
    BOOST_CHECK(CONTAINS(found, "10.20.45.32"));
    BOOST_CHECK(CONTAINS(found, "10.20.44.21"));
    BOOST_CHECK(CONTAINS(found, "10.20.44.3"));
    BOOST_CHECK(CONTAINS(found, "10.20.44.2"));
    BOOST_CHECK(CONTAINS(found, "2001:db8::2"));
    BOOST_CHECK(CONTAINS(found, "2001:db8::3"));
}

BOOST_FIXTURE_TEST_CASE(routerAdvert, RouterAdvertFixture) {
    WAIT_FOR(conn.sentMsgs.size() == 1, 1000);
    BOOST_CHECK_EQUAL(1, conn.sentMsgs.size());

    unordered_set<string> found;
    BOOST_FOREACH(ofpbuf* msg, conn.sentMsgs) {
        struct ofputil_packet_out po;
        uint64_t ofpacts_stub[1024 / 8];
        struct ofpbuf ofpact;
        ofpbuf_use_stub(&ofpact, ofpacts_stub, sizeof ofpacts_stub);
        ofputil_decode_packet_out(&po, 
                                  (ofp_header*)ofpbuf_data(msg),
                                  &ofpact);
        struct ofpbuf pkt;
        struct flow flow;
        ofpbuf_use_const(&pkt, po.packet, po.packet_len);
        flow_extract(&pkt, NULL, &flow);

        BOOST_REQUIRE_EQUAL(ETH_TYPE_IPV6, ntohs(flow.dl_type));
        size_t l4_size = ofpbuf_l4_size(&pkt);
        BOOST_REQUIRE(l4_size > sizeof(struct icmp6_hdr));
        struct ip6_hdr* ip6 = (struct ip6_hdr*) ofpbuf_l3(&pkt);
        address_v6::bytes_type bytes;
        memcpy(bytes.data(), &ip6->ip6_src, sizeof(struct in6_addr));
        LOG(INFO) << address_v6(bytes).to_string();
        found.insert(address_v6(bytes).to_string());
        struct icmp6_hdr* icmp = (struct icmp6_hdr*) ofpbuf_l4(&pkt);
        BOOST_CHECK_EQUAL(ND_ROUTER_ADVERT, icmp->icmp6_type);
    }

    BOOST_CHECK(CONTAINS(found, "fe80::200:ff:fe00:0"));
}

BOOST_AUTO_TEST_SUITE_END()
