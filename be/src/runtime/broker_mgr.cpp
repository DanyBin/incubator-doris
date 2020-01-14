// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "runtime/broker_mgr.h"

#include <sstream>

#include "common/config.h"
#include "gen_cpp/PaloBrokerService_types.h"
#include "gen_cpp/TPaloBrokerService.h"
#include "service/backend_options.h"
#include "runtime/exec_env.h"
#include "runtime/client_cache.h"
#include "util/thrift_util.h"

namespace doris {

BrokerMgr::BrokerMgr(ExecEnv* exec_env) : 
        _exec_env(exec_env), _thread_stop(false), _ping_thread(&BrokerMgr::ping_worker, this) {
}

BrokerMgr::~BrokerMgr() {
    _thread_stop = true;
    _ping_thread.join();
}

void BrokerMgr::init() {
    std::stringstream ss;
    ss << BackendOptions::get_localhost() << ":" << config::be_port;
    _client_id = ss.str();
}

const std::string& BrokerMgr::get_client_id(const TNetworkAddress& address) {
    std::lock_guard<std::mutex> l(_mutex);
    _broker_set.insert(address);
    return _client_id;
}

void BrokerMgr::ping(const TNetworkAddress& addr) {
    TBrokerPingBrokerRequest request;

    request.__set_version(TBrokerVersion::VERSION_ONE);
    request.__set_clientId(_client_id);

    TBrokerOperationStatus response;
    try {
        Status status;
        // 500ms is enough
        BrokerServiceConnection client(
            _exec_env->broker_client_cache(), addr, 500, &status);
        if (!status.ok()) {
            LOG(WARNING) << "Create broker client failed. broker=" << addr
                << ", status=" << status.get_error_msg();
            return;
        }

        try {
            client->ping(response, request);
        } catch (apache::thrift::transport::TTransportException& e) {
            status = client.reopen();
            if (!status.ok()) {
                LOG(WARNING) << "Create broker client failed. broker=" << addr
                    << ", status=" << status.get_error_msg();
                return;
            }
            client->ping(response, request);
        }
    } catch (apache::thrift::TException& e) {
        LOG(WARNING) << "Broker ping failed, broker:" << addr << " failed:" << e.what();
    }
}

void BrokerMgr::ping_worker() {
    while (!_thread_stop) {
        std::vector<TNetworkAddress> addresses;
        {
            std::lock_guard<std::mutex> l(_mutex);
            for (auto& addr : _broker_set) {
                addresses.emplace_back(addr);
            }
        }
        for (auto& addr : addresses) {
            ping(addr);
        }
        sleep(5);
    }
}

}
