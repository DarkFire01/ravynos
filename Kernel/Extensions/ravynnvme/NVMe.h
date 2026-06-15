/*
 * NVMe: register and structure definitions for NVM Express controllers
 *
 * Copyright (C) 2026 ravynOS Project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _NVME_H
#define _NVME_H

#include <stdint.h>

/* ---- Controller register offsets (BAR0) ------------------------------- */
#define NVME_REG_CAP    0x00      /* Controller Capabilities (64-bit) */
#define NVME_REG_VS     0x08      /* Version */
#define NVME_REG_INTMS  0x0C      /* Interrupt Mask Set */
#define NVME_REG_INTMC  0x10      /* Interrupt Mask Clear */
#define NVME_REG_CC     0x14      /* Controller Configuration */
#define NVME_REG_CSTS   0x1C      /* Controller Status */
#define NVME_REG_AQA    0x24      /* Admin Queue Attributes */
#define NVME_REG_ASQ    0x28      /* Admin Submission Queue Base (64-bit) */
#define NVME_REG_ACQ    0x30      /* Admin Completion Queue Base (64-bit) */
#define NVME_REG_DBS    0x1000    /* Doorbell registers base */

/* CAP fields */
#define NVME_CAP_MQES(c)  (((c) & 0xFFFF) + 1)        /* Max Queue Entries Supported */
#define NVME_CAP_DSTRD(c) (((c) >> 32) & 0xF)         /* Doorbell Stride (2^(2+x) bytes) */
#define NVME_CAP_TO(c)    (((c) >> 24) & 0xFF)        /* Timeout (500ms units) */

/* CC fields */
#define NVME_CC_EN        (1U << 0)                    /* Enable */
#define NVME_CC_CSS_NVM   (0U << 4)                    /* I/O command set: NVM */
#define NVME_CC_MPS(x)    (((x) & 0xF) << 7)           /* Memory Page Size (2^(12+x)) */
#define NVME_CC_AMS_RR    (0U << 11)                   /* Arbitration: round robin */
#define NVME_CC_SHN_NONE  (0U << 14)                   /* Shutdown notification */
#define NVME_CC_IOSQES(x) (((x) & 0xF) << 16)          /* I/O SQ entry size (2^x) */
#define NVME_CC_IOCQES(x) (((x) & 0xF) << 20)          /* I/O CQ entry size (2^x) */

/* CSTS fields */
#define NVME_CSTS_RDY     (1U << 0)                    /* Ready */
#define NVME_CSTS_CFS     (1U << 1)                    /* Controller Fatal Status */

/* ---- Admin opcodes ---------------------------------------------------- */
#define NVME_ADMIN_DELETE_SQ    0x00
#define NVME_ADMIN_CREATE_SQ    0x01
#define NVME_ADMIN_DELETE_CQ    0x04
#define NVME_ADMIN_CREATE_CQ    0x05
#define NVME_ADMIN_IDENTIFY     0x06
#define NVME_ADMIN_SET_FEATURES 0x09

/* IDENTIFY CNS values */
#define NVME_IDENTIFY_CNS_NS    0x00      /* Identify Namespace */
#define NVME_IDENTIFY_CNS_CTRL  0x01      /* Identify Controller */

/* Set Features feature identifiers */
#define NVME_FEAT_NUM_QUEUES    0x07

/* ---- NVM I/O opcodes -------------------------------------------------- */
#define NVME_IO_FLUSH   0x00
#define NVME_IO_WRITE   0x01
#define NVME_IO_READ    0x02

/* ---- Submission Queue Entry (64 bytes) -------------------------------- */
struct NVMeCommand {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t commandId;
    uint32_t nsid;
    uint64_t reserved2;
    uint64_t metadata;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed));

/* ---- Completion Queue Entry (16 bytes) -------------------------------- */
struct NVMeCompletion {
    uint32_t result;
    uint32_t reserved;
    uint16_t sqHead;
    uint16_t sqId;
    uint16_t commandId;
    uint16_t status;        /* bit 0 = phase tag; bits 1..15 = status field */
} __attribute__((packed));

#define NVME_CQE_PHASE(s)   ((s) & 0x1)
#define NVME_CQE_STATUS(s)  (((s) >> 1) & 0x7FFF)

/* ---- Identify Namespace (subset; offsets per NVMe spec) --------------- */
struct NVMeIdentifyNamespace {
    uint64_t nsze;          /* Namespace Size (in logical blocks) */
    uint64_t ncap;          /* Namespace Capacity */
    uint64_t nuse;          /* Namespace Utilization */
    uint8_t  nsfeat;
    uint8_t  nlbaf;         /* Number of LBA Formats (0-based) */
    uint8_t  flbas;         /* Formatted LBA Size (low 4 bits = format index) */
    uint8_t  mc;
    uint8_t  dpc;
    uint8_t  dps;
    uint8_t  reserved30[98];
    uint32_t lbaf[16];      /* LBA Format descriptors; bits 16..23 = LBADS (2^x) */
    uint8_t  reserved192[192];
    uint8_t  vendor[3712];
} __attribute__((packed));

#define NVME_LBAF_LBADS(f)  (((f) >> 16) & 0xFF)       /* log2(LBA data size) */

/* ---- Identify Controller (subset) ------------------------------------- */
struct NVMeIdentifyController {
    uint16_t vid;
    uint16_t ssvid;
    char     sn[20];        /* Serial Number (space padded, not NUL) */
    char     mn[40];        /* Model Number */
    char     fr[8];         /* Firmware Revision */
    uint8_t  rab;
    uint8_t  ieee[3];
    uint8_t  cmic;
    uint8_t  mdts;
    uint8_t  reserved78[178];
    /* ... remaining fields not used by this minimal driver ... */
    uint8_t  reserved256[3840];
} __attribute__((packed));

#endif /* _NVME_H */
