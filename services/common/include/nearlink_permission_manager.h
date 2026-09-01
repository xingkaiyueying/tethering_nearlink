/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OHOS_NEARLINK_PERMISSION_MANAGER_H
#define OHOS_NEARLINK_PERMISSION_MANAGER_H

#include <map>
#include <string>

#include "nearlink_permission_item.h"

#ifdef CHECK_PERM
#undef CHECK_PERM
#endif
#define CHECK_PERM(isSystemApi, perm) NearLinkPermissionManager::CreateItem(isSystemApi, perm)
#define VERIFY_PERMISSIONS(perm)                                                                                       \
do {                                                                                                                   \
    int ret = NearLinkPermissionManager::VerifyMultiPermissions(perm);                                                 \
    if (ret != NL_NO_ERROR) {                                                                                          \
        HILOGE("[PERMISSION]failed callingName(%{public}s)", NearLinkPermissionManager::GetCallingName().c_str());     \
        NL_CHECK_RETURN_RET(reply.WriteInt32(ret), TRANSACTION_ERR, "WriteInt failed.");                               \
        return TRANSACTION_ERR;                                                                                        \
    }                                                                                                                  \
} while (0)

#ifdef MULTI_PERM
#undef MULTI_PERM
#endif
#define MULTI_PERM(...) {__VA_ARGS__}

#define CHECK_PERMISSION_AND_EXECUTE(stubClassName)                                         \
do {                                                                                        \
    HILOGD("cmd(%{public}u), flags(%{public}d)", code, option.GetFlags());                  \
    if (stubClassName::GetDescriptor() != data.ReadInterfaceToken()) {                      \
        HILOGE("interface token check failed.");                                            \
        return NL_ERR_IPC_TRANS_FAILED;                                                     \
    }                                                                                       \
    auto itFunc = memberFuncMap_.find(code);                                                \
    if (itFunc == memberFuncMap_.end()) {                                                   \
        HILOGE("code(%{public}d) is not exist.", code);                                     \
        return IPCObjectStub::OnRemoteRequest(code, data, reply, option);                   \
    }                                                                                       \
    auto memberFunc = itFunc->second.first;                                                 \
    if (memberFunc == nullptr) {                                                            \
        HILOGE("memberFunc is nullptr. code(%{public}d)", code);                            \
        return IPCObjectStub::OnRemoteRequest(code, data, reply, option);                   \
    }                                                                                       \
    int errCode = NearLinkPermissionManager::VerifyMultiPermissions(itFunc->second.second); \
    if (errCode != NL_NO_ERROR) {                                                           \
        HILOGE("[PERMISSION] failed. code(%{public}d), callingName(%{public}s)",            \
            code, NearLinkPermissionManager::GetCallingName().c_str());                     \
        reply.WriteInt32(errCode);                                                          \
        return NL_NO_ERROR;                                                                 \
    }                                                                                       \
    return memberFunc(this, data, reply);                                                   \
} while (0)

namespace OHOS {
namespace Nearlink {

// Nearlink permission.
const char* const GET_NEARLINK_LOCAL_MAC = "ohos.permission.GET_NEARLINK_LOCAL_MAC";
const char* const GET_NEARLINK_PEER_MAC = "ohos.permission.GET_NEARLINK_PEER_MAC";
const char* const ACCESS_NEARLINK = "ohos.permission.ACCESS_NEARLINK";
const char* const MANAGE_NEARLINK = "ohos.permission.MANAGE_NEARLINK";
const char* const CONNECTIVITY_INTERNAL = "ohos.permission.CONNECTIVITY_INTERNAL";

class NearLinkPermissionManager {
public:
    /**
     * @Description Verify permission.
     *
     * @param permissionName Permission name.
     * @return true - permission granted, false - permission denied.
     */
    static bool VerifyPermission(const std::string &permissionName);

    /**
     * @Description Verify permission with token id.
     *
     * @param permissionName Permission name.
     * @param callerToken The app's token id.
     * @return true - permission granted, false - permission denied.
     */
    static bool VerifyPermission(const std::string &permissionName, const uint32_t &callerToken);

    /**
     * @Description Verify permissions.
     *
     * @param item Permissions.
     * @return BT_NO_ERROR - permission granted, otherwise - permission denied
     */
    static int32_t VerifyMultiPermissions(const std::shared_ptr<NearLinkPermissionItem> &item);

    /**
     * @Description Get api version.
     *
     * @return which version of the SDK is used to develop this hap.
     */
    static int32_t GetNearlinkApiVersion();

    /**
     * @Description Get api version.
     *
     * @param tokenId The app's token id.
     * @return int32_t Api version.
     */
    static int32_t GetNearlinkApiVersion(uint32_t tokenId);

    /**
     * @Description Get BundleName.
     *
     * @return string BundleName.
     */
    static std::string GetCallingName();

    /**
     * @Description Get BundleName.
     *
     * @param tokenId The app's token id.
     * @return string BundleName.
     */
    static std::string GetCallingName(const uint32_t &tokenId);

    /**
     * @Description Check whether the caller is hap.
     *
     * @param tokenId The app's token id.
     * @return true or false.
     */
    static bool IsHapCaller(void);

    /**
     * @DescriptionCheck whether the caller is hap with token id.
     *
     * @param tokenId The app's token id.
     * @return true or false.
     */
    static bool IsHapCaller(uint32_t tokenId);

    /**
     * @DescriptionCheck whether the caller is native process.
     *
     * @return true or false.
     */
    static bool IsNativeCaller(void);

    /**
     * @DescriptionCheck whether the caller is native with token id.
     *
     * @param tokenId The app's token id.
     * @return true or false.
     */
    static bool IsNativeCaller(const uint32_t &tokenId);

    /**
     * @Description Check whether the caller has system permission.
     *
     * @return true or false.
     */
    static bool CheckSystemPermission();

    /**
     * @Description Check whether the caller has system permission with token id.
     *
     * @return true or false.
     */
    static bool CheckSystemPermission(uint64_t fullTokenId);

    /**
     * @Description Check is system hap application.
     *
     * @return true or false.
     */
    static bool IsSystemHap();

    /**
     * @Description Check is system hap application with token id.
     *
     * @param tokenId The app's token id.
     * @return true or false.
     */
    static bool IsSystemHap(uint64_t fullTokenId);

    /**
     * @Description Check whether to use real address.
     *
     * @return true or false.
     */
    static bool IsUseRealAddr();

    /**
     * @Description Check whether to use real address with full token id.
     *
     * @param tokenId The app's token id.
     * @return true or false.
     */
    static bool IsUseRealAddr(const uint32_t &tokenId);

    /**
     * @Description Create permission items.
     *
     * @return true or false.
     */
    static std::shared_ptr<NearLinkPermissionItem> CreateItem(bool isSystemApi, const std::set<std::string> permSet);

    static std::string GetHapAppIdentifier(int32_t userId, const std::string &bundleName);
    static std::string GetHapAppId(uint32_t tokenId);
    static std::string GetHapBundleName(uint32_t tokenId);
private:
    static bool IsPermissionsGranted(const std::set<std::string> &permissions);
    static bool IsNeedAddPermissionUsedRecord(const std::string &permissionName, const uint32_t &callerToken);
};

}  // namespace Nearlink
}  // namespace OHOS
#endif
