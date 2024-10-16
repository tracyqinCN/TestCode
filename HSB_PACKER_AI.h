/**************************************************************************
HAB_PACKET_AI.h

小电流接地选线人工智能AI算法_HSB报文接收和发送头文件

Copyright (c) 2024 SNAC(Guodian Nanjing Automation Co., Ltd.)
All Rights Reserved.

History:
Revision     Date          Who            Comment
-----------------------------------------------------------------
V1.00         2024.10.15    changsong-qin         初始版本
***************************************************************************/

#ifndef HSB_PACKER_AI_H
#define HSB_PACKER_AI_H

#include <stdint.h>
#include <semaphore.h>

#define HSB_BUFFER_SIZE 2048           // 缓冲区大小
#define HSB_TARGET_SIZE 840           // 目标报文大小
#define HSB_FLOAT_ARRAY_SIZE 100      // 每个浮点数组大小
#define HSB_PACKET_INTERFACE "hsb0"          // 网络接口名称
#define HSB_FPGA_DST_MAC "\x00\x00\x00\x00\x00\x01" // 目标MAC地址
#define HSB_FPGA_SRC_MAC "\x00\x00\x00\x00\x00\x02" // 源MAC地址
#define HSB_PACKET_APPFLAG_BYTE_16 0x44
#define HSB_PACKET_APPFIX_SEQUENCE_BYTES "\x00\x00\x00\x01\x02\x03"
#define HSB_PACKET_APPFIX_FOOTER "\x9A\xAB\xCD\xEF"

// 共享内存结构体定义
typedef struct {
    uint32_t ulPackFromCpu;               // HSB收到了CPU发来的包
    uint32_t ulAIWriteFlag;               // AI收到了共享内存写入的数据
    uint32_t ulAIComResult;               // AI的分析结果
    float fVoltageData[HSB_FLOAT_ARRAY_SIZE];  // HSB收到的电压数据
    float fCurrentData[HSB_FLOAT_ARRAY_SIZE];  // HSB收到的电流数据
} TFP_AISharedMemory;
#define XJTUAI_SHM_NAME "/tracy_shared_AI_mem"   // 共享内存名称
#define XJTUAI_SHM_SIZE sizeof(TFP_AISharedMemory)

//接收HSB网络报文结构体定义
typedef struct hsb_recv_str 
{
    uint8_t uDstMac[6]; // 目标MAC地址
    uint8_t uSrcMac[6]; // 发送方MAC地址
    union
    {
        uint32_t ulhsb_define;
        struct
        {
            uint32_t ulLen : 11;
            uint32_t ulnodeAddr : 16;
            uint32_t  ulpriority : 2;
            uint32_t  ulreserved : 3;
        }s;
    } hsbext;
    uint8_t uHsbData[8];
    uint32_t ulCPUFIXDATA;
    uint32_t ulRecvStNum;
    uint32_t ulRecvSqNum;
    uint32_t ulRecvDataCnt;
    float fRecvVoltageData[HSB_FLOAT_ARRAY_SIZE];
    float fRecvCurrentData[HSB_FLOAT_ARRAY_SIZE];
} HSB_RECV_PACKET_STR;

// 定义大端转小端函数
#define unswap_32(x) \
    ((((x) & 0xff000000) >> 24) | \
     (((x) & 0x00ff0000) >>  8) | \
     (((x) & 0x0000ff00) <<  8) | \
     (((x) & 0x000000ff) << 24))

/*交换16bit的两个字节顺序，用于大小端转换*/
#define bswap_16(x) \
    ((((x) & 0xff00) >> 8) | \
     (((x) & 0x00ff) << 8) )
/* Get uint32_t combined by 4 uint8_t. */
#define U8_TO_U32(ucHH8, ucHL8, ucLH8, ucLL8)\
	(uint32_t)((((uint8_t)(ucHH8))<<24)|(((uint8_t)(ucHL8))<<16)|\
	((uint8_t)(ucLH8)<<8)|(uint8_t)(ucLL8))     
// 定义大端转小端函数
#define cpu_to_be32(x)   unswap_32(x)
#define le32_to_cpu(x)   (x)
#define be32_to_cpu(x)   unswap_32(x)
#define cpu_to_le32(x)   (x)
#define tole(x)  cpu_to_le32(x)
/* 用于浮点数到字节流转换的联合 */
typedef union
{
    float fVal;
    uint32_t ulVal;
    int32_t lVal;
} FLT_U32_UNION;

/* Get float combined by 4 uint8_t.
 * Para:
 *     pucOut, 用于存放4字节结果.
 *     ulIn, 待转换的32位数.
 * Return:
 *     result.
 */
static __inline__ float U8_TO_FLT(uint8_t ucHH8, uint8_t ucHL8,
                                  uint8_t ucLH8, uint8_t ucLL8)
{
    FLT_U32_UNION fu32;

    fu32.ulVal = U8_TO_U32(ucHH8, ucHL8, ucLH8, ucLL8);

    return fu32.fVal;
}
/* Intel次序(Little Endian)的字节流转化为32位整数
 * 参数：pucIn，待转换的Intel次序(Little Endian)4字节输入
 * 返回值：32位整数转换结果
 * 注意：输入的4字节必须按照低字节低地址(Little Endian)的Intel次序
 *       (和EDP 01的内部通信规约相匹配)
 */
#define BYTES_TO_U32(pucIn) U8_TO_U32(((uint8_t*)(pucIn))[3],\
	((uint8_t*)(pucIn))[2], ((uint8_t*)(pucIn))[1], ((uint8_t*)(pucIn))[0])

/* Intel次序(Little Endian)的字节流转化为浮点数
 * 参数：pucIn，待转换的Intel次序(Little Endian)4字节输入
 * 返回值：浮点数转换结果
 * 注意：输入的4字节必须按照低字节低地址(Little Endian)的Intel次序
 *       (和EDP 01的内部通信规约相匹配)
 */
#define BYTES_TO_FLT(pucIn) U8_TO_FLT(((uint8_t*)(pucIn))[3],\
	((uint8_t*)(pucIn))[2], ((uint8_t*)(pucIn))[1], ((uint8_t*)(pucIn))[0])



// 函数原型声明
void init_shared_memory(void);                   // 初始化共享内存
void* receive_HSB_PACKET_task(void* arg);                   // 接收任务函数
void* send_HSB_PACKET_AI_task(void* arg);                      // 发送任务函数
void* monitor_HSB_AI_task(void* arg);                   // 监视任务函数
void decode_HSB_PACKET_packet(const char* cBuffer, size_t iLen); // 解码网络报文函数

// 外部变量声明
extern sem_t semAISync;       // 信号量，用于任务同步
extern TFP_AISharedMemory* pAIShm;  // 指向共享内存的指针

#endif // HSB_PACKER_AI_H
