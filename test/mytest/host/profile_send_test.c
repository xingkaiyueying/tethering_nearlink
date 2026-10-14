#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include "iposl_profile.h"
#include "iposl_codec.c"
static atomic_uint_fast64_t g_sendGeneration=42;
static atomic_bool g_initialized=true;
static atomic_uint_fast64_t allowedGeneration=42;
static atomic_bool blockSend=false,sendEntered=false,releaseSend=false;
static bool SecureSend(uint16_t l,uint8_t t,uint8_t pi,uint64_t g)
{
 if(blockSend){sendEntered=true;while(!releaseSend){} }
 return (l==1 || l==3) && t==2 && (pi==1 || pi==2) && g==allowedGeneration;
}
static atomic_int live;
static int completed, rejected, submitted, inCp, failSend, posts, failPost;
static void Complete(uint16_t l,uint8_t t,uint8_t p,uint64_t g,int32_t result)
{ (void)l;(void)t;(void)p;(void)g;assert(inCp);++completed;if(result)++rejected;else ++submitted; }
static const IposlProfileCallbacks *IposlGetCallbacks(void)
{ static IposlProfileCallbacks cb={.canSend=SecureSend,.onSendComplete=Complete};return g_initialized?&cb:NULL; }
#define DTAP_PI_IPV4 1
#define NLSTK_LOG_ERROR(...) ((void)0)
typedef struct {unsigned char data[1500];} SDF_Buff_S;
typedef struct {int pi;uint16_t lcid;uint8_t tcid;SDF_Buff_S *buff;} DTAP_Data_S;
static void *SDF_MemAlloc(size_t n){++live;return malloc(n);}
static void SDF_MemFree(void *p){--live;free(p);}
static SDF_Buff_S *SDF_BuffNewWithReserve(int n){(void)n;return SDF_MemAlloc(sizeof(SDF_Buff_S));}
static uint8_t *SDF_BuffAppend(SDF_Buff_S *b,int n){(void)n;return b->data;}
static void SDF_BuffFree(SDF_Buff_S *b){SDF_MemFree(b);}
static unsigned sentMarkers[4096], sentLcids[4096], sends;
static int DTAP_DataSend(DTAP_Data_S *p){
 assert(inCp && p->buff->data[0]==0x60 && p->pi==2);
 sentMarkers[sends]=p->buff->data[100];sentLcids[sends]=p->lcid;++sends;
 if(failSend)return 7;
 SDF_BuffFree(p->buff);return 0;
}
typedef struct {void(*fn)(void*);void *arg;} Work;
static Work work[4096];static unsigned readWork,writeWork;
static uint32_t CP_PostTask(void(*fn)(void*),void *arg,void(*release)(void*)){
 assert(release==NULL);++posts;if(failPost==posts)return 1;
 assert(writeWork<4096);work[writeWork++]=(Work){fn,arg};return 0;
}
#include "profile_send.inc"
static void RunOne(void){assert(readWork<writeWork);Work w=work[readWork++];inCp=1;w.fn(w.arg);inCp=0;}
static void Drain(void){while(readWork<writeWork)RunOne();}
static void *DrainWorker(void *arg){(void)arg;Drain();return NULL;}
static void *CapacityWorker(void *arg){(void)arg;assert(!IposlProfileWaitSendCapacity(1,2,42));return NULL;}
static void Bytes(uint8_t *p,unsigned marker){memset(p,0,1500);p[0]=0x60;p[4]=5;p[5]=180;p[6]=17;p[100]=marker;}
int main(void){
 uint8_t bytes[1500];Bytes(bytes,11);
 assert(IposlProfileSendIp(1,2,2,bytes,1500,42)==0 && sends==0);
 bytes[100]=99;assert(!IposlProfileSendIsDrained(1,2,42));Drain();
 assert(sentMarkers[0]==11 && submitted==1 && live==0);
 assert(IposlProfileSendIsDrained(1,2,42));
 unsigned before=sends;
 for(unsigned i=0;i<64;++i){Bytes(bytes,i);assert(IposlProfileSendIp(1,2,2,bytes,1500,42)==0);}
 assert(IposlProfileSendIp(1,2,2,bytes,1500,42)!=0 && live==128);
 assert(!IposlProfileWaitSendCapacity(1,2,42));
 Bytes(bytes,201);assert(IposlProfileSendIp(3,2,2,bytes,1500,42)==0);
 RunOne();assert(sends>before && sends<=before+8);
 RunOne();assert(sentLcids[sends-1]==3);
 while(g_sendQueues[0].count>16)RunOne();
 assert(IposlProfileWaitSendCapacity(1,2,42));Drain();assert(live==0);
 unsigned sequence=0;
 for(unsigned i=before;i<sends;++i)if(sentLcids[i]==1)assert(sentMarkers[i]==sequence++);
 assert(sequence==64);
 Bytes(bytes,4);assert(IposlProfileSendIp(1,2,2,bytes,1500,42)==0);
 IposlProfileCancelSend(1,2,42);assert(!IposlProfileSendIsDrained(1,2,42));
 before=sends;Drain();assert(sends==before && live==0 && rejected==1);
 assert(IposlProfileSendIsDrained(1,2,42));
 assert(IposlProfileSendIp(1,2,2,bytes,1500,42)==0);allowedGeneration=43;
 Drain();assert(sends==before && live==0 && rejected==2);allowedGeneration=42;
 failSend=1;assert(IposlProfileSendIp(1,2,2,bytes,1500,42)==0);Drain();
 assert(rejected==3 && live==0);failSend=0;
 failPost=posts+1;assert(IposlProfileSendIp(1,2,2,bytes,1500,42)!=0);assert(live==0);failPost=0;
 for(unsigned i=0;i<12;++i)assert(IposlProfileSendIp(1,2,2,bytes,1500,42)==0);
 failPost=posts+1;RunOne();assert(IposlAllSendsDrained() && live==0 && rejected==7);failPost=0;
 assert(IposlProfileSendIp(1,2,1,bytes,1500,42)!=0);
 assert(IposlProfileSendIp(1,2,2,bytes,1501,42)!=0 && live==0);
 assert(IposlProfileSendIp(1,2,2,bytes,1500,0)!=0);
 assert(IposlProfileSendIp(1,2,2,bytes,1500,42)==0);IposlCancelAllSends();
 assert(!IposlAllSendsDrained());Drain();assert(IposlAllSendsDrained() && live==0);
 // Cancel while an admitted CP callback is outside the queue lock. Drain must stay false.
 for(unsigned i=0;i<48;++i)assert(IposlProfileSendIp(1,2,2,bytes,1500,42)==0);
 blockSend=true;sendEntered=false;releaseSend=false;pthread_t worker,waiter;
 pthread_create(&worker,NULL,DrainWorker,NULL);
 while(!sendEntered){}
 pthread_create(&waiter,NULL,CapacityWorker,NULL);
 IposlProfileCancelSend(1,2,42);
 assert(!IposlProfileSendIsDrained(1,2,42));
 assert(IposlProfileSendIp(1,2,2,bytes,1500,42)!=0);
 pthread_join(waiter,NULL);before=sends;releaseSend=true;
 pthread_join(worker,NULL);blockSend=false;
 assert(sends==before && live==0 && IposlProfileSendIsDrained(1,2,42));
 g_initialized=false;assert(IposlProfileSendIp(1,2,2,bytes,1500,42)!=0);
 puts("IPoSL async queue: ownership, limits, hysteresis, peer fairness, generation, failure, concurrent cancel/drain PASS");
}
