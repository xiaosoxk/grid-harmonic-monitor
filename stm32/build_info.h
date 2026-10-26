/**
 * build_info.h — 构建追溯信息（由 CI 脚本在编译前生成，禁止手工编辑/提交）
 *
 * 生成方式(在构建脚本/CI 里执行):
 *   printf '#define BUILD_GIT_HASH "%s"\n' "$(git rev-parse --short=8 HEAD)"  > build_info.h
 *   printf '#define BUILD_UTC      %sU\n'  "$(date +%%s)"                    >> build_info.h
 *   printf '#define FIRMWARE_VER   "%s"\n' "1.1.0"                          >> build_info.h
 * 用途:
 *   1) HELLO 帧携带 BUILD_GIT_HASH, 上位机界面显示设备固件版本;
 *   2) 现场故障与固件版本一一对应 —— 没有构建追溯, 问题复现无从谈起;
 *   3) 版本号规则: 主.次.修订 (协议不兼容+1, 加功能+1, 修bug+1)。
 */
#ifndef BUILD_INFO_H
#define BUILD_INFO_H

#define BUILD_GIT_HASH  "3f9c2a1d"      /* 示例值, 实际由 CI 注入 */
#define BUILD_UTC       1761321600U
#define FIRMWARE_VER    "1.1.0"
#define PROTO_VER_MAJOR 1U
#define PROTO_VER_MINOR 1U

#endif /* BUILD_INFO_H */
