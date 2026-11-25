#include <cassert>
#include <cstring>
#include "iposl_codec.c"
#include "iposl_client.c"
static int failures, configured, prepared, rollbacks, supported, capabilityUpdates;
static bool secure = true, reserveFails = false;
static uint8_t lastMode;
static void Configured(const uint8_t *, bool, int32_t e, uint8_t m, uint64_t g)
{ assert(g == 42); if (e) ++failures; else ++configured; lastMode = m; }
static void Supported(const uint8_t *, bool s, int32_t, uint8_t m, bool k, uint64_t g)
{ assert(g == 42 && s && k && m == 3); ++supported; }
static void PeerCapabilities(const uint8_t *, uint8_t modes, uint64_t g)
{ assert(g == 42); assert(modes == 0 || modes == 1 || modes == 3); ++capabilityUpdates; }
static int Prepare(const uint8_t *, uint8_t m, uint64_t)
{ if (m == 0) ++rollbacks; else ++prepared; return reserveFails ? -1 : 0; }
static bool Secure(const uint8_t *, uint64_t) { return secure; }
const IposlProfileCallbacks *IposlGetCallbacks()
{ static IposlProfileCallbacks cb = {Supported,Configured,Prepare,Secure,nullptr,PeerCapabilities}; return &cb; }
static void Discover(uint8_t mode = 3, bool terminal = true)
{
    IposlClientStop(); secure = true; reserveFails = false;
    uint8_t peer[6] = {2,1,2,3,4,5};
    assert(IposlClientStart(peer,0,terminal,peer,mode,42) == 0);
    OnConnectionStateChanged(7,SSAP_CONNECT_STATE_CONNECTED,0,0);
    assert(memcmp(discovered.uuid,g_identifierUuid,16) == 0);
    NLSTK_SsapServ_S service = {};
    OnGetServices(7,&discovered,&service,1,nullptr);
    assert(memcmp(discovered.uuid,g_configUuid,16) == 0);
    NLSTK_SsapPrty_S prop = {4,{}}, method = {5,{}};
    memcpy(prop.uuid.uuid,g_gatewayCapabilityUuid,16); memcpy(method.uuid.uuid,g_methodUuid,16);
    service = {1,&prop,1,&method}; OnGetServices(7,&discovered,&service,1,nullptr);
}
static void Capability(uint8_t mode)
{
    uint8_t bytes[12]; memcpy(bytes,g_iposlGatewayCapability,12); bytes[11] = mode;
    NLSTK_SsapClientReadPropertyInfo_S reply = {4,{},0,{12,bytes}};
    OnReadCapability(7,&reply,0);
}
static void Response(uint8_t opcode, uint8_t result, int transport = 0, uint8_t ssapError = 0)
{
    uint8_t bytes[8]; IposlCodecEncodeResponse(opcode,g_localLayer2,result,bytes,8);
    NLSTK_SsapClientCallMethodResult_S reply = {5,{},ssapError,{8,bytes}};
    OnCallMethod(7,&reply,transport);
}
static bool VerifyGoldenVectors(void)
{
    static const uint8_t layer2[IPOSL_LAYER2_ID_LEN] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    static const uint8_t configRequest[IPOSL_CONFIG_REQUEST_LEN] = {0x01, 0x02, 0x11, 0x22, 0x33, 0x44,
                                                                    0x55, 0x05, 0xDC, 0x01, 0x01};
    static const uint8_t openRequest[IPOSL_OPEN_REQUEST_LEN] = {0x02, 0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    static const uint8_t configResponse[IPOSL_RESPONSE_LEN] = {0x01, 0x02, 0x11, 0x22, 0x33, 0x44, 0x55, 0x00};
    uint8_t out[IPOSL_CONFIG_REQUEST_LEN] = {0};
    uint8_t decodedLayer2[IPOSL_LAYER2_ID_LEN] = {0};
    uint8_t opcode = 0;
    uint8_t result = 0xFF;
    if (IposlCodecEncodeConfigMode(layer2, IPOSL_IP_TYPE_IPV4, out, sizeof(out)) != (int32_t)sizeof(configRequest) ||
        memcmp(out, configRequest, sizeof(configRequest)) != 0 ||
        IposlCodecDecodeRequest(out, sizeof(configRequest), &opcode, decodedLayer2) != IPOSL_SUCCESS ||
        opcode != IPOSL_OPCODE_CONFIGURE || memcmp(decodedLayer2, layer2, sizeof(layer2)) != 0) {
        return false;
    }
    if (IposlCodecEncodeOpenRequest(layer2, out, sizeof(out)) != (int32_t)sizeof(openRequest) ||
        memcmp(out, openRequest, sizeof(openRequest)) != 0 ||
        IposlCodecDecodeRequest(out, sizeof(openRequest), &opcode, decodedLayer2) != IPOSL_SUCCESS ||
        opcode != IPOSL_OPCODE_ENABLE) {
        return false;
    }
    if (IposlCodecDecodeResponse(configResponse, sizeof(configResponse), IPOSL_OPCODE_CONFIGURE, decodedLayer2,
                                 &result) != IPOSL_SUCCESS ||
        result != 0) {
        return false;
    }
    out[0] = 0x7F;
    return IposlCodecDecodeRequest(out, 1, &opcode, decodedLayer2) == IPOSL_ERR_INVALID_PARAM;
}

int main()
{
    assert(VerifyGoldenVectors());
    Discover(); Capability(3); assert(methodCalls == 1 && g_selectedMode == 3 && prepared == 1 && capabilityUpdates == 1);
    Response(1,0); assert(methodCalls == 2 && g_expectedOpcode == 2);
    Response(2,0); assert(configured == 2 && lastMode == 3);
    Response(2,0); assert(configured == 2); // duplicate enable response is consumed once
    Discover(); Capability(1); assert(g_selectedMode == 1 && methodCalls == 3);
    Discover(); Capability(3); int before = methodCalls;
    Response(1,7); assert(methodCalls == before+1 && g_selectedMode == 1 && g_retried && rollbacks == 1);
    Response(1,255); assert(methodCalls == before+1 && g_clientAppId == -1 && failures == 1);
    Discover(); Capability(3); secure = false; before = methodCalls;
    Response(1,255); assert(methodCalls == before && failures == 2 && g_clientAppId == -1);
    Discover(); Capability(3); before = methodCalls;
    Response(1,255,1); assert(methodCalls == before && failures == 3);
    Discover(); Capability(3); before = methodCalls;
    Response(1,255,0,1); assert(methodCalls == before && failures == 4);
    Discover(); Capability(3); Response(1,0); before = methodCalls;
    Response(2,255); assert(methodCalls == before && failures == 5); // never fallback after enable
    Discover(1); Capability(3); assert(g_selectedMode == 1);
    Discover(3,false); before = methodCalls; int updates = capabilityUpdates;
    Capability(3); assert(supported == 1 && methodCalls == before && capabilityUpdates == updates);
    Discover(); before = methodCalls; Capability(2); assert(methodCalls == before && failures == 6);
    Discover(); reserveFails = true; before = methodCalls; Capability(3); assert(methodCalls == before && failures == 7);
    uint8_t modes = 99, alternate[] = {42,0,9,4,3,3,7,2,6,64,1,2};
    assert(IposlCodecGatewayModes(alternate,sizeof(alternate),&modes) && modes == 3);
    alternate[3] = 1; assert(!IposlCodecGatewayModes(alternate,sizeof(alternate),&modes));
    for (size_t n = 0; n < sizeof(alternate); ++n) assert(!IposlCodecGatewayModes(alternate,n,&modes));
    Discover(); Capability(3); before=methodCalls;
    int oldRollbacks=rollbacks;
    Response(1,0xfe);
    assert(methodCalls==before && g_selectedMode==3 && !g_retried && rollbacks==oldRollbacks);
    assert(g_clientAppId==-1 && failures==8); // busy never downgrades the requested mode
    reserveFails = false;
    Discover(); Capability(3); before=methodCalls;
    Response(1,255);
    assert(methodCalls==before && !g_retried && failures==9 && g_clientAppId==-1);
}
