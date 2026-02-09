#include "orderbook_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  TEST: %s... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* ====================================================================== */

static void test_create_destroy(void) {
    TEST("create and destroy orderbook");
    ob_orderbook_t* book = ob_orderbook_create();
    ASSERT(book != NULL, "create returned NULL");
    ob_orderbook_destroy(book);
    PASS();
}

static void test_empty_book_snapshot(void) {
    TEST("empty book snapshot");
    ob_orderbook_t* book = ob_orderbook_create();
    ob_price_data_t snap = ob_orderbook_get_snapshot(book);
    ASSERT(snap.bid_price == 0, "bid should be 0");
    ASSERT(snap.ask_price == 0, "ask should be 0");
    ASSERT(snap.spread == 0, "spread should be 0");
    ASSERT(snap.mid_price == 0, "mid should be 0");
    ob_orderbook_destroy(book);
    PASS();
}

static void test_add_limit_order(void) {
    TEST("add limit buy order");
    ob_orderbook_t* book = ob_orderbook_create();

    ob_order_t order = {
        .trader_id = "traderA",
        .id = 1,
        .price = 10050,   /* $100.50 */
        .quantity = 100,
        .side = OB_SIDE_BUY,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_ALLOW,
        .has_price = true
    };

    ob_order_result_t* result = ob_orderbook_add_order(book, &order);
    ASSERT(result != NULL, "result is NULL");
    ASSERT(result->accepted == true, "order should be accepted");
    ASSERT(result->trades_len == 0, "no trades expected");
    ASSERT(result->remaining_quantity == 100, "full qty should remain");

    /* Verify book state */
    ASSERT(ob_orderbook_get_best_bid(book) == 10050, "best bid should be 10050");
    ASSERT(ob_orderbook_get_best_ask(book) == 0, "best ask should be 0 (empty)");

    ob_free_order_result(result);
    ob_orderbook_destroy(book);
    PASS();
}

static void test_matching_trade(void) {
    TEST("limit buy matches limit sell -> trade");
    ob_orderbook_t* book = ob_orderbook_create();

    /* Place a sell at $100.50 */
    ob_order_t sell = {
        .trader_id = "seller",
        .id = 1,
        .price = 10050,
        .quantity = 50,
        .side = OB_SIDE_SELL,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_ALLOW,
        .has_price = true
    };
    ob_order_result_t* r1 = ob_orderbook_add_order(book, &sell);
    ASSERT(r1->accepted, "sell should be accepted");
    ASSERT(r1->trades_len == 0, "no trades for resting sell");
    ob_free_order_result(r1);

    /* Place a buy at $100.50 -> should match */
    ob_order_t buy = {
        .trader_id = "buyer",
        .id = 2,
        .price = 10050,
        .quantity = 30,
        .side = OB_SIDE_BUY,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_ALLOW,
        .has_price = true
    };
    ob_order_result_t* r2 = ob_orderbook_add_order(book, &buy);
    ASSERT(r2->accepted, "buy should be accepted");
    ASSERT(r2->trades_len == 1, "should have 1 trade");
    ASSERT(r2->trades[0].quantity == 30, "trade qty should be 30");
    ASSERT(r2->trades[0].price == 10050, "trade price should be 10050");
    ASSERT(r2->trades[0].buy_order_id == 2, "buyer should be order 2");
    ASSERT(r2->trades[0].sell_order_id == 1, "seller should be order 1");
    ASSERT(r2->trades[0].trade_id > 0, "trade should have an ID");
    ASSERT(r2->remaining_quantity == 0, "buy fully filled");

    /* Verify last trade */
    ASSERT(ob_orderbook_get_last_trade_price(book) == 10050, "last trade price");
    ASSERT(ob_orderbook_get_last_trade_qty(book) == 30, "last trade qty");

    ob_free_order_result(r2);
    ob_orderbook_destroy(book);
    PASS();
}

static void test_market_order(void) {
    TEST("market buy against resting sell");
    ob_orderbook_t* book = ob_orderbook_create();

    /* Place sell */
    ob_order_t sell = {
        .trader_id = "seller",
        .id = 1,
        .price = 10000,
        .quantity = 100,
        .side = OB_SIDE_SELL,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_ALLOW,
        .has_price = true
    };
    ob_order_result_t* r1 = ob_orderbook_add_order(book, &sell);
    ob_free_order_result(r1);

    /* Market buy */
    ob_order_t mkt_buy = {
        .trader_id = "buyer",
        .id = 2,
        .price = 0,
        .quantity = 40,
        .side = OB_SIDE_BUY,
        .order_type = OB_ORDER_TYPE_MARKET,
        .time_in_force = OB_TIF_IOC,
        .stp_mode = OB_STP_ALLOW,
        .has_price = false
    };
    ob_order_result_t* r2 = ob_orderbook_add_order(book, &mkt_buy);
    ASSERT(r2->accepted, "market buy should be accepted");
    ASSERT(r2->trades_len == 1, "should have 1 trade");
    ASSERT(r2->trades[0].quantity == 40, "trade qty should be 40");
    ASSERT(r2->remaining_quantity == 0, "fully filled");

    ob_free_order_result(r2);
    ob_orderbook_destroy(book);
    PASS();
}

static void test_cancel_order(void) {
    TEST("cancel existing order");
    ob_orderbook_t* book = ob_orderbook_create();

    ob_order_t order = {
        .trader_id = "traderA",
        .id = 1,
        .price = 10000,
        .quantity = 100,
        .side = OB_SIDE_BUY,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_ALLOW,
        .has_price = true
    };
    ob_order_result_t* r = ob_orderbook_add_order(book, &order);
    ob_free_order_result(r);

    ASSERT(ob_orderbook_cancel_order(book, 1) == true, "cancel should succeed");
    ASSERT(ob_orderbook_get_best_bid(book) == 0, "bid should be empty after cancel");
    ASSERT(ob_orderbook_cancel_order(book, 999) == false, "cancel non-existent should fail");

    ob_orderbook_destroy(book);
    PASS();
}

static void test_modify_order(void) {
    TEST("modify order price and quantity");
    ob_orderbook_t* book = ob_orderbook_create();

    /* Add a sell so we have a spread to test against */
    ob_order_t sell = {
        .trader_id = "seller",
        .id = 10,
        .price = 10500,
        .quantity = 50,
        .side = OB_SIDE_SELL,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_ALLOW,
        .has_price = true
    };
    ob_order_result_t* rs = ob_orderbook_add_order(book, &sell);
    ob_free_order_result(rs);

    /* Add a buy */
    ob_order_t buy = {
        .trader_id = "buyer",
        .id = 1,
        .price = 10000,
        .quantity = 100,
        .side = OB_SIDE_BUY,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_ALLOW,
        .has_price = true
    };
    ob_order_result_t* r = ob_orderbook_add_order(book, &buy);
    ob_free_order_result(r);

    /* Modify quantity */
    ob_modify_result_t* m1 = ob_orderbook_modify_order(book, 1, 10000, 60);
    ASSERT(m1->accepted, "qty modify should succeed");
    ASSERT(m1->old_quantity == 100, "old qty should be 100");
    ASSERT(m1->new_quantity == 60, "new qty should be 60");
    ob_free_modify_result(m1);

    /* Modify price */
    ob_modify_result_t* m2 = ob_orderbook_modify_order(book, 1, 10200, 60);
    ASSERT(m2->accepted, "price modify should succeed");
    ASSERT(m2->old_price == 10000, "old price should be 10000");
    ASSERT(m2->new_price == 10200, "new price should be 10200");
    ASSERT(ob_orderbook_get_best_bid(book) == 10200, "best bid should update");
    ob_free_modify_result(m2);

    /* Modify that would cross spread -> reject */
    ob_modify_result_t* m3 = ob_orderbook_modify_order(book, 1, 10500, 60);
    ASSERT(!m3->accepted, "cross-spread modify should be rejected");
    ASSERT(m3->reject_reason != NULL, "should have reject reason");
    ob_free_modify_result(m3);

    /* Modify non-existent order */
    ob_modify_result_t* m4 = ob_orderbook_modify_order(book, 999, 10000, 50);
    ASSERT(!m4->accepted, "modify non-existent should fail");
    ob_free_modify_result(m4);

    ob_orderbook_destroy(book);
    PASS();
}

static void test_fok_rejection(void) {
    TEST("FOK order rejected for insufficient liquidity");
    ob_orderbook_t* book = ob_orderbook_create();

    /* Place sell of 50 */
    ob_order_t sell = {
        .trader_id = "seller",
        .id = 1,
        .price = 10000,
        .quantity = 50,
        .side = OB_SIDE_SELL,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_ALLOW,
        .has_price = true
    };
    ob_order_result_t* r1 = ob_orderbook_add_order(book, &sell);
    ob_free_order_result(r1);

    /* FOK buy of 100 -> should fail */
    ob_order_t fok_buy = {
        .trader_id = "buyer",
        .id = 2,
        .price = 10000,
        .quantity = 100,
        .side = OB_SIDE_BUY,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_FOK,
        .stp_mode = OB_STP_ALLOW,
        .has_price = true
    };
    ob_order_result_t* r2 = ob_orderbook_add_order(book, &fok_buy);
    ASSERT(!r2->accepted, "FOK should be rejected");
    ASSERT(r2->reject_reason != NULL, "should have reject reason");
    ob_free_order_result(r2);

    ob_orderbook_destroy(book);
    PASS();
}

static void test_stp_cancel_newest(void) {
    TEST("STP CANCEL_NEWEST prevents self-trade");
    ob_orderbook_t* book = ob_orderbook_create();

    /* Place sell from traderA */
    ob_order_t sell = {
        .trader_id = "traderA",
        .id = 1,
        .price = 10000,
        .quantity = 50,
        .side = OB_SIDE_SELL,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_CANCEL_NEWEST,
        .has_price = true
    };
    ob_order_result_t* r1 = ob_orderbook_add_order(book, &sell);
    ob_free_order_result(r1);

    /* Buy from same traderA -> should trigger STP */
    ob_order_t buy = {
        .trader_id = "traderA",
        .id = 2,
        .price = 10000,
        .quantity = 30,
        .side = OB_SIDE_BUY,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_CANCEL_NEWEST,
        .has_price = true
    };
    ob_order_result_t* r2 = ob_orderbook_add_order(book, &buy);
    ASSERT(r2->accepted, "order accepted (STP handled internally)");
    ASSERT(r2->trades_len == 0, "no trades (self-trade prevented)");
    /* CANCEL_NEWEST kills the incoming order: qty goes to 0, not placed on book */
    ASSERT(r2->remaining_quantity == 0, "incoming order killed by STP");
    /* Resting sell should still be on the book */
    ASSERT(ob_orderbook_get_best_ask(book) == 10000, "resting sell survives");

    ob_free_order_result(r2);
    ob_orderbook_destroy(book);
    PASS();
}

static void test_duplicate_order_id(void) {
    TEST("reject duplicate order ID");
    ob_orderbook_t* book = ob_orderbook_create();

    ob_order_t order = {
        .trader_id = "traderA",
        .id = 1,
        .price = 10000,
        .quantity = 100,
        .side = OB_SIDE_BUY,
        .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC,
        .stp_mode = OB_STP_ALLOW,
        .has_price = true
    };
    ob_order_result_t* r1 = ob_orderbook_add_order(book, &order);
    ASSERT(r1->accepted, "first order accepted");
    ob_free_order_result(r1);

    ob_order_result_t* r2 = ob_orderbook_add_order(book, &order);
    ASSERT(!r2->accepted, "duplicate should be rejected");
    ASSERT(r2->reject_reason != NULL, "should have reason");
    ob_free_order_result(r2);

    ob_orderbook_destroy(book);
    PASS();
}

static void test_snapshot_after_trades(void) {
    TEST("snapshot reflects trades");
    ob_orderbook_t* book = ob_orderbook_create();

    ob_order_t sell = {
        .trader_id = "seller", .id = 1, .price = 10100, .quantity = 100,
        .side = OB_SIDE_SELL, .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC, .stp_mode = OB_STP_ALLOW, .has_price = true
    };
    ob_order_t buy = {
        .trader_id = "buyer", .id = 2, .price = 9900, .quantity = 200,
        .side = OB_SIDE_BUY, .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC, .stp_mode = OB_STP_ALLOW, .has_price = true
    };
    ob_order_result_t* r1 = ob_orderbook_add_order(book, &sell);
    ob_order_result_t* r2 = ob_orderbook_add_order(book, &buy);
    ob_free_order_result(r1);
    ob_free_order_result(r2);

    ob_price_data_t snap = ob_orderbook_get_snapshot(book);
    ASSERT(snap.bid_price == 9900, "bid should be 9900");
    ASSERT(snap.ask_price == 10100, "ask should be 10100");
    ASSERT(snap.spread == 200, "spread should be 200 (2.00)");
    ASSERT(snap.mid_price == 10000, "mid should be 10000");

    /* Now cross the spread */
    ob_order_t cross = {
        .trader_id = "crosser", .id = 3, .price = 10100, .quantity = 50,
        .side = OB_SIDE_BUY, .order_type = OB_ORDER_TYPE_LIMIT,
        .time_in_force = OB_TIF_GTC, .stp_mode = OB_STP_ALLOW, .has_price = true
    };
    ob_order_result_t* r3 = ob_orderbook_add_order(book, &cross);
    ASSERT(r3->trades_len == 1, "should have 1 trade");
    ob_free_order_result(r3);

    ASSERT(ob_orderbook_get_last_trade_price(book) == 10100, "last trade at 10100");
    ASSERT(ob_orderbook_get_last_trade_qty(book) == 50, "last trade qty 50");

    ob_orderbook_destroy(book);
    PASS();
}

/* ====================================================================== */

int main(void) {
    printf("=== C FFI Wrapper Tests ===\n\n");

    test_create_destroy();
    test_empty_book_snapshot();
    test_add_limit_order();
    test_matching_trade();
    test_market_order();
    test_cancel_order();
    test_modify_order();
    test_fok_rejection();
    test_stp_cancel_newest();
    test_duplicate_order_id();
    test_snapshot_after_trades();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
