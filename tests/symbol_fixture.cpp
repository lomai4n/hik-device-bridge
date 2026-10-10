// SPDX-License-Identifier: Apache-2.0
// Synthetic symbol inventory only. These are never called and define no vendor ABI.
extern "C" {
void NET_DVR_Init() {}
#ifdef COMPLETE_SYMBOLS
void NET_DVR_Cleanup() {}
void NET_DVR_GetSDKVersion() {}
void NET_DVR_GetLastError() {}
void NET_DVR_Login_V40() {}
void NET_DVR_Logout() {}
void NET_DVR_SetDVRMessageCallBack_V31() {}
void NET_DVR_SetExceptionCallBack_V30() {}
void NET_DVR_SetupAlarmChan_V41() {}
void NET_DVR_CloseAlarmChan_V30() {}
#endif
}
