/*!
 * \file      lr1_mac_utilities.h
 *
 * \brief     LoRaWan utilities definition
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

#ifndef __LR1MAC_UTILITIES_H__
#define __LR1MAC_UTILITIES_H__

#ifdef __cplusplus
extern "C" {
#endif
/*
 *-----------------------------------------------------------------------------------
 * --- DEPENDENCIES -----------------------------------------------------------------
 */
#include <stdint.h>
#include <stdbool.h>
#include "ral_defs.h"
/*
 *-----------------------------------------------------------------------------------
 * --- PUBLIC MACROS ----------------------------------------------------------------
 */

/*!
 * \brief Returns the minimum value between a and b
 *
 * \param [IN] a 1st value
 * \param [IN] b 2nd value
 * \retval minValue Minimum value
 */
#define MIN( a, b ) ( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )

/*!
 * \brief Returns the maximum value between a and b
 *
 * \param [IN] a 1st value
 * \param [IN] b 2nd value
 * \retval maxValue Maximum value
 */
#define MAX( a, b ) ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

/*
 *-----------------------------------------------------------------------------------
 *--- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------------
 */

/*!
 * \brief Copy size elements of src array to dst array
 *
 * \remark STM32 Standard memcpy function only works on pointers that are aligned
 *
 * \param [OUT] dst   Destination array
 * \param [IN]  src   Source array
 * \param [IN]  size  Number of bytes to be copied
 */
void memcpy1( uint8_t* dst, const uint8_t* src, uint16_t size );

/*!
 * \brief Set size elements of dst array with value
 *
 * \remark STM32 Standard memset function only works on pointers that are aligned
 *
 * \param [OUT] dst   Destination array
 * \param [IN]  value Default value
 * \param [IN]  size  Number of bytes to be copied
 */
void memset1( uint8_t* dst, uint8_t value, uint16_t size );

/*!
 * \brief Crc32 implementation for flash corruption
 *
 */
uint32_t lr1mac_utilities_crc( uint8_t* buf, int len );

/*!
 * \brief Compute symbol time in µs
 *
 */
uint32_t lr1mac_utilities_get_symb_time_us( const uint16_t nb_symb, const ral_lora_sf_t sf, const ral_lora_bw_t bw );

#ifdef __cplusplus
}
#endif

#endif  // __LR1MAC_UTILITIES_H__
