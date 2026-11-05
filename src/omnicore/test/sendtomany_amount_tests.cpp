#include <omnicore/omnicore.h>
#include <omnicore/tx.h>

#include <test/util/setup_common.h>

#include <limits>
#include <stdint.h>
#include <tuple>
#include <vector>

#include <boost/test/unit_test.hpp>

typedef std::vector<std::tuple<uint8_t, uint64_t>> StmValues;

static StmValues MakeValues(const std::vector<uint64_t>& amounts)
{
    StmValues values;
    uint8_t output = 0;
    for (uint64_t amount : amounts) {
        values.push_back(std::make_tuple(output++, amount));
    }
    return values;
}

BOOST_FIXTURE_TEST_SUITE(omnicore_sendtomany_amount_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(valid_amounts_sum_exactly)
{
    uint64_t total = 0;
    BOOST_CHECK(CMPTransaction::CheckStmAmounts(MakeValues({1}), total));
    BOOST_CHECK_EQUAL(total, uint64_t(1));
    BOOST_CHECK(CMPTransaction::CheckStmAmounts(MakeValues({100000000, 250000000, 5}), total));
    BOOST_CHECK_EQUAL(total, uint64_t(350000005));
    BOOST_CHECK(CMPTransaction::CheckStmAmounts(MakeValues({MAX_INT_8_BYTES}), total));
    BOOST_CHECK_EQUAL(total, uint64_t(MAX_INT_8_BYTES));
    BOOST_CHECK(CMPTransaction::CheckStmAmounts(MakeValues({MAX_INT_8_BYTES - 1, 1}), total));
    BOOST_CHECK_EQUAL(total, uint64_t(MAX_INT_8_BYTES));
}

BOOST_AUTO_TEST_CASE(empty_list_sums_to_zero)
{
    uint64_t total = 7;
    BOOST_CHECK(CMPTransaction::CheckStmAmounts(MakeValues({}), total));
    BOOST_CHECK_EQUAL(total, uint64_t(0));
}

BOOST_AUTO_TEST_CASE(zero_amount_rejected)
{
    uint64_t total = 5;
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({0}), total));
    BOOST_CHECK_EQUAL(total, uint64_t(0));
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({5, 0, 5}), total));
}

BOOST_AUTO_TEST_CASE(amount_above_int64_rejected)
{
    uint64_t total = 0;
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({uint64_t(MAX_INT_8_BYTES) + 1}), total));
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({std::numeric_limits<uint64_t>::max()}), total));
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({1, std::numeric_limits<uint64_t>::max()}), total));
}

BOOST_AUTO_TEST_CASE(sum_overflow_rejected)
{
    uint64_t total = 0;
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({MAX_INT_8_BYTES, 1}), total));
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({MAX_INT_8_BYTES - 1, 2}), total));
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({1, 2, MAX_INT_8_BYTES - 2}), total));
    BOOST_CHECK_EQUAL(total, uint64_t(0));
}

BOOST_AUTO_TEST_CASE(mainnet_exploit_payloads_rejected)
{
    // TEXITcoin mainnet, 2026-11-04, blocks 378127, 378131 and 378133: each pair
    // sums to 2^64 + 1, which the unchecked uint64 sum wrapped to one token unit.
    uint64_t total = 0;
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({18446738718893895116ULL, 5354815656501ULL}), total));
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({18446742482742551616ULL, 1590967000001ULL}), total));
    BOOST_CHECK(!CMPTransaction::CheckStmAmounts(MakeValues({18446742581709551616ULL, 1492000000001ULL}), total));
    BOOST_CHECK_EQUAL(total, uint64_t(0));
}

BOOST_AUTO_TEST_SUITE_END()
