#include <omnicore/test/utils_tx.h>

#include <omnicore/createpayload.h>
#include <omnicore/encoding.h>
#include <omnicore/omnicore.h>
#include <omnicore/parsing.h>
#include <omnicore/rules.h>
#include <omnicore/script.h>
#include <omnicore/tx.h>

#include <base58.h>
#include <coins.h>
#include <key_io.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/test_bitcoin.h>

#include <stdint.h>
#include <limits>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(omnicore_parsing_a_tests, BasicTestingSetup)

/** Creates a dummy transaction with the given inputs and outputs. */
static CTransaction TxClassA(const std::vector<CTxOut>& txInputs, const std::vector<CTxOut>& txOuts)
{
    CMutableTransaction mutableTx;

    // Inputs:
    for (const auto& txOut : txInputs)
    {
        // Create transaction for input:
        CMutableTransaction inputTx;
        inputTx.vout.push_back(txOut);
        CTransaction tx(inputTx);

        // Populate transaction cache:
        Coin newcoin;
        newcoin.out.scriptPubKey = txOut.scriptPubKey;
        newcoin.out.nValue = txOut.nValue;
        view.AddCoin(COutPoint(tx.GetHash(), 0), std::move(newcoin), true);

        // Add input:
        CTxIn txIn(tx.GetHash(), 0);
        mutableTx.vin.push_back(txIn);
    }

    for (std::vector<CTxOut>::const_iterator it = txOuts.begin(); it != txOuts.end(); ++it)
    {
        const CTxOut& txOut = *it;
        mutableTx.vout.push_back(txOut);
    }

    return CTransaction(mutableTx);
}

/** Helper to create a CTxOut object. */
static CTxOut createTxOut(int64_t amount, const std::string& dest)
{
    return CTxOut(amount, GetScriptForDestination(DecodeDestination(dest)));
}

BOOST_AUTO_TEST_CASE(valid_class_a)
{
    {
        int nBlock = 0;

        std::vector<CTxOut> txInputs;
        txInputs.push_back(createTxOut(1765000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));
        txInputs.push_back(createTxOut(50000, "6git9Fx3a7RpuV52jWPxdnEdb542tJNyMu"));

        std::vector<CTxOut> txOutputs;
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(createTxOut(6000, "6qDfvh53TmFJopWAz3Mw1FeXjdu4NhqwzB"));
        txOutputs.push_back(createTxOut(6000, "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W"));
        txOutputs.push_back(createTxOut(1747000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        CTransaction dummyTx = TxClassA(txInputs, txOutputs);

        CMPTransaction metaTx;
        BOOST_CHECK_EQUAL(ParseTransaction(dummyTx, nBlock, 1, metaTx), 0);
        BOOST_CHECK_EQUAL(metaTx.getFeePaid(), 50000);
        BOOST_CHECK_EQUAL(metaTx.getSender(), "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST");
        BOOST_CHECK_EQUAL(metaTx.getReceiver(), "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W");
        BOOST_CHECK_EQUAL(metaTx.getPayload(), "000000000000000100000002540be400000000");
    }
    {
        int nBlock = 0;

        std::vector<CTxOut> txInputs;
        txInputs.push_back(createTxOut(907500, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));
        txInputs.push_back(createTxOut(907500, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        std::vector<CTxOut> txOutputs;
        txOutputs.push_back(createTxOut(6000, "6qDfvh53TmFJowfaFDGp7cnZKatrKT6kBA"));
        txOutputs.push_back(createTxOut(6000, "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W"));
        txOutputs.push_back(createTxOut(1747000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));

        CTransaction dummyTx = TxClassA(txInputs, txOutputs);

        CMPTransaction metaTx;
        BOOST_CHECK(ParseTransaction(dummyTx, nBlock, 1, metaTx) == 0);
        BOOST_CHECK_EQUAL(metaTx.getFeePaid(), 50000);
        BOOST_CHECK_EQUAL(metaTx.getSender(), "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST");
        BOOST_CHECK_EQUAL(metaTx.getReceiver(), "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W");
        BOOST_CHECK_EQUAL(metaTx.getPayload(), "000000000000000200000002540be400000000");
    }
    {
        int nBlock = std::numeric_limits<int>::max();

        std::vector<CTxOut> txInputs;
        txInputs.push_back(createTxOut(1815000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        std::vector<CTxOut> txOutputs;
        txOutputs.push_back(NonStandardOutput());
        txOutputs.push_back(NonStandardOutput());
        txOutputs.push_back(NonStandardOutput());
        txOutputs.push_back(NonStandardOutput());
        txOutputs.push_back(NonStandardOutput());
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(PayToPubKey_Unrelated());
        txOutputs.push_back(PayToPubKey_Unrelated());
        txOutputs.push_back(PayToPubKey_Unrelated());
        txOutputs.push_back(createTxOut(6000, "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W"));
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(OpReturn_Unrelated());
        txOutputs.push_back(OpReturn_Unrelated());
        txOutputs.push_back(createTxOut(6000, "6qDfvh53TmFJopWAz3Mw1FeXjdu4NhqwzB"));
        txOutputs.push_back(createTxOut(1747000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        CTransaction dummyTx = TxClassA(txInputs, txOutputs);

        CMPTransaction metaTx;
        BOOST_CHECK(ParseTransaction(dummyTx, nBlock, 1, metaTx) == 0);
        BOOST_CHECK_EQUAL(metaTx.getSender(), "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST");
        BOOST_CHECK_EQUAL(metaTx.getReceiver(), "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W");
        BOOST_CHECK_EQUAL(metaTx.getPayload(), "000000000000000100000002540be400000000");
    }
    {
        int nBlock = 0;

        std::vector<CTxOut> txInputs;
        txInputs.push_back(createTxOut(87000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        std::vector<CTxOut> txOutputs;
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(createTxOut(6000, "6v3ujrksZ7mLigo2WU7MT1kHr4XumMtqeq"));
        txOutputs.push_back(createTxOut(6000, "6uxd4fdZ8wXeCPXaxxDohSn1afeTYEaxVc"));
        txOutputs.push_back(createTxOut(7000, "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W"));
        txOutputs.push_back(createTxOut(7000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        CTransaction dummyTx = TxClassA(txInputs, txOutputs);

        CMPTransaction metaTx;
        BOOST_CHECK(ParseTransaction(dummyTx, nBlock, 1, metaTx) == 0);
        BOOST_CHECK_EQUAL(metaTx.getFeePaid(), 55000);
        BOOST_CHECK_EQUAL(metaTx.getSender(), "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST");
        BOOST_CHECK_EQUAL(metaTx.getReceiver(), "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST");
        BOOST_CHECK_EQUAL(metaTx.getPayload(), "000000000000000100000002540be400000000");
    }
    {
        int nBlock = ConsensusParams().SCRIPTHASH_BLOCK;

        std::vector<CTxOut> txInputs;
        txInputs.push_back(createTxOut(100000, "34xhWktMFEGRTRmWf1hdNn1SyDDWiXa18H"));
        txInputs.push_back(createTxOut(100000, "34xhWktMFEGRTRmWf1hdNn1SyDDWiXa18H"));
        txInputs.push_back(createTxOut(200000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));
        txInputs.push_back(createTxOut(100000, "34xhWktMFEGRTRmWf1hdNn1SyDDWiXa18H"));
        txInputs.push_back(createTxOut(200000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        std::vector<CTxOut> txOutputs;
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(createTxOut(6000, "6v3ujrksZ7mLigo2WU7MT1kHr4XumMtqeq"));
        txOutputs.push_back(createTxOut(6000, "6v9CR3tByJ13FUi56feRe3GgTFQXbfENGN"));
        txOutputs.push_back(createTxOut(6001, "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W"));
        txOutputs.push_back(createTxOut(665999, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        CTransaction dummyTx = TxClassA(txInputs, txOutputs);

        CMPTransaction metaTx;
        BOOST_CHECK(ParseTransaction(dummyTx, nBlock, 1, metaTx) == 0);
        BOOST_CHECK_EQUAL(metaTx.getFeePaid(), 10000);
        BOOST_CHECK_EQUAL(metaTx.getSender(), "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST");
        BOOST_CHECK_EQUAL(metaTx.getReceiver(), "6v9CR3tByJ13FUi56feRe3GgTFQXbfENGN");
        BOOST_CHECK_EQUAL(metaTx.getPayload(), "000000000000000100000002540be400000000");
    }
    {
        int nBlock = 0;

        std::vector<CTxOut> txInputs;
        txInputs.push_back(createTxOut(70000, "6eNTWs4Frm9A91FK53hggxU2vw8pdiE3zF"));

        std::vector<CTxOut> txOutputs;
        txOutputs.push_back(createTxOut(9001, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(createTxOut(9001, "6eNTWs4Frm9A91FK53hggxU2vw8pdiE3zF"));
        txOutputs.push_back(createTxOut(9001, "6eNTWs4Frm9A9kDie5AyM9JCSd7aGQ5tVa"));

        CTransaction dummyTx = TxClassA(txInputs, txOutputs);

        CMPTransaction metaTx;
        BOOST_CHECK(ParseTransaction(dummyTx, nBlock, 1, metaTx) == 0);
        BOOST_CHECK_EQUAL(metaTx.getSender(), "6eNTWs4Frm9A91FK53hggxU2vw8pdiE3zF");
        BOOST_CHECK_EQUAL(metaTx.getReceiver(), "6eNTWs4Frm9A9kDie5AyM9JCSd7aGQ5tVa");
        BOOST_CHECK_EQUAL(metaTx.getPayload(), "00000000000000010000000777777700000000");
    }
    {
        int nBlock = ConsensusParams().SCRIPTHASH_BLOCK;

        std::vector<CTxOut> txInputs;
        txInputs.push_back(createTxOut(1815000, "3Kpeeo8MVoYnx7PeNb5FUus8bkJsZFPbw7"));

        std::vector<CTxOut> txOutputs;
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(createTxOut(6001, "344VVXmhPVTYnaNYNj3xgkcy3wasEEZtur"));
        txOutputs.push_back(createTxOut(6002, "34CxDzguRHnxA6fwccVBfC3wCZfvwmHAxV"));
        txOutputs.push_back(createTxOut(6003, "3J7F31dxvHXWqTse4rjzS7XayWJnr5fZqW"));

        CTransaction dummyTx = TxClassA(txInputs, txOutputs);

        CMPTransaction metaTx;
        BOOST_CHECK(ParseTransaction(dummyTx, nBlock, 1, metaTx) == 0);
        BOOST_CHECK_EQUAL(metaTx.getSender(), "3Kpeeo8MVoYnx7PeNb5FUus8bkJsZFPbw7");
        BOOST_CHECK_EQUAL(metaTx.getReceiver(), "34CxDzguRHnxA6fwccVBfC3wCZfvwmHAxV");
        BOOST_CHECK_EQUAL(metaTx.getPayload(), "000000000000000200000002540be400000000");
    }
}

BOOST_AUTO_TEST_CASE(invalid_class_a)
{
    // More than one data packet
    {
        int nBlock = 0;

        std::vector<CTxOut> txInputs;
        txInputs.push_back(createTxOut(1815000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        std::vector<CTxOut> txOutputs;
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(createTxOut(6000, "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W"));
        txOutputs.push_back(createTxOut(6000, "6qDfvh53TmFJopWAz3Mw1FeXjdu4NhqwzB"));
        txOutputs.push_back(createTxOut(6000, "6qDfvh53TmFJopWAz3Mw1FeXjdu4NhqwzB"));
        txOutputs.push_back(createTxOut(1747000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        CTransaction dummyTx = TxClassA(txInputs, txOutputs);

        CMPTransaction metaTx;
        BOOST_CHECK(ParseTransaction(dummyTx, nBlock, 1, metaTx) != 0);
    }
    // Not MSC or TMSC
    {
        int nBlock = 0;

        std::vector<CTxOut> txInputs;
        txInputs.push_back(createTxOut(1815000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        std::vector<CTxOut> txOutputs;
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(createTxOut(6000, "6qDfvh53TmFJpZUaZ4qDfSUhFKsowzcQ6G"));
        txOutputs.push_back(createTxOut(6000, "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W"));
        txOutputs.push_back(createTxOut(1747000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        CTransaction dummyTx = TxClassA(txInputs, txOutputs);

        CMPTransaction metaTx;
        BOOST_CHECK(ParseTransaction(dummyTx, nBlock, 1, metaTx) != 0);
    }
    // Seq collision
    {
        int nBlock = 0;

        std::vector<CTxOut> txInputs;
        txInputs.push_back(createTxOut(1815000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        std::vector<CTxOut> txOutputs;
        txOutputs.push_back(createTxOut(6000, "LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
        txOutputs.push_back(createTxOut(6000, "6v3ujrksZ7mLigo2WU7MT1kHr4XumMtqeq"));
        txOutputs.push_back(createTxOut(6000, "6v9CR3tByJ13FUi56feRe3GgTFQXbfENGN"));
        txOutputs.push_back(createTxOut(6000, "6qMhVN4gUP4B3fp4hyDN2MNW97TfHMX42W"));
        txOutputs.push_back(createTxOut(1747000, "6vBuAESPceiDMvbqRzvN2Jbho4JwXcUTST"));

        CTransaction dummyTx = TxClassA(txInputs, txOutputs);

        CMPTransaction metaTx;
        BOOST_CHECK(ParseTransaction(dummyTx, nBlock, 1, metaTx) != 0);
    }
}


BOOST_AUTO_TEST_SUITE_END()
