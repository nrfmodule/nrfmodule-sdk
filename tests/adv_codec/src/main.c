/*
 * Copyright (c) 2026 nRFModule
 * SPDX-License-Identifier: Apache-2.0
 *
 * Golden vectors GV1-GV4 are copied verbatim from
 * nRFTrackerFW docs/ble/advertising.md. BeeScales vectors are derived from
 * BeeScalesFirmware src/application/beacon_handler.c (wire packing) and
 * advertising_converter/advertising_convert.py (the reference decode), with
 * the company/reserved/type_field header prepended per this codec's buffer
 * convention. iBS05 vectors are derived from BeeScalesFirmware
 * src/ble/ble_beacons/ble_beacons.c (detection + parse offsets), translated
 * from raw-AD-buffer offsets to manufacturer-data-element offsets.
 */

#include <zephyr/ztest.h>
#include <zephyr/sys/byteorder.h>

#include <errno.h>
#include <string.h>

#include <nrfmodule_adv_codec.h>

/* Wire offset of the 2-byte LE company id shared by every payload below. */
#define TEST_ADV_OFFSET_COMPANY_ID (0)

/* Wire offset of the tracker v2 event_flags byte (advertising.md offset 6). */
#define TEST_TRACKER_OFFSET_FLAGS (6)

/* ---- Tracker v2 golden vectors (advertising.md) -------------------------- */

static const uint8_t gv1[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN] = {
	0x34, 0x0E, 0x02, 0x04, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t gv2[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN] = {
	0x34, 0x0E, 0x02, 0x04, 0x55, 0x03, 0x07, 0x09, 0x04, 0x03, 0x02,
	0x01, 0xF4, 0xF3, 0xF2, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t gv3[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN] = {
	0x34, 0x0E, 0x02, 0x04, 0x3C, 0x04, 0x40, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12, 0x03,
};

static const uint8_t gv4[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN] = {
	0x34, 0x0E, 0x02, 0x04, 0x5A, 0x02, 0x22, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xBB, 0xAA, 0x66, 0x00,
};

/* ---- BeeScales vectors ---------------------------------------------------- */

/* Header (company=0x0E34, reserved=0x0000, type_field=0x0001) + the 16-byte
 * user_data example from advertising_convert.py. advertising_convert.py
 * itself scales weight/delta as 0.1 kg (wrong); the wire unit is really
 * 10 g / 0.01 kg (weight_manager.c measures grams, beacon_handler.c divides
 * by 10). At that scale: battery=96, weight=-6.25kg, delta_today=1.3kg,
 * delta_yesterday=0.1kg, temp1=temp2=INT16_MIN (no beacon bound),
 * timestamp=1763728020.
 */
static const uint8_t beescales_v1[NRFMODULE_BEESCALES_ADV_PAYLOAD_MIN_LEN] = {
	0x34, 0x0E, 0x00, 0x00, 0x01, 0x00, 0x60, 0x00, 0x8F, 0xFD, 0x82,
	0x00, 0x0A, 0x00, 0x00, 0x80, 0x00, 0x80, 0x94, 0x5A, 0x20, 0x69,
};

/* Hand-built second vector: battery=85, weight=4.52kg, delta_today=0.03kg,
 * delta_yesterday=-0.10kg, temp1=21.34degC, temp2=INT16_MIN, timestamp=1e8.
 */
static const uint8_t beescales_v2[NRFMODULE_BEESCALES_ADV_PAYLOAD_MIN_LEN] = {
	0x34, 0x0E, 0x00, 0x00, 0x01, 0x00, 0x55, 0x00, 0xC4, 0x01, 0x03,
	0x00, 0xF6, 0xFF, 0x56, 0x08, 0x00, 0x80, 0x00, 0xE1, 0xF5, 0x05,
};

/* ---- iBS05 vectors ---------------------------------------------------------- */

/* company=0x082C, code_and_type=0xBC83, battery=300 (3.00V), event=NORMAL,
 * temperature=2150 (21.50 degC), sub_type=0x32.
 */
static const uint8_t ibs05_v1[NRFMODULE_IBS05_ADV_PAYLOAD_MIN_LEN] = {
	0x2C, 0x08, 0x83, 0xBC, 0x2C, 0x01, 0x00, 0x66, 0x08,
	0x00, 0x00, 0x00, 0x00, 0x32,
};

/* Same beacon, button pressed: battery=280 (2.80V), event=BUTTON,
 * temperature=-557 (-5.57 degC).
 */
static const uint8_t ibs05_v2[NRFMODULE_IBS05_ADV_PAYLOAD_MIN_LEN] = {
	0x2C, 0x08, 0x83, 0xBC, 0x18, 0x01, 0x01, 0xD3, 0xFD,
	0x00, 0x00, 0x00, 0x00, 0x32,
};

ZTEST_SUITE(adv_codec, NULL, NULL, NULL, NULL, NULL);

/* ---- Tracker: encode matches the golden bytes exactly --------------------- */

ZTEST(adv_codec, test_tracker_encode_gv1)
{
	const struct nrfmodule_tracker_adv_data data = {
		.product_id = NRFMODULE_TRACKER_ADV_PRODUCT_LIVETRACKER,
		.battery_percent = NRFMODULE_TRACKER_ADV_BATTERY_UNKNOWN,
		.status = 1,
	};
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];

	zassert_equal(nrfmodule_tracker_adv_encode(&data, buf, sizeof(buf)),
		      NRFMODULE_TRACKER_ADV_PAYLOAD_LEN, "GV1 encode length");
	zassert_mem_equal(buf, gv1, sizeof(gv1), "GV1 encode bytes");
}

ZTEST(adv_codec, test_tracker_encode_gv2)
{
	const struct nrfmodule_tracker_adv_data data = {
		.product_id = NRFMODULE_TRACKER_ADV_PRODUCT_LIVETRACKER,
		.battery_percent = 0x55,
		.status = 3,
		.gps_fix = true,
		.lte_connected = true,
		.charging = true,
		.gps_satellites = 9,
		.latitude_e7 = 0x01020304,
		.longitude_e7 = (int32_t)0xF1F2F3F4,
	};
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];

	zassert_equal(nrfmodule_tracker_adv_encode(&data, buf, sizeof(buf)),
		      NRFMODULE_TRACKER_ADV_PAYLOAD_LEN, "GV2 encode length");
	zassert_mem_equal(buf, gv2, sizeof(gv2), "GV2 encode bytes");
}

ZTEST(adv_codec, test_tracker_encode_gv3)
{
	const struct nrfmodule_tracker_adv_data data = {
		.product_id = NRFMODULE_TRACKER_ADV_PRODUCT_LIVETRACKER,
		.battery_percent = 0x3C,
		.status = 4,
		.wake_kind = NRFMODULE_TRACKER_ADV_WAKE_KIND_SLEEP,
		.next_wake_unix = 0x12345678,
		.last_error = NRFMODULE_TRACKER_ADV_ERR_UPLOAD,
	};
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];

	zassert_equal(nrfmodule_tracker_adv_encode(&data, buf, sizeof(buf)),
		      NRFMODULE_TRACKER_ADV_PAYLOAD_LEN, "GV3 encode length");
	zassert_mem_equal(buf, gv3, sizeof(gv3), "GV3 encode bytes");
}

ZTEST(adv_codec, test_tracker_encode_gv4)
{
	const struct nrfmodule_tracker_adv_data data = {
		.product_id = NRFMODULE_TRACKER_ADV_PRODUCT_LIVETRACKER,
		.battery_percent = 0x5A,
		.status = 2,
		.lte_connected = true,
		.wake_kind = NRFMODULE_TRACKER_ADV_WAKE_KIND_BEGIN,
		.next_wake_unix = 0x66AABBCC,
	};
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];

	zassert_equal(nrfmodule_tracker_adv_encode(&data, buf, sizeof(buf)),
		      NRFMODULE_TRACKER_ADV_PAYLOAD_LEN, "GV4 encode length");
	zassert_mem_equal(buf, gv4, sizeof(gv4), "GV4 encode bytes");
}

ZTEST(adv_codec, test_tracker_encode_rejects_short_buffer)
{
	const struct nrfmodule_tracker_adv_data data = { 0 };
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN - 1];

	zassert_equal(nrfmodule_tracker_adv_encode(&data, buf, sizeof(buf)), 0,
		      "short buffer rejected");
}

ZTEST(adv_codec, test_tracker_encode_rejects_null_data)
{
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];

	zassert_equal(nrfmodule_tracker_adv_encode(NULL, buf, sizeof(buf)), 0,
		      "NULL data rejected");
}

ZTEST(adv_codec, test_tracker_encode_rejects_null_buf)
{
	const struct nrfmodule_tracker_adv_data data = { 0 };

	zassert_equal(nrfmodule_tracker_adv_encode(&data, NULL, NRFMODULE_TRACKER_ADV_PAYLOAD_LEN),
		      0, "NULL buf rejected");
}

/* ---- Tracker: encode/decode round trips ------------------------------------ */

ZTEST(adv_codec, test_tracker_encode_decode_low_batt_button_hub)
{
	/* advertising.md's golden vectors never combine LOW_BATT, BUTTON and
	 * wake_kind HUB in one flags byte -- pin that combination directly.
	 */
	const struct nrfmodule_tracker_adv_data data = {
		.product_id = NRFMODULE_TRACKER_ADV_PRODUCT_LIVETRACKER,
		.low_battery = true,
		.button_event = true,
		.wake_kind = NRFMODULE_TRACKER_ADV_WAKE_KIND_HUB,
	};
	const uint8_t expect_flags =
		(uint8_t)(NRFMODULE_TRACKER_ADV_FLAG_LOW_BATT | NRFMODULE_TRACKER_ADV_FLAG_BUTTON |
			  (NRFMODULE_TRACKER_ADV_WAKE_KIND_HUB
			   << NRFMODULE_TRACKER_ADV_WAKE_KIND_SHIFT));
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];
	struct nrfmodule_tracker_adv_data decoded;

	zassert_equal(nrfmodule_tracker_adv_encode(&data, buf, sizeof(buf)),
		      NRFMODULE_TRACKER_ADV_PAYLOAD_LEN, "encode length");
	zassert_equal(buf[TEST_TRACKER_OFFSET_FLAGS], expect_flags, "flags byte on the wire");

	zassert_equal(nrfmodule_tracker_adv_decode(buf, sizeof(buf), &decoded), 0, "decode ok");
	zassert_true(decoded.low_battery, "low_battery");
	zassert_true(decoded.button_event, "button_event");
	zassert_false(decoded.gps_fix, "gps_fix stays false");
	zassert_false(decoded.lte_connected, "lte_connected stays false");
	zassert_false(decoded.charging, "charging stays false");
	zassert_equal(decoded.wake_kind, NRFMODULE_TRACKER_ADV_WAKE_KIND_HUB, "wake_kind HUB");
}

ZTEST(adv_codec, test_tracker_encode_decode_full_struct_round_trip)
{
	const struct nrfmodule_tracker_adv_data data = {
		.product_id = NRFMODULE_TRACKER_ADV_PRODUCT_PIGEONTRACKER,
		.battery_percent = 42,
		.status = 5,
		.gps_satellites = 12,
		.latitude_e7 = -123456789,
		.longitude_e7 = 987654321,
		.next_wake_unix = 0xDEADBEEF,
		.wake_kind = NRFMODULE_TRACKER_ADV_WAKE_KIND_SLEEP,
		.last_error = NRFMODULE_TRACKER_ADV_ERR_STORAGE,
		.gps_fix = true,
		.lte_connected = true,
		.charging = true,
		.low_battery = true,
		.button_event = true,
	};
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];
	struct nrfmodule_tracker_adv_data decoded;

	zassert_equal(nrfmodule_tracker_adv_encode(&data, buf, sizeof(buf)),
		      NRFMODULE_TRACKER_ADV_PAYLOAD_LEN, "encode length");
	zassert_equal(nrfmodule_tracker_adv_decode(buf, sizeof(buf), &decoded), 0, "decode ok");

	zassert_equal(decoded.product_id, data.product_id, "product_id");
	zassert_equal(decoded.battery_percent, data.battery_percent, "battery_percent");
	zassert_equal(decoded.status, data.status, "status");
	zassert_equal(decoded.gps_satellites, data.gps_satellites, "gps_satellites");
	zassert_equal(decoded.latitude_e7, data.latitude_e7, "latitude_e7");
	zassert_equal(decoded.longitude_e7, data.longitude_e7, "longitude_e7");
	zassert_equal(decoded.next_wake_unix, data.next_wake_unix, "next_wake_unix");
	zassert_equal(decoded.wake_kind, data.wake_kind, "wake_kind");
	zassert_equal(decoded.last_error, data.last_error, "last_error");
	zassert_equal(decoded.gps_fix, data.gps_fix, "gps_fix");
	zassert_equal(decoded.lte_connected, data.lte_connected, "lte_connected");
	zassert_equal(decoded.charging, data.charging, "charging");
	zassert_equal(decoded.low_battery, data.low_battery, "low_battery");
	zassert_equal(decoded.button_event, data.button_event, "button_event");
}

/* ---- Tracker: decode matches golden struct values -------------------------- */

ZTEST(adv_codec, test_tracker_decode_gv1)
{
	struct nrfmodule_tracker_adv_data data;

	zassert_equal(nrfmodule_tracker_adv_decode(gv1, sizeof(gv1), &data), 0, "GV1 decode ok");
	zassert_equal(data.product_id, NRFMODULE_TRACKER_ADV_PRODUCT_LIVETRACKER, "product");
	zassert_equal(data.battery_percent, NRFMODULE_TRACKER_ADV_BATTERY_UNKNOWN, "battery");
	zassert_equal(data.status, 1, "status");
	zassert_false(data.gps_fix, "gps_fix");
	zassert_equal(data.gps_satellites, 0, "sats");
	zassert_equal(data.latitude_e7, 0, "lat");
	zassert_equal(data.longitude_e7, 0, "lon");
	zassert_equal(data.next_wake_unix, 0, "wake");
	zassert_equal(data.wake_kind, NRFMODULE_TRACKER_ADV_WAKE_KIND_NONE, "wake_kind");
	zassert_equal(data.last_error, NRFMODULE_TRACKER_ADV_ERR_NONE, "err");
}

ZTEST(adv_codec, test_tracker_decode_gv2)
{
	struct nrfmodule_tracker_adv_data data;

	zassert_equal(nrfmodule_tracker_adv_decode(gv2, sizeof(gv2), &data), 0, "GV2 decode ok");
	zassert_equal(data.battery_percent, 0x55, "battery");
	zassert_equal(data.status, 3, "status");
	zassert_true(data.gps_fix, "gps_fix");
	zassert_true(data.lte_connected, "lte_connected");
	zassert_true(data.charging, "charging");
	zassert_false(data.low_battery, "low_battery");
	zassert_false(data.button_event, "button_event");
	zassert_equal(data.gps_satellites, 9, "sats");
	zassert_equal(data.latitude_e7, 0x01020304, "lat");
	zassert_equal(data.longitude_e7, (int32_t)0xF1F2F3F4, "lon");
	zassert_equal(data.wake_kind, NRFMODULE_TRACKER_ADV_WAKE_KIND_NONE, "wake_kind");
}

ZTEST(adv_codec, test_tracker_decode_gv3)
{
	struct nrfmodule_tracker_adv_data data;

	zassert_equal(nrfmodule_tracker_adv_decode(gv3, sizeof(gv3), &data), 0, "GV3 decode ok");
	zassert_equal(data.battery_percent, 0x3C, "battery");
	zassert_equal(data.status, 4, "status");
	zassert_false(data.gps_fix, "gps_fix");
	zassert_equal(data.wake_kind, NRFMODULE_TRACKER_ADV_WAKE_KIND_SLEEP, "wake_kind");
	zassert_equal(data.next_wake_unix, 0x12345678, "wake");
	zassert_equal(data.last_error, NRFMODULE_TRACKER_ADV_ERR_UPLOAD, "err");
}

ZTEST(adv_codec, test_tracker_decode_gv4)
{
	struct nrfmodule_tracker_adv_data data;

	zassert_equal(nrfmodule_tracker_adv_decode(gv4, sizeof(gv4), &data), 0, "GV4 decode ok");
	zassert_equal(data.battery_percent, 0x5A, "battery");
	zassert_equal(data.status, 2, "status");
	zassert_false(data.gps_fix, "gps_fix");
	zassert_true(data.lte_connected, "lte_connected");
	zassert_equal(data.wake_kind, NRFMODULE_TRACKER_ADV_WAKE_KIND_BEGIN, "wake_kind");
	zassert_equal(data.next_wake_unix, 0x66AABBCC, "wake");
	zassert_equal(data.last_error, NRFMODULE_TRACKER_ADV_ERR_NONE, "err");
}

ZTEST(adv_codec, test_tracker_decode_rejects_short_buffer)
{
	struct nrfmodule_tracker_adv_data data;

	zassert_equal(nrfmodule_tracker_adv_decode(gv1, NRFMODULE_TRACKER_ADV_PAYLOAD_LEN - 1,
						    &data),
		      -EINVAL, "one byte short rejected");
}

ZTEST(adv_codec, test_tracker_decode_rejects_wrong_company_id)
{
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];
	struct nrfmodule_tracker_adv_data data;

	memcpy(buf, gv1, sizeof(buf));
	buf[0] = 0x34;
	buf[1] = 0x12; /* company id becomes 0x1234 */

	zassert_equal(nrfmodule_tracker_adv_decode(buf, sizeof(buf), &data), -EINVAL,
		      "wrong company id rejected");
}

ZTEST(adv_codec, test_tracker_decode_rejects_old_protocol_version)
{
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];
	struct nrfmodule_tracker_adv_data data;

	memcpy(buf, gv1, sizeof(buf));
	buf[2] = 0x01; /* protocol_version 0x01 predates this codec */

	zassert_equal(nrfmodule_tracker_adv_decode(buf, sizeof(buf), &data), -EINVAL,
		      "old protocol_version rejected");
}

ZTEST(adv_codec, test_tracker_decode_rejects_null)
{
	struct nrfmodule_tracker_adv_data data;

	zassert_equal(nrfmodule_tracker_adv_decode(NULL, sizeof(gv1), &data), -EINVAL,
		      "NULL buf rejected");
	zassert_equal(nrfmodule_tracker_adv_decode(gv1, sizeof(gv1), NULL), -EINVAL,
		      "NULL out rejected");
}

/* ---- BeeScales decode ------------------------------------------------------- */

ZTEST(adv_codec, test_beescales_decode_v1)
{
	struct nrfmodule_beescales_adv_data data;

	zassert_equal(nrfmodule_beescales_adv_decode(beescales_v1, sizeof(beescales_v1), &data),
		      0, "v1 decode ok");
	zassert_equal(data.type_field, 0x0001, "type_field");
	zassert_equal(data.battery_percent, 96, "battery");
	zassert_equal(data.weight_10g, -625, "weight");
	zassert_equal(data.delta_today_10g, 130, "delta_today");
	zassert_equal(data.delta_yesterday_10g, 10, "delta_yesterday");
	zassert_equal(data.temperature_1_cdegc, INT16_MIN, "temp1 unbound");
	zassert_equal(data.temperature_2_cdegc, INT16_MIN, "temp2 unbound");
	zassert_equal(data.unix_timestamp, 1763728020, "timestamp");
}

ZTEST(adv_codec, test_beescales_decode_v2)
{
	struct nrfmodule_beescales_adv_data data;

	zassert_equal(nrfmodule_beescales_adv_decode(beescales_v2, sizeof(beescales_v2), &data),
		      0, "v2 decode ok");
	zassert_equal(data.battery_percent, 85, "battery");
	zassert_equal(data.weight_10g, 452, "weight");
	zassert_equal(data.delta_today_10g, 3, "delta_today");
	zassert_equal(data.delta_yesterday_10g, -10, "delta_yesterday");
	zassert_equal(data.temperature_1_cdegc, 2134, "temp1");
	zassert_equal(data.temperature_2_cdegc, INT16_MIN, "temp2 unbound");
	zassert_equal(data.unix_timestamp, 100000000, "timestamp");
}

ZTEST(adv_codec, test_beescales_decode_rejects_short_buffer)
{
	struct nrfmodule_beescales_adv_data data;

	zassert_equal(nrfmodule_beescales_adv_decode(
			      beescales_v1, NRFMODULE_BEESCALES_ADV_PAYLOAD_MIN_LEN - 1, &data),
		      -EINVAL, "one byte short rejected");
}

ZTEST(adv_codec, test_beescales_decode_rejects_wrong_company_id)
{
	uint8_t buf[NRFMODULE_BEESCALES_ADV_PAYLOAD_MIN_LEN];
	struct nrfmodule_beescales_adv_data data;

	memcpy(buf, beescales_v1, sizeof(buf));
	buf[0] = 0x2C;
	buf[1] = 0x08; /* company id becomes 0x082C (Ingics) */

	zassert_equal(nrfmodule_beescales_adv_decode(buf, sizeof(buf), &data), -EINVAL,
		      "wrong company id rejected");
}

ZTEST(adv_codec, test_beescales_decode_rejects_nonzero_reserved)
{
	uint8_t buf[NRFMODULE_BEESCALES_ADV_PAYLOAD_MIN_LEN];
	struct nrfmodule_beescales_adv_data data;

	memcpy(buf, beescales_v1, sizeof(buf));
	buf[2] = 0x01; /* reserved word no longer 0x0000 */

	zassert_equal(nrfmodule_beescales_adv_decode(buf, sizeof(buf), &data), -EINVAL,
		      "nonzero reserved rejected");
}

/* ---- iBS05 decode ------------------------------------------------------------ */

ZTEST(adv_codec, test_ibs05_decode_v1_normal)
{
	struct nrfmodule_ibs05_adv_data data;

	zassert_equal(nrfmodule_ibs05_adv_decode(ibs05_v1, sizeof(ibs05_v1), &data), 0,
		      "v1 decode ok");
	zassert_equal(data.battery_cv, 300, "battery 3.00V");
	zassert_equal(data.temperature_cdegc, 2150, "temperature 21.50C");
	zassert_false(data.button_pressed, "no button event");
}

ZTEST(adv_codec, test_ibs05_decode_v2_button_pressed)
{
	struct nrfmodule_ibs05_adv_data data;

	zassert_equal(nrfmodule_ibs05_adv_decode(ibs05_v2, sizeof(ibs05_v2), &data), 0,
		      "v2 decode ok");
	zassert_equal(data.battery_cv, 280, "battery 2.80V");
	zassert_equal(data.temperature_cdegc, -557, "temperature -5.57C");
	zassert_true(data.button_pressed, "button event");
}

ZTEST(adv_codec, test_ibs05_decode_rejects_truncated_buffer)
{
	struct nrfmodule_ibs05_adv_data data;

	/* Drop the trailing sub_type byte -- exactly the bug class the
	 * reference ble_beacons.c parser has (unchecked absolute offsets).
	 */
	zassert_equal(nrfmodule_ibs05_adv_decode(ibs05_v1, sizeof(ibs05_v1) - 1, &data), -EINVAL,
		      "truncated buffer rejected");
}

ZTEST(adv_codec, test_ibs05_decode_rejects_wrong_company_id)
{
	uint8_t buf[NRFMODULE_IBS05_ADV_PAYLOAD_MIN_LEN];
	struct nrfmodule_ibs05_adv_data data;

	memcpy(buf, ibs05_v1, sizeof(buf));
	buf[0] = 0x34;
	buf[1] = 0x0E; /* company id becomes 0x0E34 (nRFModule) */

	zassert_equal(nrfmodule_ibs05_adv_decode(buf, sizeof(buf), &data), -EINVAL,
		      "wrong company id rejected");
}

ZTEST(adv_codec, test_ibs05_decode_rejects_wrong_sub_type)
{
	uint8_t buf[NRFMODULE_IBS05_ADV_PAYLOAD_MIN_LEN];
	struct nrfmodule_ibs05_adv_data data;

	memcpy(buf, ibs05_v1, sizeof(buf));
	buf[13] = 0x00; /* not the 0x32 sensor-frame marker */

	zassert_equal(nrfmodule_ibs05_adv_decode(buf, sizeof(buf), &data), -EINVAL,
		      "wrong sub_type rejected");
}

ZTEST(adv_codec, test_ibs05_decode_rejects_wrong_code_and_type)
{
	uint8_t buf[NRFMODULE_IBS05_ADV_PAYLOAD_MIN_LEN];
	struct nrfmodule_ibs05_adv_data data;

	memcpy(buf, ibs05_v1, sizeof(buf));
	buf[2] = 0x00;
	buf[3] = 0x00; /* code_and_type no longer 0xBC83 */

	zassert_equal(nrfmodule_ibs05_adv_decode(buf, sizeof(buf), &data), -EINVAL,
		      "wrong code_and_type rejected");
}

/* ---- Top-level classify/decode ---------------------------------------------- */

ZTEST(adv_codec, test_classify_tracker)
{
	struct nrfmodule_adv_decoded out;

	zassert_equal(nrfmodule_adv_decode(NRFMODULE_ADV_COMPANY_ID_NRFMODULE, gv2, sizeof(gv2),
					    &out),
		      0, "classify tracker ok");
	zassert_equal(out.type, NRFMODULE_ADV_DEVICE_TRACKER, "type tracker");
	zassert_equal(out.data.tracker.battery_percent, 0x55, "battery");
}

ZTEST(adv_codec, test_classify_beescales)
{
	struct nrfmodule_adv_decoded out;

	zassert_equal(nrfmodule_adv_decode(NRFMODULE_ADV_COMPANY_ID_NRFMODULE, beescales_v1,
					    sizeof(beescales_v1), &out),
		      0, "classify beescales ok");
	zassert_equal(out.type, NRFMODULE_ADV_DEVICE_BEESCALES, "type beescales");
	zassert_equal(out.data.beescales.weight_10g, -625, "weight");
}

ZTEST(adv_codec, test_classify_ibs05)
{
	struct nrfmodule_adv_decoded out;

	zassert_equal(nrfmodule_adv_decode(NRFMODULE_ADV_COMPANY_ID_INGICS, ibs05_v2,
					    sizeof(ibs05_v2), &out),
		      0, "classify ibs05 ok");
	zassert_equal(out.type, NRFMODULE_ADV_DEVICE_IBS05, "type ibs05");
	zassert_true(out.data.ibs05.button_pressed, "button event");
}

ZTEST(adv_codec, test_classify_rejects_unknown_company_id)
{
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];
	struct nrfmodule_adv_decoded out;
	const uint16_t unknown_company_id = 0x1234;

	/* Embedded id must match the argument, or this hits the mismatch path
	 * (test_classify_rejects_company_id_mismatch) instead of "unknown".
	 */
	memcpy(buf, gv1, sizeof(buf));
	sys_put_le16(unknown_company_id, &buf[TEST_ADV_OFFSET_COMPANY_ID]);

	zassert_equal(nrfmodule_adv_decode(unknown_company_id, buf, sizeof(buf), &out), -ENOTSUP,
		      "unknown company id rejected");
	zassert_equal(out.type, NRFMODULE_ADV_DEVICE_UNKNOWN, "type stays unknown");
}

ZTEST(adv_codec, test_classify_rejects_company_id_mismatch)
{
	struct nrfmodule_adv_decoded out;

	/* gv1's embedded id is 0x0E34; asking for 0x082C is a caller bug. */
	zassert_equal(nrfmodule_adv_decode(NRFMODULE_ADV_COMPANY_ID_INGICS, gv1, sizeof(gv1),
					    &out),
		      -EINVAL, "company id mismatch rejected");
}

ZTEST(adv_codec, test_classify_rejects_unknown_disambiguator)
{
	uint8_t buf[NRFMODULE_TRACKER_ADV_PAYLOAD_LEN];
	struct nrfmodule_adv_decoded out;

	memcpy(buf, gv1, sizeof(buf));
	buf[2] = 0x01; /* neither BeeScales (0x00) nor tracker (>= 0x02) */

	zassert_equal(nrfmodule_adv_decode(NRFMODULE_ADV_COMPANY_ID_NRFMODULE, buf, sizeof(buf),
					    &out),
		      -ENOTSUP, "unknown disambiguator rejected");
	zassert_equal(out.type, NRFMODULE_ADV_DEVICE_UNKNOWN, "type stays unknown");
}

ZTEST(adv_codec, test_classify_rejects_too_short_for_disambiguator)
{
	uint8_t buf[NRFMODULE_ADV_DISAMBIGUATOR_OFFSET];
	struct nrfmodule_adv_decoded out;

	memcpy(buf, gv1, sizeof(buf));

	zassert_equal(nrfmodule_adv_decode(NRFMODULE_ADV_COMPANY_ID_NRFMODULE, buf, sizeof(buf),
					    &out),
		      -ENOTSUP, "too-short payload rejected");
}

ZTEST(adv_codec, test_classify_rejects_payload_too_short_for_company_id)
{
	struct nrfmodule_adv_decoded out;

	zassert_equal(nrfmodule_adv_decode(NRFMODULE_ADV_COMPANY_ID_NRFMODULE, gv1, 0, &out),
		      -EINVAL, "zero-length payload rejected");
	zassert_equal(nrfmodule_adv_decode(NRFMODULE_ADV_COMPANY_ID_NRFMODULE, gv1, 1, &out),
		      -EINVAL, "one-byte payload rejected");
}

ZTEST(adv_codec, test_classify_rejects_null)
{
	struct nrfmodule_adv_decoded out;

	zassert_equal(nrfmodule_adv_decode(NRFMODULE_ADV_COMPANY_ID_NRFMODULE, NULL, sizeof(gv1),
					    &out),
		      -EINVAL, "NULL payload rejected");
	zassert_equal(nrfmodule_adv_decode(NRFMODULE_ADV_COMPANY_ID_NRFMODULE, gv1, sizeof(gv1),
					    NULL),
		      -EINVAL, "NULL out rejected");
}
