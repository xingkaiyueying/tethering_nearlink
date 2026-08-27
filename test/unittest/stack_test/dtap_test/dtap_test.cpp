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
#include <gtest/gtest.h>
#include "securec.h"

#include "byte_codec.h"
#include "dtap.h"
#include "dtap_frame.h"
#include "dtap_trans.h"
#include "dtap_errno.h"
#include "dtap_tcid.h"
#include "cm_errno.h"
#include "cm_logic_link_api.h"
#include "cm_api.h"
#include "cm_trans_channel_api.h"
#include "transport.h"
#include "transport_cltp.h"
#include "transport_errno.h"
#include "transport_internal.h"
#include "sle_logic_link_mgr.h"
#include "sdf_buff.h"
#include "sdf_mem.h"
#include "sdf_worker.h"
#include "cp_worker.h"
#include "dli_layer.h"
#include "dli_layer_callback.h"
#include "dli_callback.h"

#include "stack_schedule_mock.h"
#include "stack_schedule_stub.h"
#include "stack_dli_layer_mock.h"
#include "stack_dli_layer_stub.h"
#include "stack_cm_mock.h"
#include "stack_cm_stub.h"

using namespace testing;
using namespace testing::ext;
using namespace OHOS;

typedef struct {
    SDF_DListEntry_S entry;
    uint8_t sar; /* 分片状态指示 */
    uint16_t txSeq;
    SDF_Buff_S *buff;
    uint64_t timeStamp;
    uint8_t pi;
    uint16_t lcid;
    uint8_t tcid;
} StreamRxNode;
#define UT_DTAP_TRANS_RELIABLE_STUB_NUM 5
static std::vector<DTAP_Frame_S *> g_dtapFrames;
static DTAP_Basic_Channel basicTrans = { .isReady = true };
static DTAP_Channel_S g_dtapChannel = { .attr = (void *)&basicTrans };
static void DTAP_ClearFrames()
{
    for (DTAP_Frame_S *frame : g_dtapFrames) {
        DTAP_DestroyFrame(frame);
    }
    g_dtapFrames.clear();
}
#define TEST_NUM 10
static void Test_DTAP_TransChannelStatusChangeCbk(uint16_t lcid, uint16_t tcid, uint8_t result)
{
    (void)lcid;
    (void)tcid;
    (void)result;
}

static int TEST_recvCb(DTAP_Data_Info_S *info, SDF_Buff_S *buf)
{
    if (info == NULL) {
        return DTAP_TRANS_RELIABLE_TRANS_CHANNEL_FULL;
    }
    (void)buf;
    return 0;
}

static void TestDliCallbackInit(void)
{
    DLI_Callback dliCallback = {0};
    dliCallback.postOtherThread = CP_PostTask;
    dliCallback.postOtherBlockedThread = CP_PostTaskBlocked;
    dliCallback.dftReportKill = NULL;
    dliCallback.recvAcbHandler = DTAP_DataRecv;
    DLI_SetCallback(&dliCallback);
    return;
}

class UT_DTAP_TEST : public testing::Test {
public:
    NiceMock<ScheduleMock> scheduleMock;
    NiceMock<CmMock> cmMock;
protected:
    // SetUP 在每一个 TEST_F 测试开始前执行一次
    virtual void SetUp()
    {
        TEST_ScheduleInit();
        EXPECT_CALL(scheduleMock, SchedulePostTask).WillRepeatedly(TEST_SchedulePostTaskStub);
        EXPECT_CALL(scheduleMock, SchedulePostTaskBlocked).WillRepeatedly(TEST_SchedulePostTaskBlockedStub);
        EXPECT_CALL(scheduleMock, ScheduleTimerAdd).WillRepeatedly(TEST_ScheduleTimerAddStub);
        EXPECT_CALL(scheduleMock, ScheduleTimerDel).WillRepeatedly(TEST_ScheduleTimerDelStub);
        TestDliCallbackInit();
        DLI_LayerInit();
        DLI_LayerEnable();
        TEST_CM_Init();
        EXPECT_CALL(cmMock, CM_RegTransChannelListener).WillRepeatedly(TEST_CM_RegTransChannelListener);
        EXPECT_CALL(cmMock, CM_UnRegTransChannelListener).WillRepeatedly(TEST_CM_UnRegTransChannelListener);
        EXPECT_CALL(cmMock, CM_GetLogicLinkConnectedSize).WillRepeatedly(TEST_CM_GetLogicLinkConnectedSize);
        DTAP_Init();
    }

    // TearDown 在每一个 TEST_F 测试完成后执行一次
    virtual void TearDown()
    {
        DTAP_DeInit();
        TEST_CM_DeInit();
        DLI_LayerDisable();
        DLI_LayerDeinit();
        TEST_StackScheduleDeInit();
    }

    // SetUpTestCase 在所有 TEST_F 测试开始前执行一次
    static void SetUpTestCase()
    {
    }

    // TearDownTestCase 在所有 TEST_F 测试完成后执行一次
    static void TearDownTestCase()
    {
    }
};

static CM_TransChan_S *CM_TransChanCreate(uint8_t castMode, uint16_t lcid, uint8_t srcTcid,
    uint8_t dstTcid, uint8_t mode)
{
    CM_TransChan_S *channel = (CM_TransChan_S *)SDF_MemZalloc(sizeof(CM_TransChan_S));
    if (channel == NULL) {
        return NULL;
    }
    channel->castMode = castMode;
    channel->lcid = lcid;
    channel->srcTcid = srcTcid;
    channel->dstTcid = dstTcid;
    channel->config.transMode = mode;
    channel->config.mtu = UINT16_MAX;
    channel->config.mps = UINT16_MAX;

    return channel;
}

TEST_F(UT_DTAP_TEST, DTAP_ParseFrameTest)
{
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(100);
    EXPECT_NE(buff, nullptr);
    EXPECT_NE(SDF_BuffAppend(buff, 100), nullptr);

    DTAP_Frame_S frame;
    memset_s(&frame, sizeof(DTAP_Frame_S), 0, sizeof(DTAP_Frame_S));
    uint32_t ret = DTAP_ParseFrame(DTAP_FRAME_MAX, buff, &frame);
    EXPECT_NE(ret, DTAP_SUCCESS);

    ret = DTAP_ParseFrame(DTAP_FRAME_BASIC, buff, &frame);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    SDF_BuffFree(buff);
}

TEST_F(UT_DTAP_TEST, DTAP_CreateFrameTestInvalidType)
{
    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_MAX);
    EXPECT_EQ(frame, nullptr);
    DTAP_DestroyFrame(frame);
}

TEST_F(UT_DTAP_TEST, DTAP_CopyFrameTest)
{
    DTAP_Frame_S *dstFrame = DTAP_CopyFrame(NULL);
    EXPECT_EQ(dstFrame, nullptr);

    DTAP_Frame_S srcFrame;
    memset_s(&srcFrame, sizeof(DTAP_Frame_S), 0, sizeof(DTAP_Frame_S));
    dstFrame = DTAP_CopyFrame(&srcFrame);
    EXPECT_NE(dstFrame, nullptr);
    EXPECT_EQ(dstFrame->buff, nullptr);
    DTAP_DestroyFrame(dstFrame);
}

TEST_F(UT_DTAP_TEST, DTAP_ParseExtensionTest)
{
    uint32_t ret = DTAP_ParseExtension(NULL);
    EXPECT_EQ(ret, DTAP_FRAME_NULL_PTR_ERR);

    DTAP_Frame_S frame;
    memset_s(&frame, sizeof(DTAP_Frame_S), 0, sizeof(DTAP_Frame_S));
    ret = DTAP_ParseExtension(&frame);
    EXPECT_EQ(ret, DTAP_FRAME_NULL_PTR_ERR);

    frame.buff = SDF_BuffNewWithReserve(100);
    EXPECT_NE(frame.buff, nullptr);
    ret = DTAP_ParseExtension(&frame);
    EXPECT_EQ(ret, DTAP_FRAME_NULL_PTR_ERR);

    frame.header = frame.buff;
    frame.headerLen = 1;
    ret = DTAP_ParseExtension(&frame);
    EXPECT_EQ(ret, DTAP_FRAME_INVALID_BUFF_LEN);

    frame.headerLen = 0;
    ret = DTAP_ParseExtension(&frame);
    EXPECT_EQ(ret, DTAP_FRAME_INVALID_EXT_HDR_LEN);

    EXPECT_NE(SDF_BuffAppend(frame.buff, 100), nullptr);
    frame.headerLen = 0;
    frame.header = SDF_DataOffset(frame.buff);
    DTAP_ExtensionHeader_S *extHeader = (DTAP_ExtensionHeader_S *)frame.header;
    extHeader->num = 1;
    DTAP_Extension_S *extension = extHeader->data;
    extension->length = 100;
    ret = DTAP_ParseExtension(&frame);
    EXPECT_EQ(ret, DTAP_FRAME_INVALID_EXT_LEN);

    extension->length = 1;
    ret = DTAP_ParseExtension(&frame);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    SDF_BuffFree(frame.buff);
}

TEST_F(UT_DTAP_TEST, DTAP_SetFrameBitTest)
{
    uint32_t ret = DTAP_SetFrameBit(NULL, 0);
    EXPECT_EQ(ret, DTAP_FRAME_NULL_PTR_ERR);

    DTAP_BasicHeader_S frameHeader;
    memset_s(&frameHeader, sizeof(DTAP_BasicHeader_S), 0, sizeof(DTAP_BasicHeader_S));
    ret = DTAP_SetFrameBit(&frameHeader, 0);
    EXPECT_EQ(ret, DTAP_SUCCESS);
}

TEST_F(UT_DTAP_TEST, DTAP_ReCalculateCrcValueTest)
{
    uint32_t ret = DTAP_ReCalculateCrcValue(0, NULL);
    EXPECT_EQ(ret, DTAP_FRAME_NULL_PTR_ERR);

    DTAP_Frame_S frame;
    memset_s(&frame, sizeof(DTAP_Frame_S), 0, sizeof(DTAP_Frame_S));
    ret = DTAP_ReCalculateCrcValue(0, &frame);
    EXPECT_EQ(ret, DTAP_FRAME_NULL_PTR_ERR);

    frame.buff = SDF_BuffNewWithReserve(100);
    EXPECT_NE(frame.buff, nullptr);
    ret = DTAP_ReCalculateCrcValue(0, &frame);
    EXPECT_EQ(ret, DTAP_FRAME_INVALID_BUFF_LEN);

    EXPECT_NE(SDF_BuffAppend(frame.buff, 10), nullptr);
    ret = DTAP_ReCalculateCrcValue(0, &frame);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    SDF_BuffFree(frame.buff);
}

TEST_F(UT_DTAP_TEST, DTAP_TransModeInvalid)
{
    DTAP_TransMode_S *mode = DTAP_GetTransMode(CM_TRANS_MODE_MAX);
    EXPECT_EQ(mode, nullptr);
}

TEST_F(UT_DTAP_TEST, DTAP_GetNextFrameSeq)
{
    uint16_t seq = DTAP_GetNextFrameSeq(DTAP_FRAME_SEQ_MAX);
    EXPECT_EQ(seq, 0);
}

TEST_F(UT_DTAP_TEST, DTAP_TransStopTimer)
{
    int timer = INVALID_TIMER_HANDLE;
    DTAP_TransStopTimer(NULL);
    EXPECT_EQ(timer, INVALID_TIMER_HANDLE);
    DTAP_TransStopTimer(&timer);
    EXPECT_EQ(timer, INVALID_TIMER_HANDLE);
}

TEST_F(UT_DTAP_TEST, DTAP_TransBasicMode)
{
    DTAP_Frame_S *dtapFrame = DTAP_CreateFrame(DTAP_FRAME_BASIC);
    EXPECT_NE(dtapFrame, nullptr);
    DTAP_BasicFrameHeader_S head;
    dtapFrame->header = &head;
    DTAP_TransMode_S *mode = DTAP_GetTransMode(CM_TRANS_MODE_BASIC);
    EXPECT_NE(mode, nullptr);
    // getModeType
    EXPECT_NE(mode->getModeType, nullptr);
    EXPECT_EQ(mode->getModeType(), CM_TRANS_MODE_BASIC);
    // checkFrame
    EXPECT_NE(mode->checkFrame, nullptr);
    head.header.frameType = DTAP_FRAME_SIMPLEX_AGGR;
    EXPECT_FALSE(mode->checkFrame(nullptr, dtapFrame));
    head.header.frameType = DTAP_FRAME_BASIC;
    EXPECT_TRUE(mode->checkFrame(nullptr, dtapFrame));
    // recvFrame
    EXPECT_NE(mode->recvFrame, nullptr);
    DTAP_Channel_S transChan;
    (void)memset_s(&transChan, sizeof(transChan), 0, sizeof(transChan));
    SDF_DListHeadInit(&transChan.pktList);
    SDF_DListEntryInit(&transChan.schedEntry);
    SDF_DListEntryInit(&transChan.entry);
    DTAP_Basic_Channel_S basic;
    basic.isReady = true;
    transChan.attr = &basic;
    // dtapFrame->buff error
    dtapFrame->buff = NULL;
    EXPECT_EQ(mode->recvFrame(&transChan, dtapFrame, TEST_recvCb), DTAP_TRANS_BASIC_RECV_TRIM_PRE_ERR);
    SDF_Buff_S *buf = SDF_BuffNewWithReserve(TEST_NUM);
    EXPECT_NE(buf, nullptr);
    dtapFrame->buff = buf;
    mode->recvFrame(&transChan, dtapFrame, TEST_recvCb);
    EXPECT_EQ(mode->recvFrame(&transChan, dtapFrame, NULL), DTAP_SUCCESS);
    basic.isReady = false;
    SDF_DListHeadInit(&basic.cacheRxBuffs);
    EXPECT_EQ(mode->recvFrame(&transChan, dtapFrame, TEST_recvCb), DTAP_SUCCESS);
    DTAP_RecvBasicFrameContinue(&transChan);
    basic.isReady = false;
    EXPECT_EQ(mode->recvFrame(&transChan, dtapFrame, NULL), DTAP_SUCCESS);
    DTAP_RecvBasicFrameContinue(&transChan);
    // sendFrame
    transChan.priority = DTAP_PRIORITY_MAX;
    EXPECT_NE(mode->sendFrame, nullptr);
    mode->sendFrame(&transChan, 0, buf);
    DTAP_DestroyFrame(dtapFrame);
    SDF_Buff_S *failBuf = SDF_BuffNew(TEST_NUM);
    EXPECT_NE(mode->sendFrame(&transChan, 0, failBuf), DTAP_SUCCESS);
    SDF_BuffFree(failBuf);
}

TEST_F(UT_DTAP_TEST, DTAP_TransReliableMode)
{
    DTAP_Channel_S transChan;
    DTAP_ReliableChannel_S channel;
    (void)memset_s(&channel, sizeof(channel), 0, sizeof(channel));
    (void)memset_s(&transChan, sizeof(transChan), 0, sizeof(transChan));
    SDF_DListHeadInit(&channel.txWindow.txList);
    SDF_DListHeadInit(&channel.rxWindow.rxList);
    SDF_DListHeadInit(&transChan.pktList);
    SDF_DListEntryInit(&transChan.schedEntry);
    SDF_DListEntryInit(&transChan.entry);
    DTAP_Frame_S *dtapFrame = DTAP_CreateFrame(CM_TRANS_MODE_RELIABLE);
    EXPECT_NE(dtapFrame, nullptr);
    DTAP_TransMode_S *mode = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE);
    EXPECT_NE(mode, nullptr);
    // getModeType
    EXPECT_NE(mode->getModeType, nullptr);
    EXPECT_EQ(mode->getModeType(), CM_TRANS_MODE_RELIABLE);
    // checkFrame
    EXPECT_NE(mode->checkFrame, nullptr);
    transChan.attr = &channel;
    SDF_Buff_S *buf = SDF_BuffNewWithReserve(TEST_NUM * TEST_NUM);
    EXPECT_NE(buf, nullptr);
    EXPECT_NE(SDF_BuffAppend(buf, TEST_NUM), nullptr);
    dtapFrame->buff = buf;
    EXPECT_TRUE(mode->checkFrame(&transChan, dtapFrame));
    // recvFrame
    EXPECT_NE(mode->recvFrame, nullptr);
    mode->recvFrame(&transChan, dtapFrame, TEST_recvCb);
    mode->recvFrame(&transChan, dtapFrame, TEST_recvCb);
    // sendFrame
    EXPECT_NE(mode->sendFrame, nullptr);
    transChan.mps = DTAP_SIMPLEX_FRAG_FRAME_HEADER_LEN + DTAP_CRC_LEN;
    mode->sendFrame(&transChan, 0, buf);
    // /增加覆盖率
    EXPECT_NE(mode->setTransChannelStatus, nullptr);
    mode->setTransChannelStatus(&transChan, 0, TEST_recvCb);
    channel.isRxWindowFull = true;
    mode->setTransChannelStatus(&transChan, 0, TEST_recvCb);
    DTAP_DestroyFrame(dtapFrame);
}

TEST_F(UT_DTAP_TEST, DTAP_TransStreamMode)
{
    DTAP_Channel_S transChan;
    DTAP_SimplexFragFrameHeader_S simHeader;
    (void)memset_s(&simHeader, sizeof(simHeader), 0, sizeof(simHeader));
    DTAP_Stream_Channel_S streamChannel;
    (void)memset_s(&streamChannel, sizeof(streamChannel), 0, sizeof(streamChannel));
    SDF_DListHeadInit(&streamChannel.rxList);
    SDF_DListHeadInit(&streamChannel.txList);
    transChan.attr = &streamChannel;
    SDF_DListHeadInit(&transChan.pktList);
    SDF_DListEntryInit(&transChan.schedEntry);
    SDF_DListEntryInit(&transChan.entry);
    DTAP_Frame_S *dtapFrame = DTAP_CreateFrame(DTAP_FRAME_BASIC);
    EXPECT_NE(dtapFrame, nullptr);
    dtapFrame->header = &simHeader;
    dtapFrame->headerLen = sizeof(DTAP_SimplexFragFrameHeader_S);
    DTAP_TransMode_S *mode = DTAP_GetTransMode(CM_TRANS_MODE_STREAM);
    EXPECT_NE(mode, nullptr);
    SDF_Buff_S *buf = SDF_BuffNewWithReserve(TEST_NUM * TEST_NUM);
    EXPECT_NE(buf, nullptr);
    EXPECT_NE(SDF_BuffAppend(buf, TEST_NUM), nullptr);
    dtapFrame->buff = buf;
    // getModeType
    EXPECT_NE(mode->getModeType, nullptr);
    EXPECT_EQ(mode->getModeType(), CM_TRANS_MODE_STREAM);
    // checkFrame
    EXPECT_NE(mode->checkFrame, nullptr);
    simHeader.header.frameType = DTAP_FRAME_BASIC;
    EXPECT_FALSE(mode->checkFrame(nullptr, dtapFrame));
    simHeader.header.frameType = DTAP_FRAME_SIMPLEX_FRAG;
    EXPECT_TRUE(mode->checkFrame(nullptr, dtapFrame));
    // recvFrame
    EXPECT_NE(mode->recvFrame, nullptr);
    mode->recvFrame(&transChan, dtapFrame, TEST_recvCb);
    simHeader.sar = DTAP_SAR_UNSEG;
    mode->recvFrame(&transChan, dtapFrame, TEST_recvCb);
    // sendFrame
    EXPECT_NE(mode->sendFrame, nullptr);
    transChan.castMode = DTAP_FRAME_BROADCAST;
    mode->sendFrame(&transChan, 0, buf);
    DTAP_DestroyFrame(dtapFrame);
}

TEST_F(UT_DTAP_TEST, DTAP_TransTransparentMode)
{
    DTAP_Channel_S transChan;
    (void)memset_s(&transChan, sizeof(transChan), 0, sizeof(transChan));
    SDF_DListHeadInit(&transChan.pktList);
    SDF_DListEntryInit(&transChan.schedEntry);
    SDF_DListEntryInit(&transChan.entry);
    DTAP_TransMode_S *mode = DTAP_GetTransMode(CM_TRANS_MODE_TRANSPARENT);
    EXPECT_NE(mode, nullptr);
    // getModeType
    EXPECT_NE(mode->getModeType, nullptr);
    EXPECT_EQ(mode->getModeType(), CM_TRANS_MODE_TRANSPARENT);
    // checkFrame
    EXPECT_EQ(mode->checkFrame, nullptr);
    // recvFrame
    EXPECT_EQ(mode->recvFrame, nullptr);
    // sendFrame
    EXPECT_NE(mode->sendFrame, nullptr);
    SDF_Buff_S *buf = SDF_BuffNewWithReserve(TEST_NUM);
    EXPECT_NE(buf, nullptr);
    EXPECT_NE(SDF_BuffAppend(buf, 1), nullptr);
    EXPECT_EQ(mode->sendFrame(&transChan, 0, buf), DTAP_TRANS_TRANSPARENT_PACK_EXCEED_MPS);
    transChan.mps = TEST_NUM;
    transChan.priority = DTAP_PRIORITY_MAX;
    mode->sendFrame(&transChan, 0, buf);
    EXPECT_NE(mode->transFrame, nullptr);
    mode->transFrame(&transChan, buf, TEST_recvCb);
    mode->transFrame(&transChan, buf, NULL);
    SDF_BuffFree(buf);
}

// 基础帧
TEST_F(UT_DTAP_TEST, DTAP_BasicFrameCtx)
{
    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_BASIC);
    EXPECT_NE(frame, nullptr);
    // getFrameType
    EXPECT_NE(frame->ctx.getFrameType, nullptr);
    EXPECT_EQ(frame->ctx.getFrameType(), DTAP_FRAME_BASIC);
    // getHeaderLen
    EXPECT_NE(frame->ctx.getHeaderLen, nullptr);
    EXPECT_EQ(frame->ctx.getHeaderLen(), DTAP_BASIC_HEADER_LEN);
    // buildFrame
    EXPECT_NE(frame->ctx.buildFrame, nullptr);
    EXPECT_EQ(frame->ctx.buildFrame(&frame->ctx, TCID_FTC_RFU_END + 1, 0), DTAP_TRANS_BASIC_SEND_PREPEND_ERR);
    SDF_Buff_S *buf = SDF_BuffNewWithReserve(DTAP_BASIC_HEADER_LEN + TEST_NUM);
    EXPECT_NE(buf, nullptr);
    frame->buff = buf;
    EXPECT_EQ(frame->ctx.buildFrame(&frame->ctx, TCID_FTC_RFU_END + 1, 0), DTAP_SUCCESS);
    // parseFrame
    EXPECT_NE(frame->ctx.parseFrame, nullptr);
    DTAP_BasicFrameHeader_S basicHeader;
    (void)memset_s(&basicHeader, sizeof(basicHeader), 0, sizeof(basicHeader));
    frame->header = &basicHeader;
    basicHeader.header.tcid = TCID_FTC_RFU_END;
    EXPECT_EQ(frame->ctx.parseFrame(&frame->ctx), DTAP_SUCCESS);
    basicHeader.header.tcid = TCID_MC_END;
    EXPECT_EQ(frame->ctx.parseFrame(&frame->ctx), DTAP_SUCCESS);
    // checkFrameHeader
    EXPECT_NE(frame->ctx.checkFrameHeader, nullptr);
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), true);
    basicHeader.header.fBit = 1;
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), false);
    basicHeader.header.typeBits = 0;
    basicHeader.header.pBit = 1;
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), false);
    basicHeader.header.typeBits = 0;
    basicHeader.header.crcBit = 1;
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), false);
    basicHeader.header.typeBits = 0;
    basicHeader.header.optionBit = 1;
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), false);
    DTAP_DestroyFrame(frame);
}

TEST_F(UT_DTAP_TEST, DTAP_RecvAggregateFrame)
{
    DTAP_Frame_S frame;
    DTAP_Data_Info_S info = {0};
    memset_s(&frame, sizeof(DTAP_Frame_S), 0, sizeof(DTAP_Frame_S));
    uint32_t ret = DTAP_RecvAggregateFrame(NULL, &info, NULL);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_NULL);
    ret = DTAP_RecvAggregateFrame(&frame, &info, NULL);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_NULL);

    char payloadBuf[100] = {0};
    frame.payload = (uint8_t *)payloadBuf;
    ret = DTAP_RecvAggregateFrame(&frame, &info, NULL);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    ret = DTAP_RecvAggregateFrame(&frame, &info, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_SUCCESS);

    frame.payloadLen = 1;
    ret = DTAP_RecvAggregateFrame(&frame, &info, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_SDU_HDR_LEN_TOO_SHORT);

    frame.payloadLen = sizeof(DTAP_AggregateSdu_S);
    ret = DTAP_RecvAggregateFrame(&frame, &info, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_SDU_LEN_TOO_SHORT);

    DTAP_AggregateSdu_S *sdu = (DTAP_AggregateSdu_S *)frame.payload;
    ENCODE2BYTE_LITTLE(&sdu->length, 200);
    ret = DTAP_RecvAggregateFrame(&frame, &info, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_SDU_LEN_TOO_SHORT);

    frame.payloadLen = sizeof(DTAP_AggregateSdu_S) + 1;
    ENCODE2BYTE_LITTLE(&sdu->length, 1);
    ret = DTAP_RecvAggregateFrame(&frame, NULL, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_TRANS_RELIABLE_TRANS_CHANNEL_FULL);
    frame.payloadLen = sizeof(DTAP_AggregateSdu_S) + 1;
    ENCODE2BYTE_LITTLE(&sdu->length, 1);
    ret = DTAP_RecvAggregateFrame(&frame, &info, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_SUCCESS);
}

TEST_F(UT_DTAP_TEST, DTAP_RecvFragmentFrameInvalidPktLen)
{
    DTAP_Frame_S frame;
    memset_s(&frame, sizeof(DTAP_Frame_S), 0, sizeof(DTAP_Frame_S));

    frame.buff = SDF_BuffNewWithReserve(UINT16_MAX);
    EXPECT_NE(frame.buff, nullptr);
    EXPECT_NE(SDF_BuffAppend(frame.buff, UINT16_MAX), nullptr);

    SDF_Buff_S *buffs = NULL;
    DTAP_Data_Info_S info = {0};
    frame.enhance.sar = DTAP_SAR_FIRST;
    uint32_t ret = DTAP_RecvFragmentFrame(&buffs, &frame, &info, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    EXPECT_NE(buffs, nullptr);

    frame.enhance.sar = DTAP_SAR_LAST;
    ret = DTAP_RecvFragmentFrame(&buffs, &frame, &info, TEST_recvCb);
    EXPECT_EQ(buffs, nullptr);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_FRAGMENT_FRAME_LEN_ERR);
    SDF_BuffFree(frame.buff);
}

TEST_F(UT_DTAP_TEST, DTAP_RecvFragmentFrame)
{
    uint32_t ret = DTAP_RecvFragmentFrame(NULL, NULL, NULL, NULL);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_NULL);

    SDF_Buff_S *buffs = NULL;
    ret = DTAP_RecvFragmentFrame(&buffs, NULL, NULL, NULL);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_NULL);

    DTAP_Frame_S frame;
    memset_s(&frame, sizeof(DTAP_Frame_S), 0, sizeof(DTAP_Frame_S));
    ret = DTAP_RecvFragmentFrame(&buffs, &frame, NULL, NULL);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_TRIM_PREFIX_ERR);

    frame.buff = SDF_BuffNewWithReserve(100);
    EXPECT_NE(frame.buff, nullptr);
    ret = DTAP_RecvFragmentFrame(&buffs, &frame, NULL, NULL);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_TRIM_SUFFIX_ERR);

    EXPECT_NE(SDF_BuffAppend(frame.buff, 100), nullptr);
    frame.enhance.sar = DTAP_SAR_UNSEG;
    ret = DTAP_RecvFragmentFrame(&buffs, &frame, NULL, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_TRANS_RELIABLE_TRANS_CHANNEL_FULL);

    frame.enhance.sar = DTAP_SAR_MID;
    ret = DTAP_RecvFragmentFrame(&buffs, &frame, NULL, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_NO_FIRST_FRAG_FRAME);

    frame.enhance.sar = DTAP_SAR_FIRST;
    ret = DTAP_RecvFragmentFrame(&buffs, &frame, NULL, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    EXPECT_NE(buffs, nullptr);
    ret = DTAP_RecvFragmentFrame(&buffs, &frame, NULL, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_ENHANCED_FRAME_DUP_FIRST_FRAG_FRAME);
    EXPECT_NE(buffs, nullptr);

    frame.enhance.sar = DTAP_SAR_MID;
    ret = DTAP_RecvFragmentFrame(&buffs, &frame, NULL, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_SUCCESS);

    DTAP_Data_Info_S info = {0};
    frame.enhance.sar = DTAP_SAR_LAST;
    ret = DTAP_RecvFragmentFrame(&buffs, &frame, &info, TEST_recvCb);
    EXPECT_EQ(buffs, nullptr);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    SDF_BuffFree(frame.buff);
}

TEST_F(UT_DTAP_TEST, DTAP_RecvFragmentFrameNullCbk)
{
    DTAP_Frame_S frame;
    memset_s(&frame, sizeof(DTAP_Frame_S), 0, sizeof(DTAP_Frame_S));
    frame.buff = SDF_BuffNewWithReserve(100);
    EXPECT_NE(frame.buff, nullptr);
    EXPECT_NE(SDF_BuffAppend(frame.buff, 100), nullptr);

    SDF_Buff_S *buffs = NULL;
    frame.enhance.sar = DTAP_SAR_FIRST;
    uint32_t ret = DTAP_RecvFragmentFrame(&buffs, &frame, NULL, TEST_recvCb);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    EXPECT_NE(buffs, nullptr);

    DTAP_Data_Info_S info = {0};
    frame.enhance.sar = DTAP_SAR_LAST;
    ret = DTAP_RecvFragmentFrame(&buffs, &frame, &info, NULL);
    EXPECT_EQ(buffs, nullptr);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    SDF_BuffFree(frame.buff);
}

TEST_F(UT_DTAP_TEST, DTAP_FragmentFrame)
{
    DTAP_FragmentFrame(NULL, 0, DTAP_FRAME_BASIC, NULL, NULL);

    SDF_Buff_S *buff = SDF_BuffNewWithReserve(200);
    EXPECT_NE(buff, nullptr);
    DTAP_FragmentFrame(buff, 0, DTAP_FRAME_BASIC, NULL, NULL);

    EXPECT_NE(SDF_BuffAppend(buff, 200), nullptr);
    DTAP_FragmentFrame(buff, 0, DTAP_FRAME_BASIC, NULL, NULL);

    DTAP_Frame_S *outFrames[DTAP_MAX_FRAGMENT_NUM] = {NULL};
    DTAP_FragmentFrame(buff, 0, DTAP_FRAME_BASIC, outFrames, NULL);

    uint16_t outCnt = 0;
    DTAP_FragmentFrame(buff, 0, DTAP_FRAME_BASIC, outFrames, &outCnt);

    DTAP_FragmentFrame(buff, 1, DTAP_FRAME_SIMPLEX_FRAG, outFrames, &outCnt);
    EXPECT_EQ(outCnt, 0);

    DTAP_FragmentFrame(buff, 0, DTAP_FRAME_SIMPLEX_FRAG, outFrames, &outCnt);
    EXPECT_EQ(outCnt, 1);
    for (uint32_t i = 0; i < outCnt; i++) {
        EXPECT_NE(outFrames[i], nullptr);
        DTAP_DestroyFrame(outFrames[i]);
        outFrames[i] = NULL;
    }

    DTAP_FragmentFrame(buff, 50, DTAP_FRAME_SIMPLEX_FRAG, outFrames, &outCnt);
    EXPECT_EQ(outCnt, 4);
    for (uint32_t i = 0; i < outCnt; i++) {
        EXPECT_NE(outFrames[i], nullptr);
        DTAP_DestroyFrame(outFrames[i]);
        outFrames[i] = NULL;
    }

    SDF_BuffFree(buff);
}

// 单向聚合帧
TEST_F(UT_DTAP_TEST, DTAP_SimplexFrameCtx)
{
    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_SIMPLEX_AGGR);
    EXPECT_NE(frame, nullptr);
    // getFrameType
    EXPECT_NE(frame->ctx.getFrameType, nullptr);
    EXPECT_EQ(frame->ctx.getFrameType(), DTAP_FRAME_SIMPLEX_AGGR);
    // getHeaderLen
    EXPECT_NE(frame->ctx.getHeaderLen, nullptr);
    EXPECT_EQ(frame->ctx.getHeaderLen(), DTAP_SIMPLEX_AGGR_FRAME_HEADER_LEN);
    // buildFrame
    EXPECT_NE(frame->ctx.buildFrame, nullptr);
    EXPECT_EQ(frame->ctx.buildFrame(&frame->ctx, 0, 0), DTAP_ENHANCED_FRAME_NOT_SUPPORT_ERR);
    // parseFrame
    EXPECT_NE(frame->ctx.parseFrame, nullptr);
    uint8_t tmp[100];
    (void)memset_s(tmp, sizeof(tmp), 0, sizeof(tmp));
    DTAP_BasicHeader_S *basicHeader = (DTAP_BasicHeader_S *)tmp;
    frame->header = basicHeader;
    SDF_Buff_S *buf = SDF_BuffNewWithReserve(DTAP_SIMPLEX_AGGR_FRAME_HEADER_LEN + TEST_NUM * TEST_NUM);
    EXPECT_NE(buf, nullptr);
    frame->buff = buf;
    EXPECT_EQ(frame->ctx.parseFrame(&frame->ctx), DTAP_ENHANCED_FRAME_HDR_LEN_TOO_SHORT);
    EXPECT_NE(SDF_BuffAppend(buf, DTAP_SIMPLEX_AGGR_FRAME_HEADER_LEN + 4), nullptr);
    EXPECT_EQ(frame->ctx.parseFrame(&frame->ctx), DTAP_SUCCESS);
    basicHeader->optionBit = 1;
    DTAP_ExtensionHeader_S *extHeader = (DTAP_ExtensionHeader_S *)&tmp[sizeof(DTAP_BasicHeader_S)];
    frame->extension = extHeader;
    extHeader->num = 1;
    DTAP_Extension_S *extension = extHeader->data;
    extension->length = 2;
    frame->ctx.parseFrame(&frame->ctx);
    // checkFrameHeader
    EXPECT_NE(frame->ctx.checkFrameHeader, nullptr);
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), false);
    basicHeader->crcBit = 1;
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), true);
    DTAP_DestroyFrame(frame);
}

// 单向分片帧
TEST_F(UT_DTAP_TEST, DTAP_SimplexFragFrameCtx)
{
    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_SIMPLEX_FRAG);
    EXPECT_NE(frame, nullptr);
    // getFrameType
    EXPECT_NE(frame->ctx.getFrameType, nullptr);
    EXPECT_EQ(frame->ctx.getFrameType(), DTAP_FRAME_SIMPLEX_FRAG);
    // getHeaderLen
    EXPECT_NE(frame->ctx.getHeaderLen, nullptr);
    EXPECT_EQ(frame->ctx.getHeaderLen(), DTAP_SIMPLEX_FRAG_FRAME_HEADER_LEN);
    // buildFrame
    EXPECT_NE(frame->ctx.buildFrame, nullptr);
    SDF_Buff_S *buf = SDF_BuffNewWithReserve(DTAP_SIMPLEX_FRAG_FRAME_HEADER_LEN + TEST_NUM * TEST_NUM);
    EXPECT_NE(buf, nullptr);
    frame->buff = buf;
    // parseFrame fail
    EXPECT_NE(frame->ctx.parseFrame(&frame->ctx), DTAP_SUCCESS);
    frame->ctx.buildFrame(&frame->ctx, 0, 0);
    // parseFrame
    EXPECT_NE(frame->ctx.parseFrame, nullptr);
    uint8_t tmp[100];
    (void)memset_s(tmp, sizeof(tmp), 0, sizeof(tmp));
    DTAP_BasicHeader_S *basicHeader = (DTAP_BasicHeader_S *)tmp;
    frame->header = basicHeader;
    EXPECT_NE(SDF_BuffAppend(buf, DTAP_SIMPLEX_AGGR_FRAME_HEADER_LEN + 4), nullptr);
    basicHeader->optionBit = 1;
    DTAP_ExtensionHeader_S *extHeader = (DTAP_ExtensionHeader_S *)&tmp[sizeof(DTAP_BasicHeader_S)];
    frame->extension = extHeader;
    extHeader->num = 1;
    DTAP_Extension_S *extension = extHeader->data;
    extension->length = 2;
    frame->ctx.parseFrame(&frame->ctx);
    // checkFrameHeader
    EXPECT_NE(frame->ctx.checkFrameHeader, nullptr);
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), false);
    basicHeader->crcBit = 1;
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), true);
    DTAP_DestroyFrame(frame);
}

// 双向聚合帧
TEST_F(UT_DTAP_TEST, DTAP_DuplexAggrFrameCtx)
{
    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_DUPLEX_AGGR);
    EXPECT_NE(frame, nullptr);
    // getFrameType
    EXPECT_NE(frame->ctx.getFrameType, nullptr);
    EXPECT_EQ(frame->ctx.getFrameType(), DTAP_FRAME_DUPLEX_AGGR);
    // getHeaderLen
    EXPECT_NE(frame->ctx.getHeaderLen, nullptr);
    EXPECT_EQ(frame->ctx.getHeaderLen(), DTAP_DUPLEX_AGGR_FRAME_HEADER_LEN);
    // buildFrame
    EXPECT_NE(frame->ctx.buildFrame, nullptr);
    SDF_Buff_S *buf = SDF_BuffNewWithReserve(DTAP_DUPLEX_AGGR_FRAME_HEADER_LEN + TEST_NUM * TEST_NUM);
    EXPECT_NE(buf, nullptr);
    EXPECT_NE(SDF_BuffAppend(buf, DTAP_SIMPLEX_AGGR_FRAME_HEADER_LEN + 4), nullptr);
    frame->buff = buf;
    EXPECT_EQ(frame->ctx.buildFrame(&frame->ctx, 0, 0), DTAP_ENHANCED_FRAME_NOT_SUPPORT_ERR);
    // parseFrame
    EXPECT_NE(frame->ctx.parseFrame, nullptr);
    uint8_t tmp[100];
    (void)memset_s(tmp, sizeof(tmp), 0, sizeof(tmp));
    DTAP_BasicHeader_S *basicHeader = (DTAP_BasicHeader_S *)tmp;
    frame->header = basicHeader;
    basicHeader->optionBit = 1;
    DTAP_ExtensionHeader_S *extHeader = (DTAP_ExtensionHeader_S *)&tmp[sizeof(DTAP_BasicHeader_S)];
    frame->extension = extHeader;
    extHeader->num = 1;
    DTAP_Extension_S *extension = extHeader->data;
    extension->length = 2;
    frame->ctx.parseFrame(&frame->ctx);
    // checkFrameHeader
    EXPECT_NE(frame->ctx.checkFrameHeader, nullptr);
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), false);
    basicHeader->crcBit = 1;
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), true);
    DTAP_DestroyFrame(frame);
}

// 双向分片帧
TEST_F(UT_DTAP_TEST, DTAP_DuplexFragFrameCtx)
{
    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_DUPLEX_FRAG);
    EXPECT_NE(frame, nullptr);
    // getFrameType
    EXPECT_NE(frame->ctx.getFrameType, nullptr);
    EXPECT_EQ(frame->ctx.getFrameType(), DTAP_FRAME_DUPLEX_FRAG);
    // getHeaderLen
    EXPECT_NE(frame->ctx.getHeaderLen, nullptr);
    EXPECT_EQ(frame->ctx.getHeaderLen(), DTAP_DUPLEX_FRAG_FRAME_HEADER_LEN);
    // buildFrame
    EXPECT_NE(frame->ctx.buildFrame, nullptr);
    SDF_Buff_S *buf = SDF_BuffNewWithReserve(DTAP_DUPLEX_FRAG_FRAME_HEADER_LEN + TEST_NUM * TEST_NUM);
    EXPECT_NE(buf, nullptr);
    frame->buff = buf;
    // parseFrame fail
    EXPECT_NE(frame->ctx.parseFrame(&frame->ctx), DTAP_SUCCESS);
    EXPECT_NE(SDF_BuffAppend(buf, DTAP_DUPLEX_FRAG_FRAME_HEADER_LEN + 4), nullptr);
    frame->ctx.buildFrame(&frame->ctx, 0, 1);
    // parseFrame
    EXPECT_NE(frame->ctx.parseFrame, nullptr);
    uint8_t tmp[100];
    (void)memset_s(tmp, sizeof(tmp), 0, sizeof(tmp));
    DTAP_BasicHeader_S *basicHeader = (DTAP_BasicHeader_S *)tmp;
    frame->header = basicHeader;
    basicHeader->optionBit = 1;
    DTAP_ExtensionHeader_S *extHeader = (DTAP_ExtensionHeader_S *)&tmp[sizeof(DTAP_BasicHeader_S)];
    frame->extension = extHeader;
    extHeader->num = 1;
    DTAP_Extension_S *extension = extHeader->data;
    extension->length = 2;
    frame->ctx.parseFrame(&frame->ctx);
    // checkFrameHeader
    EXPECT_NE(frame->ctx.checkFrameHeader, nullptr);
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), false);
    basicHeader->crcBit = 1;
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), true);
    DTAP_DestroyFrame(frame);
}

// 应答帧
TEST_F(UT_DTAP_TEST, DTAP_AckFrameCtx)
{
    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_ACK);
    EXPECT_NE(frame, nullptr);
    // getFrameType
    EXPECT_NE(frame->ctx.getFrameType, nullptr);
    EXPECT_EQ(frame->ctx.getFrameType(), DTAP_FRAME_ACK);
    // getHeaderLen
    EXPECT_NE(frame->ctx.getHeaderLen, nullptr);
    EXPECT_EQ(frame->ctx.getHeaderLen(), DTAP_ACK_FRAME_HEADER_LEN);
    SDF_Buff_S *buf = SDF_BuffNewWithReserve(DTAP_ACK_FRAME_HEADER_LEN + TEST_NUM * TEST_NUM);
    EXPECT_NE(buf, nullptr);
    frame->buff = buf;
    // parseFrame fail
    EXPECT_NE(frame->ctx.parseFrame(&frame->ctx), DTAP_SUCCESS);
    SDF_BuffFree(buf);
    // buildFrame
    EXPECT_NE(frame->ctx.buildFrame, nullptr);
    frame->ctx.buildFrame(&frame->ctx, 0, 1);
    // parseFrame
    EXPECT_NE(frame->ctx.parseFrame, nullptr);
    frame->header = SDF_DataOffset(frame->buff);
    frame->ctx.parseFrame(&frame->ctx);
    // checkFrameHeader
    EXPECT_NE(frame->ctx.checkFrameHeader, nullptr);
    EXPECT_EQ(frame->ctx.checkFrameHeader(&frame->ctx), true);
    DTAP_DestroyFrame(frame);
}

TEST_F(UT_DTAP_TEST, DTAP_RegisterDataRecvCb_NotInited)
{
    uint32_t ret;
    static bool isRecv = false;
    auto recvCb = [](DTAP_Data_Info *, SDF_Buff_S *) -> int {
        isRecv = true;
        return 0;
    };

    DTAP_DeInit();
    ret = DTAP_RegisterDataRecvCb(CM_TCID_FTC_RFU_END, recvCb);
    EXPECT_EQ(ret, DTAP_MODULE_NOT_INITED_ERR);
}

TEST_F(UT_DTAP_TEST, DTAP_RegisterDataRecvCb_NullCbk)
{
    uint32_t ret;
    ret = DTAP_RegisterDataRecvCb(CM_TCID_FTC_RFU_END, NULL);
    EXPECT_EQ(ret, DTAP_REGISTER_RECV_CB_NULL_ERR);
}

TEST_F(UT_DTAP_TEST, DTAP_RegisterDataRecvCb_Success)
{
    uint32_t ret;
    static bool isRecv = false;
    auto recvCb1 = [](DTAP_Data_Info *, SDF_Buff_S *) -> int {
        isRecv = true;
        return 0;
    };

    ret = DTAP_RegisterDataRecvCb(CM_TCID_FTC_RFU_END, recvCb1);
    EXPECT_EQ(ret, DTAP_SUCCESS);

    // Update recv cbk
    auto recvCb2 = [](DTAP_Data_Info *, SDF_Buff_S *) -> int {
        isRecv = true;
        return 0;
    };
    ret = DTAP_RegisterDataRecvCb(CM_TCID_FTC_RFU_END, recvCb2);
    EXPECT_EQ(ret, DTAP_SUCCESS);

    ret = DTAP_UnregisterDataRecvCb(CM_TCID_FTC_RFU_END);
    EXPECT_EQ(ret, DTAP_SUCCESS);
}

TEST_F(UT_DTAP_TEST, DTAP_UnregisterDataRecvCb_NotInited)
{
    uint32_t ret;
    DTAP_DeInit();
    ret = DTAP_UnregisterDataRecvCb(CM_TCID_FTC_RFU_END);
    EXPECT_EQ(ret, DTAP_MODULE_NOT_INITED_ERR);
}

TEST_F(UT_DTAP_TEST, DTAP_RegisterProtoRecvCbk_NotInited)
{
    uint32_t ret;
    static bool isRecv = false;
    auto recvCb = [](DTAP_Data_Info *, SDF_Buff_S *) -> int {
        isRecv = true;
        return 0;
    };
    DTAP_DeInit();
    ret = DTAP_RegisterProtoRecvCbk(DTAP_PI_NONE, recvCb);
    EXPECT_EQ(ret, DTAP_MODULE_NOT_INITED_ERR);
}

TEST_F(UT_DTAP_TEST, DTAP_RegisterProtoRecvCbk_InvalidPi)
{
    uint32_t ret;
    static bool isRecv = false;
    auto recvCb = [](DTAP_Data_Info *, SDF_Buff_S *) -> int {
        isRecv = true;
        return 0;
    };
    ret = DTAP_RegisterProtoRecvCbk(DTAP_PI_MAX, recvCb);
    EXPECT_EQ(ret, DTAP_REGISTER_RECV_CB_INVALID_PI);
}

TEST_F(UT_DTAP_TEST, DTAP_RegisterProtoRecvCbk_NullCbk)
{
    uint32_t ret;
    ret = DTAP_RegisterProtoRecvCbk(DTAP_PI_NONE, NULL);
    EXPECT_EQ(ret, DTAP_REGISTER_RECV_CB_NULL_ERR);
}

TEST_F(UT_DTAP_TEST, DTAP_RegisterProtoRecvCbk_Success)
{
    uint32_t ret;
    static bool isRecv = false;
    auto recvCb = [](DTAP_Data_Info *, SDF_Buff_S *) -> int {
        isRecv = true;
        return 0;
    };
    ret = DTAP_RegisterProtoRecvCbk(DTAP_PI_NONE, recvCb);
    EXPECT_EQ(ret, DTAP_SUCCESS);

    ret = DTAP_UnregisterProtoRecvCbk(DTAP_PI_NONE);
    EXPECT_EQ(ret, DTAP_SUCCESS);
}

TEST_F(UT_DTAP_TEST, DTAP_PI_IPV4_RegisterOnce)
{
    EXPECT_EQ(DTAP_PI_IPV4, 1);
    EXPECT_LT(DTAP_PI_IPV4, DTAP_PI_MAX);
    auto recvCb = [](DTAP_Data_Info *, SDF_Buff_S *) -> int {
        return 0;
    };
    uint32_t ret = DTAP_RegisterProtoRecvCbk(DTAP_PI_IPV4, recvCb);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    ret = DTAP_UnregisterProtoRecvCbk(DTAP_PI_IPV4);
    EXPECT_EQ(ret, DTAP_SUCCESS);
}

TEST_F(UT_DTAP_TEST, DTAP_UnregisterProtoRecvCbk_NotInited)
{
    uint32_t ret;
    DTAP_DeInit();
    ret = DTAP_UnregisterProtoRecvCbk(DTAP_PI_NONE);
    EXPECT_EQ(ret, DTAP_MODULE_NOT_INITED_ERR);
}

TEST_F(UT_DTAP_TEST, DTAP_UnregisterProtoRecvCbk_NotRegisterBefore)
{
    uint32_t ret;
    ret = DTAP_UnregisterProtoRecvCbk(DTAP_PI_NONE);
    EXPECT_EQ(ret, DTAP_REGISTER_RECV_CB_INVALID_PI);
}

TEST_F(UT_DTAP_TEST, DTAP_DataSend_InvalidData)
{
    uint32_t ret;

    ret = DTAP_DataSend(NULL);
    EXPECT_EQ(ret, DTAP_TRANS_INVALID_DATA);

    DTAP_Data_S data = {0};
    ret = DTAP_DataSend(&data);
    EXPECT_EQ(ret, DTAP_TRANS_INVALID_DATA);

    SDF_Buff_S buff = {0};
    data.buff = &buff;
    ret = DTAP_DataSend(&data);
    EXPECT_EQ(ret, DTAP_TRANS_INVALID_DATA);
}

TEST_F(UT_DTAP_TEST, DTAP_DataSend_ExccedMaxPayloadSize)
{
    uint32_t ret;
    DTAP_Data_S data = {0};
    SDF_Buff_S *buff = NULL;

    buff = SDF_BuffNewWithReserve(UINT16_MAX + 1);
    EXPECT_NE(buff, nullptr);
    uint8_t *tmp = SDF_BuffAppend(buff, UINT16_MAX + 1);
    EXPECT_NE(tmp, nullptr);
    data.buff = buff;
    ret = DTAP_DataSend(&data);
    EXPECT_EQ(ret, DTAP_TRANS_INVALID_DATA);
    SDF_BuffFree(buff);
}

TEST_F(UT_DTAP_TEST, DTAP_DataSend_Basic)
{
    uint32_t ret;
    DTAP_Data_S data = {0};
    SDF_Buff_S *buff = NULL;

    buff = SDF_BuffNewWithReserve(sizeof(uint8_t));
    EXPECT_NE(buff, nullptr);
    uint8_t *tmp = SDF_BuffAppend(buff, sizeof(uint8_t));
    EXPECT_NE(tmp, nullptr);
    data.buff = buff;
    g_dtapChannel.srcTcid = CM_TCID_SLE_CUTC;
    g_dtapChannel.dstTcid = CM_TCID_SLE_CUTC;
    g_dtapChannel.mtu = UINT16_MAX;
    g_dtapChannel.mps = UINT16_MAX;
    ret = DTAP_DataSend(&data);
    EXPECT_NE(ret, DTAP_SUCCESS);
}

TEST_F(UT_DTAP_TEST, DTAP_DataSend_Transparent)
{
    uint32_t ret;
    DTAP_Data_S data = {0};
    SDF_Buff_S *buff = NULL;

    buff = SDF_BuffNewWithReserve(sizeof(uint8_t));
    EXPECT_NE(buff, nullptr);
    uint8_t *tmp = SDF_BuffAppend(buff, sizeof(uint8_t));
    EXPECT_NE(tmp, nullptr);
    data.buff = buff;
    g_dtapChannel.mode = CM_TRANS_MODE_TRANSPARENT;
    ret = DTAP_DataSend(&data);
    EXPECT_NE(ret, DTAP_SUCCESS);
}

TEST_F(UT_DTAP_TEST, DTAP_DataSend_NotInit)
{
    uint32_t ret;
    DTAP_Data_S data = {0};
    SDF_Buff_S *buff = NULL;
    buff = SDF_BuffNewWithReserve(sizeof(uint8_t));
    EXPECT_NE(buff, nullptr);
    uint8_t *tmp = SDF_BuffAppend(buff, sizeof(uint8_t));
    EXPECT_NE(tmp, nullptr);
    data.buff = buff;

    ret = DTAP_DataSend(&data);
    EXPECT_NE(ret, DTAP_SUCCESS);
    SDF_BuffFree(buff);
}

TEST_F(UT_DTAP_TEST, DTAP_DataSend_ExceedMtu)
{
    CM_TransChannelStateList_S stateLists;
    SDF_Traits transChannelTraits = {.dtor = SDF_MemFree};
    stateLists.channelVector = SDF_CreateVector(transChannelTraits);
    EXPECT_NE(stateLists.channelVector, nullptr);

    CM_TransChan_S *chan = CM_TransChanCreate(0, 0, 0, 0, CM_TRANS_MODE_BASIC);
    EXPECT_NE(chan, nullptr);
    chan->config.mtu = 0;
    bool isSuccess = SDF_VectorEmplaceBack(stateLists.channelVector, chan);
    EXPECT_EQ(isSuccess, true);

    stateLists.result = CM_TRANS_CHANNEL_STATE_ACTIVATED;
    TEST_CM_TransChannelDo(&stateLists);

    uint32_t ret;
    DTAP_Data_S data = {0};
    SDF_Buff_S *buff = NULL;
    buff = SDF_BuffNewWithReserve(sizeof(uint8_t));
    EXPECT_NE(buff, nullptr);
    uint8_t *tmp = SDF_BuffAppend(buff, sizeof(uint8_t));
    EXPECT_NE(tmp, nullptr);
    data.buff = buff;

    ret = DTAP_DataSend(&data);
    EXPECT_NE(ret, DTAP_SUCCESS);

    DTAP_Channel_S *channle = DTAP_ChannelSearch(0, 0);
    EXPECT_NE(channle, nullptr);
    channle->mtu = 100;
    ret = DTAP_DataSend(&data);
    EXPECT_EQ(ret, DTAP_SUCCESS);

    stateLists.result = CM_TRANS_CHANNEL_STATE_RELEASED;
    TEST_CM_TransChannelDo(&stateLists);
}

TEST_F(UT_DTAP_TEST, DTAP_ChannelSetStatus)
{
    DTAP_ChannelSetStatus(0, 0, 0);

    CM_TransChannelStateList_S stateLists;
    SDF_Traits transChannelTraits = {.dtor = SDF_MemFree};
    stateLists.channelVector = SDF_CreateVector(transChannelTraits);
    EXPECT_NE(stateLists.channelVector, nullptr);

    CM_TransChan_S *chan = CM_TransChanCreate(0, 0, 0, 0, CM_TRANS_MODE_BASIC);
    EXPECT_NE(chan, nullptr);
    chan->config.mtu = 0;
    bool isSuccess = SDF_VectorEmplaceBack(stateLists.channelVector, chan);
    EXPECT_EQ(isSuccess, true);

    stateLists.result = CM_TRANS_CHANNEL_STATE_ACTIVATED;
    TEST_CM_TransChannelDo(&stateLists);

    DTAP_ChannelSetStatus(0, 0, 0);

    stateLists.result = CM_TRANS_CHANNEL_STATE_RELEASED;
    TEST_CM_TransChannelDo(&stateLists);
}

TEST_F(UT_DTAP_TEST, DTAP_TransChannelStatusChange)
{
    DTAP_Data_Send_Cbks_S cbk = {0};
    cbk.transChannelStatusChangeCbk = Test_DTAP_TransChannelStatusChangeCbk;
    DTAP_RegisterDataSendCbks(&cbk);
    DTAP_Channel_S *channel = (DTAP_Channel_S *)malloc(sizeof(DTAP_Channel_S));
    DTAP_ReliableChannel_S *reliable = (DTAP_ReliableChannel_S *)malloc(sizeof(DTAP_ReliableChannel_S));
    memset_s(channel, sizeof(DTAP_Channel_S), 0x00, sizeof(DTAP_Channel_S));
    SDF_DListHeadInit(&channel->pktList);
    SDF_DListEntryInit(&channel->schedEntry);
    SDF_DListEntryInit(&channel->entry);
    memset_s(reliable, sizeof(DTAP_ReliableChannel_S), 0x00, sizeof(DTAP_ReliableChannel_S));
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(1);
    EXPECT_NE(buff, nullptr);
    channel->mode = CM_TRANS_MODE_RELIABLE;
    reliable->cacheBuff = buff;
    reliable->isTxWindowFull = true;
    reliable->txWindow.size = (uint8_t)10;
    channel->mps = DTAP_SIMPLEX_FRAG_FRAME_HEADER_LEN + DTAP_CRC_LEN;
    channel->attr = reliable;
    DTAP_TransChannelStatusChange(channel, 0x00);
    reliable->isTxWindowFull = false;
    DTAP_TransChannelStatusChange(channel, 0x00);
    reliable->cacheBuff = NULL;
    reliable->isTxWindowFull = true;
    DTAP_TransChannelStatusChange(channel, 0x00);
    EXPECT_EQ(reliable->isTxWindowFull, false);
    DTAP_UnRegisterDataSendCbks();
    free(channel);
    free(reliable);
}

TEST_F(UT_DTAP_TEST, DTAP_RecvBasicFrameContinue)
{
    DTAP_Channel_S transChan;
    (void)memset_s(&transChan, sizeof(transChan), 0, sizeof(transChan));
    SDF_DListHeadInit(&transChan.pktList);
    SDF_DListEntryInit(&transChan.schedEntry);
    SDF_DListEntryInit(&transChan.entry);
    DTAP_Basic_Channel_S basic;
    basic.isReady = false;
    transChan.attr = &basic;
    SDF_DListHeadInit(&basic.cacheRxBuffs);
    StreamRxNode *node = (StreamRxNode *)SDF_MemZalloc(sizeof(StreamRxNode));
    EXPECT_NE(node, NULL);
    FreeStreamList((SDF_DListEntry_S *)node);
    DTAP_RecvBasicFrameContinue(&transChan);
    EXPECT_EQ(basic.isReady, true);
}

TEST_F(UT_DTAP_TEST, DTAP_RspTimerExpireProcTest)
{
    DTAP_Channel_S chan;
    DTAP_Basic_Channel_S basic;
    (void)memset_s(&chan, sizeof(chan), 0, sizeof(chan));
    (void)memset_s(&basic, sizeof(basic), 0, sizeof(basic));
    SDF_DListHeadInit(&basic.cacheRxBuffs);
    chan.attr = &basic;
    DTAP_DestroyBasicCacheFrame(nullptr);
    DTAP_RspTimerExpireProc(NULL);
    EXPECT_EQ(g_dtapFrames.size(), 0);
    chan.priority = DTAP_PRIORITY_MAX;
    DTAP_RspTimerExpireProc(&chan);
}

TEST_F(UT_DTAP_TEST, DTAP_SendEnhanceFrameInvalidMps)
{
    DTAP_Channel_S channel;
    (void)memset_s(&channel, sizeof(channel), 0, sizeof(channel));
    DTAP_ReliableChannel_S reliable;
    (void)memset_s(&reliable, sizeof(reliable), 0, sizeof(reliable));
    channel.attr = &reliable;
    uint32_t ret = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->sendFrame(&channel, 0, NULL);
    EXPECT_EQ(ret, DTAP_TRANS_RELIABLE_INVALID_MPS);
}

TEST_F(UT_DTAP_TEST, DTAP_SendEnhanceFrameWindowFull)
{
    DTAP_Channel_S channel;
    (void)memset_s(&channel, sizeof(channel), 0, sizeof(channel));
    channel.mps = 100;
    DTAP_ReliableChannel_S reliable;
    (void)memset_s(&reliable, sizeof(reliable), 0, sizeof(reliable));
    SDF_DListHeadInit(&reliable.txWindow.txList);
    channel.attr = &reliable;
    SDF_Buff_S *buff = SDF_BuffNew(100);
    EXPECT_NE(buff, nullptr);
    uint8_t *data = SDF_BuffAppend(buff, 100);
    EXPECT_NE(data, nullptr);
    uint32_t ret = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->sendFrame(&channel, 0, buff);
    EXPECT_EQ(ret, DTAP_TRANS_RELIABLE_TX_WINDOW_FULL);
    SDF_BuffFree(buff);
    EXPECT_NE(reliable.cacheBuff, nullptr);
    SDF_BuffFree(reliable.cacheBuff);
}

TEST_F(UT_DTAP_TEST, DTAP_CheckFrameInvalidFrameType)
{
    DTAP_Channel_S channel;
    (void)memset_s(&channel, sizeof(channel), 0, sizeof(channel));
    DTAP_ReliableChannel_S reliable;
    (void)memset_s(&reliable, sizeof(reliable), 0, sizeof(reliable));
    reliable.crcInit = 0x5555;
    SDF_DListHeadInit(&reliable.rxWindow.rxList);
    channel.attr = &reliable;

    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_BASIC);
    EXPECT_NE(frame, nullptr);
    bool pass = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->checkFrame(&channel, frame);
    EXPECT_EQ(pass, false);
    DTAP_DestroyFrame(frame);
}

TEST_F(UT_DTAP_TEST, DTAP_CheckFrameInvalidCrc)
{
    DTAP_Channel_S channel;
    (void)memset_s(&channel, sizeof(channel), 0, sizeof(channel));
    DTAP_ReliableChannel_S reliable;
    (void)memset_s(&reliable, sizeof(reliable), 0, sizeof(reliable));
    SDF_DListHeadInit(&reliable.rxWindow.rxList);
    channel.attr = &reliable;

    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_SIMPLEX_FRAG);
    EXPECT_NE(frame, nullptr);
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(sizeof(uint8_t));
    EXPECT_NE(buff, nullptr);
    uint8_t *data = SDF_BuffAppend(buff, sizeof(uint8_t));
    EXPECT_NE(data, nullptr);
    frame->buff = buff;
    uint32_t ret = frame->ctx.buildFrame(&frame->ctx, 0x1f, 0x5555);
    EXPECT_EQ(ret, DTAP_SUCCESS);

    reliable.crcInit = 0x5551;
    bool pass = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->checkFrame(&channel, frame);
    EXPECT_EQ(pass, false);
    DTAP_DestroyFrame(frame);
}

TEST_F(UT_DTAP_TEST, DTAP_CheckFrameAckSuccess)
{
    DTAP_Channel_S channel;
    (void)memset_s(&channel, sizeof(channel), 0, sizeof(channel));
    DTAP_ReliableChannel_S reliable;
    (void)memset_s(&reliable, sizeof(reliable), 0, sizeof(reliable));
    SDF_DListHeadInit(&reliable.rxWindow.rxList);
    channel.attr = &reliable;

    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_ACK);
    EXPECT_NE(frame, nullptr);
    uint32_t ret = frame->ctx.buildFrame(&frame->ctx, 0x1f, 0x5555);
    EXPECT_EQ(ret, DTAP_SUCCESS);

    reliable.crcInit = 0x5555;
    bool pass = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->checkFrame(&channel, frame);
    EXPECT_EQ(pass, true);
    DTAP_DestroyFrame(frame);
}

TEST_F(UT_DTAP_TEST, DTAP_CheckFrameInvalidTxSeq)
{
    DTAP_Channel_S channel;
    (void)memset_s(&channel, sizeof(channel), 0, sizeof(channel));
    DTAP_ReliableChannel_S reliable;
    (void)memset_s(&reliable, sizeof(reliable), 0, sizeof(reliable));
    reliable.crcInit = 0x5555;
    SDF_DListHeadInit(&reliable.rxWindow.rxList);
    channel.attr = &reliable;
    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_SIMPLEX_FRAG);
    EXPECT_NE(frame, nullptr);
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(0);
    EXPECT_NE(buff, nullptr);
    frame->buff = buff;
    uint32_t ret = frame->ctx.buildFrame(&frame->ctx, 0x1f, 0x5555);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    // txSeq < expectedTxSeq
    reliable.rxWindow.expectedTxSeq = 0x1;
    bool pass = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->checkFrame(&channel, frame);
    EXPECT_EQ(pass, false);
    // txSeq > bufferSeq + window
    reliable.rxWindow.expectedTxSeq = DTAP_FRAME_SEQ_MAX;
    reliable.rxWindow.bufferSeq = DTAP_FRAME_SEQ_MAX;
    reliable.rxWindow.size = 255;
    frame->enhance.txSeq = 255;
    pass = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->checkFrame(&channel, frame);
    EXPECT_EQ(pass, false);

    DTAP_DestroyFrame(frame);
}

TEST_F(UT_DTAP_TEST, DTAP_CheckFrameSuccess)
{
    DTAP_Channel_S channel;
    (void)memset_s(&channel, sizeof(channel), 0, sizeof(channel));
    DTAP_ReliableChannel_S reliable;
    (void)memset_s(&reliable, sizeof(reliable), 0, sizeof(reliable));
    reliable.crcInit = 0x5555;
    SDF_DListHeadInit(&reliable.rxWindow.rxList);
    channel.attr = &reliable;

    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_SIMPLEX_FRAG);
    EXPECT_NE(frame, nullptr);
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(0);
    EXPECT_NE(buff, nullptr);
    frame->buff = buff;
    uint32_t ret = frame->ctx.buildFrame(&frame->ctx, 0x1f, 0x5555);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    // txSeq < expectedTxSeq
    bool pass = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->checkFrame(&channel, frame);
    EXPECT_EQ(pass, true);
    // txSeq > bufferSeq + window
    reliable.rxWindow.size = 255;
    frame->enhance.txSeq = 254;
    pass = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->checkFrame(&channel, frame);
    EXPECT_EQ(pass, true);

    DTAP_DestroyFrame(frame);
}

TEST_F(UT_DTAP_TEST, DTAP_RecvNackFrame)
{
    DTAP_Channel_S channel;
    (void)memset_s(&channel, sizeof(channel), 0, sizeof(channel));
    DTAP_ReliableChannel_S reliable;
    (void)memset_s(&reliable, sizeof(reliable), 0, sizeof(reliable));
    SDF_DListHeadInit(&reliable.txWindow.txList);
    SDF_DListHeadInit(&reliable.rxWindow.rxList);
    channel.attr = &reliable;
    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_ACK);
    EXPECT_NE(frame, nullptr);
    frame->ack.sBit = true;
    uint32_t ret = frame->ctx.buildFrame(&frame->ctx, 0x1f, 0x5555);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    ret = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->recvFrame(&channel, frame, NULL);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    DTAP_DestroyFrame(frame);
}

TEST_F(UT_DTAP_TEST, DTAP_RecvDuplexFragFrame)
{
    DTAP_Channel_S channel;
    (void)memset_s(&channel, sizeof(channel), 0, sizeof(channel));
    DTAP_ReliableChannel_S reliable;
    (void)memset_s(&reliable, sizeof(reliable), 0, sizeof(reliable));
    SDF_DListHeadInit(&reliable.txWindow.txList);
    SDF_DListHeadInit(&reliable.rxWindow.rxList);
    reliable.reorderTimer = INVALID_TIMER_HANDLE;
    reliable.maxTxThreshold = 1;
    channel.attr = &reliable;
    // 构造已发送数据包
    DTAP_Frame_S *frame = DTAP_CreateFrame(DTAP_FRAME_DUPLEX_FRAG);
    EXPECT_NE(frame, nullptr);
    frame->bits = DTAP_FRAME_C_BIT;
    frame->enhance.sar = DTAP_SAR_UNSEG;
    SDF_DListElmTailInsert(&reliable.txWindow.txList, frame, entry);
    reliable.txWindow.nextTxSeq = 1;
    // 构造收到乱序包
    frame = DTAP_CreateFrame(DTAP_FRAME_DUPLEX_FRAG);
    EXPECT_NE(frame, nullptr);
    frame->bits = DTAP_FRAME_P_BIT | DTAP_FRAME_C_BIT;
    frame->enhance.sar = DTAP_SAR_UNSEG;
    frame->enhance.txSeq = 254;
    frame->enhance.reqSeq = 1;
    reliable.rxWindow.size = 255;
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(sizeof(uint8_t));
    EXPECT_NE(buff, nullptr);
    uint8_t *data = SDF_BuffAppend(buff, sizeof(uint8_t));
    EXPECT_NE(data, nullptr);
    frame->buff = buff;
    uint32_t ret = frame->ctx.buildFrame(&frame->ctx, 0x1f, 0x5555);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    channel.priority = DTAP_PRIORITY_MAX;
    ret = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->recvFrame(&channel, frame, NULL);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    DTAP_DestroyFrame(frame);
    // 构造收到首包
    frame = DTAP_CreateFrame(DTAP_FRAME_DUPLEX_FRAG);
    EXPECT_NE(frame, nullptr);
    frame->bits = DTAP_FRAME_P_BIT | DTAP_FRAME_C_BIT;
    frame->enhance.sar = DTAP_SAR_UNSEG;
    frame->enhance.txSeq = 0;
    frame->enhance.reqSeq = 1;
    reliable.rxWindow.size = 255;
    buff = SDF_BuffNewWithReserve(sizeof(uint8_t));
    EXPECT_NE(buff, nullptr);
    data = SDF_BuffAppend(buff, sizeof(uint8_t));
    EXPECT_NE(data, nullptr);
    frame->buff = buff;
    ret = frame->ctx.buildFrame(&frame->ctx, 0x1f, 0x5555);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    ret = DTAP_GetTransMode(CM_TRANS_MODE_RELIABLE)->recvFrame(&channel, frame, NULL);
    EXPECT_EQ(ret, DTAP_SUCCESS);
    DTAP_DestroyFrame(frame);
    DTAP_Frame_S *node = NULL;
    DTAP_Frame_S *tmpNode = NULL;
    SDF_DListElmAllFree(node, tmpNode, &reliable.rxWindow.rxList, entry, DTAP_DestroyFrame);
}

TEST_F(UT_DTAP_TEST, DTAP_RecvDataNull)
{
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(TEST_NUM);
    EXPECT_NE(buff, nullptr);

    DLI_AcbRecvHander(0, NULL);
    DLI_AcbRecvHander(0, buff);
    SDF_BuffFree(buff);
}

TEST_F(UT_DTAP_TEST, DTAP_RecvDataSearchChlFail)
{
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(TEST_NUM * TEST_NUM);
    EXPECT_NE(buff, nullptr);
    uint8_t *data = SDF_BuffAppend(buff, TEST_NUM);
    EXPECT_NE(data, nullptr);

    DLI_AcbRecvHander(0, buff);
    SDF_BuffFree(buff);
}

TEST_F(UT_DTAP_TEST, DTAP_RecvDataBasic)
{
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(sizeof(DTAP_BasicHeader_S));
    EXPECT_NE(buff, nullptr);
    uint8_t *data = SDF_BuffAppend(buff, 1);
    EXPECT_NE(data, nullptr);

    CM_TransChannelStateList_S stateLists;
    SDF_Traits transChannelTraits = {.dtor = SDF_MemFree};
    stateLists.channelVector = SDF_CreateVector(transChannelTraits);
    EXPECT_NE(stateLists.channelVector, nullptr);

    CM_TransChan_S *chan = CM_TransChanCreate(0, 0, 0, 0, CM_TRANS_MODE_BASIC);
    EXPECT_NE(chan, nullptr);
    chan->config.mps = 0;
    bool isSuccess = SDF_VectorEmplaceBack(stateLists.channelVector, chan);
    EXPECT_EQ(isSuccess, true);

    stateLists.result = CM_TRANS_CHANNEL_STATE_ACTIVATED;
    TEST_CM_TransChannelDo(&stateLists);

    // Invalid header len
    DLI_AcbRecvHander(0, buff);
    // Invalid frame len
    DTAP_BasicHeader_S *header = (DTAP_BasicHeader_S *)SDF_BuffPrepend(buff, sizeof(DTAP_BasicHeader_S));
    EXPECT_NE(header, nullptr);
    DLI_AcbRecvHander(0, buff);

    // Invalid frame type
    ENCODE2BYTE_LITTLE(&header->length, 1);
    header->frameType = DTAP_FRAME_MAX;
    DLI_AcbRecvHander(0, buff);

    // Invalid mps
    header->frameType = DTAP_FRAME_BASIC;
    header->crcBit = 1;
    DLI_AcbRecvHander(0, buff);

    // Invalid crc bit
    DTAP_Channel_S *channle = DTAP_ChannelSearch(0, 0);
    EXPECT_NE(channle, nullptr);
    channle->mps = 100;
    DLI_AcbRecvHander(0, buff);

    header->crcBit = 0;
    DLI_AcbRecvHander(0, buff);

    stateLists.result = CM_TRANS_CHANNEL_STATE_RELEASED;
    TEST_CM_TransChannelDo(&stateLists);

    SDF_BuffFree(buff);
}

TEST_F(UT_DTAP_TEST, DTAP_DataRecv_Transparent)
{
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(sizeof(DTAP_BasicHeader_S));
    EXPECT_NE(buff, nullptr);
    uint8_t *data = SDF_BuffAppend(buff, 1);
    EXPECT_NE(data, nullptr);

    CM_TransChannelStateList_S stateLists;
    SDF_Traits transChannelTraits = {.dtor = SDF_MemFree};
    stateLists.channelVector = SDF_CreateVector(transChannelTraits);
    EXPECT_NE(stateLists.channelVector, nullptr);

    CM_TransChan_S *chan = CM_TransChanCreate(0, 0, 0, 0, CM_TRANS_MODE_TRANSPARENT);
    chan->config.mps = 0;
    EXPECT_NE(chan, nullptr);
    bool isSuccess = SDF_VectorEmplaceBack(stateLists.channelVector, chan);
    EXPECT_EQ(isSuccess, true);

    stateLists.result = CM_TRANS_CHANNEL_STATE_ACTIVATED;
    TEST_CM_TransChannelDo(&stateLists);

    DLI_AcbRecvHander(0, buff);

    DTAP_Channel_S *channle = DTAP_ChannelSearch(0, 0);
    EXPECT_NE(channle, nullptr);
    channle->mps = 100;
    DLI_AcbRecvHander(0, buff);

    stateLists.result = CM_TRANS_CHANNEL_STATE_RELEASED;
    TEST_CM_TransChannelDo(&stateLists);

    SDF_BuffFree(buff);
}