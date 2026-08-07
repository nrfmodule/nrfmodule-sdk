/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLE advertising wire-format codecs: tracker v2, BeeScales, Ingics iBS05.
 * Pure: no Bluetooth stack, no kernel calls.
 */

#include <nrfmodule_adv_codec.h>

#include <errno.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>

/* Every payload this codec reads starts with a 2-byte LE company id. */
#define ADV_OFFSET_COMPANY_ID (0)
#define ADV_COMPANY_ID_LEN    (2)

/* ---- Tracker v2 --------------------------------------------------------- */

#define TRACKER_OFFSET_PROTO_VERSION (2)
#define TRACKER_OFFSET_PRODUCT_ID    (3)
#define TRACKER_OFFSET_BATTERY       (4)
#define TRACKER_OFFSET_STATUS        (5)
#define TRACKER_OFFSET_FLAGS         (6)
#define TRACKER_OFFSET_SATS          (7)
#define TRACKER_OFFSET_LAT           (8)
#define TRACKER_OFFSET_LON           (12)
#define TRACKER_OFFSET_NEXT_WAKE     (16)
#define TRACKER_OFFSET_LAST_ERROR    (20)

size_t nrfmodule_tracker_adv_encode(const struct nrfmodule_tracker_adv_data *data,
				     uint8_t *buf, size_t buf_len)
{
	if (data == NULL || buf == NULL || buf_len < NRFMODULE_TRACKER_ADV_PAYLOAD_LEN) {
		return 0;
	}

	uint8_t flags = 0;

	flags |= data->gps_fix ? NRFMODULE_TRACKER_ADV_FLAG_GPS_FIX : 0;
	flags |= data->lte_connected ? NRFMODULE_TRACKER_ADV_FLAG_LTE_CONN : 0;
	flags |= data->charging ? NRFMODULE_TRACKER_ADV_FLAG_CHARGING : 0;
	flags |= data->low_battery ? NRFMODULE_TRACKER_ADV_FLAG_LOW_BATT : 0;
	flags |= data->button_event ? NRFMODULE_TRACKER_ADV_FLAG_BUTTON : 0;
	flags |= (uint8_t)((data->wake_kind << NRFMODULE_TRACKER_ADV_WAKE_KIND_SHIFT) &
			    NRFMODULE_TRACKER_ADV_WAKE_KIND_MASK);

	sys_put_le16(NRFMODULE_ADV_COMPANY_ID_NRFMODULE, &buf[ADV_OFFSET_COMPANY_ID]);
	buf[TRACKER_OFFSET_PROTO_VERSION] = NRFMODULE_TRACKER_ADV_PROTO_VERSION;
	buf[TRACKER_OFFSET_PRODUCT_ID] = data->product_id;
	buf[TRACKER_OFFSET_BATTERY] = data->battery_percent;
	buf[TRACKER_OFFSET_STATUS] = data->status;
	buf[TRACKER_OFFSET_FLAGS] = flags;
	buf[TRACKER_OFFSET_SATS] = data->gps_satellites;
	sys_put_le32((uint32_t)data->latitude_e7, &buf[TRACKER_OFFSET_LAT]);
	sys_put_le32((uint32_t)data->longitude_e7, &buf[TRACKER_OFFSET_LON]);
	sys_put_le32(data->next_wake_unix, &buf[TRACKER_OFFSET_NEXT_WAKE]);
	buf[TRACKER_OFFSET_LAST_ERROR] = data->last_error;

	return NRFMODULE_TRACKER_ADV_PAYLOAD_LEN;
}

int nrfmodule_tracker_adv_decode(const uint8_t *buf, size_t buf_len,
				  struct nrfmodule_tracker_adv_data *out)
{
	if (buf == NULL || out == NULL || buf_len < NRFMODULE_TRACKER_ADV_PAYLOAD_LEN) {
		return -EINVAL;
	}
	if (sys_get_le16(&buf[ADV_OFFSET_COMPANY_ID]) != NRFMODULE_ADV_COMPANY_ID_NRFMODULE) {
		return -EINVAL;
	}

	const uint8_t protocol_version = buf[TRACKER_OFFSET_PROTO_VERSION];

	if (protocol_version < NRFMODULE_TRACKER_ADV_PROTO_VERSION) {
		return -EINVAL;
	}

	const uint8_t flags = buf[TRACKER_OFFSET_FLAGS];

	memset(out, 0, sizeof(*out));
	out->product_id = buf[TRACKER_OFFSET_PRODUCT_ID];
	out->battery_percent = buf[TRACKER_OFFSET_BATTERY];
	out->status = buf[TRACKER_OFFSET_STATUS];
	out->gps_fix = (flags & NRFMODULE_TRACKER_ADV_FLAG_GPS_FIX) != 0;
	out->lte_connected = (flags & NRFMODULE_TRACKER_ADV_FLAG_LTE_CONN) != 0;
	out->charging = (flags & NRFMODULE_TRACKER_ADV_FLAG_CHARGING) != 0;
	out->low_battery = (flags & NRFMODULE_TRACKER_ADV_FLAG_LOW_BATT) != 0;
	out->button_event = (flags & NRFMODULE_TRACKER_ADV_FLAG_BUTTON) != 0;
	out->wake_kind = (uint8_t)((flags & NRFMODULE_TRACKER_ADV_WAKE_KIND_MASK) >>
				    NRFMODULE_TRACKER_ADV_WAKE_KIND_SHIFT);
	out->gps_satellites = buf[TRACKER_OFFSET_SATS];
	out->latitude_e7 = (int32_t)sys_get_le32(&buf[TRACKER_OFFSET_LAT]);
	out->longitude_e7 = (int32_t)sys_get_le32(&buf[TRACKER_OFFSET_LON]);
	out->next_wake_unix = sys_get_le32(&buf[TRACKER_OFFSET_NEXT_WAKE]);
	out->last_error = buf[TRACKER_OFFSET_LAST_ERROR];

	return 0;
}

/* ---- BeeScales ------------------------------------------------------------ */

#define BEESCALES_OFFSET_RESERVED        (2)
#define BEESCALES_OFFSET_TYPE_FIELD      (4)
#define BEESCALES_OFFSET_BATTERY         (6)
#define BEESCALES_OFFSET_WEIGHT          (8)
#define BEESCALES_OFFSET_DELTA_TODAY     (10)
#define BEESCALES_OFFSET_DELTA_YESTERDAY (12)
#define BEESCALES_OFFSET_TEMP_1          (14)
#define BEESCALES_OFFSET_TEMP_2          (16)
#define BEESCALES_OFFSET_TIMESTAMP       (18)

int nrfmodule_beescales_adv_decode(const uint8_t *buf, size_t buf_len,
				    struct nrfmodule_beescales_adv_data *out)
{
	if (buf == NULL || out == NULL || buf_len < NRFMODULE_BEESCALES_ADV_PAYLOAD_MIN_LEN) {
		return -EINVAL;
	}
	if (sys_get_le16(&buf[ADV_OFFSET_COMPANY_ID]) != NRFMODULE_ADV_COMPANY_ID_NRFMODULE) {
		return -EINVAL;
	}
	if (sys_get_le16(&buf[BEESCALES_OFFSET_RESERVED]) != NRFMODULE_BEESCALES_ADV_RESERVED) {
		return -EINVAL;
	}

	memset(out, 0, sizeof(*out));
	out->type_field = sys_get_le16(&buf[BEESCALES_OFFSET_TYPE_FIELD]);
	out->battery_percent = (int16_t)sys_get_le16(&buf[BEESCALES_OFFSET_BATTERY]);
	out->weight_10g = (int16_t)sys_get_le16(&buf[BEESCALES_OFFSET_WEIGHT]);
	out->delta_today_10g = (int16_t)sys_get_le16(&buf[BEESCALES_OFFSET_DELTA_TODAY]);
	out->delta_yesterday_10g = (int16_t)sys_get_le16(&buf[BEESCALES_OFFSET_DELTA_YESTERDAY]);
	out->temperature_1_cdegc = (int16_t)sys_get_le16(&buf[BEESCALES_OFFSET_TEMP_1]);
	out->temperature_2_cdegc = (int16_t)sys_get_le16(&buf[BEESCALES_OFFSET_TEMP_2]);
	out->unix_timestamp = (int32_t)sys_get_le32(&buf[BEESCALES_OFFSET_TIMESTAMP]);

	return 0;
}

/* ---- Ingics iBS05 ----------------------------------------------------------- */

#define IBS05_OFFSET_CODE_AND_TYPE (2)
#define IBS05_OFFSET_BATTERY       (4)
#define IBS05_OFFSET_EVENT_TYPE    (6)
#define IBS05_OFFSET_TEMPERATURE   (7)
#define IBS05_OFFSET_SUB_TYPE      (13)

int nrfmodule_ibs05_adv_decode(const uint8_t *buf, size_t buf_len,
				struct nrfmodule_ibs05_adv_data *out)
{
	if (buf == NULL || out == NULL || buf_len < NRFMODULE_IBS05_ADV_PAYLOAD_MIN_LEN) {
		return -EINVAL;
	}
	if (sys_get_le16(&buf[ADV_OFFSET_COMPANY_ID]) != NRFMODULE_ADV_COMPANY_ID_INGICS) {
		return -EINVAL;
	}
	if (sys_get_le16(&buf[IBS05_OFFSET_CODE_AND_TYPE]) != NRFMODULE_IBS05_CODE_AND_TYPE) {
		return -EINVAL;
	}
	if (buf[IBS05_OFFSET_SUB_TYPE] != NRFMODULE_IBS05_SUB_TYPE_SENSOR) {
		return -EINVAL;
	}

	memset(out, 0, sizeof(*out));
	out->battery_cv = (int16_t)sys_get_le16(&buf[IBS05_OFFSET_BATTERY]);
	out->temperature_cdegc = (int16_t)sys_get_le16(&buf[IBS05_OFFSET_TEMPERATURE]);
	out->button_pressed = (buf[IBS05_OFFSET_EVENT_TYPE] == NRFMODULE_IBS05_EVENT_BUTTON);

	return 0;
}

/* ---- Top-level classify/decode ---------------------------------------------- */

/* Disambiguates and decodes the two layouts sharing company 0x0E34. Split out
 * of nrfmodule_adv_decode() so that switch case does not need its own braced
 * scope for locals.
 */
static int decode_nrfmodule_company(const uint8_t *payload, size_t payload_len,
				     struct nrfmodule_adv_decoded *out)
{
	if (payload_len <= NRFMODULE_ADV_DISAMBIGUATOR_OFFSET) {
		return -ENOTSUP;
	}

	const uint8_t disambiguator = payload[NRFMODULE_ADV_DISAMBIGUATOR_OFFSET];

	if (disambiguator == NRFMODULE_ADV_DISAMBIGUATOR_BEESCALES) {
		if (nrfmodule_beescales_adv_decode(payload, payload_len, &out->data.beescales) !=
		    0) {
			return -ENOTSUP;
		}
		out->type = NRFMODULE_ADV_DEVICE_BEESCALES;
		return 0;
	}
	if (disambiguator >= NRFMODULE_TRACKER_ADV_PROTO_VERSION) {
		if (nrfmodule_tracker_adv_decode(payload, payload_len, &out->data.tracker) != 0) {
			return -ENOTSUP;
		}
		out->type = NRFMODULE_ADV_DEVICE_TRACKER;
		return 0;
	}

	return -ENOTSUP;
}

int nrfmodule_adv_decode(uint16_t company_id, const uint8_t *payload, size_t payload_len,
			  struct nrfmodule_adv_decoded *out)
{
	if (payload == NULL || out == NULL || payload_len < ADV_COMPANY_ID_LEN) {
		return -EINVAL;
	}

	out->type = NRFMODULE_ADV_DEVICE_UNKNOWN;

	if (sys_get_le16(&payload[ADV_OFFSET_COMPANY_ID]) != company_id) {
		return -EINVAL;
	}

	switch (company_id) {
	case NRFMODULE_ADV_COMPANY_ID_NRFMODULE:
		return decode_nrfmodule_company(payload, payload_len, out);
	case NRFMODULE_ADV_COMPANY_ID_INGICS:
		if (nrfmodule_ibs05_adv_decode(payload, payload_len, &out->data.ibs05) != 0) {
			return -ENOTSUP;
		}
		out->type = NRFMODULE_ADV_DEVICE_IBS05;
		return 0;
	default:
		return -ENOTSUP;
	}
}
