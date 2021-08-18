/*!
 * \file      modem_supervisor.h
 *
 * \brief     soft modem task scheduler
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

#ifndef __MODEM_SUPERVISOR__H
#define __MODEM_SUPERVISOR__H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */
#include "radio_planner.h"

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC CONSTANTS --------------------------------------------------------
 */

#define DM_PERIOD_AFTER_JOIN 10
#define MODEM_TASK_DELAY_MS 200
#define MODEM_MAX_TIME 0x1FFFFF
#define CALL_LR1MAC_PERIOD_MS 400
#define MODEM_MAX_ALARM_S 0x7FFFFFFF
/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */

/*!
 * \typedef task_id_t
 * \brief   Descriptor of all the tasks mange by the supervisor
 */
typedef enum
{
    SEND_TASK,          //!< task managed by the application such as sensor uplink for example
    SEND_AT_TIME_TASK,  //!< not used
    JOIN_TASK,          //!< task mange by the modem itself to join a network
    DM_TASK,            //!< task mange by the modem itself to report periodically status
    DM_TASK_NOW,        //!< task mange by the modem when requested by the host or the cloud to report status
    FILE_UPLOAD_TASK,  //!< task initiate by the application layer but manage by the modem itself to transfer "big file"
    IDLE_TASK,         //!< mean no more active task schedule
    MUTE_TASK,         //!< task managed by the modem to un-mute the modem
    RETRIEVE_DL_TASK,  //!< task managed by the modem to create downlink opportunities
    STREAM_TASK,  //!< task initiated by the application layer, but managed by the modem itself to transfer long streams
    ALC_SYNC_TIME_REQ_TASK,  //!< task managed by the modem to launch Application Layer Clock Synchronisation
    ALC_SYNC_ANS_TASK,       //!< task managed by the modem to launch Application Layer Clock Synchronisation answer
    NUMBER_OF_TASKS          //!< number of tasks

} task_id_t;

/*!
 * \typedef eTask_priority
 * \brief   Descriptor of priorities for task
 */
typedef enum
{
    TASK_VERY_HIGH_PRIORITY,    //!< Very high priority, RESERVED for Emergency Tx only
    TASK_HIGH_PRIORITY,         //!< High priority
    TASK_MEDIUM_HIGH_PRIORITY,  //!< Medium priority
    TASK_LOW_PRIORITY,          //!< Low priority
    TASK_FINISH,                //!< task finished
} eTask_priority;

/*!
 * \typedef eTask_valid_t
 * \brief   task valid or note
 */
typedef enum eTask_valid
{
    TASK_VALID,     //!< Task valid
    TASK_NOT_VALID  //!< Task not valid
} eTask_valid_t;

/*!
 * \typedef smodem_task
 * \brief   Supervisor task description
 */
typedef struct smodem_task
{
    task_id_t      id;                 //!< Type ID of the task
    uint32_t       time_to_execute_s;  //!< The date to execute the task in second
    eTask_priority priority;           //!< The priority
    uint8_t        fPort;              //!< LoRaWAN frame port
    const uint8_t* dataIn;             //!< Data in task
    uint8_t        sizeIn;             //!< Data length in byte(s)
    uint8_t        PacketType;         //!< LoRaWAN packet type ( Tx confirmed/Unconfirmed )
} smodem_task;

/*!
 * \typedef stask_manager
 * \brief   Supervisor task manager
 */
typedef struct stask_manager
{
    smodem_task modem_task[NUMBER_OF_TASKS];
    task_id_t   current_task_id;
    task_id_t   next_task_id;
    uint32_t    sleep_duration;

} stask_manager;

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

/*!
 * \brief   Supervisor Initialization
 * \remark
 * \param [in]  callback*   - pointer to the callback
 * \param [in]  rp*         - pointer to the radio planner
 * \retval None
 */
void modem_supervisor_init( void ( *callback )( void ), radio_planner_t* rp );

/*!
 * \brief Supervisor Engine
 * \retval return the maximum delay in ms at which time the engine MUST be recalled
 */
uint32_t modem_supervisor_engine( void );

/*!
 * \brief   Init all task to Idle
 * \retval none
 */
void init_task( void );

/*!
 * \brief   Remove a task in supervisor
 * \param [in]  id   - Task id
 * \retval eTask_valid_t
 */
eTask_valid_t modem_supervisor_remove_task( task_id_t id );

/*!
 * \brief   Add a task in supervisor
 * \remark
 * \param task*  smodem_task
 * \retval eTask_valid_t
 */
eTask_valid_t modem_supervisor_add_task( smodem_task* task );

#ifdef __cplusplus
}
#endif

#endif  //__MODEM_SUPERVISOR__H
