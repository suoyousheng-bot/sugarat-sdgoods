/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "cv_copy.h"
static const char *const copy[C_COUNT][2] = {
    [C_TITLE]={"Sunset voyage","落日航线"}, [C_SWIPE]={"Swipe to change cover","左右滑动，切换封面"},
    [C_PHONE]={"Phone upload","手机上传"}, [C_EXCHANGE]={"Share a photo","交换图片"},
    [C_SEND]={"Send this photo","赠送这张"}, [C_RECEIVE]={"Receive photo","接收图片"},
    [C_CONNECT]={"Connect phone","连接手机"}, [C_WIFI_STEP]={"1  Open phone Wi-Fi settings","1  打开手机 Wi-Fi 设置"},
    [C_PAGE_STEP]={"2  Scan with phone camera","2  用手机相机扫码打开"}, [C_UPLOAD]={"Upload cover","上传封面"},
    [C_CHOOSE]={"Choose badge","选择设备"}, [C_SEARCHING]={"Looking for badges…","正在寻找附近徽章…"},
    [C_WAIT]={"Ready to receive","等待赠图"}, [C_MY_BADGE]={"My badge","我的徽章"},
    [C_ASK_PEER]={"Ask them to choose you","让对方选择这台设备"}, [C_INVITE]={"Photo invitation","收到赠图邀请"},
    [C_FROM]={"From badge","来自徽章"}, [C_LATER]={"Later","稍后"}, [C_ACCEPT]={"Accept","接收"},
    [C_PAIR]={"Check the code","核对配对码"}, [C_COMPARE]={"Same code on both badges?","两台设备的配对码相同吗"},
    [C_MATCH]={"Codes match","配对码相同"}, [C_SENDING]={"Sending photo","正在发送"},
    [C_RECEIVING]={"Receiving photo","正在接收"}, [C_KEEP_NEAR]={"Keep both badges nearby","请保持两台设备靠近"},
    [C_SAVED]={"Saved to gallery","已存入相册"}, [C_SHOW]={"Show","展示"},
    [C_SENT]={"Photo delivered","对方已保存"}, [C_DONE]={"Done","完成"}, [C_BACK]={"Back","返回"},
    [C_LANGUAGE]={"English / 中文","English / 中文"}, [C_RETRY]={"Try again","重试"},
    [C_NO_STORAGE]={"Storage unavailable","存储不可用"}, [C_CLOSING]={"Closing connection…","正在关闭连接…"},
    [C_PHOTO]={"Photo","图片"}, [C_NO_PEERS]={"Ask them to tap Receive","请对方进入“接收图片”"},
    [C_PREPARING]={"Preparing…","正在准备…"}, [C_CANCEL]={"Cancel","取消"},
    [C_BADGE]={"Badge","徽章"},
    [C_NO_PASSWORD]={"No password needed","无需密码，直接连接"},
    [C_PASSWORD]={"Password","密码"},
    [C_KEEP_WIFI]={"Stay connected: no internet","提示无互联网时保持连接"},
    [C_ENABLE_PASSWORD]={"Use password","开启密码"},
    [C_DISABLE_PASSWORD]={"Remove password","取消密码"},
    [C_BACK_SWIPE]={"Swipe right to go back","从左向右滑动返回"},
};
static const char *const errors[CV_ERROR_COUNT][2] = {
    [CV_OK]={"",""}, [CV_INVALID]={"Invalid request. Try again.","请求无效，请重试"},
    [CV_JPEG]={"Choose a photo and crop it again.","图片无法读取，请重新裁剪上传"},
    [CV_FULL]={"Gallery is full. Delete a photo first.","相册已满，请先删除一张图片"},
    [CV_IO]={"Could not save. Keep the original and retry.","保存失败，原图片已保留，请重试"},
    [CV_SETTINGS]={"Settings unconfirmed. Restart and check.","未确认设置是否保存，请重启后检查"},
    [CV_BUSY]={"Another operation is running. Wait and retry.","设备正在处理其他操作，请稍后重试"},
    [CV_TIMEOUT]={"Connection timed out. Move closer and retry.","连接超时，请靠近后重试"},
    [CV_CANCELLED]={"Cancelled. Your cover is unchanged.","已取消，当前封面保持不变"},
    [CV_AUTH]={"Pairing failed. Check both badges and retry.","配对失败，请核对两台设备后重试"},
    [CV_RADIO]={"Wireless unavailable. Restart the badge.","无线连接不可用，请重启设备"},
    [CV_MEMORY]={"Not enough memory. Close sharing and retry.","内存不足，请退出传图后重试"},
    [CV_CORRUPT]={"Photo is damaged. Upload it again.","图片已损坏，请重新上传"},
    [CV_UNKNOWN]={"Delivery unconfirmed. Check the other gallery.","未确认对方是否保存，请检查对方相册"},
};
const char *cv_text(cv_locale_t locale, cv_copy_key_t key)
{ return copy[key >= 0 && key < C_COUNT ? key : C_TITLE][locale == CV_ZH]; }
const char *cv_error_text(cv_locale_t locale, cv_error_t error)
{ return errors[error >= 0 && error < CV_ERROR_COUNT ? error : CV_INVALID][locale == CV_ZH]; }
