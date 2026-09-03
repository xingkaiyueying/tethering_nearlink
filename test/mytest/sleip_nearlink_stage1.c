/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "iposl_profile.h"
#include "nearlink_ipshare_client_c.h"

#define WAIT_COUNT 300
#define WAIT_INTERVAL_US 200000

static const char *StateName(int32_t state)
{
    static const char *names[] = {
        "IDLE", "STARTING", "DISCOVERING", "CONFIGURING", "IFACE_READY", "CHANNEL_READY",
        "DHCP", "SERVING", "SERVING_NO_UPSTREAM", "ACTIVE", "STOPPING", "ERROR"
    };
    if (state < 0 || state >= (int32_t)(sizeof(names) / sizeof(names[0]))) {
        return "UNKNOWN";
    }
    return names[state];
}

static void MaskAddress(const char *address, char masked[NL_IPSHARE_ADDRESS_TEXT_LEN])
{
    if (address == NULL || strlen(address) != 17) {
        (void)snprintf(masked, NL_IPSHARE_ADDRESS_TEXT_LEN, "**:**:**:**:**:**");
        return;
    }
    (void)snprintf(masked, NL_IPSHARE_ADDRESS_TEXT_LEN, "**:**:**:%s", address + 9);
}

static int PrintStatusResult(int32_t commandResult, bool failOnErrorState)
{
    NlIpShareStatusC status = {0};
    int32_t statusResult = NlIpShareGetStatus(&status);
    int32_t code = commandResult != 0 ? commandResult : statusResult;
    bool passed = code == 0 && (!failOnErrorState || status.state != 11);
    char masked[NL_IPSHARE_ADDRESS_TEXT_LEN] = {0};
    MaskAddress(status.peerAddress, masked);
    printf("peer=%s iface=%s\n", masked, status.ifaceName[0] == '\0' ? "-" : status.ifaceName);
    printf("RESULT=%s state=%s code=%d\n", passed ? "PASS" : "FAIL", StateName(status.state),
        passed ? status.errorCode : (code != 0 ? code : status.errorCode));
    return passed ? 0 : 1;
}

static int WaitForState(int32_t wanted)
{
    NlIpShareStatusC status = {0};
    for (int i = 0; i < WAIT_COUNT; ++i) {
        int32_t ret = NlIpShareGetStatus(&status);
        if (ret != 0) {
            return PrintStatusResult(ret, true);
        }
        if (status.state == wanted) {
            return PrintStatusResult(0, true);
        }
        if (status.state == 11) {
            return PrintStatusResult(status.errorCode, true);
        }
        usleep(WAIT_INTERVAL_US);
    }
    return PrintStatusResult(-1, true);
}

static int RunVector(void)
{
    bool passed = IposlCodecVerifyGoldenVectors() && IposlProfileIdentityServiceMemberCount() == 0 &&
        IposlProfileDataProtocolIndicator() == 0x01 && IPOSL_IP_TYPE_IPV4 == 0x01 &&
        strcmp(IPOSL_IDENTIFIER_SERVICE_UUID, "8f6f1d00-7b0c-4a73-9d4e-6e6561726c01") == 0;
    printf("identity_members=0 pi=0x%02x opcode_config=0x%02x opcode_enable=0x%02x\n",
        IPOSL_IP_TYPE_IPV4, IPOSL_OPCODE_CONFIGURE, IPOSL_OPCODE_ENABLE);
    printf("RESULT=%s state=VECTOR code=%d\n", passed ? "PASS" : "FAIL", passed ? 0 : -1);
    return passed ? 0 : 1;
}

static int RunSupport(const char *address)
{
    int32_t supported = 0;
    int32_t ret = NlIpShareIsPeerSupported(address, &supported);
    char masked[NL_IPSHARE_ADDRESS_TEXT_LEN] = {0};
    MaskAddress(address, masked);
    printf("peer=%s supported=%s\n", masked, supported ? "true" : "false");
    printf("RESULT=%s state=%s code=%d\n", ret == 0 && supported ? "PASS" : "FAIL",
        ret == 0 ? "IDLE" : "ERROR", ret == 0 && supported ? 0 : (ret != 0 ? ret : -4));
    return ret == 0 && supported ? 0 : 1;
}

static void Usage(const char *program)
{
    fprintf(stderr, "usage: %s vector|status|stop|support ADDRESS|gateway-start ADDRESS|terminal-start ADDRESS\n",
        program);
}

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "vector") == 0) {
        return RunVector();
    }
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        return PrintStatusResult(0, true);
    }
    if (argc == 2 && strcmp(argv[1], "stop") == 0) {
        int32_t ret = NlIpShareStop();
        return ret == 0 ? WaitForState(0) : PrintStatusResult(ret, true);
    }
    if (argc == 3 && strcmp(argv[1], "support") == 0) {
        return RunSupport(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "gateway-start") == 0) {
        int32_t ret = NlIpShareStartGateway(argv[2]);
        return ret == 0 ? WaitForState(5) : PrintStatusResult(ret, true);
    }
    if (argc == 3 && strcmp(argv[1], "terminal-start") == 0) {
        int32_t ret = NlIpShareStartTerminal(argv[2]);
        return ret == 0 ? WaitForState(5) : PrintStatusResult(ret, true);
    }
    Usage(argv[0]);
    return 2;
}
