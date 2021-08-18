/*!
 * \file      lr1_stack_mac_layer.c
 *
 * \brief     LoRaWan stack mac layer definition
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

/*
 *-----------------------------------------------------------------------------------
 * --- DEPENDENCIES -----------------------------------------------------------------
 */
#include <stdlib.h>
#include <stdio.h>
#include "lr1_stack_mac_layer.h"
#include "lr1mac_utilities.h"
#include "radio_planner.h"
#include "crypto.h"
#include "smtc_real.h"
#include "lorawan_api.h"
#include "smtc_bsp.h"
#include <math.h>

/*
 *-----------------------------------------------------------------------------------
 * --- PRIVATE MACROS ---------------------------------------------------------------
 */

/*
 *-----------------------------------------------------------------------------------
 *--- PRIVATE VARIABLES -------------------------------------------------------------
 */

#if( BSP_DBG_TRACE == BSP_FEATURE_ON )
static const char* name_rx_windows[] = { "RX1", "RX2" };
static const char* name_bw[]         = { "BW007", "BW010", "BW015", "BW020", "BW031", "BW041", "BW062",
                                 "BW125", "BW200", "BW250", "BW400", "BW500", "BW800", "BW1600" };
#endif

static const uint16_t MAX_FCNT_GAP = 16384UL;

/*
 *-----------------------------------------------------------------------------------
 *--- PRIVATE FUNCTIONS DECLARATION -------------------------------------------------
 */

/*!
 *
 */
static void mac_header_set( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static void frame_header_set( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static valid_dev_addr_t check_dev_addr( lr1_stack_mac_t* lr1_mac, uint32_t devAddr_to_test );
/*!
 *
 */
static status_lorawan_t rx_payload_size_check( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static status_lorawan_t rx_mhdr_extract( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static void compute_rx_window_parameters( lr1_stack_mac_t* lr1_mac, uint8_t sf, lr1mac_bandwidth_t bw,
                                          uint32_t clock_accuracy, uint32_t rx_delay_ms, uint8_t board_delay_ms,
                                          modulation_type_t rx_modulation_type );
/*!
 *
 */
static int rx_fhdr_extract( lr1_stack_mac_t* lr1_mac, uint16_t* fcnt_dwn_tmp, uint32_t dev_addr );

/*!
 *
 */
static int fcnt_dwn_accept( uint16_t fcnt_dwn_tmp, uint32_t* fcnt_lorawan );
/*!
 *
 */
static void link_check_parser( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static void link_adr_parser( lr1_stack_mac_t* lr1_mac, uint8_t nb_link_adr_req );
/*!
 *
 */
static void duty_cycle_parser( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static void rx_param_setup_parser( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static void dev_status_parser( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static void new_channel_parser( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static void rx_timing_setup_parser( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static void tx_param_setup_parser( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
static void dl_channel_parser( lr1_stack_mac_t* lr1_mac );
/*!
 *
 */
uint8_t lr1_stack_mac_cmd_ans_cut( uint8_t* nwk_ans, uint8_t nwk_ans_size_in, uint8_t max_allowed_size );
/*
 *-----------------------------------------------------------------------------------
 *--- PUBLIC FUNCTIONS DEFINITIONS --------------------------------------------------
 */

void lr1_stack_mac_init( lr1_stack_mac_t* lr1_mac, lorawan_keys_t* lorawan_keys, smtc_real_t* real )
{
    lr1_mac->tx_major_bits             = LORAWANR1;
    lr1_mac->radio_process_state       = RADIOSTATE_IDLE;
    lr1_mac->next_time_to_join_seconds = 0;
    lr1_mac->join_status               = NOT_JOINED;
    lr1_mac->tx_modulation_type        = LORA;
    lr1_mac->rx1_modulation_type       = LORA;
    lr1_mac->rx2_modulation_type       = LORA;
    lr1_mac->type_of_ans_to_send       = NOFRAME_TOSEND;
    lr1_mac->otaa_device               = lorawan_keys->otaaDevice;
    lr1_mac->nb_trans                  = 1;
    lr1_mac->available_app_packet      = NO_LORA_RXPACKET_AVAILABLE;
    lr1_mac->tx_power_offset           = 0;
    lr1_mac->real                      = real;

#if defined( PERF_TEST_ENABLED )
    // bypass join process to allow perf testbench to trigger some modem send tx commands
    lr1_mac->join_status = JOINED;
#endif

    lr1_stack_mac_session_init( lr1_mac );
}

void lr1_stack_mac_session_init( lr1_stack_mac_t* lr1_mac )
{
    lr1_mac->fcnt_dwn                    = ~0;
    lr1_mac->fcnt_up                     = 0;
    lr1_mac->retry_join_cpt              = 0;
    lr1_mac->adr_ack_cnt                 = 0;
    lr1_mac->adr_ack_cnt_confirmed_frame = 0;
    lr1_mac->tx_fopts_current_length     = 0;
    lr1_mac->tx_fopts_length             = 0;
    lr1_mac->tx_fopts_lengthsticky       = 0;
    lr1_mac->nwk_ans_size                = 0;
    lr1_mac->nwk_payload_size            = 0;
    lr1_mac->nwk_payload_index           = 0;
    lr1_mac->max_eirp_dbm                = smtc_real_default_max_eirp_get( lr1_mac );
    lr1_mac->uplink_dwell_time           = 1;
    lr1_mac->downlink_dwell_time         = 1;
    lr1_mac->max_duty_cycle_index        = 0;
    lr1_mac->tx_duty_cycle_time_off_ms   = 0;
    lr1_mac->tx_duty_cycle_timestamp_ms  = bsp_rtc_get_time_ms( );
}

/**************************************************************************************************/
/*                                  build lorawan frame */
/*                                  encrypt lorawan frame */
/*                       enqueue tx frame in radioplanner to proceed transmit */
/*                                                                                                */
/**************************************************************************************************/

void lr1_stack_mac_tx_frame_build( lr1_stack_mac_t* lr1_mac )
{
    lr1_mac->tx_fctrl = 0;
    lr1_mac->tx_fctrl = ( lr1_mac->adr_enable << 7 ) + ( lr1_mac->adr_ack_req << 6 ) + ( lr1_mac->tx_ack_bit << 5 ) +
                        ( lr1_mac->tx_fopts_current_length & 0x0F );
    lr1_mac->tx_ack_bit = 0;
    lr1_mac->rx_ack_bit = 0;
    mac_header_set( lr1_mac );
    frame_header_set( lr1_mac );
    lr1_mac->tx_payload_size = lr1_mac->app_payload_size + FHDROFFSET + lr1_mac->tx_fopts_current_length;
}

void lr1_stack_mac_tx_frame_encrypt( lr1_stack_mac_t* lr1_mac )
{
    lora_crypto_payload_encrypt(
        &lr1_mac->tx_payload[FHDROFFSET + lr1_mac->tx_fopts_current_length], lr1_mac->app_payload_size,
        ( lr1_mac->tx_fport == PORTNWK ) ? lr1_mac->nwk_skey : lr1_mac->app_skey, lr1_mac->dev_addr, UP_LINK,
        lr1_mac->fcnt_up, &lr1_mac->tx_payload[FHDROFFSET + lr1_mac->tx_fopts_current_length] );

    lora_crypto_add_mic( &lr1_mac->tx_payload[0], lr1_mac->tx_payload_size, lr1_mac->nwk_skey, lr1_mac->dev_addr,
                         UP_LINK, lr1_mac->fcnt_up );
    lr1_mac->tx_payload_size = lr1_mac->tx_payload_size + 4;
}

void lr1_stack_mac_tx_radio_start( lr1_stack_mac_t* lr1_mac )
{
    rp_radio_params_t radio_params = { 0 };
    rp_task_t         rp_task      = { 0 };

    if( lr1_mac->tx_modulation_type == LORA )
    {
        radio_params.pkt_type                 = RAL_PKT_TYPE_LORA;
        radio_params.tx.lora.cr               = smtc_real_coding_rate_get( lr1_mac );
        radio_params.tx.lora.sync_word        = smtc_real_sync_word_get( lr1_mac );
        radio_params.tx.lora.crc_is_on        = true;
        radio_params.tx.lora.invert_iq_is_on  = false;
        radio_params.tx.lora.pld_is_fix       = false;
        radio_params.tx.lora.bw               = ( ral_lora_bw_t ) lr1_mac->tx_bw;
        radio_params.tx.lora.sf               = ( ral_lora_sf_t ) lr1_mac->tx_sf;
        radio_params.tx.lora.freq_in_hz       = lr1_mac->tx_frequency;
        radio_params.tx.lora.pld_len_in_bytes = lr1_mac->tx_payload_size;
        radio_params.tx.lora.pwr_in_dbm       = lr1_mac->tx_power + lr1_mac->tx_power_offset;
        radio_params.tx.lora.pbl_len_in_symb  = smtc_real_preamble_get( lr1_mac, radio_params.tx.lora.sf );
    }
    else if( lr1_mac->tx_modulation_type == FSK )
    {
        radio_params.pkt_type                       = RAL_PKT_TYPE_GFSK;
        radio_params.tx.gfsk.freq_in_hz             = lr1_mac->tx_frequency;
        radio_params.tx.gfsk.pld_is_fix             = false;
        radio_params.tx.gfsk.pld_len_in_bytes       = lr1_mac->tx_payload_size;
        radio_params.tx.gfsk.pwr_in_dbm             = lr1_mac->tx_power + lr1_mac->tx_power_offset;
        radio_params.tx.gfsk.fdev_in_hz             = 25000;  // TODO check value
        radio_params.tx.gfsk.pbl_len_in_bytes       = 5;      // TODO check value
        radio_params.tx.gfsk.sync_word              = smtc_real_gfsk_sync_word_get( lr1_mac );
        radio_params.tx.gfsk.sync_word_len_in_bytes = 3;
        radio_params.tx.gfsk.br_in_bps              = lr1_mac->tx_sf * 1000;
        radio_params.tx.gfsk.bw_ssb_in_hz           = lr1_mac->tx_sf * 1000;
        radio_params.tx.gfsk.dc_free_is_on          = true;
        radio_params.tx.gfsk.whitening_seed         = GFSK_WHITENING_SEED;
        radio_params.tx.gfsk.crc_type               = RAL_GFSK_CRC_2_BYTES_INV;
        radio_params.tx.gfsk.crc_seed               = GFSK_CRC_SEED;
        radio_params.tx.gfsk.crc_polynomial         = GFSK_CRC_POLYNOMIAL;

        BSP_DBG_TRACE_PRINTF( "  TxFrequency = %lu, FSK \n", lr1_mac->tx_frequency );
    }
    else
    {
        BSP_DBG_TRACE_ERROR( " TX MODULATION NOT SUPPORTED\n" );
        bsp_mcu_handle_lr1mac_issue( );
    }

    uint8_t my_hook_id;
    if( rp_hook_get_id( lr1_mac->rp, lr1_mac, &my_hook_id ) != RP_HOOK_STATUS_OK )
    {
        bsp_mcu_handle_lr1mac_issue( );
    }

    // uint32_t toa;
    // ral_get_lora_time_on_air_in_ms( lr1_mac->rp->ral, ( &radio_params.tx.lora ), &toa );
    // 2000; // Need time on air api with no ral dependencies dangerous in case on more than 1 hook in the rp

    rp_task.hook_id          = my_hook_id;
    rp_task.duration_time_ms = 2000;
    rp_task.type             = ( lr1_mac->tx_modulation_type == LORA ) ? RP_TASK_TYPE_TX_LORA : RP_TASK_TYPE_TX_FSK;
    rp_task.start_time_ms    = lr1_mac->rtc_target_timer_ms;

    if( lr1_mac->send_at_time == true )
    {
        lr1_mac->send_at_time = false;  // reinit the flag
        rp_task.state         = RP_TASK_STATE_SCHEDULE;
    }
    else
    {
        rp_task.state = RP_TASK_STATE_ASAP;
    }

    if( rp_task_enqueue( lr1_mac->rp, &rp_task, lr1_mac->tx_payload, lr1_mac->tx_payload_size, &radio_params ) ==
        RP_HOOK_STATUS_OK )
    {
        if( radio_params.pkt_type == RAL_PKT_TYPE_LORA )
        {
            BSP_DBG_TRACE_PRINTF( "  Tx  LoRa at %u ms: freq:%lu, SF%u, %s, len %u bytes %d dBm\n",
                                  rp_task.start_time_ms, lr1_mac->tx_frequency, lr1_mac->tx_sf, name_bw[lr1_mac->tx_bw],
                                  lr1_mac->tx_payload_size, lr1_mac->tx_power + lr1_mac->tx_power_offset );
        }
        else if( radio_params.pkt_type == RAL_PKT_TYPE_GFSK )
        {
            BSP_DBG_TRACE_PRINTF( "  Tx  FSK  at %u ms: freq:%lu, len %u bytes %d dBm\n", rp_task.start_time_ms,
                                  lr1_mac->tx_frequency, lr1_mac->tx_payload_size,
                                  lr1_mac->tx_power + lr1_mac->tx_power_offset );
        }
        lr1_mac->radio_process_state = RADIOSTATE_TXON;
        if( lr1_mac->tx_mtype == CONF_DATA_UP )
        {
            lr1_mac->adr_ack_cnt_confirmed_frame++;
        }
        else
        {
            lr1_mac->adr_ack_cnt++;  // increment adr counter each uplink frame
        }
    }
    else
    {
        BSP_DBG_TRACE_PRINTF( "Radio planner hook %d is busy \n", my_hook_id );
    }
}

void lr1_stack_mac_rx_radio_start( lr1_stack_mac_t* lr1_mac, const rx_win_type_t type, const uint32_t time_to_start )
{
    rp_radio_params_t radio_params = { 0 };

    if( ( ( type == RX1 ) && ( lr1_mac->rx1_modulation_type == LORA ) ) ||
        ( ( type == RX2 ) && ( lr1_mac->rx2_modulation_type == LORA ) ) )
    {
        radio_params.pkt_type                 = RAL_PKT_TYPE_LORA;
        radio_params.rx.lora.cr               = smtc_real_coding_rate_get( lr1_mac );
        radio_params.rx.lora.sync_word        = smtc_real_sync_word_get( lr1_mac );
        radio_params.rx.lora.crc_is_on        = false;
        radio_params.rx.lora.invert_iq_is_on  = true;
        radio_params.rx.lora.pld_is_fix       = false;
        radio_params.rx.lora.pld_len_in_bytes = 255;
        radio_params.rx.lora.symb_nb_timeout  = lr1_mac->rx_window_symb;
#if defined( SX1280 )
        radio_params.rx.timeout_in_ms = MAX( lr1_mac->rx_timeout_ms, BSP_MIN_RX_TIMEOUT_DELAY_MS );
#elif defined( SX126X )
        radio_params.rx.timeout_in_ms = 3000;
#else
#error "Please select radio board.."
#endif

        switch( type )
        {
        case RX1:
            radio_params.rx.lora.sf         = ( ral_lora_sf_t ) lr1_mac->rx1_sf;
            radio_params.rx.lora.bw         = ( ral_lora_bw_t ) lr1_mac->rx1_bw;
            radio_params.rx.lora.freq_in_hz = lr1_mac->rx1_frequency;
            break;

        case RX2:
            radio_params.rx.lora.sf         = ( ral_lora_sf_t ) lr1_mac->rx2_sf;
            radio_params.rx.lora.bw         = ( ral_lora_bw_t ) lr1_mac->rx2_bw;
            radio_params.rx.lora.freq_in_hz = lr1_mac->rx2_frequency;
            break;

        default:
            BSP_DBG_TRACE_ERROR( " RX windows unknow \n" );
            bsp_mcu_handle_lr1mac_issue( );
            break;
        }
        radio_params.rx.lora.pbl_len_in_symb = smtc_real_preamble_get( lr1_mac, radio_params.rx.lora.sf );
    }
    else if( ( ( type == RX1 ) && ( lr1_mac->rx1_modulation_type == FSK ) ) ||
             ( ( type == RX2 ) && ( lr1_mac->rx2_modulation_type == FSK ) ) )
    {
        radio_params.pkt_type                       = RAL_PKT_TYPE_GFSK;
        radio_params.rx.gfsk.pbl_len_in_bytes       = 5;
        radio_params.rx.gfsk.sync_word_len_in_bytes = 3;
        radio_params.rx.gfsk.sync_word              = smtc_real_gfsk_sync_word_get( lr1_mac );
        radio_params.rx.gfsk.pld_is_fix             = false;
        radio_params.rx.gfsk.pld_len_in_bytes       = 255;
        radio_params.rx.gfsk.dc_free_is_on          = true;
        radio_params.rx.gfsk.whitening_seed         = GFSK_WHITENING_SEED;
        radio_params.rx.gfsk.crc_type               = RAL_GFSK_CRC_2_BYTES_INV;
        radio_params.rx.gfsk.crc_seed               = GFSK_CRC_SEED;
        radio_params.rx.gfsk.crc_polynomial         = GFSK_CRC_POLYNOMIAL;
        radio_params.rx.timeout_in_ms               = lr1_mac->rx_timeout_ms;

        switch( type )
        {
        case RX1:
            radio_params.rx.gfsk.freq_in_hz   = lr1_mac->rx1_frequency;
            radio_params.rx.gfsk.br_in_bps    = lr1_mac->rx1_sf * 1000;
            radio_params.rx.gfsk.bw_ssb_in_hz = lr1_mac->rx1_sf * 1000;
            break;

        case RX2:
            radio_params.rx.gfsk.freq_in_hz   = lr1_mac->rx2_frequency;
            radio_params.rx.gfsk.br_in_bps    = lr1_mac->rx2_sf * 1000;
            radio_params.rx.gfsk.bw_ssb_in_hz = lr1_mac->rx2_sf * 1000;
            break;

        default:
            BSP_DBG_TRACE_ERROR( " RX windows unknow \n" );
            bsp_mcu_handle_lr1mac_issue( );
            break;
        }
    }
    else
    {
        BSP_DBG_TRACE_ERROR( " MODULATION NOT SUPPORTED\n" );
        bsp_mcu_handle_lr1mac_issue( );
    }

    uint8_t my_hook_id;
    if( rp_hook_get_id( lr1_mac->rp, lr1_mac, &my_hook_id ) != RP_HOOK_STATUS_OK )
    {
        bsp_mcu_handle_lr1mac_issue( );
    }

    rp_task_t rp_task = {
        .hook_id          = my_hook_id,
        .type             = ( radio_params.pkt_type == RAL_PKT_TYPE_LORA ) ? RP_TASK_TYPE_RX_LORA : RP_TASK_TYPE_RX_FSK,
        .state            = RP_TASK_STATE_SCHEDULE,
        .start_time_ms    = time_to_start,
        .duration_time_ms = lr1_mac->rx_timeout_ms,
    };

    if( rp_task_enqueue( lr1_mac->rp, &rp_task, lr1_mac->rx_payload, 255, &radio_params ) == RP_HOOK_STATUS_OK )
    {
        if( radio_params.pkt_type == RAL_PKT_TYPE_LORA )
        {
            BSP_DBG_TRACE_PRINTF( "  %s LoRa at %u ms: freq:%lu, SF%u, %s, sync word = 0x%02X\n", name_rx_windows[type],
                                  time_to_start, radio_params.rx.lora.freq_in_hz, radio_params.rx.lora.sf,
                                  name_bw[radio_params.rx.lora.bw], smtc_real_sync_word_get( lr1_mac ) );
        }
        else
        {
            BSP_DBG_TRACE_PRINTF( "  %s FSK freq:%lu\n", name_rx_windows[type], radio_params.rx.gfsk.freq_in_hz );
        }
    }
    else
    {
        BSP_DBG_TRACE_PRINTF( "Radio planner hook %d is busy \n", my_hook_id );
    }
}

int lr1_stack_mac_downlink_check_under_it( lr1_stack_mac_t* lr1_mac )
{
    int status = OKLORAWAN;

    uint8_t my_hook_id;
    rp_hook_get_id( lr1_mac->rp, lr1_mac, &my_hook_id );
    BSP_DBG_TRACE_PRINTF( "payload size receive = %u, snr = %d , rssi = %d\n", lr1_mac->rx_payload_size,
                          lr1_mac->rp->radio_params[my_hook_id].rx.lora_pkt_status.snr_pkt_in_db,
                          lr1_mac->rp->radio_params[my_hook_id].rx.lora_pkt_status.rssi_pkt_in_dbm );

    valid_dev_addr_t is_valid_dev_addr = UNVALID_DEV_ADDR;

    // check Mtype
    uint8_t rx_mtype_tmp = lr1_mac->rx_payload[0] >> 5;
    if( ( rx_mtype_tmp == JOIN_REQUEST ) || ( rx_mtype_tmp == UNCONF_DATA_UP ) || ( rx_mtype_tmp == CONF_DATA_UP ) ||
        ( rx_mtype_tmp == REJOIN_REQUEST ) )
    {
        status += ERRORLORAWAN;
        BSP_DBG_TRACE_PRINTF( " BAD Mtype = %u for RX Frame \n", rx_mtype_tmp );
        is_valid_dev_addr = UNVALID_DEV_ADDR;
    }
    // check devaddr
    if( lr1_mac->join_status == JOINED )
    {
        uint32_t dev_addr_tmp = lr1_mac->rx_payload[1] + ( lr1_mac->rx_payload[2] << 8 ) +
                                ( lr1_mac->rx_payload[3] << 16 ) + ( lr1_mac->rx_payload[4] << 24 );
        is_valid_dev_addr = check_dev_addr( lr1_mac, dev_addr_tmp );
        if( is_valid_dev_addr == UNVALID_DEV_ADDR )
        {
            status += ERRORLORAWAN;
            BSP_DBG_TRACE_INFO( " BAD DevAddr = %lx for RX Frame and %lx \n \n", lr1_mac->dev_addr, dev_addr_tmp );
        }
        if( status != OKLORAWAN )
        {
            lr1_mac->rx_payload_size = 0;
        }
    }
    else
    {
        is_valid_dev_addr = UNVALID_DEV_ADDR;
    }

    return ( status );
}

void lr1_stack_mac_rp_callback( lr1_stack_mac_t* lr1_mac )
{
    int      status = OKLORAWAN;
    uint32_t tcurrent_ms;
    uint8_t  my_hook_id;
    rp_hook_get_id( lr1_mac->rp, lr1_mac, &my_hook_id );
    rp_get_status( lr1_mac->rp, my_hook_id, &tcurrent_ms, &( lr1_mac->planner_status ) );

    switch( lr1_mac->planner_status )
    {
    case RP_STATUS_TX_DONE:
        break;

    case RP_STATUS_RX_PACKET:

        // save rssi and snr
        lr1_mac->rx_snr          = lr1_mac->rp->radio_params[my_hook_id].rx.lora_pkt_status.snr_pkt_in_db;
        lr1_mac->rx_rssi         = lr1_mac->rp->radio_params[my_hook_id].rx.lora_pkt_status.rssi_pkt_in_dbm;
        lr1_mac->rx_payload_size = ( uint8_t ) lr1_mac->rp->payload_size[my_hook_id];

        status = lr1_stack_mac_downlink_check_under_it( lr1_mac );
        if( status != OKLORAWAN )
        {  // Case receive a packet but it isn't a valid packet
            tcurrent_ms = bsp_rtc_get_time_ms( );
            BSP_DBG_TRACE_MSG( "Receive a packet But rejected and too late to restart\n" );
            lr1_mac->planner_status = RP_STATUS_RX_TIMEOUT;
        }
        break;

    case RP_STATUS_RX_TIMEOUT:
        break;

    default:
        BSP_DBG_TRACE_PRINTF( "receive It RADIO error %u\n", lr1_mac->planner_status );
        tcurrent_ms = bsp_rtc_get_time_ms( );
        break;
    }

    switch( lr1_mac->radio_process_state )
    {
    case RADIOSTATE_TXON:
        lr1_mac->isr_radio_timestamp = tcurrent_ms;  //@info Timestamp only on txdone it
        lr1_mac->radio_process_state = RADIOSTATE_TXFINISHED;
        break;

    case RADIOSTATE_TXFINISHED:
        lr1_mac->radio_process_state = RADIOSTATE_RX1FINISHED;
        break;

    case RADIOSTATE_RX1FINISHED:
        lr1_mac->radio_process_state = RADIOSTATE_IDLE;
        break;

    default:
        BSP_DBG_TRACE_ERROR( "Unknown state in Radio Process %d \n", lr1_mac->radio_process_state );
        bsp_mcu_handle_lr1mac_issue( );
        break;
    }
}

int lr1_stack_mac_radio_state_get( lr1_stack_mac_t* lr1_mac )
{
    return ( lr1_mac->radio_process_state );
}

void lr1_stack_mac_rx_timer_configure( lr1_stack_mac_t* lr1_mac, const rx_win_type_t type )
{
    const uint32_t    tcurrent_ms = bsp_rtc_get_time_ms( );
    bool              is_type_ok  = true;
    ral_lora_sf_t     sf;
    ral_lora_bw_t     bw;
    modulation_type_t mod_type = LORA;
    uint32_t          delay_ms;

    smtc_real_rx_config_set( lr1_mac, type );

    switch( type )
    {
    case RX1:
        sf       = ( ral_lora_sf_t ) lr1_mac->rx1_sf;
        bw       = ( ral_lora_bw_t ) lr1_mac->rx1_bw;
        delay_ms = lr1_mac->rx1_delay_s * 1000;
        mod_type = lr1_mac->rx1_modulation_type;
        break;

    case RX2:
        sf       = ( ral_lora_sf_t ) lr1_mac->rx2_sf;
        bw       = ( ral_lora_bw_t ) lr1_mac->rx2_bw;
        delay_ms = ( lr1_mac->rx1_delay_s * 1000 ) + 1000;
        mod_type = lr1_mac->rx2_modulation_type;
        break;

    default:
        is_type_ok = false;
        BSP_DBG_TRACE_ERROR( " RX windows unknow \n" );
        bsp_mcu_handle_lr1mac_issue( );
        break;
    }

    if( is_type_ok == true )
    {
        compute_rx_window_parameters( lr1_mac, ( uint8_t ) sf, ( lr1mac_bandwidth_t ) bw, BSP_CRYSTAL_ERROR, delay_ms,
                                      BSP_BOARD_DELAY_RX_SETTING_MS, mod_type );

        uint32_t talarm_ms = delay_ms + lr1_mac->isr_radio_timestamp - tcurrent_ms;
        if( ( int32_t )( talarm_ms - lr1_mac->rx_offset_ms ) < 0 )
        {
            // too late to launch a timer
            switch( type )
            {
            case RX1:
                lr1_mac->radio_process_state = RADIOSTATE_RX1FINISHED;
                break;

            case RX2:
                lr1_mac->radio_process_state = RADIOSTATE_IDLE;
                break;

            default:
                bsp_mcu_handle_lr1mac_issue( );
                break;
            }
        }
        else
        {
            smtc_real_rx_config_set( lr1_mac, type );
            lr1_stack_mac_rx_radio_start( lr1_mac, type, bsp_rtc_get_time_ms( ) + talarm_ms - lr1_mac->rx_offset_ms );
            BSP_DBG_TRACE_PRINTF( "  Timer will expire in %ld ms\n", ( talarm_ms - lr1_mac->rx_offset_ms ) );
        }
    }
}

rx_packet_type_t lr1_stack_mac_rx_frame_decode( lr1_stack_mac_t* lr1_mac )
{
    int              status         = OKLORAWAN;
    rx_packet_type_t rx_packet_type = NO_MORE_VALID_RX_PACKET;
    uint32_t         mic_in;
    status += rx_payload_size_check( lr1_mac );
    status += rx_mhdr_extract( lr1_mac );
    /************************************************************************/
    /*                 Case : the receive packet is a JoinResponse          */
    /************************************************************************/
    if( lr1_mac->rx_mtype == JOIN_ACCEPT )
    {
        join_decrypt( &lr1_mac->rx_payload[1], lr1_mac->rx_payload_size - 1, lr1_mac->app_key,
                      &lr1_mac->rx_payload[1] );
        lr1_mac->rx_payload_size = lr1_mac->rx_payload_size - MICSIZE;
        memcpy( ( uint8_t* ) &mic_in, &lr1_mac->rx_payload[lr1_mac->rx_payload_size], MICSIZE );
        status += check_join_mic( lr1_mac->rx_payload, lr1_mac->rx_payload_size, lr1_mac->app_key, mic_in );
        BSP_DBG_TRACE_PRINTF( " status = %d\n", status );
        if( status == OKLORAWAN )
        {
            return JOIN_ACCEPT_PACKET;
        }
    }
    else
    {
        /************************************************************************/
        /*               Case : the receive packet is not a JoinResponse */
        /************************************************************************/
        uint16_t fcnt_dwn_tmp = 0;
        status += rx_fhdr_extract( lr1_mac, &fcnt_dwn_tmp, lr1_mac->dev_addr );
        if( status == OKLORAWAN )
        {
            status = fcnt_dwn_accept( fcnt_dwn_tmp, &lr1_mac->fcnt_dwn );
        }
        if( status == OKLORAWAN )
        {
            lr1_mac->rx_payload_size = lr1_mac->rx_payload_size - MICSIZE;
            memcpy( ( uint8_t* ) &mic_in, &lr1_mac->rx_payload[lr1_mac->rx_payload_size], MICSIZE );
            status += check_mic( &lr1_mac->rx_payload[0], lr1_mac->rx_payload_size, lr1_mac->nwk_skey,
                                 lr1_mac->dev_addr, lr1_mac->fcnt_dwn, mic_in );
        }
        if( status == OKLORAWAN )
        {
            lr1_mac->adr_ack_cnt                 = 0;  // reset adr counter, receive a valid frame.
            lr1_mac->adr_ack_cnt_confirmed_frame = 0;  // reset adr counter i, case of confirmed frame
            lr1_mac->tx_fopts_lengthsticky       = 0;  // reset the fopts of the sticky cmd receive a valide frame
                                                       // if received on RX1 or RX2
            // else reset the retransmission counter
            if( !( ( ( lr1_mac->rx_fctrl & 0x20 ) != 0x20 ) && ( lr1_mac->tx_mtype == CONF_DATA_UP ) ) )
            {
                // reset retransmission counter if received on RX1 or RX2 with
                lr1_mac->nb_trans_cpt = 1;
            }
            // test the ack bit when tx_mtype == CONF_DATA_UP
            if( ( ( lr1_mac->rx_fctrl & 0x20 ) == 0x20 ) && ( lr1_mac->tx_mtype == CONF_DATA_UP ) )
            {
                lr1_mac->rx_ack_bit = 1;
            }

            if( lr1_mac->rx_payload_empty == 0 )  // rx payload not empty
            {
                lr1_mac->rx_payload_size = lr1_mac->rx_payload_size - FHDROFFSET - lr1_mac->rx_fopts_length;
                /*
                     Receive a management frame
                     => set rx_packet_type = NWKRXPACKET
                     => if ack bit is set to one : notify the upper layer that the stack have received an ack bit =>
                   set available_app_packet to LORA_RX_PACKET_AVAILABLE with length = 0
                */
                if( lr1_mac->rx_fport == 0 )
                {  // receive a mac management frame without fopts
                    if( lr1_mac->rx_fopts_length == 0 )
                    {
                        payload_decrypt( &lr1_mac->rx_payload[FHDROFFSET], lr1_mac->rx_payload_size, lr1_mac->nwk_skey,
                                         lr1_mac->dev_addr, 1, lr1_mac->fcnt_dwn, &lr1_mac->nwk_payload[0] );
                        lr1_mac->nwk_payload_size = lr1_mac->rx_payload_size;
                        rx_packet_type            = NWKRXPACKET;
                    }
                    else
                    {
                        BSP_DBG_TRACE_WARNING( " Receive an not valid packet with fopt bytes on port zero\n" );
                    }
                }
                /*
                    Receive a app frame with size > 0
                    =>  if rx_fopts_length > 0 set rx_packet_type = USERRX_FOPTSPACKET and copy fopts data
                    =>  notify the upper layer that the stack have received a payload : set available_app_packet to
                   LORA_RX_PACKET_AVAILABLE with length > 0
                */
                else
                {
                    payload_decrypt( &lr1_mac->rx_payload[FHDROFFSET + lr1_mac->rx_fopts_length],
                                     lr1_mac->rx_payload_size, lr1_mac->app_skey, lr1_mac->dev_addr, 1,
                                     lr1_mac->fcnt_dwn, &lr1_mac->rx_payload[0] );
                    if( lr1_mac->rx_fopts_length != 0 )
                    {
                        memcpy( lr1_mac->nwk_payload, lr1_mac->rx_fopts, lr1_mac->rx_fopts_length );
                        lr1_mac->nwk_payload_size = lr1_mac->rx_fopts_length;
                        rx_packet_type            = USERRX_FOPTSPACKET;
                    }
                    lr1_mac->available_app_packet = LORA_RX_PACKET_AVAILABLE;
                }
            }
            /*
                Receive an empty user payload
                => if rx_fopts_length > 0 set rx_packet_type = USERRX_FOPTSPACKET and copy fopts data
                => notify the upper layer that the stack have received a payload : set available_app_packet to
               LORA_RX_PACKET_AVAILABLE with length = 0
            */
            else
            {
                if( lr1_mac->rx_fopts_length != 0 )
                {
                    memcpy( lr1_mac->nwk_payload, lr1_mac->rx_fopts, lr1_mac->rx_fopts_length );
                    lr1_mac->nwk_payload_size = lr1_mac->rx_fopts_length;
                    rx_packet_type            = USERRX_FOPTSPACKET;
                }
            }
        }
    }
    BSP_DBG_TRACE_PRINTF( " rx_packet_type = %d \n", rx_packet_type );
    return ( rx_packet_type );
}

void lr1_stack_mac_update( lr1_stack_mac_t* lr1_mac )
{
    lr1_mac->adr_ack_limit       = smtc_real_adr_ack_limit_get( lr1_mac );
    lr1_mac->adr_ack_delay       = smtc_real_adr_ack_delay_get( lr1_mac );
    lr1_mac->type_of_ans_to_send = NOFRAME_TOSEND;

    if( lr1_mac->join_status == NOT_JOINED )
    {
        // get current timestamp to check with duty cycle will be applied
        uint32_t current_time_s = bsp_rtc_get_time_s( );

        lr1_mac->retry_join_cpt++;

        if( current_time_s < ( lr1_mac->first_join_timestamp + 3600 ) )
        {
            // during first hour after first join try => duty cycle of 1/100 ie 36s over 1 hour
            lr1_mac->next_time_to_join_seconds =
                current_time_s + ( ( smtc_real_get_join_sf5_toa_in_ms( lr1_mac ) << ( lr1_mac->tx_sf - 5 ) ) ) / 10;
            // ts=cur_ts+(toa_s*100) = cur_ts + (toa_ms / 1000) * 100 = cur_ts + toa_ms/10
            // toa_ms is evaluated using the current sf and the theoretical value of a join @sf5. This method is very
            // conservative as the multiplication by 2^sf gives always an overvalued timing
        }
        else if( current_time_s < ( lr1_mac->first_join_timestamp + 36000 + 3600 ) )
        {
            // during the 10 hours following first hour after first join try =>duty cycle of 1/1000 ie 36s over 10 hours
            lr1_mac->next_time_to_join_seconds =
                current_time_s + ( ( smtc_real_get_join_sf5_toa_in_ms( lr1_mac ) << ( lr1_mac->tx_sf - 5 ) ) );
            // ts=cur_ts+(toa_s*1000) = cur_ts + (toa_ms / 1000) * 1000 = cur_ts + toa_ms
            // toa_ms is evaluated using the current sf and the theoretical value of a join @sf5. This method is very
            // conservative as the multiplication by 2^sf gives always an overvalued timing
        }
        else
        {
            // Following the first 11 hours after first join try => duty cycle of 1/10000 ie 8.7s over 24 hours
            lr1_mac->next_time_to_join_seconds =
                current_time_s + ( ( smtc_real_get_join_sf5_toa_in_ms( lr1_mac ) << ( lr1_mac->tx_sf - 5 ) ) ) * 10;
            // ts=cur_ts+(toa_s*10000) = cur_ts + (toa_ms / 1000) * 10000 = cur_ts + toa_ms*10
            // toa_ms is evaluated using the current sf and the theoretical value of a join @sf5. This method is very
            // conservative as the multiplication by 2^sf gives always an overvalued timing
        }
    }
    else
    {
        smtc_real_next_dr_get( lr1_mac );  // get next datarate mandatory for
                                           // lr1_mac->next_time_to_join_seconds  estimation
    }
    if( ( lr1_mac->adr_ack_cnt >= lr1_mac->adr_ack_limit ) &&
        ( lr1_mac->adr_ack_cnt <= ( lr1_mac->adr_ack_limit + lr1_mac->adr_ack_delay ) ) )
    {
        lr1_mac->adr_ack_req = 1;
    }

    if( ( lr1_mac->adr_ack_cnt < lr1_mac->adr_ack_limit ) ||
        ( lr1_mac->adr_ack_cnt > ( lr1_mac->adr_ack_limit + lr1_mac->adr_ack_delay ) ) )
    {
        lr1_mac->adr_ack_req = 0;
    }

    if( lr1_mac->adr_ack_cnt >= lr1_mac->adr_ack_limit + lr1_mac->adr_ack_delay )
    {
        smtc_real_dr_decrement( lr1_mac );
        if( lr1_mac->tx_data_rate_adr != smtc_real_min_dr_channel_get( lr1_mac ) )
        {
            lr1_mac->adr_ack_cnt = lr1_mac->adr_ack_limit;
        }
    }

    if( lr1_mac->adr_ack_cnt_confirmed_frame >= ADR_LIMIT_CONF_UP )
    {
        lr1_mac->adr_ack_cnt_confirmed_frame = 0;
        smtc_real_dr_decrement( lr1_mac );
    }
    if( ( lr1_mac->adr_ack_cnt + lr1_mac->adr_ack_cnt_confirmed_frame ) >= NO_RX_PACKET_CNT )
    {
        BSP_DBG_TRACE_ERROR( "Reach max tx frame without dl, ul unconf:%d, ul conf:%d\n", lr1_mac->adr_ack_cnt,
                             lr1_mac->adr_ack_cnt_confirmed_frame );
        bsp_mcu_handle_lr1mac_issue( );
    }
    if( lr1_mac->nb_trans_cpt <= 1 )
    {  // could also be set to 1 if receive valid ans
        lr1_mac->fcnt_up++;
        lr1_mac->nb_trans_cpt = 1;  // error case shouldn't exist
    }
    else
    {
        lr1_mac->type_of_ans_to_send = USRFRAME_TORETRANSMIT;
        lr1_mac->nb_trans_cpt--;
    }

    if( ( lr1_mac->tx_fopts_length + lr1_mac->tx_fopts_lengthsticky ) > 15 )
    {
        lr1_mac->nwk_ans_size = lr1_mac->tx_fopts_lengthsticky + lr1_mac->tx_fopts_length;
        memcpy( lr1_mac->nwk_ans, lr1_mac->tx_fopts_datasticky, lr1_mac->tx_fopts_lengthsticky );
        memcpy( lr1_mac->nwk_ans + lr1_mac->tx_fopts_lengthsticky, lr1_mac->tx_fopts_data, lr1_mac->tx_fopts_length );
        lr1_mac->type_of_ans_to_send = NWKFRAME_TOSEND;
    }
    else
    {
        lr1_mac->tx_fopts_current_length = lr1_mac->tx_fopts_lengthsticky + lr1_mac->tx_fopts_length;
        memcpy( lr1_mac->tx_fopts_current_data, lr1_mac->tx_fopts_datasticky, lr1_mac->tx_fopts_lengthsticky );
        memcpy( lr1_mac->tx_fopts_current_data + lr1_mac->tx_fopts_lengthsticky, lr1_mac->tx_fopts_data,
                lr1_mac->tx_fopts_length );
    }
    lr1_mac->tx_fopts_length = 0;

    switch( lr1_mac->type_of_ans_to_send )
    {
    case NOFRAME_TOSEND:

        break;
    case NWKFRAME_TOSEND: {
        status_lorawan_t status;

        status = smtc_real_is_valid_size( lr1_mac, lr1_mac->tx_data_rate, lr1_mac->nwk_ans_size );
        if( status != OKLORAWAN )
        {
            lr1_mac->nwk_ans_size =
                lr1_stack_mac_cmd_ans_cut( lr1_mac->nwk_ans, lr1_mac->nwk_ans_size,
                                           smtc_real_max_payload_size_get( lr1_mac, lr1_mac->tx_data_rate ) );
        }
        memcpy( &lr1_mac->tx_payload[FHDROFFSET], lr1_mac->nwk_ans, lr1_mac->nwk_ans_size );
        lr1_mac->app_payload_size = lr1_mac->nwk_ans_size;
        lr1_mac->tx_fport         = PORTNWK;
        lr1_mac->tx_mtype         = UNCONF_DATA_UP;  //@note Mtype have to be confirm
        lr1_stack_mac_tx_frame_build( lr1_mac );
        lr1_stack_mac_tx_frame_encrypt( lr1_mac );
    }
    break;
    case USERACK_TOSEND:

        break;
    }
}

uint8_t lr1_stack_mac_cmd_ans_cut( uint8_t* nwk_ans, uint8_t nwk_ans_size_in, uint8_t max_allowed_size )
{
    uint8_t* p_tmp = nwk_ans;
    uint8_t* p     = nwk_ans;

    while( p_tmp - nwk_ans < MIN( nwk_ans_size_in, max_allowed_size ) )
    {
        p_tmp += lr1mac_cmd_mac_ans_size[nwk_ans[p_tmp - nwk_ans]];

        if( ( p_tmp - nwk_ans ) <= max_allowed_size )
        {
            p = p_tmp;
        }
        else
        {
            break;
        }
    }

    return p - nwk_ans;  // New payload size
}

status_lorawan_t lr1_stack_mac_cmd_parse( lr1_stack_mac_t* lr1_mac )
{
    uint8_t          cmd_identifier;
    uint8_t          nb_link_adr_req = 0;
    status_lorawan_t status          = OKLORAWAN;
    lr1_mac->nwk_payload_index       = 0;
    lr1_mac->nwk_ans_size            = 0;
    lr1_mac->tx_fopts_length         = 0;
    lr1_mac->tx_fopts_lengthsticky   = 0;

    while( lr1_mac->nwk_payload_size > lr1_mac->nwk_payload_index )
    {  //@note MacNwkPayloadSize and lr1_mac->nwk_payload[0] are updated in
        // Parser's method

        if( lr1_mac->tx_fopts_length > 200 )
        {
            BSP_DBG_TRACE_WARNING( "too much cmd in the payload \n" );
            return ( ERRORLORAWAN );
        }
        cmd_identifier = lr1_mac->nwk_payload[lr1_mac->nwk_payload_index];
        switch( cmd_identifier )
        {
        case LINK_CHECK_ANS:

            link_check_parser( lr1_mac );
            break;
        case LINK_ADR_REQ:

            nb_link_adr_req = 0;
            /* extract the number of multiple link adr req specification in
             * LoRAWan1.0.2 */
            while(
                ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( nb_link_adr_req * LINK_ADR_REQ_SIZE )] ==
                  LINK_ADR_REQ ) &&
                ( lr1_mac->nwk_payload_index + ( nb_link_adr_req * LINK_ADR_REQ_SIZE ) < lr1_mac->nwk_payload_size ) )
            {
                nb_link_adr_req++;
            }
            link_adr_parser( lr1_mac, nb_link_adr_req );
            break;
        case DUTY_CYCLE_REQ:

            duty_cycle_parser( lr1_mac );  //@note send answer but do nothing

            break;
        case RXPARRAM_SETUP_REQ:

            rx_param_setup_parser( lr1_mac );

            break;
        case DEV_STATUS_REQ:

            dev_status_parser( lr1_mac );  //@note  Done but margin have no sense
                                           // tb implemented
            break;
        case NEW_CHANNEL_REQ:

            new_channel_parser( lr1_mac );

            break;
        case RXTIMING_SETUP_REQ:

            rx_timing_setup_parser( lr1_mac );

            break;
        case TXPARAM_SETUP_REQ:

            tx_param_setup_parser( lr1_mac );

            break;
        case DL_CHANNEL_REQ:

            dl_channel_parser( lr1_mac );

            break;
        default:
            lr1_mac->nwk_payload_size = 0;
            BSP_DBG_TRACE_MSG( " Illegal state in mac layer\n " );

            break;
        }
    }
    return ( status );
}
void lr1_stack_mac_join_request_build( lr1_stack_mac_t* lr1_mac )
{
    BSP_DBG_TRACE_ARRAY( "DevEUI", lr1_mac->dev_eui, 8 );
    BSP_DBG_TRACE_ARRAY( "appEUI", lr1_mac->app_eui, 8 );
    BSP_DBG_TRACE_ARRAY( "appKey", lr1_mac->app_key, 16 );
    lr1_mac->dev_nonce += 1;
    lr1_mac->tx_mtype     = JOIN_REQUEST;
    lr1_mac->nb_trans_cpt = 1;
    lr1_mac->nb_trans     = 1;
    mac_header_set( lr1_mac );
    for( int i = 0; i < 8; i++ )
    {
        lr1_mac->tx_payload[1 + i] = lr1_mac->app_eui[7 - i];
        lr1_mac->tx_payload[9 + i] = lr1_mac->dev_eui[7 - i];
    }
    lr1_mac->tx_payload[17]  = ( uint8_t )( ( lr1_mac->dev_nonce & 0x00FF ) );
    lr1_mac->tx_payload[18]  = ( uint8_t )( ( lr1_mac->dev_nonce & 0xFF00 ) >> 8 );
    lr1_mac->tx_payload_size = 19;
    uint32_t mic;
    //    FcntUp = 1;
    join_compute_mic( &lr1_mac->tx_payload[0], lr1_mac->tx_payload_size, lr1_mac->app_key, &mic );
    memcpy( &lr1_mac->tx_payload[lr1_mac->tx_payload_size], ( uint8_t* ) &mic, 4 );
    lr1_mac->tx_payload_size = lr1_mac->tx_payload_size + 4;
    smtc_real_memory_save( lr1_mac );  // to save devnonce
}

void lr1_stack_mac_join_accept( lr1_stack_mac_t* lr1_mac )
{
    uint8_t app_nonce[6];
    int     i;
    memcpy( app_nonce, &lr1_mac->rx_payload[1], 6 );
    join_compute_skeys( lr1_mac->app_key, app_nonce, lr1_mac->dev_nonce, lr1_mac->nwk_skey, lr1_mac->app_skey );
    if( lr1_mac->rx_payload_size > 13 )
    {  // cflist are presents
        for( i = 0; i < 16; i++ )
        {
            lr1_mac->cf_list[i] = lr1_mac->rx_payload[13 + i];
        }
        smtc_real_cflist_get( lr1_mac );
    }
    else
    {
        smtc_real_join_snapshot_channel_mask_init( lr1_mac );
    }
    lr1_mac->dev_addr = ( lr1_mac->rx_payload[7] + ( lr1_mac->rx_payload[8] << 8 ) + ( lr1_mac->rx_payload[9] << 16 ) +
                          ( lr1_mac->rx_payload[10] << 24 ) );
    lr1_mac->rx1_dr_offset = ( lr1_mac->rx_payload[11] & 0x70 ) >> 4;
    lr1_mac->rx2_data_rate = ( lr1_mac->rx_payload[11] & 0x0F );
    lr1_mac->rx1_delay_s   = lr1_mac->rx_payload[12];
    if( lr1_mac->rx1_delay_s == 0 )
    {
        lr1_mac->rx1_delay_s = 1;  // Lorawan standart define 0 such as a delay of 1
    }
    if( lr1_mac->rx1_delay_s > 15 )
    {
        lr1_mac->rx1_delay_s = 15;
    }

    lr1_mac->join_status = JOINED;

    lr1_stack_mac_session_init( lr1_mac );

    BSP_DBG_TRACE_PRINTF( " DevAddr= %lx\n", lr1_mac->dev_addr );
    BSP_DBG_TRACE_PRINTF( " MacRx1DataRateOffset= %d\n", lr1_mac->rx1_dr_offset );
    BSP_DBG_TRACE_PRINTF( " MacRx2DataRate= %d\n", lr1_mac->rx2_data_rate );
    BSP_DBG_TRACE_PRINTF( " MacRx1Delay= %d\n", lr1_mac->rx1_delay_s );
    BSP_DBG_TRACE_MSG( " Save In Flash After Join suceed \n" );
    // uint32_t c2 = bsp_rtc_get_time_ms();
    // BSP_DBG_TRACE_PRINTF ("GET Time %d Join duration =%d\n",c2,c2-c1 );
}

uint8_t lr1_stack_mac_min_dr_get( lr1_stack_mac_t* lr1_mac )
{
    return smtc_real_min_dr_channel_get( lr1_mac );
}
uint8_t lr1_stack_mac_max_dr_get( lr1_stack_mac_t* lr1_mac )
{
    return smtc_real_max_dr_channel_get( lr1_mac );
}
void lr1_stack_rx1_join_delay_set( lr1_stack_mac_t* lr1_mac )
{
    lr1_mac->rx1_delay_s = smtc_real_rx1_join_delay_get( lr1_mac );
}
void lr1_stack_rx2_join_dr_set( lr1_stack_mac_t* lr1_mac )
{
    lr1_mac->rx2_data_rate = smtc_real_rx2_join_dr_get( lr1_mac );
}

int32_t lr1_stack_network_next_free_duty_cycle_ms_get( lr1_stack_mac_t* lr1_mac )
{
    int32_t  time_off_left = 0;
    uint32_t delta_t       = 0;

    if( lr1_mac->tx_duty_cycle_time_off_ms > 0 )
    {
        uint32_t rtc_now = bsp_rtc_get_time_ms( );
        if( rtc_now >= lr1_mac->tx_duty_cycle_timestamp_ms )
        {
            delta_t = rtc_now - lr1_mac->tx_duty_cycle_timestamp_ms;
        }
        else
        {
            delta_t = 0xFFFFFFFFUL - lr1_mac->tx_duty_cycle_timestamp_ms;
            delta_t += rtc_now;
        }

        if( delta_t > lr1_mac->tx_duty_cycle_time_off_ms )
        {
            time_off_left = 0;
        }
        else
        {
            time_off_left = lr1_mac->tx_duty_cycle_time_off_ms - delta_t;
        }
    }
    return time_off_left;
}

/*
 *-----------------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITIONS ------------------------------------------------
 */

static void mac_header_set( lr1_stack_mac_t* lr1_mac )
{
    lr1_mac->tx_payload[0] = ( ( lr1_mac->tx_mtype & 0x7 ) << 5 ) + ( lr1_mac->tx_major_bits & 0x3 );
}

static void frame_header_set( lr1_stack_mac_t* lr1_mac )
{
    lr1_mac->tx_payload[1] = ( uint8_t )( ( lr1_mac->dev_addr & 0x000000FF ) );
    lr1_mac->tx_payload[2] = ( uint8_t )( ( lr1_mac->dev_addr & 0x0000FF00 ) >> 8 );
    lr1_mac->tx_payload[3] = ( uint8_t )( ( lr1_mac->dev_addr & 0x00FF0000 ) >> 16 );
    lr1_mac->tx_payload[4] = ( uint8_t )( ( lr1_mac->dev_addr & 0xFF000000 ) >> 24 );
    lr1_mac->tx_payload[5] = lr1_mac->tx_fctrl;
    lr1_mac->tx_payload[6] = ( uint8_t )( ( lr1_mac->fcnt_up & 0x000000FF ) );
    lr1_mac->tx_payload[7] = ( uint8_t )( ( lr1_mac->fcnt_up & 0x0000FF00 ) >> 8 );
    for( int i = 0; i < lr1_mac->tx_fopts_current_length; i++ )
    {
        lr1_mac->tx_payload[8 + i] = lr1_mac->tx_fopts_current_data[i];
    }
    lr1_mac->tx_payload[8 + lr1_mac->tx_fopts_current_length] = lr1_mac->tx_fport;
}

static valid_dev_addr_t check_dev_addr( lr1_stack_mac_t* lr1_mac, uint32_t devAddr_to_test )
{
    valid_dev_addr_t status = UNVALID_DEV_ADDR;
    if( devAddr_to_test == lr1_mac->dev_addr )
    {
        status = VALID_DEV_ADDR_UNICAST;
    }
    return status;
}

static void compute_rx_window_parameters( lr1_stack_mac_t* lr1_mac, uint8_t sf, lr1mac_bandwidth_t bw,
                                          uint32_t clock_accuracy, uint32_t rx_delay_ms, uint8_t board_delay_ms,
                                          modulation_type_t rx_modulation_type )
{
    // ClockAccuracy is set in Define.h, it is board dependent. It must be equal
    // to error in per thousand

    int      bw_temp = 125;
    uint32_t rx_error_ms =
        ( clock_accuracy * rx_delay_ms ) /
        1000;  // for example with an clockaccuracy = 30 (3%)  and a rx windows set to 5s => rxerror = 150 ms
    double   tsymbol        = 0.0;
    uint8_t  min_rx_symbols = 6;
    uint16_t rx_window_symb;

    if( rx_modulation_type == LORA )
    {
        // Use lr1mac_utilities_get_symb_time_us
        switch( bw )
        {
        case BW125:
            bw_temp = 125;
            break;
        case BW250:
            bw_temp = 250;
            break;
        case BW500:
            bw_temp = 500;
            break;
        case BW800:
            bw_temp = 800;
            break;
        case BW1600:
            bw_temp = 1600;
            break;
        default:
            bw_temp = 125;
            break;
        }
        tsymbol        = ( double ) ( 1 << sf ) / ( double ) bw_temp;
        rx_window_symb = ( uint16_t )(
            MAX( ( 2 * min_rx_symbols - 8 ) + ( ( 2 * rx_error_ms * bw_temp ) >> sf ) + 1, min_rx_symbols ) );
    }
    else
    {                                              // FSK
        tsymbol        = ( 8.0 / ( double ) sf );  // 1 symbol equals 1 byte
        rx_window_symb = ( uint16_t )( MAX( ( 2 * min_rx_symbols - 8 ) + ( ( 2 * rx_error_ms * sf ) >> 3 ) + 1,
                                            min_rx_symbols ) );  // Computed number of symbols
    }

    lr1_mac->rx_offset_ms =
        ( int32_t )( ( ceil( ( 4.0 * tsymbol ) - ( ( rx_window_symb * tsymbol ) / 2.0 ) - board_delay_ms ) ) * ( -1 ) );

    lr1_mac->rx_window_symb = rx_window_symb;
    lr1_mac->rx_timeout_ms  = ceil( rx_window_symb * tsymbol );
}

static status_lorawan_t rx_payload_size_check( lr1_stack_mac_t* lr1_mac )
{
    status_lorawan_t status = OKLORAWAN;
    if( lr1_mac->rx_payload_size < MIN_LORAWAN_PAYLOAD_SIZE )
    {
        status = ERRORLORAWAN;
        BSP_DBG_TRACE_ERROR( " ERROR CheckRxPayloadLength = %d \n", lr1_mac->rx_payload_size );
        return ( status );
    }
    return ( status );
}

static status_lorawan_t rx_mhdr_extract( lr1_stack_mac_t* lr1_mac )
{
    status_lorawan_t status = OKLORAWAN;
    lr1_mac->rx_mtype       = lr1_mac->rx_payload[0] >> 5;
    lr1_mac->rx_major       = lr1_mac->rx_payload[0] & 0x3;
    if( ( lr1_mac->rx_mtype == JOIN_REQUEST ) || ( lr1_mac->rx_mtype == UNCONF_DATA_UP ) ||
        ( lr1_mac->rx_mtype == CONF_DATA_UP ) || ( lr1_mac->rx_mtype == REJOIN_REQUEST ) )
    {
        status = ERRORLORAWAN;
        BSP_DBG_TRACE_MSG( " BAD RX MHDR\n " );
    }
    lr1_mac->tx_ack_bit = ( lr1_mac->rx_mtype == CONF_DATA_DOWN ) ? 1 : 0;

    return ( status );
}

static int rx_fhdr_extract( lr1_stack_mac_t* lr1_mac, uint16_t* fcnt_dwn_tmp, uint32_t dev_addr )
{
    int      status       = OKLORAWAN;
    uint32_t dev_addr_tmp = 0;
    dev_addr_tmp = lr1_mac->rx_payload[1] + ( lr1_mac->rx_payload[2] << 8 ) + ( lr1_mac->rx_payload[3] << 16 ) +
                   ( lr1_mac->rx_payload[4] << 24 );
    status            = ( dev_addr_tmp == dev_addr ) ? OKLORAWAN : ERRORLORAWAN;
    lr1_mac->rx_fctrl = lr1_mac->rx_payload[5];

    *fcnt_dwn_tmp            = lr1_mac->rx_payload[6] + ( lr1_mac->rx_payload[7] << 8 );
    lr1_mac->rx_fopts_length = lr1_mac->rx_fctrl & 0x0F;
    memcpy( &lr1_mac->rx_fopts[0], &lr1_mac->rx_payload[8], lr1_mac->rx_fopts_length );
    // case empty payload without fport :
    if( lr1_mac->rx_payload_size > 8 + MICSIZE + lr1_mac->rx_fopts_length )
    {
        lr1_mac->rx_fport         = lr1_mac->rx_payload[8 + lr1_mac->rx_fopts_length];
        lr1_mac->rx_payload_empty = 0;
    }
    else
    {
        lr1_mac->rx_payload_empty = 1;
        BSP_DBG_TRACE_MSG( " EMPTY MSG \n" );
    }
    /**************************/
    /* manage Fctrl Byte      */
    /**************************/
    if( status == ERRORLORAWAN )
    {
        BSP_DBG_TRACE_ERROR( " ERROR Bad DevAddr %lx\n ", dev_addr_tmp );
    }
    return ( status );
}

static int fcnt_dwn_accept( uint16_t fcnt_dwn_tmp, uint32_t* fcnt_lorawan )
{
    int      status       = OKLORAWAN;
    uint16_t fcnt_dwn_lsb = ( *fcnt_lorawan & 0x0000FFFF );
    uint32_t fcnt_dwn_msb = ( *fcnt_lorawan & 0xFFFF0000 );
    if( ( fcnt_dwn_tmp > fcnt_dwn_lsb ) || ( *fcnt_lorawan == 0xFFFFFFFF ) )
    {
        if( *fcnt_lorawan == 0xFFFFFFFF )  // manage the case of the first downlink with fcnt down = 0
        {
            *fcnt_lorawan = fcnt_dwn_tmp;
        }
        else
        {
            *fcnt_lorawan = fcnt_dwn_msb + fcnt_dwn_tmp;
        }
    }
    else if( ( fcnt_dwn_lsb - fcnt_dwn_tmp ) > MAX_FCNT_GAP )
    {
        *fcnt_lorawan = fcnt_dwn_msb + ( 1UL << 16 ) + fcnt_dwn_tmp;
    }
    else
    {
        status = ERRORLORAWAN;
        BSP_DBG_TRACE_PRINTF(
            " ERROR FcntDwn is not acceptable fcntDwnReceive = %u "
            "fcntLoraStack = %lu\n",
            fcnt_dwn_tmp, ( *fcnt_lorawan ) );
    }
    return ( status );
}

/************************************************************************************************/
/*                    Private NWK MANAGEMENTS Methods */
/************************************************************************************************/
static void link_check_parser( lr1_stack_mac_t* lr1_mac )
{
    BSP_DBG_TRACE_PRINTF( " Margin = %d , GwCnt = %d \n", lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1],
                          lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 2] );
    lr1_mac->nwk_payload_index += LINK_CHECK_ANS_SIZE;
}
/**********************************************************************************************************************/
/*                                               Private NWK MANAGEMENTS :
 * LinkADR                                    */
/*  Note : describe multiple adr specification */
/*                                                                                                                    */
/*  Step 1 : Create a "unwrapped channel mask" in case of multiple adr cmd with
 * both Channem Mask and ChannnelMaskCntl*/
/*       2 : Extract from the last adr cmd datarate candidate */
/*       3 : Extract from the last adr cmd TxPower candidate */
/*       4 : Extract from the last adr cmd NBRetry candidate */
/*       5 : Check errors cases (described below) */
/*       6 : If No error Set new channel mask, txpower,datarate and nbretry */
/*       7 : Compute duplicated LinkAdrAns */
/*                                                                                                                    */
/*  Error cases    1 : Channel Cntl mask RFU for each adr cmd (in case of
 * multiple cmd)                               */
/*                 2 : Undefined channel ( freq = 0 ) for active bit in the
 * unwrapped channel mask                    */
/*                 3 : Unwrapped channel mask = 0 (none active channel) */
/*                 4 : For the last adr cmd not valid tx power */
/*                 5 : For the last adr cmd not valid datarate */
/*                     ( datarate > dRMax or datarate < dRMin for all active
 * channel )                                */
/**********************************************************************************************************************/

static void link_adr_parser( lr1_stack_mac_t* lr1_mac, uint8_t nb_link_adr_req )
{
    for( uint8_t i = 0; i < nb_link_adr_req; i++ )
    {
        BSP_DBG_TRACE_PRINTF( "%u - Cmd link_adr_parser = %02x %02x %02x %02x \n", i,
                              lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( i * LINK_ADR_REQ_SIZE ) + 1],
                              lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( i * LINK_ADR_REQ_SIZE ) + 2],
                              lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( i * LINK_ADR_REQ_SIZE ) + 3],
                              lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( i * LINK_ADR_REQ_SIZE ) + 4] );
    }
    status_lorawan_t status        = OKLORAWAN;
    status_channel_t statusChannel = OKCHANNEL;
    uint8_t          status_ans    = 0x7;  // initialised for ans answer ok
    uint8_t          ChMAstCntlTemp;
    uint16_t         channel_mask_temp = 0;
    uint8_t          dr_tmp;
    uint8_t          tx_power_tmp;
    uint8_t          nb_trans_tmp;

    for( uint8_t i = 0; i < nb_link_adr_req; i++ )
    {
        channel_mask_temp = lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( i * LINK_ADR_REQ_SIZE ) + 2] +
                            ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( i * LINK_ADR_REQ_SIZE ) + 3] << 8 );
        ChMAstCntlTemp =
            ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( i * LINK_ADR_REQ_SIZE ) + 4] & 0x70 ) >> 4;
        BSP_DBG_TRACE_PRINTF( "%u - MULTIPLE LINK ADR REQ , channel mask = 0x%x , ChMAstCntl = 0x%x\n", i,
                              channel_mask_temp, ChMAstCntlTemp );
        statusChannel = smtc_real_channel_mask_build( lr1_mac, ChMAstCntlTemp, channel_mask_temp );
        if( statusChannel == ERROR_CHANNEL_CNTL )
        {  // Test ChannelCNTL not defined
            status_ans &= 0x6;
            BSP_DBG_TRACE_WARNING( "INVALID CHANNEL CNTL \n" );
        }
    }
    /* Valid global channel mask  */
    if( statusChannel == ERROR_CHANNEL_MASK )
    {  // Test Channelmask enables a not defined channel or Channelmask = 0
        status_ans &= 0x6;
        BSP_DBG_TRACE_WARNING( "INVALID CHANNEL MASK \n" );
    }
    /* At This point global temporary channel mask is built and validated */
    /* Valid the last DataRate */
    dr_tmp =
        ( ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( ( nb_link_adr_req - 1 ) * LINK_ADR_REQ_SIZE ) + 1] &
            0xF0 ) >>
          4 );
    status = smtc_real_is_acceptable_dr( lr1_mac, dr_tmp );
    if( status == ERRORLORAWAN )
    {  // Test Channelmask enables a not defined channel
        status_ans &= 0x5;
        BSP_DBG_TRACE_WARNING( "INVALID DATARATE \n" );
    }

    /* Valid the last TxPower  And Prepare Ans */
    tx_power_tmp =
        ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( ( nb_link_adr_req - 1 ) * LINK_ADR_REQ_SIZE ) + 1] &
          0x0F );
    status = smtc_real_is_valid_tx_power( lr1_mac, tx_power_tmp );
    if( status == ERRORLORAWAN )
    {  // Test tx power
        status_ans &= 0x3;
        BSP_DBG_TRACE_WARNING( "INVALID TXPOWER \n" );
    }

    nb_trans_tmp =
        ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + ( ( nb_link_adr_req - 1 ) * LINK_ADR_REQ_SIZE ) + 4] &
          0x0F );

    /* Update the mac parameters if case of no error */

    if( status_ans == 0x7 )
    {
        smtc_real_channel_mask_set( lr1_mac );
        smtc_real_power_set( lr1_mac, tx_power_tmp );
        lr1_mac->nb_trans         = nb_trans_tmp;
        lr1_mac->tx_data_rate_adr = dr_tmp;
        BSP_DBG_TRACE_PRINTF( "MacTxDataRateAdr = %d\n", lr1_mac->tx_data_rate_adr );
        BSP_DBG_TRACE_PRINTF( "MacTxPower = %d\n", lr1_mac->tx_power );
        BSP_DBG_TRACE_PRINTF( "MacNbTrans = %d\n", lr1_mac->nb_trans );
    }

    /* Prepare repeated Ans*/
    for( uint8_t i = 0; i < nb_link_adr_req; i++ )
    {
        lr1_mac->tx_fopts_data[lr1_mac->tx_fopts_length + ( i * LINK_ADR_ANS_SIZE )]     = LINK_ADR_ANS;  // copy Cid
        lr1_mac->tx_fopts_data[lr1_mac->tx_fopts_length + ( i * LINK_ADR_ANS_SIZE ) + 1] = status_ans;
    }
    lr1_mac->nwk_payload_index += ( nb_link_adr_req * LINK_ADR_REQ_SIZE );
    lr1_mac->tx_fopts_length += ( nb_link_adr_req * LINK_ADR_ANS_SIZE );
}

/**********************************************************************************************************************/
/*                                                 Private NWK MANAGEMENTS :
 * rx_param_setup_parser                       */
/**********************************************************************************************************************/

static void rx_param_setup_parser( lr1_stack_mac_t* lr1_mac )
{
    BSP_DBG_TRACE_PRINTF(
        " Cmd rx_param_setup_parser = %x %x %x %x \n", lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1],
        lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 2], lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 3],
        lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 4] );
    int      status     = OKLORAWAN;
    uint8_t  status_ans = 0x7;  // initialised for ans answer ok
    uint8_t  rx1_dr_offset_temp;
    uint8_t  rx2_dr_temp;
    uint32_t rx2_frequency_temp;
    /* Valid Rx1DrOffset And Prepare Ans */
    rx1_dr_offset_temp = ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1] & 0x70 ) >> 4;
    status             = smtc_real_is_valid_rx1_dr_offset( lr1_mac, rx1_dr_offset_temp );

    if( status == ERRORLORAWAN )
    {
        status_ans &= 0x6;
        BSP_DBG_TRACE_MSG( "INVALID RX1DROFFSET \n" );
    }

    /* Valid MacRx2Dr And Prepare Ans */
    status      = OKLORAWAN;
    rx2_dr_temp = ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1] & 0x0F );
    status      = smtc_real_is_valid_dr( lr1_mac, rx2_dr_temp );
    if( status == ERRORLORAWAN )
    {
        status_ans &= 0x5;
        BSP_DBG_TRACE_MSG( "INVALID RX2DR \n" );
    }

    /* Valid MacRx2Frequency And Prepare Ans */
    status = OKLORAWAN;

    rx2_frequency_temp =
        smtc_real_decode_freq_from_buf( lr1_mac, &lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 2] );

    status = smtc_real_is_valid_rx_frequency( lr1_mac, rx2_frequency_temp );
    if( status == ERRORLORAWAN )
    {
        status_ans &= 0x3;
        BSP_DBG_TRACE_MSG( "INVALID RX2 FREQUENCY \n" );
    }

    /* Update the mac parameters if case of no error */

    if( status_ans == 0x7 )
    {
        lr1_mac->rx1_dr_offset = rx1_dr_offset_temp;
        lr1_mac->rx2_data_rate = rx2_dr_temp;
        lr1_mac->rx2_frequency = rx2_frequency_temp;
        BSP_DBG_TRACE_PRINTF( "MacRx1DataRateOffset = %d\n", lr1_mac->rx1_dr_offset );
        BSP_DBG_TRACE_PRINTF( "MacRx2DataRate = %d\n", lr1_mac->rx2_data_rate );
        BSP_DBG_TRACE_PRINTF( "MacRx2Frequency = %lu\n", lr1_mac->rx2_frequency );
    }
    /* Prepare Ans*/

    lr1_mac->tx_fopts_datasticky[lr1_mac->tx_fopts_lengthsticky]     = RXPARRAM_SETUP_ANS;
    lr1_mac->tx_fopts_datasticky[lr1_mac->tx_fopts_lengthsticky + 1] = status_ans;
    lr1_mac->tx_fopts_lengthsticky += RXPARRAM_SETUP_ANS_SIZE;
    lr1_mac->nwk_payload_index += RXPARRAM_SETUP_REQ_SIZE;
}

/**********************************************************************************************************************/
/*                                                 Private NWK MANAGEMENTS :
 * duty_cycle_parser                          */
/**********************************************************************************************************************/

static void duty_cycle_parser( lr1_stack_mac_t* lr1_mac )
{
    BSP_DBG_TRACE_PRINTF( "Cmd duty_cycle_parser %x \n", lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1] );
    lr1_mac->max_duty_cycle_index = ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1] & 0x0F );

    /* Prepare Ans*/
    lr1_mac->tx_fopts_data[lr1_mac->tx_fopts_length] = DUTY_CYCLE_ANS;  // copy Cid
    lr1_mac->tx_fopts_length += DUTY_CYCLE_ANS_SIZE;
    lr1_mac->nwk_payload_index += DUTY_CYCLE_REQ_SIZE;
}
/**********************************************************************************************************************/
/*                                                 Private NWK MANAGEMENTS :
 * dev_status_parser                          */
/**********************************************************************************************************************/

static void dev_status_parser( lr1_stack_mac_t* lr1_mac )
{
    uint8_t my_hook_id;
    rp_hook_get_id( lr1_mac->rp, lr1_mac, &my_hook_id );
    BSP_DBG_TRACE_MSG( "Receive a dev status req\n" );
    lr1_mac->tx_fopts_data[lr1_mac->tx_fopts_length]     = DEV_STATUS_ANS;  // copy Cid
    lr1_mac->tx_fopts_data[lr1_mac->tx_fopts_length + 1] = bsp_mcu_get_battery_level( );
    lr1_mac->tx_fopts_data[lr1_mac->tx_fopts_length + 2] =
        ( lr1_mac->rp->radio_params[my_hook_id].rx.lora_pkt_status.snr_pkt_in_db ) & 0x3F;
    lr1_mac->tx_fopts_length += DEV_STATUS_ANS_SIZE;
    lr1_mac->nwk_payload_index += DEV_STATUS_REQ_SIZE;
}
/**********************************************************************************************************************/
/*                                                 Private NWK MANAGEMENTS :
 * new_channel_parser                         */
/**********************************************************************************************************************/
static void new_channel_parser( lr1_stack_mac_t* lr1_mac )
{
    BSP_DBG_TRACE_PRINTF(
        " Cmd new_channel_parser = %x %x %x %x %x \n", lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1],
        lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 2], lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 3],
        lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 4], lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 5] );
    int      status     = OKLORAWAN;
    uint8_t  status_ans = 0x3;  // initialized for ans answer ok
    uint8_t  channel_index_temp;
    uint8_t  dr_range_max_temp;
    uint8_t  dr_range_min_temp;
    uint32_t frequency_temp;
    /* Valid Channel Index */
    channel_index_temp = lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1];
    status             = smtc_real_is_valid_channel_index( lr1_mac, channel_index_temp );
    if( status == ERRORLORAWAN )
    {
        status_ans &= 0x0;
        BSP_DBG_TRACE_MSG( "INVALID CHANNEL INDEX \n" );
    }
    /* Valid Frequency  */
    frequency_temp = smtc_real_decode_freq_from_buf( lr1_mac, &lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 2] );
    status         = smtc_real_is_valid_tx_frequency( lr1_mac, frequency_temp );
    if( status == ERRORLORAWAN )
    {
        status_ans &= 0x2;
        BSP_DBG_TRACE_MSG( "INVALID FREQUENCY\n" );
    }
    /* Valid DRMIN/MAX */
    dr_range_min_temp = lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 5] & 0xF;
    status            = smtc_real_is_valid_dr( lr1_mac, dr_range_min_temp );
    if( status == ERRORLORAWAN )
    {
        status_ans &= 0x1;
        BSP_DBG_TRACE_MSG( "INVALID DR MIN \n" );
    }
    dr_range_max_temp = ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 5] & 0xF0 ) >> 4;
    status            = smtc_real_is_valid_dr( lr1_mac, dr_range_max_temp );
    if( status == ERRORLORAWAN )
    {
        status_ans &= 0x1;
        BSP_DBG_TRACE_MSG( "INVALID DR MAX \n" );
    }
    if( dr_range_max_temp < dr_range_min_temp )
    {
        status_ans &= 0x1;
        BSP_DBG_TRACE_MSG( "INVALID DR MAX < DR MIN \n" );
    }

    /* Update the mac parameters if case of no error */

    if( status_ans == 0x3 )
    {
        smtc_real_tx_frequency_channel_set( lr1_mac, frequency_temp, channel_index_temp );
        smtc_real_rx1_frequency_channel_set( lr1_mac, frequency_temp, channel_index_temp );
        smtc_real_min_dr_channel_set( lr1_mac, dr_range_min_temp, channel_index_temp );
        smtc_real_max_dr_channel_set( lr1_mac, dr_range_max_temp, channel_index_temp );
        if( frequency_temp == 0 )
        {
            smtc_real_channel_enabled_set( lr1_mac, CHANNEL_DISABLED, channel_index_temp );
        }
        else
        {
            smtc_real_channel_enabled_set( lr1_mac, CHANNEL_ENABLED, channel_index_temp );
        }
        BSP_DBG_TRACE_PRINTF( "MacTxFrequency [ %d ] = %lu\n", channel_index_temp,
                              smtc_real_tx_frequency_channel_get( lr1_mac, channel_index_temp ) );
    }

    /* Prepare Ans*/
    lr1_mac->tx_fopts_data[lr1_mac->tx_fopts_length]     = NEW_CHANNEL_ANS;  // copy Cid
    lr1_mac->tx_fopts_data[lr1_mac->tx_fopts_length + 1] = status_ans;
    lr1_mac->tx_fopts_length += NEW_CHANNEL_ANS_SIZE;
    lr1_mac->nwk_payload_index += NEW_CHANNEL_REQ_SIZE;
}
/*********************************************************************************************************************/
/*                                                 Private NWK MANAGEMENTS :
 * rx_timing_setup_parser                     */
/*********************************************************************************************************************/

static void rx_timing_setup_parser( lr1_stack_mac_t* lr1_mac )
{
    BSP_DBG_TRACE_PRINTF( "Cmd rx_timing_setup_parser = %x \n", lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1] );
    lr1_mac->rx1_delay_s = ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1] & 0xF );
    if( lr1_mac->rx1_delay_s == 0 )
    {
        lr1_mac->rx1_delay_s = 1;  // Lorawan standart define 0 such as a delay of 1
    }

    /* Prepare Ans*/
    lr1_mac->tx_fopts_datasticky[lr1_mac->tx_fopts_lengthsticky] = RXTIMING_SETUP_ANS;
    lr1_mac->tx_fopts_lengthsticky += RXTIMING_SETUP_ANS_SIZE;
    lr1_mac->nwk_payload_index += RXTIMING_SETUP_REQ_SIZE;
}

/*********************************************************************************************************************/
/*                                                 Private NWK MANAGEMENTS :
 * tx_param_setup_parser                  */
/*********************************************************************************************************************/

static void tx_param_setup_parser( lr1_stack_mac_t* lr1_mac )
{
    BSP_DBG_TRACE_PRINTF( "Cmd tx_param_setup_parser = %x \n", lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1] );

    lr1_mac->max_eirp_dbm =
        smtc_real_max_eirp_dbm_from_idx[( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1] & 0x0F )];
    lr1_mac->uplink_dwell_time =
        ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1] & 0x10 ) >> 4;  // TODO set but not yet used in stack
    lr1_mac->downlink_dwell_time =
        ( lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1] & 0x20 ) >> 5;  // TODO set but not yet used in stack

    /* Prepare Ans*/
    lr1_mac->tx_fopts_datasticky[lr1_mac->tx_fopts_lengthsticky] = TXPARAM_SETUP_ANS;  // copy Cid
    lr1_mac->tx_fopts_lengthsticky += TXPARAM_SETUP_ANS_SIZE;
    lr1_mac->nwk_payload_index += TXPARAM_SETUP_REQ_SIZE;
}

/*********************************************************************************************************************/
/*                                                 Private NWK MANAGEMENTS :
 * dl_channel_parser                        */
/*********************************************************************************************************************/

static void dl_channel_parser( lr1_stack_mac_t* lr1_mac )
{
    BSP_DBG_TRACE_PRINTF(
        "Cmd dl_channel_parser = %x %x %x %x  \n", lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1],
        lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 2], lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 3],
        lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 4] );
    int      status     = OKLORAWAN;
    uint8_t  status_ans = 0x3;  // initialised for ans answer ok
    uint8_t  channel_index_temp;
    uint32_t frequency_temp;
    /* Valid Channel Index */
    channel_index_temp = lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 1];
    if( smtc_real_tx_frequency_channel_get( lr1_mac, channel_index_temp ) == 0 )
    {
        status_ans &= 0x1;
        BSP_DBG_TRACE_MSG( "INVALID CHANNEL INDEX \n" );
    }
    /* Valid Frequency  */
    frequency_temp = smtc_real_decode_freq_from_buf( lr1_mac, &lr1_mac->nwk_payload[lr1_mac->nwk_payload_index + 2] );
    status         = smtc_real_is_valid_rx_frequency( lr1_mac, frequency_temp );
    if( status == ERRORLORAWAN )
    {
        status_ans &= 0x2;
        BSP_DBG_TRACE_MSG( "INVALID FREQUENCY\n" );
    }
    /* Update the mac parameters if case of no error */
    if( status_ans == 0x3 )
    {
        smtc_real_rx1_frequency_channel_set( lr1_mac, frequency_temp, channel_index_temp );
        BSP_DBG_TRACE_PRINTF( "MacRx1Frequency [ %u ] = %lu\n", channel_index_temp,
                              smtc_real_rx1_frequency_channel_get( lr1_mac, channel_index_temp ) );
    }

    /* Prepare Ans*/
    lr1_mac->tx_fopts_datasticky[lr1_mac->tx_fopts_lengthsticky]     = DL_CHANNEL_ANS;
    lr1_mac->tx_fopts_datasticky[lr1_mac->tx_fopts_lengthsticky + 1] = status_ans;
    lr1_mac->tx_fopts_lengthsticky += DL_CHANNEL_ANS_SIZE;
    lr1_mac->nwk_payload_index += DL_CHANNEL_REQ_SIZE;
}
