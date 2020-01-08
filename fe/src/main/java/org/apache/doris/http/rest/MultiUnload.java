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

package org.apache.doris.http.rest;

import org.apache.doris.cluster.ClusterNamespace;
import org.apache.doris.common.DdlException;
import org.apache.doris.http.ActionController;
import org.apache.doris.http.BaseRequest;
import org.apache.doris.http.BaseResponse;
import org.apache.doris.http.IllegalArgException;
import org.apache.doris.mysql.privilege.PrivPredicate;
import org.apache.doris.qe.ConnectContext;
import org.apache.doris.service.ExecuteEnv;

import com.google.common.base.Strings;

import io.netty.handler.codec.http.HttpMethod;

public class MultiUnload extends RestBaseAction {
    private static final String SUB_LABEL_KEY = "sub_label";

    private ExecuteEnv execEnv;

    public MultiUnload(ActionController controller, ExecuteEnv execEnv) {
        super(controller);
        this.execEnv = execEnv;
    }

    public static void registerAction(ActionController controller) throws IllegalArgException {
        ExecuteEnv executeEnv = ExecuteEnv.getInstance();
        MultiUnload action = new MultiUnload(controller, executeEnv);
        controller.registerHandler(HttpMethod.POST, "/api/{" + DB_KEY + "}/_multi_unload", action);
    }

    @Override
    public void executeWithoutPassword(BaseRequest request, BaseResponse response)
            throws DdlException {
        String db = request.getSingleParameter(DB_KEY);
        if (Strings.isNullOrEmpty(db)) {
            throw new DdlException("No database selected");
        }
        String label = request.getSingleParameter(LABEL_KEY);
        if (Strings.isNullOrEmpty(label)) {
            throw new DdlException("No label selected");
        }
        String subLabel = request.getSingleParameter(SUB_LABEL_KEY);
        if (Strings.isNullOrEmpty(subLabel)) {
            throw new DdlException("No sub_label selected");
        }

        String fullDbName = ClusterNamespace.getFullName(ConnectContext.get().getClusterName(), db);
        checkDbAuth(ConnectContext.get().getCurrentUserIdentity(), fullDbName, PrivPredicate.LOAD);

        if (redirectToMaster(request, response)) {
            return;
        }

        execEnv.getMultiLoadMgr().unload(fullDbName, label, subLabel);
        sendResult(request, response, RestBaseResult.getOk());
    }
}
