/*!
 * \file      real_defs_str.h
 *
 * \brief     Region Abstraction Layer (REAL) strings definition
 *
 * Revised BSD License
 * Copyright Semtech Corporation 2020. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __REAL_DEFS_STR_H__
#define __REAL_DEFS_STR_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if BSP_DBG_TRACE == BSP_FEATURE_ON
static const char* smtc_real_region_list_str[] = {
#if defined( REGION_EU_868 )
    [SMTC_REAL_REGION_EU_868] = "EU868",
#endif
#if defined( REGION_AS_923 )
    [SMTC_REAL_REGION_AS_923] = "AS923",
#endif
#if defined( REGION_US_915 )
    [SMTC_REAL_REGION_US_915] = "US915",
#endif
#if defined( REGION_AU_915 )
    [SMTC_REAL_REGION_AU_915] = "AU915",
#endif
#if defined( REGION_CN_470 )
    [SMTC_REAL_REGION_CN_470] = "CN470",
#endif
#if defined( REGION_WW2G4 )
    [SMTC_REAL_REGION_WW2G4] = "WW2G4",
#endif
};
#endif

#endif  // __REAL_DEFS_STR_H__
