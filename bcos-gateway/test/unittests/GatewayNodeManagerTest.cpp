/**
 *  Copyright (C) 2021 FISCO BCOS.
 *  SPDX-License-Identifier: Apache-2.0
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * @brief test for GatewayNodeManager
 * @file GatewayNodeManagerTest.cpp
 * @author: octopus
 * @date 2021-05-14
 */

#include <bcos-crypto/signature/key/KeyFactoryImpl.h>
#include <bcos-front/FrontServiceFactory.h>
#include <bcos-gateway/Gateway.h>
#include <bcos-gateway/gateway/GatewayNodeManager.h>
#include <bcos-gateway/protocol/GatewayNodeStatus.h>
#include <bcos-utilities/testutils/TestPromptFixture.h>
#include <boost/test/unit_test.hpp>

using namespace bcos;
using namespace bcos::gateway;
using namespace bcos::test;

BOOST_FIXTURE_TEST_SUITE(GatewayNodeManagerTest, TestPromptFixture)

class FakeGatewayNodeManager : public GatewayNodeManager
{
public:
    FakeGatewayNodeManager(std::shared_ptr<bcos::crypto::KeyFactory> _keyFactory)
      : GatewayNodeManager(_keyFactory)
    {}
    ~FakeGatewayNodeManager() override {}

    bool statusChanged(std::string const& _p2pNodeID, uint32_t _seq)
    {
        return GatewayNodeManager::statusChanged(_p2pNodeID, _seq);
    }
    uint32_t statusSeq() { return GatewayNodeManager::statusSeq(); }

    bytesPointer generateNodeStatus() override { return GatewayNodeManager::generateNodeStatus(); }
    void updatePeerStatus(std::string const& _p2pID, GatewayNodeStatus::Ptr _status) override
    {
        return GatewayNodeManager::updatePeerStatus(_p2pID, _status);
    }
    void setStatusSeq(std::string const& _nodeID, uint32_t _seq) { m_p2pID2Seq[_nodeID] = _seq; }
    void start() override {}
    void stop() override {}
};

inline GatewayNodeStatus::Ptr createGatewayNodeStatus(
    int32_t _seq, std::string const& _uuid, std::vector<GroupNodeInfo::Ptr> _groupInfos)
{
    auto gatewayNodeStatus = std::make_shared<GatewayNodeStatus>();
    gatewayNodeStatus->setSeq(_seq);
    gatewayNodeStatus->setUUID(_uuid);
    gatewayNodeStatus->setGroupNodeInfos(std::move(_groupInfos));
    return gatewayNodeStatus;
}

inline GroupNodeInfo::Ptr createGroupNodeInfo(
    std::string const& _groupID, std::vector<std::string> _nodeIDList)
{
    auto groupNodeInfo = std::make_shared<GroupNodeInfo>(_groupID);
    groupNodeInfo->setNodeIDList(std::move(_nodeIDList));
    return groupNodeInfo;
}

class FakeGateway : public Gateway
{
public:
    FakeGateway() : Gateway() {}
    ~FakeGateway() override {}

    void start() override {}
    void stop() override {}
};

BOOST_AUTO_TEST_CASE(test_P2PMessage_statusSeqChanged)
{
    auto gatewayNodeManager = std::make_shared<FakeGatewayNodeManager>(nullptr);
    std::string p2pID = "1";
    bool changed = false;
    changed = gatewayNodeManager->statusChanged(p2pID, 1);
    BOOST_CHECK(changed);
}

BOOST_AUTO_TEST_CASE(test_GatewayNodeManager_registerFrontService)
{
    auto gatewayNodeManager = std::make_shared<FakeGatewayNodeManager>(nullptr);
    std::string groupID = "group";
    std::string strNodeID = "nodeID";
    auto keyFactory = std::make_shared<bcos::crypto::KeyFactoryImpl>();

    auto nodeID =
        keyFactory->createKey(bytesConstRef((bcos::byte*)strNodeID.data(), strNodeID.size()));

    auto frontServiceFactory = std::make_shared<bcos::front::FrontServiceFactory>();
    frontServiceFactory->setGatewayInterface(std::make_shared<FakeGateway>());

    auto frontService = frontServiceFactory->buildFrontService(groupID, nodeID);

    bool r = false;
    auto seq = gatewayNodeManager->statusSeq();
    r = gatewayNodeManager->registerNode(groupID, nodeID, frontService);
    BOOST_CHECK_EQUAL(r, true);
    BOOST_CHECK_EQUAL(seq + 1, gatewayNodeManager->statusSeq());

    auto s = gatewayNodeManager->localRouterTable()->getGroupFrontServiceList(groupID);
    BOOST_CHECK(!s.empty());

    seq = gatewayNodeManager->statusSeq();
    r = gatewayNodeManager->registerNode(groupID, nodeID, nullptr);
    BOOST_CHECK_EQUAL(r, false);
    BOOST_CHECK_EQUAL(seq, gatewayNodeManager->statusSeq());

    seq = gatewayNodeManager->statusSeq();
    r = gatewayNodeManager->unregisterNode(groupID, nodeID);
    BOOST_CHECK_EQUAL(r, true);
    BOOST_CHECK_EQUAL(seq + 1, gatewayNodeManager->statusSeq());

    s = gatewayNodeManager->localRouterTable()->getGroupFrontServiceList(groupID);
    BOOST_CHECK(s.empty());

    seq = gatewayNodeManager->statusSeq();
    r = gatewayNodeManager->registerNode(groupID, nodeID, nullptr);
    BOOST_CHECK_EQUAL(r, true);
    BOOST_CHECK_EQUAL(seq + 1, gatewayNodeManager->statusSeq());

    s = gatewayNodeManager->localRouterTable()->getGroupFrontServiceList(groupID);
    BOOST_CHECK(!s.empty());

    seq = gatewayNodeManager->statusSeq();
    r = gatewayNodeManager->registerNode(groupID, nodeID, nullptr);
    BOOST_CHECK_EQUAL(r, false);
    BOOST_CHECK_EQUAL(seq, gatewayNodeManager->statusSeq());
    s = gatewayNodeManager->localRouterTable()->getGroupFrontServiceList(groupID);
    BOOST_CHECK(!s.empty());
}

BOOST_AUTO_TEST_CASE(test_GatewayNodeManager_registerFrontService_loop)
{
    auto gatewayNodeManager = std::make_shared<FakeGatewayNodeManager>(nullptr);
    size_t loopCount = 100;
    auto keyFactory = std::make_shared<bcos::crypto::KeyFactoryImpl>();

    for (size_t i = 0; i < loopCount; i++)
    {
        std::string strNodeID = "nodeID" + std::to_string(i);
        std::string groupID = "group" + std::to_string(i);

        auto nodeID =
            keyFactory->createKey(bytesConstRef((bcos::byte*)strNodeID.data(), strNodeID.size()));

        auto seq = gatewayNodeManager->statusSeq();
        bool r = gatewayNodeManager->registerNode(groupID, nodeID, nullptr);
        BOOST_CHECK_EQUAL(r, true);
        BOOST_CHECK_EQUAL(seq + 1, gatewayNodeManager->statusSeq());

        seq = gatewayNodeManager->statusSeq();
        r = gatewayNodeManager->registerNode(groupID, nodeID, nullptr);
        BOOST_CHECK_EQUAL(r, false);
        BOOST_CHECK_EQUAL(seq, gatewayNodeManager->statusSeq());

        auto statusData = gatewayNodeManager->generateNodeStatus();
        BOOST_CHECK(!statusData->empty());

        seq = gatewayNodeManager->statusSeq();
        r = gatewayNodeManager->unregisterNode(groupID, nodeID);
        BOOST_CHECK_EQUAL(r, true);
        BOOST_CHECK_EQUAL(seq + 1, gatewayNodeManager->statusSeq());

        seq = gatewayNodeManager->statusSeq();
        r = gatewayNodeManager->unregisterNode(groupID, nodeID);
        BOOST_CHECK_EQUAL(r, false);
        BOOST_CHECK_EQUAL(seq, gatewayNodeManager->statusSeq());
    }
}

BOOST_AUTO_TEST_CASE(test_GatewayNodeManager_onRequestNodeStatus)
{
    auto gatewayNodeManager = std::make_shared<FakeGatewayNodeManager>(nullptr);
    auto keyFactory = std::make_shared<bcos::crypto::KeyFactoryImpl>();

    for (size_t i = 0; i < 100; i++)
    {
        std::string groupID = "group" + std::to_string(i);
        std::string strNodeID = "nodeID" + std::to_string(i);

        auto nodeID =
            keyFactory->createKey(bytesConstRef((bcos::byte*)strNodeID.data(), strNodeID.size()));

        bool r = false;
        auto seq = gatewayNodeManager->statusSeq();
        r = gatewayNodeManager->registerNode(groupID, nodeID, nullptr);
        BOOST_CHECK(r);
        BOOST_CHECK_EQUAL(seq + 1, gatewayNodeManager->statusSeq());

        auto nodeStatusData = gatewayNodeManager->generateNodeStatus();
        BOOST_CHECK(!nodeStatusData->empty());

        uint32_t statusSeq;
        auto gatewayStatus = std::make_shared<GatewayNodeStatus>();
        gatewayStatus->decode(bytesConstRef(nodeStatusData->data(), nodeStatusData->size()));
        BOOST_CHECK_EQUAL(seq + 1, gatewayStatus->seq());
    }
}

BOOST_AUTO_TEST_CASE(test_GatewayNodeManager_statusEncodeDecode)
{
    auto gatewayNodeManager = std::make_shared<FakeGatewayNodeManager>(nullptr);
    auto gatewayNodeStatus = std::make_shared<GatewayNodeStatus>();
    gatewayNodeStatus->setSeq(110);
    gatewayNodeStatus->setUUID("testuuid");
    std::vector<GroupNodeInfo::Ptr> groupNodeInfos;
    // group1
    auto group1Info = std::make_shared<GroupNodeInfo>("group1");
    std::vector<std::string> nodeIDList = {"a0", "b0", "c0"};
    group1Info->setNodeIDList(std::move(nodeIDList));
    groupNodeInfos.emplace_back(group1Info);

    // group2
    auto group2Info = std::make_shared<GroupNodeInfo>("group2");
    std::vector<std::string> nodeIDList2 = {"a1", "b1", "c1"};
    group2Info->setNodeIDList(std::move(nodeIDList2));
    groupNodeInfos.emplace_back(group2Info);

    // group3
    auto group3Info = std::make_shared<GroupNodeInfo>("group3");
    std::vector<std::string> nodeIDList3 = {"a2", "b2", "c2"};
    group3Info->setNodeIDList(std::move(nodeIDList3));
    groupNodeInfos.emplace_back(group3Info);

    gatewayNodeStatus->setGroupNodeInfos(std::move(groupNodeInfos));

    // encode
    auto encodeData = gatewayNodeStatus->encode();

    // decode
    auto decodedStatus = std::make_shared<GatewayNodeStatus>();
    decodedStatus->decode(bytesConstRef(encodeData->data(), encodeData->size()));

    // check
    BOOST_CHECK_EQUAL(decodedStatus->seq(), 110);
    BOOST_CHECK_EQUAL(decodedStatus->uuid(), "testuuid");
    auto const& groupInfos = decodedStatus->groupNodeInfos();
    BOOST_CHECK(groupInfos.size() == 3);
    BOOST_CHECK(groupInfos[0]->groupID() == "group1");
    BOOST_CHECK(groupInfos[0]->nodeIDList().size() == 3);
    BOOST_CHECK(groupInfos[0]->nodeIDList()[0] == "a0");
    BOOST_CHECK(groupInfos[0]->nodeIDList()[1] == "b0");
    BOOST_CHECK(groupInfos[0]->nodeIDList()[2] == "c0");

    BOOST_CHECK(groupInfos[1]->groupID() == "group2");
    BOOST_CHECK(groupInfos[1]->nodeIDList().size() == 3);
    BOOST_CHECK(groupInfos[1]->nodeIDList()[0] == "a1");
    BOOST_CHECK(groupInfos[1]->nodeIDList()[1] == "b1");
    BOOST_CHECK(groupInfos[1]->nodeIDList()[2] == "c1");

    BOOST_CHECK(groupInfos[2]->groupID() == "group3");
    BOOST_CHECK(groupInfos[2]->nodeIDList().size() == 3);
    BOOST_CHECK(groupInfos[2]->nodeIDList()[0] == "a2");
    BOOST_CHECK(groupInfos[2]->nodeIDList()[1] == "b2");
    BOOST_CHECK(groupInfos[2]->nodeIDList()[2] == "c2");
}

BOOST_AUTO_TEST_CASE(test_GatewayNodeManager_onReceiveNodeIDs)
{
    auto gatewayNodeManager = std::make_shared<FakeGatewayNodeManager>(nullptr);
    auto gatewayNodeStatus =
        createGatewayNodeStatus(110, "testUUID", std::vector<GroupNodeInfo::Ptr>());
    std::string p2pID = "xxxxxxxxxxxxxxxxxxxxx";

    gatewayNodeManager->updatePeerStatus(p2pID, gatewayNodeStatus);
    bool changed = false;

    changed = gatewayNodeManager->statusChanged(p2pID, 110);
    BOOST_CHECK(changed);
    gatewayNodeManager->setStatusSeq(p2pID, 110);

    changed = gatewayNodeManager->statusChanged(p2pID, 1);
    BOOST_CHECK(changed);
    gatewayNodeManager->setStatusSeq(p2pID, 1);

    changed = gatewayNodeManager->statusChanged(p2pID, 1);
    BOOST_CHECK(!changed);
}


BOOST_AUTO_TEST_CASE(test_GatewayNodeManager_query)
{
    auto keyFactory = std::make_shared<bcos::crypto::KeyFactoryImpl>();
    auto gatewayNodeManager = std::make_shared<FakeGatewayNodeManager>(keyFactory);
    std::vector<GroupNodeInfo::Ptr> groupInfos;
    std::string group1 = "group1";
    auto group1Info = createGroupNodeInfo(group1, {"a0", "b0", "c0"});
    groupInfos.emplace_back(group1Info);

    std::string group2 = "group2";
    auto group2Info = createGroupNodeInfo(group2, {"a1", "b1", "c1"});
    groupInfos.emplace_back(group2Info);

    std::string group3 = "group3";
    auto group3Info = createGroupNodeInfo(group3, {"a2", "b2", "c2"});
    groupInfos.emplace_back(group3Info);

    auto status = createGatewayNodeStatus(110, "testUUID", groupInfos);

    std::string p2pID1 = "xxxxx";
    std::string p2pID2 = "yyyyy";
    std::string p2pID3 = "zzzzz";

    gatewayNodeManager->updatePeerStatus(p2pID1, status);

    auto p2pIDs1 = gatewayNodeManager->peersRouterTable()->queryP2pIDsByGroupID(group1);
    BOOST_CHECK_EQUAL(p2pIDs1.size(), 1);
    BOOST_CHECK_EQUAL(*p2pIDs1.begin(), p2pID1);

    auto nodeIDs = gatewayNodeManager->getGroupNodeIDList(group1);
    BOOST_CHECK_EQUAL(nodeIDs->size(), 3);

    auto p2pIDs2 = gatewayNodeManager->peersRouterTable()->queryP2pIDs(group1, "a0");
    BOOST_CHECK_EQUAL(p2pIDs2.size(), 1);
    BOOST_CHECK_EQUAL(*p2pIDs2.begin(), p2pID1);

    gatewayNodeManager->updatePeerStatus(p2pID2, status);

    auto p2pIDs3 = gatewayNodeManager->peersRouterTable()->queryP2pIDsByGroupID(group2);
    BOOST_CHECK_EQUAL(p2pIDs3.size(), 2);

    auto p2pIDs4 = gatewayNodeManager->peersRouterTable()->queryP2pIDs(group2, "a1");
    BOOST_CHECK_EQUAL(p2pIDs4.size(), 2);

    gatewayNodeManager->updatePeerStatus(p2pID3, status);

    auto p2pIDs5 = gatewayNodeManager->peersRouterTable()->queryP2pIDsByGroupID(group3);
    BOOST_CHECK_EQUAL(p2pIDs5.size(), 3);

    auto p2pIDs6 = gatewayNodeManager->peersRouterTable()->queryP2pIDs(group3, "a2");
    BOOST_CHECK_EQUAL(p2pIDs6.size(), 3);
}


BOOST_AUTO_TEST_CASE(test_GatewayNodeManager_remove)
{
    auto keyFactory = std::make_shared<bcos::crypto::KeyFactoryImpl>();
    auto gatewayNodeManager = std::make_shared<FakeGatewayNodeManager>(nullptr);

    std::vector<GroupNodeInfo::Ptr> groupInfos;
    std::string group1 = "group1";
    auto group1Info = createGroupNodeInfo(group1, {"a0", "b0", "c0"});
    groupInfos.emplace_back(group1Info);

    std::string group2 = "group2";
    auto group2Info = createGroupNodeInfo(group2, {"a1", "b1", "c1"});
    groupInfos.emplace_back(group2Info);

    std::string group3 = "group3";
    auto group3Info = createGroupNodeInfo(group3, {"a2", "b2", "c2"});
    groupInfos.emplace_back(group3Info);

    auto status = createGatewayNodeStatus(110, "testUUID", groupInfos);

    std::string p2pID1 = "xxxxx";
    std::string p2pID2 = "yyyyy";
    std::string p2pID3 = "zzzzz";

    gatewayNodeManager->updatePeerStatus(p2pID1, status);
    gatewayNodeManager->updatePeerStatus(p2pID2, status);
    gatewayNodeManager->updatePeerStatus(p2pID3, status);

    {
        auto p2pIDs1 = gatewayNodeManager->peersRouterTable()->queryP2pIDsByGroupID(group1);
        BOOST_CHECK_EQUAL(p2pIDs1.size(), 3);
        BOOST_CHECK(p2pIDs1.find(p2pID2) != p2pIDs1.end());
        BOOST_CHECK(p2pIDs1.find(p2pID3) != p2pIDs1.end());
        BOOST_CHECK(p2pIDs1.find(p2pID1) != p2pIDs1.end());

        auto p2pIDs2 = gatewayNodeManager->peersRouterTable()->queryP2pIDs(group1, "a0");
        BOOST_CHECK_EQUAL(p2pIDs2.size(), 3);
        BOOST_CHECK(p2pIDs2.find(p2pID2) != p2pIDs2.end());
        BOOST_CHECK(p2pIDs2.find(p2pID3) != p2pIDs2.end());
        BOOST_CHECK(p2pIDs2.find(p2pID1) != p2pIDs2.end());
    }

    gatewayNodeManager->onRemoveNodeIDs(p2pID1);
    {
        auto p2pIDs1 = gatewayNodeManager->peersRouterTable()->queryP2pIDsByGroupID(group1);
        BOOST_CHECK_EQUAL(p2pIDs1.size(), 2);
        BOOST_CHECK(p2pIDs1.find(p2pID2) != p2pIDs1.end());
        BOOST_CHECK(p2pIDs1.find(p2pID3) != p2pIDs1.end());

        auto p2pIDs2 = gatewayNodeManager->peersRouterTable()->queryP2pIDs(group1, "a0");
        BOOST_CHECK_EQUAL(p2pIDs2.size(), 2);
        BOOST_CHECK(p2pIDs2.find(p2pID2) != p2pIDs2.end());
        BOOST_CHECK(p2pIDs2.find(p2pID3) != p2pIDs2.end());
    }

    gatewayNodeManager->onRemoveNodeIDs(p2pID2);
    {
        auto p2pIDs1 = gatewayNodeManager->peersRouterTable()->queryP2pIDsByGroupID(group1);
        BOOST_CHECK_EQUAL(p2pIDs1.size(), 1);
        BOOST_CHECK(p2pIDs1.find(p2pID3) != p2pIDs1.end());

        auto p2pIDs2 = gatewayNodeManager->peersRouterTable()->queryP2pIDs(group1, "a0");
        BOOST_CHECK_EQUAL(p2pIDs2.size(), 1);
        BOOST_CHECK(p2pIDs2.find(p2pID3) != p2pIDs2.end());
    }

    gatewayNodeManager->onRemoveNodeIDs(p2pID3);
    {
        auto p2pIDs1 = gatewayNodeManager->peersRouterTable()->queryP2pIDsByGroupID(group1);
        BOOST_CHECK(p2pIDs1.empty());

        auto p2pIDs2 = gatewayNodeManager->peersRouterTable()->queryP2pIDs(group1, "a0");
        BOOST_CHECK(p2pIDs2.empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()