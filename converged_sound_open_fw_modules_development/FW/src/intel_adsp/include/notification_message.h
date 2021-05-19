// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Intel Corporation. All rights reserved.
/*! \file notification_message.h */


#ifndef _ADSP_NOTIFICATION_H_
#define _ADSP_NOTIFICATION_H_

#include "system_service.h"
#include <stdint.h>


namespace intel_adsp
{
/*!
 * \brief The NotificationMessage class provides a generic implementation for FDK module notifications
 * - NotificationMessage<> object should be declared in module private members for VOICE_COMMAND_NOTIFICATION
 *   and AUDIO_CLASSIFIER_RESULTS notifications.
 * - For MODULE_EVENT_NOTIFICATION, the ModuleNotificationMessage<> class should be declared instead.
 *
 * \tparam MAX_DATA_SIZE is the maximum size of notification data. It shall be lesser or equal to DSP_HW_LENGTH_IPC_OUT_MAILBOX=4096.
 */
template <size_t MAX_DATA_SIZE>
class NotificationMessage
{

public:

    /*! \brief GetDataBuffer() returns a pointer to the buffer where to write the generic notification message
     *
     * The NotificationMessage::GetDataBuffer() function is the generic implementation for all notifications.
     * - It can be called directly for VOICE_COMMAND_NOTIFICATION and AUDIO_CLASSIFIER_RESULTS.
     * - For MODULE_EVENT_NOTIFICATION, the ModuleNotificationMessage::GetNotification() has to be called instead.
     *
     * \param notification_type Type of the notification.
     * \param notification_id ID of the notification message to be sent.
     * \param notification_data_size Size of the notification message to be sent.
     * \param system_service Reference to the system_service (retrieved using GetSystemService() in module).
     * \param user_val_1 Optional parameter that must be filled only for VOICE_COMMAND_NOTIFICATION.
     * \param user_val_2 Optional parameter that must be filled only for VOICE_COMMAND_NOTIFICATION.
     *
     * \return pointer to where to store the notification data. NULL pointer is returned in case of error.
     */
    uint8_t* GetDataBuffer(NotificationType notification_type,
                            uint32_t notification_id,
                            size_t notification_data_size,
                            AdspSystemService const& system_service,
                            uint16_t user_val_1 = 0,
                            uint32_t user_val_2 = 0)
    {
        if (notification_data_size > MAX_DATA_SIZE)
            return NULL;
        //Note: additionaly, notification_data_size must be < DSP_HW_LENGTH_IPC_OUT_MAILBOX=4096
        //      This is checked in NotificationCreate.

        notification_size_ = notification_data_size;

        NotificationParams params;
        params.type = notification_type;
        params.user_val_1 = user_val_1;
        params.user_val_2 = user_val_2;
        params.max_payload_size = notification_size_;
        params.payload = NULL;// params.payload is updated in NotificationCreate

        notification_handle_ = NULL;
        AdspErrorCode erc = system_service.NotificationCreate(&params,
            reinterpret_cast<uint8_t*>(&buffer_),
            NOTIFICATION_HEADER_SIZE + MAX_DATA_SIZE,
            &notification_handle_);

        //payload can't be NULL, even if max_payload_size = 0
        //-> if max_payload_size = 0, payload will point to end of notification header in the notification buffer
        if ((erc != ADSP_NO_ERROR) ||
            (params.payload == NULL))
        {
            return NULL;
        }

        uint32_t data_offset = 0;
        if (notification_type == MODULE_EVENT_NOTIFICATION)
        {
            ModuleEventNotification* module_event = (ModuleEventNotification*)params.payload;
            // module_event->module_instance_id is already filled by NotificationCreate
            module_event->event_id = notification_id;
            module_event->event_data_size = notification_data_size - sizeof(ModuleEventNotification);
            data_offset = sizeof(ModuleEventNotification);
        }

        return &params.payload[data_offset];
    }

    /*! \brief Send() sends the notification message
     *
     * The NotificationMessage::Send() function is the generic implementation for all notifications.
     * - It can be called directly for all notifications.
     *
     * \param system_service Reference to the system_service (retrieved using GetSystemService() in module).
     */
    void Send(AdspSystemService const& system_service)
    {
        system_service.NotificationSend(DSP_TO_HOST, notification_handle_, notification_size_);
    }

private:
    /* Notification buffer:
    MAX_DATA_SIZE comes from template. It is equal to:
        MAX_USERDATA_SIZE + sizeof(ModuleEventNotification) in case of MODULE_EVENT_NOTIFICATION.
        MAX_USERDATA_SIZE for other notifications types.
    Whole Size of the buffer cannot excess NOTIFICATION_HEADER_SIZE + DSP_HW_LENGTH_IPC_OUT_MAILBOX (checked in NotificationCreate).*/
    DCACHE_ALIGNED uint8_t buffer_[NOTIFICATION_HEADER_SIZE + MAX_DATA_SIZE];
    size_t notification_size_;

protected:
    AdspNotificationHandle notification_handle_;

};


/*!
 * \brief The ModuleNotificationMessage class provides a specific implementation for FDK MODULE_EVENT_NOTIFICATION.
 *
 * Usage for notifications:
 * - 1) Declare notification_message_ of templated type ModuleNotificationMessage in private members of module:<br>
 *      ModuleNotificationMessage<sizeof(UserDefinedNotification)> notification_message_
 * - 2) Retrieve pointer on notification_data with ModuleNotificationMessage::GetNotification():<br>
 *      UserDefinedNotification* notification_data = notification_message_.GetNotification<UserDefinedNotification>(user_notification_id,GetSystemService())
 * - 3) Fill notification_data with user values
 * - 4) Send notification with ModuleNotificationMessage::Send():<br>
 *      notification_message_.Send(GetSystemService())
 *
 * \tparam MAX_USERDATA_SIZE is the maximum size of notification data. It shall be lesser or
 *         equal to DSP_HW_LENGTH_IPC_OUT_MAILBOX - sizeof(ModuleEventNotification) = 4096 - 12 = 4084
 */
template <size_t MAX_USERDATA_SIZE>
class ModuleNotificationMessage : public NotificationMessage<sizeof(ModuleEventNotification) + MAX_USERDATA_SIZE>
{

public:
    /*!
     * \brief Local type definition to create a NotificationMessage object of size (sizeof(ModuleEventNotification) + MAX_USERDATA_SIZE).
     * \note internal purpose.
     */
    typedef NotificationMessage<sizeof(ModuleEventNotification) + MAX_USERDATA_SIZE> BaseType;

    /*! \brief GetDataBuffer() returns a pointer to the buffer where to write the Module notification message
     *
     * The ModuleNotificationMessage::GetDataBuffer() function is called only for MODULE_EVENT_NOTIFICATION.
     * It will then call the NotificationMessage::GetDataBuffer() generic implementation.
     *
     * \param notification_id ID of the notification message to be sent.
     * \param notification_data_size Size of the notification message to be sent.
     * \param system_service Reference to the system_service (retrieved using GetSystemService() in module).
     *
     * \return pointer to where to store the notification data
     */
    uint8_t* GetDataBuffer(uint32_t notification_id, size_t notification_data_size, AdspSystemService const& system_service)
    {  return BaseType::GetDataBuffer(MODULE_EVENT_NOTIFICATION, notification_id, sizeof(ModuleEventNotification) + notification_data_size, system_service); }

    /*! \brief GetNotification() returns a pointer to the buffer where to write the user defined notification.
     *
     * The ModuleNotificationMessage::GetNotification() function is called only for MODULE_EVENT_NOTIFICATION.
     * It will then call the ModuleNotificationMessage::GetDataBuffer().
     *
     * \tparam USER_DEFINED_NOTIFICATION is the type of the user notification (user type locally defined in user's module)
     * \param notification_id ID of the notification message to be sent.
     * \param system_service Reference to the system_service (retrieved using GetSystemService() in module).
     *
     * \return pointer to where to store the user defined notification data
     */
    template<class USER_DEFINED_NOTIFICATION>
    USER_DEFINED_NOTIFICATION* GetNotification(uint32_t notification_id, AdspSystemService const& system_service)
    { return reinterpret_cast<USER_DEFINED_NOTIFICATION*>(GetDataBuffer(notification_id, sizeof(USER_DEFINED_NOTIFICATION), system_service)); }
};

} // namespace intel_adsp


#endif // _ADSP_NOTIFICATION_H_
