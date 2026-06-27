#pragma once

namespace eka2l1::epoc::sms {
    enum {
        SMS_ADDRESS_FAMILY = 0x10,
        SMS_DTG_PROTOCOL = 0x02,
        SMS_MAX_SOCKS = 0x100,
        SMS_MAX_DTG_SIZE = 255 * 160
    };

    enum ioctl {
        SMS_IOCTL_PROVIDER = 0x100,
        SMS_IOCTL_DELETE_MSG = 0x300,
        SMS_IOCTL_ENUMERATE_MSG = 0x301,
        SMS_IOCTL_READ_MSG_SUCCESS = 0x304,
        SMS_IOCTL_READ_MSG_FAILED = 0x305,
        SMS_IOCTL_SEND_SMS = 0x306,
        SMS_IOCTL_WRITE_SMS_TO_SIM = 0x307,
        SMS_IOCTL_READ_PARAMS = 0x308,
        SMS_IOCTL_READ_PARAMS_COMPLETE = 0x309,
        SMS_IOCTL_WRITE_SMS_PARAMS = 0x310,
        SMS_IOCTL_SUPPORT_OOD_CLASS0_SMS = 0x311,
        SMS_IOCTL_SELECT_MODEM_PRESENT = 0x400,
        SMS_IOCTL_SELECT_MODEM_NOT_PRESENT = 0x401,
    };

    enum sms_pid_conversion {
        sms_pid_conversion_none = 0
    };

    enum sms_encoding_alphabet {
        sms_encoding_alphabet_ucs2 = 0x8
    };

    enum sms_time_validity_period_format {
        sms_vpf_integer = 0
    };

    enum sms_delivery {
        sms_delivery_immediately = 0
    };

    enum sms_report_handling {
        sms_report_to_inbox_visible_and_match = 0
    };

    enum sms_commdb_action {
        sms_commdb_no_action = 0
    };

    enum mobile_sms_bearer {
        mobile_sms_bearer_packet_preferred = 0
    };

    enum sms_recipient_status {
        recipient_status_not_yet_sent = 0
    };

    enum sms_acknowledge_status {
        sms_no_acknowledge = 0
    };

    struct sms_number {
        std::u16string name_;
        std::u16string number_;
        sms_recipient_status recp_status_;
        std::int32_t error_;
        std::int32_t retries_;
        std::int64_t time_;
        std::int32_t log_id_;
        sms_acknowledge_status delivery_status_;

        explicit sms_number();
        void absorb(common::chunkyseri &seri);
    };
}