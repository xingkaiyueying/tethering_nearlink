# Developer A Stage 0 evidence

Date: 2026-08-27

## Git baseline

| Repo | Branch | Created from | HEAD |
|---|---|---|---|
| `communication_nearlink_service` (`tethering_nearlink`) | `feature/ip-share-stage0-ssap-uuid` | `origin/dev` | `c2f28a6eaa3fc80895dad19d2efd27919da594eb` |
| `tethering_dhcp` | `feature/dhcp-l3-stage0-interface` | `origin/dev` | `96e26d3aef6dcc17dc0007908a25289b2f8f096e` |

Both working trees were clean at branch creation. No work on `master` or `dev`.

## UUID (P-06 table)

Frozen 7 values live in `utils/include/iposl_demo_uuid.h`. Host check:

```text
g++ -std=c++17 -Iutils/include test/host/iposl_demo_uuid_host_test.cpp
PASS 7 demo uuids
```

OHOS `nl_common_test` adds `iposl_demo_uuid_test.cpp` covering `IsValidUuid`, `UUID::FromString`, `Uuid::ConvertFromString`, `GetUuidType()==UUID128_BYTES_TYPE`, standard-base exclusion, and string/byte round-trip.

## SSAP method registration chain

Breaks found and fixed for the stage-0 probe:

1. Native `SsapServer::AddService()` copied properties only. It now copies methods into `NearlinkSsapServiceParcel`.
2. `SsapServerStackAdapter::AddService()` filled properties only. It now calls `FillMethodsToStackService()`.
3. `ssap_data.h` `Method` constructor ignored the permission argument; it now stores it.

IPC parcel already serialized `methods_`. Stack `SsapAllocServiceParam()` already copies `service->method` when `serviceMethodNum > 0`.

Discovery probe: `BuildDemoSsapDiscoveryProbe()` — empty identity service + IPoSL config service with 4 non-empty properties and 1 vendor method. Device-side service discovery (P-06 on hardware) is still required before stage 1.

## DTAP PI=1

| Symbol | Path |
|---|---|
| `DTAP_PI_IPV4` (=1) | `services/stack/src/dp/dtap/interface/dtap.h` |
| `DTAP_DataSend` | same |
| `DTAP_RegisterProtoRecvCbk(pi, cbk)` | same; one callback per PI |

Added `DTAP_PI_IPV4_RegisterOnce` in `dtap_test.cpp`. Existing tests already cover null/invalid PI and register/unregister.

## QOSM unique callback and multi-channel

| Symbol | Path |
|---|---|
| `QOSM_TransChannelParams_S` (peer addr, src/dst port, unicast access mode) | `services/stack/src/cp/bsl/sle/qosm/interface/qosm_trans_channel.h` |
| `QOSM_TransChannelRspParams_S` (lcid, tcid, mtu, status) | same |
| `QOSM_TransChannelCbksRegister` | `qosm_trans_channel.c`: assigns the single global `g_transChannelCbks` |
| `QOSM_TransChannelCreate` / `Destroy` | per-peer unicast channel |

A second `QOSM_TransChannelCbksRegister` overwrites DataTransfer. Stage 1 must dispatch `srcPort==30200` inside that unique callback. `SleDataTransferService::IsValidSrcPort()` only accepts ports already in `appConnectParamMap_` (range starts at 30300). Three-peer device probe (P-03) remains a device gate.

## Still not stage-0 complete until

- OHOS unittests run in the OpenHarmony tree (`nl_common_test`, `nl_ssap_server_service_test`, `UT_DTAP_TEST`).
- P-03 three QOSM channels on device.
- P-06 service discovery on device sees 4 properties + 1 method.
