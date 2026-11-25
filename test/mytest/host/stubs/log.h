#pragma once
#ifdef __cplusplus
template<class... Args> inline void HostCheckLog(const char*, Args&&...) {}
#define HILOGI(...) HostCheckLog(__VA_ARGS__)
#define HILOGE(...) HostCheckLog(__VA_ARGS__)
#define HILOGW(...) HostCheckLog(__VA_ARGS__)
#define HILOGD(...) HostCheckLog(__VA_ARGS__)
#else
#define HILOGI(...) ((void)0)
#define HILOGE(...) ((void)0)
#define HILOGW(...) ((void)0)
#define HILOGD(...) ((void)0)
#endif
