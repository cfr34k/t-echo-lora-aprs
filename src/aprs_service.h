/** @file
 *
 * @defgroup aprs_service APRS Service Server
 * @{
 * @ingroup ble_sdk_srv
 *
 * @brief APRS Service Server module.
 *
 * @details This module implements a custom APRS Service. It allows to set some
 *          fields of transmitted APRS packets (e.g. source call sign, comment)
 *          and to read received messages.
 */

#ifndef APRS_SERVICE_H__
#define APRS_SERVICE_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"
#include "nrf_saadc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@brief   Macro for defining a aprs_service instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define APRS_SERVICE_DEF(_name)                                                                          \
static aprs_service_t _name;                                                                             \
NRF_SDH_BLE_OBSERVER(_name ## _obs,                                                             \
                     APP_BLE_OBSERVER_PRIO,                                                     \
                     aprs_service_on_ble_evt, &_name)

#define APRS_SERVICE_UUID_BASE               { 0x00, 0x9e, 0x5c, 0x94, 0x82, 0x46, 0x6a, 0x2a, \
                                               0x5d, 0xbb, 0x93, 0xb4, 0x00, 0x00, 0x00, 0x00}
#define APRS_SERVICE_UUID_SERVICE            0x0001
#define APRS_SERVICE_UUID_MYCALL             0x0101      // Call sign of the user
#define APRS_SERVICE_UUID_COMMENT            0x0102      // Comment
#define APRS_SERVICE_UUID_RX_MESSAGE         0x0103      // The last received message

// Forward declaration of the aprs_service_t type.
typedef struct aprs_service_s aprs_service_t;

typedef enum {
	APRS_SERVICE_EVT_MYCALL_CHANGED,
	APRS_SERVICE_EVT_COMMENT_CHANGED,
} aprs_service_evt_t;

/**@brief Callback function type.
 *
 * @param evt       The event type.
 */
typedef void (*aprs_service_callback_t)(aprs_service_evt_t evt);

/** @brief Service init structure. This structure contains all options and data needed for
 *         initialization of the service.*/
typedef struct
{
	aprs_service_callback_t callback; /**< Pointer to the callback function */
} aprs_service_init_t;

/**@brief Service structure. This structure contains the service's internal state. */
struct aprs_service_s
{
	uint16_t                    service_handle;               /**< Handle of Service (as provided by the BLE stack). */
	ble_gatts_char_handles_t    mycall_char_handles;          /**< Handles related to the My Call Characteristic. */
	ble_gatts_char_handles_t    comment_char_handles;         /**< Handles related to the Comment Characteristic. */
	ble_gatts_char_handles_t    rx_message_char_handles;      /**< Handles related to the RX Message Characteristic. */
	uint8_t                     uuid_type;                    /**< UUID type for the APRS Service. */
	aprs_service_callback_t     callback;                     /**< Pointer to the callback function. */
};


/**@brief Function for initializing the Service.
 *
 * @param[out] p_srv      Service structure. This structure must be supplied by
 *                        the application. It is initialized by this function and will later
 *                        be used to identify this particular service instance.
 * @param[in] p_srv_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was initialized successfully. Otherwise, an error code is returned.
 */
uint32_t aprs_service_init(aprs_service_t * p_srv, const aprs_service_init_t * p_srv_init);


/**@brief Function for handling the application's BLE stack events.
 *
 * @details This function handles all events from the BLE stack that are of interest to the Service.
 *
 * @param[in] p_ble_evt  Event received from the BLE stack.
 * @param[in] p_context  Service structure (as returned by aprs_service_init()).
 */
void aprs_service_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);


/**@brief Get the current user call sign.
 *
 * @param[in]  p_srv      Service structure (as returned by aprs_service_init()).
 * @param[out] p_mycall   Pointer to the buffer where the call sign is written.
 * @param[in]  mycall_len Size of the buffer in p_mycall.
 * @returns               The result code from the BLE stack.
 */
ret_code_t aprs_service_get_mycall(aprs_service_t * p_srv, char *p_mycall, uint8_t mycall_len);


/**@brief Get the current user comment.
 *
 * @param[in]  p_srv       Service structure (as returned by aprs_service_init()).
 * @param[out] p_comment   Pointer to the buffer where the comment is written.
 * @param[in]  comment_len Size of the buffer in p_comment.
 * @returns                The result code from the BLE stack.
 */
ret_code_t aprs_service_get_comment(aprs_service_t * p_srv, char *p_comment, uint8_t comment_len);


/**@brief Set the received message and send a notification.
 *
 * @param[in]  p_srv       Service structure (as returned by aprs_service_init()).
 * @param[in]  p_message   Pointer to the message.
 * @param[in]  message_len Size of the message.
 * @returns                The result code from the BLE stack.
 */
ret_code_t aprs_service_notify_rx_message(aprs_service_t * p_srv, uint16_t conn_handle, uint8_t *p_message, uint8_t message_len);


#ifdef __cplusplus
}
#endif

#endif // APRS_SERVICE_H__

/** @} */
