/*
 * Copyright (c) 2026 nRFModule
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRFMODULE_ADV_CODEC_H_
#define NRFMODULE_ADV_CODEC_H_

/**
 * @file
 * @brief BLE advertising wire-format codecs: nRFModule tracker v2, BeeScales,
 *        Ingics iBS05.
 *
 * Three manufacturer-data layouts, two of them sharing company id 0x0E34:
 * - Tracker v2 status broadcast (encode + decode). Normative spec and golden
 *   vectors: nRFTrackerFW docs/ble/advertising.md.
 * - BeeScales hive-scale broadcast (decode only). Carried in the SCAN
 *   RESPONSE, not the primary advertising PDU.
 * - Ingics iBS05 temperature beacon (decode only), company id 0x082C.
 *
 * Buffer convention, picked to match every source this module was derived
 * from (tracker_adv.h documents its 21-byte payload this way; the BeeScales
 * adv_mfg_data_type struct starts with company_code the same way): every
 * function below reads or writes the manufacturer-data AD element VALUE in
 * full, company id first -- exactly what Zephyr's bt_data_parse() hands a
 * BT_DATA_MANUFACTURER_DATA callback. Some scanner APIs (Android, iOS, some
 * Python BLE libraries) pre-strip the 2-byte company id before handing you
 * the rest of the value; re-prepend it before calling into this codec.
 *
 * Pure code: no Bluetooth stack dependency, no kernel services. Safe to unit
 * test off-target.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/sys/util.h>

/** BLE company identifiers this codec recognizes (payload offset 0-1, LE). */
#define NRFMODULE_ADV_COMPANY_ID_NRFMODULE (0x0E34) /* nRFModule: tracker v2, BeeScales */
#define NRFMODULE_ADV_COMPANY_ID_INGICS    (0x082C) /* Ingics beacons: iBS05 */

/** Device types nrfmodule_adv_decode() can classify a payload as. */
enum nrfmodule_adv_device_type {
	NRFMODULE_ADV_DEVICE_UNKNOWN = 0,   /**< Not recognized; out->data is untouched. */
	NRFMODULE_ADV_DEVICE_TRACKER,       /**< out->data.tracker is valid. */
	NRFMODULE_ADV_DEVICE_BEESCALES,     /**< out->data.beescales is valid. */
	NRFMODULE_ADV_DEVICE_IBS05,         /**< out->data.ibs05 is valid. */
};

/* ------------------------------------------------------------------------
 * Tracker v2 status broadcast (company 0x0E34, protocol_version >= 0x02)
 * ------------------------------------------------------------------------ */

#define NRFMODULE_TRACKER_ADV_PROTO_VERSION   (0x02) /* current wire protocol_version */
#define NRFMODULE_TRACKER_ADV_PAYLOAD_LEN     (21)   /* full element value, company id incl. */
#define NRFMODULE_TRACKER_ADV_BATTERY_UNKNOWN (0xFF)

/** Registered product_id values (wire offset 3). */
enum nrfmodule_tracker_adv_product {
	NRFMODULE_TRACKER_ADV_PRODUCT_LIVETRACKER   = 0x04,
	NRFMODULE_TRACKER_ADV_PRODUCT_PIGEONTRACKER = 0x05,
};

/* event_flags bits (wire offset 6). Bits 5-6 are wake_kind; bit 7 reserved. */
enum nrfmodule_tracker_adv_flag {
	NRFMODULE_TRACKER_ADV_FLAG_GPS_FIX  = BIT(0),
	NRFMODULE_TRACKER_ADV_FLAG_LTE_CONN = BIT(1),
	NRFMODULE_TRACKER_ADV_FLAG_CHARGING = BIT(2),
	NRFMODULE_TRACKER_ADV_FLAG_LOW_BATT = BIT(3),
	NRFMODULE_TRACKER_ADV_FLAG_BUTTON   = BIT(4),
};

#define NRFMODULE_TRACKER_ADV_WAKE_KIND_SHIFT (5)
#define NRFMODULE_TRACKER_ADV_WAKE_KIND_MASK  (0x03U << NRFMODULE_TRACKER_ADV_WAKE_KIND_SHIFT)

/** Labels what next_wake_unix (wire offset 16) refers to. */
enum nrfmodule_tracker_adv_wake_kind {
	NRFMODULE_TRACKER_ADV_WAKE_KIND_NONE  = 0, /**< next_wake_unix carries nothing. */
	NRFMODULE_TRACKER_ADV_WAKE_KIND_BEGIN = 1, /**< scheduled race start (startAt). */
	NRFMODULE_TRACKER_ADV_WAKE_KIND_SLEEP = 2, /**< end of a duty-cycle sleep. */
	NRFMODULE_TRACKER_ADV_WAKE_KIND_HUB   = 3, /**< reserved: hub-commanded sleep. */
};

/** Wire values for last_error (wire offset 20). */
enum nrfmodule_tracker_adv_error {
	NRFMODULE_TRACKER_ADV_ERR_NONE        = 0, /**< No fault currently active. */
	NRFMODULE_TRACKER_ADV_ERR_GPS_TIMEOUT = 1, /**< GPS acquisition cycle ended without a fix. */
	NRFMODULE_TRACKER_ADV_ERR_LTE_REG     = 2, /**< LTE registration failed or timed out. */
	NRFMODULE_TRACKER_ADV_ERR_UPLOAD      = 3, /**< Server upload attempt failed. */
	NRFMODULE_TRACKER_ADV_ERR_STORAGE     = 4, /**< Local data-queue write failed. */
};

/** Decoded/encoded fields of one tracker v2 status broadcast. */
struct nrfmodule_tracker_adv_data {
	uint8_t  product_id;      /**< enum nrfmodule_tracker_adv_product. */
	uint8_t  battery_percent; /**< 0-100 percent, or NRFMODULE_TRACKER_ADV_BATTERY_UNKNOWN. */
	uint8_t  status;          /**< tracker_state leaf value; unknown values are valid states,
				    *   not decode errors (see the product's status registry).
				    */
	uint8_t  gps_satellites;  /**< satellites in view. */
	int32_t  latitude_e7;     /**< degrees * 1e7. */
	int32_t  longitude_e7;    /**< degrees * 1e7. */
	uint32_t next_wake_unix;  /**< unix epoch seconds; 0 = none, or clock not synced. */
	uint8_t  wake_kind;       /**< enum nrfmodule_tracker_adv_wake_kind. */
	uint8_t  last_error;      /**< enum nrfmodule_tracker_adv_error. */
	bool     gps_fix;         /**< last GPS acquisition cycle got a fix. */
	bool     lte_connected;   /**< registered on the LTE network. */
	bool     charging;        /**< external power present on VBUS (cable/dock fact). */
	bool     low_battery;     /**< battery at or below the low threshold. */
	bool     button_event;    /**< reserved; always false in protocol_version 0x02. */
};

/**
 * @brief Encode a tracker v2 status broadcast.
 *
 * Produces the exact bytes the tracker firmware puts on the air: a
 * NRFMODULE_TRACKER_ADV_PAYLOAD_LEN-byte manufacturer-data element value,
 * company id first.
 *
 * The size_t-returning, 0-on-failure shape deliberately mirrors the
 * application's own tracker_adv_encode() (nRFTrackerFW
 * src/advertising/tracker_adv.c) rather than the -errno convention used
 * elsewhere in this header, so the two stay drop-in compatible.
 *
 * @param data    Fields to encode.
 * @param buf     Output buffer.
 * @param buf_len Capacity of @p buf.
 * @return NRFMODULE_TRACKER_ADV_PAYLOAD_LEN on success.
 * @return 0 if @p data or @p buf is NULL, or @p buf_len is smaller than
 *         NRFMODULE_TRACKER_ADV_PAYLOAD_LEN.
 */
size_t nrfmodule_tracker_adv_encode(const struct nrfmodule_tracker_adv_data *data,
				     uint8_t *buf, size_t buf_len);

/**
 * @brief Decode a tracker v2 status broadcast.
 *
 * @param buf     Manufacturer-data element value, company id first. Longer
 *                buffers are tolerated (future tail fields ignored).
 * @param buf_len Length of @p buf.
 * @param out     Decoded fields on success; untouched on failure.
 * @return 0 on success.
 * @return -EINVAL if @p buf or @p out is NULL, @p buf_len is shorter than
 *         NRFMODULE_TRACKER_ADV_PAYLOAD_LEN, the company id does not match
 *         NRFMODULE_ADV_COMPANY_ID_NRFMODULE, or protocol_version is below
 *         NRFMODULE_TRACKER_ADV_PROTO_VERSION.
 */
int nrfmodule_tracker_adv_decode(const uint8_t *buf, size_t buf_len,
				  struct nrfmodule_tracker_adv_data *out);

/* ------------------------------------------------------------------------
 * BeeScales hive-scale broadcast (company 0x0E34, reserved word == 0)
 * ------------------------------------------------------------------------ */

/* company_code(2) + reserved(2) + type_field(2) + 7 wire fields (16 bytes:
 * 6 x int16 + 1 x int32)
 */
#define NRFMODULE_BEESCALES_ADV_PAYLOAD_MIN_LEN (22)
#define NRFMODULE_BEESCALES_ADV_RESERVED        (0x0000) /* disambiguates vs. tracker */

/** Decoded fields of one BeeScales hive-scale broadcast. */
struct nrfmodule_beescales_adv_data {
	uint16_t type_field; /**< raw product/type code (wire offset 4-5); 0x0001 in every
			       *   firmware build audited for this codec.
			       */
	int16_t  battery_percent; /**< 0-100 percent. Filled by battery_level_percent()
				    *   (beescales_app.c), which the producer calls with
				    *   the address of this uint16_t field cast down to
				    *   uint8_t* -- only the low byte is written, so the
				    *   wire value is the plain percent, not a scaled
				    *   quantity.
				    */
	int16_t  weight_10g;           /**< current weight, 10 g units (0.01 kg); the load
					 *   cell measures grams and the firmware divides
					 *   by 10 before packing this field.
					 */
	int16_t  delta_today_10g;      /**< today's weight delta, 10 g units (0.01 kg). */
	int16_t  delta_yesterday_10g;  /**< yesterday's weight delta, 10 g units (0.01 kg). */
	int16_t  temperature_1_cdegc;  /**< beacon-1 temperature, 0.01 degC; INT16_MIN = no
					 *   beacon bound.
					 */
	int16_t  temperature_2_cdegc;  /**< beacon-2 temperature, 0.01 degC; INT16_MIN = no
					 *   beacon bound.
					 */
	int32_t  unix_timestamp;       /**< weight sample's unix time, signed (matches the
					 *   firmware's int32_t; wraps in 2038).
					 */
};

/**
 * @brief Decode a BeeScales hive-scale broadcast.
 *
 * The BeeScales firmware carries this payload in the manufacturer-data
 * element of its SCAN RESPONSE, not the primary advertising PDU -- pass the
 * scan-response element value here, company id first.
 *
 * @param buf     Manufacturer-data element value, company id first. Longer
 *                buffers are tolerated (the wire struct's 22-byte user_data
 *                array carries 6 spare trailing zero bytes).
 * @param buf_len Length of @p buf.
 * @param out     Decoded fields on success; untouched on failure.
 * @return 0 on success.
 * @return -EINVAL if @p buf or @p out is NULL, @p buf_len is shorter than
 *         NRFMODULE_BEESCALES_ADV_PAYLOAD_MIN_LEN, the company id does not
 *         match NRFMODULE_ADV_COMPANY_ID_NRFMODULE, or the reserved word
 *         (offset 2-3) is not NRFMODULE_BEESCALES_ADV_RESERVED.
 */
int nrfmodule_beescales_adv_decode(const uint8_t *buf, size_t buf_len,
				    struct nrfmodule_beescales_adv_data *out);

/* ------------------------------------------------------------------------
 * Ingics iBS05 temperature beacon (company 0x082C)
 * ------------------------------------------------------------------------ */

/* company(2) + code_and_type(2) + battery(2) + event(1) + temp(2) + 4 unused + sub_type(1) */
#define NRFMODULE_IBS05_ADV_PAYLOAD_MIN_LEN (14)
#define NRFMODULE_IBS05_CODE_AND_TYPE       (0xBC83) /* Ingics device/type code for iBS05 */
#define NRFMODULE_IBS05_SUB_TYPE_SENSOR     (0x32)   /* Ingics frame-variant marker iBS05 uses */

/** event_type values (wire offset 6). Any other value decodes as "not pressed". */
enum nrfmodule_ibs05_event {
	NRFMODULE_IBS05_EVENT_NORMAL = 0,
	NRFMODULE_IBS05_EVENT_BUTTON = 1,
};

/** Decoded fields of one Ingics iBS05 temperature-beacon advertisement. */
struct nrfmodule_ibs05_adv_data {
	int16_t battery_cv;        /**< battery voltage, 0.01 V units (centivolts). */
	int16_t temperature_cdegc; /**< temperature, 0.01 degC units. */
	bool    button_pressed;    /**< event_type == NRFMODULE_IBS05_EVENT_BUTTON. */
};

/**
 * @brief Decode an Ingics iBS05 temperature-beacon advertisement.
 *
 * Validates company id, the iBS05 device/type code and the sensor-frame
 * marker byte before decoding, the same three fields the reference firmware
 * uses to recognize this beacon model (BeeScalesFirmware
 * src/ble/ble_beacons/ble_beacons.c: is_ibs05_beacon() and
 * ibs05_parse_adv_data()).
 *
 * @param buf     Manufacturer-data element value, company id first.
 * @param buf_len Length of @p buf.
 * @param out     Decoded fields on success; untouched on failure.
 * @return 0 on success.
 * @return -EINVAL if @p buf or @p out is NULL, @p buf_len is shorter than
 *         NRFMODULE_IBS05_ADV_PAYLOAD_MIN_LEN, the company id does not match
 *         NRFMODULE_ADV_COMPANY_ID_INGICS, or the code_and_type / sub_type
 *         fields do not match an iBS05 (NRFMODULE_IBS05_CODE_AND_TYPE /
 *         NRFMODULE_IBS05_SUB_TYPE_SENSOR).
 */
int nrfmodule_ibs05_adv_decode(const uint8_t *buf, size_t buf_len,
				struct nrfmodule_ibs05_adv_data *out);

/* ------------------------------------------------------------------------
 * Top-level classify/decode
 * ------------------------------------------------------------------------ */

/* Under company 0x0E34, this byte disambiguates tracker from BeeScales: a
 * tracker protocol_version (>= NRFMODULE_TRACKER_ADV_PROTO_VERSION) or the
 * low byte of BeeScales's all-zero reserved word.
 */
#define NRFMODULE_ADV_DISAMBIGUATOR_OFFSET    (2)
#define NRFMODULE_ADV_DISAMBIGUATOR_BEESCALES (0x00)

/** Tagged union of every payload this codec can decode. */
struct nrfmodule_adv_decoded {
	enum nrfmodule_adv_device_type type;
	union {
		struct nrfmodule_tracker_adv_data   tracker;
		struct nrfmodule_beescales_adv_data beescales;
		struct nrfmodule_ibs05_adv_data     ibs05;
	} data;
};

/**
 * @brief Classify and decode a manufacturer-data payload.
 *
 * Disambiguates the two company-0x0E34 layouts using @p payload offset
 * NRFMODULE_ADV_DISAMBIGUATOR_OFFSET: a tracker protocol_version (>=
 * NRFMODULE_TRACKER_ADV_PROTO_VERSION) routes to the tracker decoder, a
 * BeeScales reserved byte (NRFMODULE_ADV_DISAMBIGUATOR_BEESCALES) routes to
 * the BeeScales decoder. Any other value under 0x0E34, or any company id
 * besides NRFMODULE_ADV_COMPANY_ID_NRFMODULE / NRFMODULE_ADV_COMPANY_ID_INGICS,
 * is unknown and rejected.
 *
 * @param company_id   BLE company identifier the caller already extracted
 *                      while iterating AD structures. Cross-checked against
 *                      the id embedded in @p payload's own leading 2 bytes;
 *                      a mismatch is treated as a malformed call, not an
 *                      unknown device.
 * @param payload       Manufacturer-data element value, company id first --
 *                      same convention as the per-type decoders above.
 * @param payload_len   Length of @p payload.
 * @param out           Decoded result. out->type is NRFMODULE_ADV_DEVICE_UNKNOWN
 *                      and out->data is untouched on failure.
 * @return 0 on success.
 * @return -EINVAL if @p payload or @p out is NULL, @p payload_len is too
 *         short to read the company id, or @p company_id does not match the
 *         id embedded in @p payload.
 * @return -ENOTSUP if the company id / disambiguator byte do not match any
 *         known layout, or the matched per-type decoder rejects the payload.
 */
int nrfmodule_adv_decode(uint16_t company_id, const uint8_t *payload, size_t payload_len,
			  struct nrfmodule_adv_decoded *out);

#endif /* NRFMODULE_ADV_CODEC_H_ */
