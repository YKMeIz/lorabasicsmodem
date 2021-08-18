/*!
 * \file      radio_planner_stats.h
 *
 * \brief     Radio planner statistics
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
#ifndef __RADIO_PLANNER_STASTS_H__
#define __RADIO_PLANNER_STASTS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stdint.h>   // C99 types
#include <stdbool.h>  // bool type

#include "radio_planner_types.h"

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC MACROS -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC CONSTANTS --------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */

/*!
 *
 */
typedef struct rp_stats_s
{
    uint32_t tx_last_toa_ms[RP_NB_HOOKS];
    uint32_t tx_consumption_ms[RP_NB_HOOKS];
    uint32_t rx_consumption_ms[RP_NB_HOOKS];
    uint32_t tx_consumption_ma[RP_NB_HOOKS];
    uint32_t rx_consumption_ma[RP_NB_HOOKS];
    uint32_t tx_total_consumption_ms;
    uint32_t rx_total_consumption_ms;
    uint32_t tx_total_consumption_ma;
    uint32_t rx_total_consumption_ma;
    uint32_t tx_timestamp;
    uint32_t rx_timestamp;
    uint32_t task_hook_aborted_nb[RP_NB_HOOKS];
    uint32_t rp_error;
} rp_stats_t;

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

/*!
 *
 */
static inline void rp_stats_init( rp_stats_t* rp_stats )
{
    for( int32_t i = 0; i < RP_NB_HOOKS; i++ )
    {
        rp_stats->tx_last_toa_ms[i]       = 0;
        rp_stats->tx_consumption_ms[i]    = 0;
        rp_stats->rx_consumption_ms[i]    = 0;
        rp_stats->tx_consumption_ma[i]    = 0;
        rp_stats->rx_consumption_ma[i]    = 0;
        rp_stats->task_hook_aborted_nb[i] = 0;
    }
    rp_stats->tx_total_consumption_ms = 0;
    rp_stats->rx_total_consumption_ms = 0;
    rp_stats->tx_total_consumption_ma = 0;
    rp_stats->rx_total_consumption_ma = 0;
    rp_stats->tx_timestamp            = 0;
    rp_stats->rx_timestamp            = 0;
    rp_stats->rp_error                = 0;
}

/*!
 *
 */
static inline void rp_stats_set_tx_timestamp( rp_stats_t* rp_stats, uint32_t timestamp )
{
    rp_stats->tx_timestamp = timestamp;
}

/*!
 *
 */
static inline void rp_stats_set_rx_timestamp( rp_stats_t* rp_stats, uint32_t timestamp )
{
    rp_stats->rx_timestamp = timestamp;
}

/*!
 *
 */
static inline void rp_stats_update( rp_stats_t* rp_stats, uint32_t timestamp, uint8_t hook_id, uint32_t micro_ampere )
{
    uint32_t computed_time        = 0;
    uint32_t computed_consumption = 0;
    if( rp_stats->tx_timestamp != 0 )
    {
        // wrapping is impossible with this time base
        computed_time                     = timestamp - rp_stats->tx_timestamp;
        rp_stats->tx_last_toa_ms[hook_id] = computed_time;
        rp_stats->tx_consumption_ms[hook_id] += computed_time;
        rp_stats->tx_total_consumption_ms += computed_time;

        computed_consumption = ( computed_time * micro_ampere );
        rp_stats->tx_consumption_ma[hook_id] += ( computed_consumption / 1000 );
        rp_stats->tx_total_consumption_ma += ( computed_consumption / 1000 );
    }
    if( rp_stats->rx_timestamp != 0 )
    {
        computed_time = timestamp - rp_stats->rx_timestamp;
        rp_stats->rx_consumption_ms[hook_id] += computed_time;
        rp_stats->rx_total_consumption_ms += computed_time;

        computed_consumption = ( computed_time * micro_ampere );
        rp_stats->rx_consumption_ma[hook_id] += ( computed_consumption / 1000 );
        rp_stats->rx_total_consumption_ma += ( computed_consumption / 1000 );
    }
    rp_stats->tx_timestamp = 0;
    rp_stats->rx_timestamp = 0;
}

/*!
 *
 */
static inline void rp_stats_print( rp_stats_t* rp_stats )
{
    BSP_DBG_TRACE_PRINTF_RP( "\n" );
    BSP_DBG_TRACE_PRINTF_RP( "###### ===================================== ######\n" );
    BSP_DBG_TRACE_PRINTF_RP( "###### ===== Radio Planner Statistics ====== ######\n" );
    BSP_DBG_TRACE_PRINTF_RP( "###### ===================================== ######\n" );
    for( int32_t i = 0; i < RP_NB_HOOKS; i++ )
    {
        BSP_DBG_TRACE_PRINTF_RP( "Tx consumption hook #%ld = %lu ms\n", i, rp_stats->tx_consumption_ms[i] );
        BSP_DBG_TRACE_PRINTF_RP( "Tx consumption hook #%ld = %lu ua\n", i, rp_stats->tx_consumption_ma[i] );
    }
    for( int32_t i = 0; i < RP_NB_HOOKS; i++ )
    {
        BSP_DBG_TRACE_PRINTF_RP( "Rx consumption hook #%ld = %lu ms\n", i, rp_stats->rx_consumption_ms[i] );
        BSP_DBG_TRACE_PRINTF_RP( "Rx consumption hook #%ld = %lu ua\n", i, rp_stats->rx_consumption_ma[i] );
    }
    BSP_DBG_TRACE_PRINTF_RP( "Tx total consumption     = %lu ms\n ", rp_stats->tx_total_consumption_ms );
    BSP_DBG_TRACE_PRINTF_RP( "Tx total consumption     = %lu uA\n ", rp_stats->tx_total_consumption_ma );
    BSP_DBG_TRACE_PRINTF_RP( "Rx total consumption     = %lu ms\n ", rp_stats->rx_total_consumption_ms );
    BSP_DBG_TRACE_PRINTF_RP( "Rx total consumption     = %lu uA\n ", rp_stats->rx_total_consumption_ma );
    for( int32_t i = 0; i < RP_NB_HOOKS; i++ )
    {
        BSP_DBG_TRACE_PRINTF_RP( "Number of aborted tasks for hook #%ld = %lu \n", i,
                                 rp_stats->task_hook_aborted_nb[i] );
    }
    BSP_DBG_TRACE_PRINTF_RP( "RP: number of errors is %lu\n\n\n", rp_stats->rp_error );
}

#ifdef __cplusplus
}
#endif

#endif  // __RADIO_PLANNER_STASTS_H__

/* --- EOF ------------------------------------------------------------------ */
