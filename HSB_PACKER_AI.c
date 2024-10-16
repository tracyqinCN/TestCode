/**************************************************************************
HAB_PACKET_AI.c

小电流接地选线人工智能AI算法_HSB报文接收和发送

Copyright (c) 2024 SNAC(Guodian Nanjing Automation Co., Ltd.)
All Rights Reserved.

History:
Revision     Date          Who            Comment
-----------------------------------------------------------------
V1.00         2024.10.15    changsong-qin         初始版本
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stddef.h>
#include "HSB_PACKER_AI.h"

// 全局变量
sem_t semAISync;
TFP_AISharedMemory *pAIShm = NULL;

/**
 * @brief 初始化共享内存并映射到进程地址空间
 * @param [in]  无
 * @param [out] 无
 *
 * 修改日期         版本号       修改人         修改内容     \n
 * ----------------------------------------------------------\n
 * 2024/10/15    V1.0   changsong-qin    新增该函数
 * @see
 * @note
 * @warning
 */
void init_AI_shared_memory(void)
{
    printf("Tracy begin create shared memory...\n");
    int shm_fd = shm_open(XJTUAI_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if(shm_fd == -1)
    {
        perror("Tracy begin create shared memory failed\n");
        exit(EXIT_FAILURE);
    }
    // 设置共享内存大小
    ftruncate(shm_fd, XJTUAI_SHM_SIZE);

    // 将共享内存映射到进程地址空间
    pAIShm = mmap(0, XJTUAI_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(pAIShm == MAP_FAILED)
    {
        perror("Tracy mmap shared memory failed\n");
        exit(EXIT_FAILURE);
    }

    //初始化共享内存的数据
    memset(pAIShm, 0, XJTUAI_SHM_SIZE);
    printf("Tracy init shared memory success!!! \n");
}

/**
 * @brief 接收任务函数，用于从hsb网络接口接收报文并进行处理
 * @param [in]  void* arg 传递给线程的参数
 * @param [out] 无
 *
 * 修改日期         版本号       修改人         修改内容     \n
 * ----------------------------------------------------------\n
 * 2024/10/15    V1.0   changsong-qin    新增该函数
 * @see
 * @note
 * @warning
 */
void *receive_HSB_PACKET_task(void *arg)
{
    int sockfd;
    struct sockaddr_ll sa;
    unsigned char buffer[HSB_BUFFER_SIZE];
    printf("Tracy receve task begining...\n");
    sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(sockfd == -1)
    {
        perror("Tracy recv Socket creat failed\n");
        exit(EXIT_FAILURE);
    }
    printf("Tracy recv Socket creat failed\n");

    memset(&sa, 0, sizeof(struct sockaddr_ll));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ALL);
    sa.sll_ifindex = if_nametoindex(HSB_PACKET_INTERFACE);
    // if(bind(sockfd, (struct sockaddr *)&sa, sizeof(struct sockaddr_ll)) == -1)
    // {
    //     perror("Socket 绑定失败");
    //     exit(EXIT_FAILURE);
    // }

    while(1)
    {
        ssize_t len = recvfrom(sockfd, buffer, HSB_BUFFER_SIZE, 0, NULL, NULL);
        //长度不匹配直接continue
        if(len != HSB_TARGET_SIZE)
            continue;

        if(len == HSB_TARGET_SIZE)
        {
            //长度匹配则解包分析，首先检查是否正在执行AI算法
            if(pAIShm->ulPackFromCpu == 0)
            {
                decode_HSB_PACKET_packet((const char *)buffer, len);
                sem_post(&semAISync);
                printf("Tracy recevie succes from CPU!\n");
            }
            else
            {
                printf("Tracy decode packet error\n");
            }
        }
    }
    close(sockfd);
    return NULL;
}

/**
 * @brief 发送任务函数，负责等待信号量并触发后续处理
 * @param [in]  void* arg 传递给线程的参数
 * @param [out] 无
 *
 * 修改日期         版本号       修改人         修改内容     \n
 * ----------------------------------------------------------\n
 * 2024/10/15    V1.0   changsong-qin    新增该函数
 * @see
 * @note
 * @warning
 */
void *send_HSB_PACKET_AI_task(void *arg)
{
    while(1)
    {
        sem_wait(&semAISync);
        printf("Tracy release semAISync success\n");
    }
    return NULL;
}

/**
 * @brief 监视任务函数，监视共享内存中数据变化并根据条件释放信号量
 * @param [in]  void* arg 传递给线程的参数
 * @param [out] 无
 *
 * 修改日期         版本号       修改人         修改内容     \n
 * ----------------------------------------------------------\n
 * 2024/10/15    V1.0   changsong-qin    新增该函数
 * @see
 * @note
 * @warning
 */
void *monitor_HSB_AI_task(void *arg)
{
    while(1)
    {
        if(pAIShm->ulPackFromCpu == 1)
        {
            printf("Tracy monitor task find new data\n");
            if(pAIShm->ulAIWriteFlag == 1)
            {
                sem_post(&semAISync);
                pAIShm->ulPackFromCpu = 0;
                pAIShm->ulAIWriteFlag = 0;
                printf("Tracy monitor release data success\n");
            }
        }
        sleep(1);//等待1s
    }
    return NULL;
}

/**
 * @brief 解码网络报文并将数据写入共享内存浮点数组
 * @param [in]  const char* cBuffer   接收到的网络报文数据缓冲区
 *             size_t iLen           缓冲区数据长度
 * @param [out] 无
 *
 * 修改日期         版本号       修改人         修改内容     \n
 * ----------------------------------------------------------\n
 * 2024/10/15    V1.0   changsong-qin    新增该函数
 * @see
 * @note
 * @warning
 */
void decode_HSB_PACKET_packet(const char *cBuffer, size_t iLen)
{
    int i = 0;
    if(iLen != HSB_TARGET_SIZE)
    {
        printf("Tracy Received packet length is %zu, expected %d\n", iLen, HSB_TARGET_SIZE);
        return;
    }

    // Check the source and destination MAC addresses
    if(memcmp(cBuffer, HSB_FPGA_DST_MAC, 6) != 0 ||
            memcmp(cBuffer + 6, HSB_FPGA_SRC_MAC, 6) != 0)
    {
        printf("Tracy receive packet MAC address mismatch\n");
        return;
    }

    // Check the specific byte values in the packet
    if(cBuffer[16] != HSB_PACKET_APPFLAG_BYTE_16)
    {
        printf("Tracy receive packet appflag byte mismatch at position 16\n");
        return;
    }

    // Check the sequence of bytes from 17 to 22
    if(memcmp(cBuffer + 17, HSB_PACKET_APPFIX_SEQUENCE_BYTES, 6) != 0)
    {
        printf("Tracy receive packet sequence mismatch from bytes 17 to 22\n");
        return;
    }

    // Check the footer bytes from 24 to 27
    if(memcmp(cBuffer + 24, HSB_PACKET_APPFIX_FOOTER, 4) != 0)
    {
        printf("Tracy receive packet footer mismatch from bytes 24 to 27\n");
        return;
    }

    //定义1个结构体存储数据
    HSB_RECV_PACKET_STR hsbpacket;
    memcpy(&hsbpacket, cBuffer, sizeof(HSB_RECV_PACKET_STR));

    // 进行字节序转换
    hsbpacket.ulCPUFIXDATA = be32_to_cpu(hsbpacket.ulCPUFIXDATA);
    hsbpacket.ulRecvStNum = be32_to_cpu(hsbpacket.ulRecvStNum);
    hsbpacket.ulRecvSqNum = be32_to_cpu(hsbpacket.ulRecvSqNum);
    hsbpacket.ulRecvDataCnt = be32_to_cpu(hsbpacket.ulRecvDataCnt);

    printf("Tracy Received data from HSB:%04X--st:%d--sq%d--Cnt%d\n",
           hsbpacket.ulCPUFIXDATA, hsbpacket.ulRecvStNum, hsbpacket.ulRecvSqNum, hsbpacket.ulRecvDataCnt);

    // 检查共享内存是否可写入
    if(pAIShm->ulPackFromCpu == 1)
    {
        printf("Tracy Warning: Floating point data has not been analyzed, cannot write new data\n");
        return;
    }

    // 解析浮点数据并写入共享内存
    // 定义偏移量变量来避免复杂宏展开的问题
    size_t voltageOffset = offsetof(HSB_RECV_PACKET_STR, fRecvVoltageData);
    size_t currentOffset = offsetof(HSB_RECV_PACKET_STR, fRecvCurrentData);
    for(i = 0; i < HSB_FLOAT_ARRAY_SIZE; i++)
    {
        // 获取电压数据
        pAIShm->fVoltageData[i] = BYTES_TO_FLT(cBuffer + (voltageOffset + (i * sizeof(float))));

        // 获取电流数据
        pAIShm->fCurrentData[i] = BYTES_TO_FLT(cBuffer + (currentOffset + (i * sizeof(float))));
        
        printf("Voltage data:i%d\t: Voltage:%f\t current:%f\n", i, pAIShm->fVoltageData[i], pAIShm->fCurrentData[i]);
    }
    pAIShm->ulPackFromCpu = 1;

    printf("Packet decoded successfully and data written to shared memory\n");
}


int main()
{
    //创建三个任务，分别是HSB接收报文、HSB发送报文和监视功能
    pthread_t tidHSBAIReceive, tidHSBAISend, tidAIMonitor;

    //初始化共享内存
    init_AI_shared_memory();

    //初始化信号量
    sem_init(&semAISync, 0, 0);

    //创建3个任务
    pthread_create(&tidHSBAIReceive, NULL, receive_HSB_PACKET_task, NULL);
    printf("Tracy receive_HSB_PACKET_task success\n");
    pthread_create(&tidHSBAISend, NULL, send_HSB_PACKET_AI_task, NULL);
    printf("Tracy send_HSB_PACKET_AI_task success\n");
    pthread_create(&tidAIMonitor, NULL, monitor_HSB_AI_task, NULL);
    printf("Tracy monitor_HSB_AI_task success\n");

    pthread_join(tidHSBAIReceive, NULL);
    pthread_join(tidHSBAISend, NULL);
    pthread_join(tidAIMonitor, NULL);

    sem_destroy(&semAISync);
    munmap(pAIShm, XJTUAI_SHM_SIZE);
    shm_unlink(XJTUAI_SHM_NAME);

    return 0;
}
