#include <omnicore/encoding.h>

#include <omnicore/script.h>

#include <base58.h>
#include <key_io.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/test_bitcoin.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <stdint.h>
#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(omnicore_encoding_b_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(class_b_empty)
{
    const std::string strSeed;
    const CPubKey pubKey;
    const std::vector<unsigned char> vchPayload;

    std::vector<std::pair<CScript, int64_t> > vTxOuts;
    BOOST_CHECK(OmniCore_Encode_ClassB(strSeed, pubKey, vchPayload, vTxOuts));
    BOOST_CHECK_EQUAL(vTxOuts.size(), 1);

    const CScript& scriptPubKey = vTxOuts[0].first;
    CTxDestination dest;
    BOOST_CHECK(ExtractDestination(scriptPubKey, dest));
    BOOST_CHECK_EQUAL(EncodeDestination(dest), "LTceXoduS2cetpWJSe47M25i5oKjEccN1h");
}

BOOST_AUTO_TEST_CASE(class_b_maidsafe)
{
    const std::string strSeed("6vArkJSRKaMrWRFqsvTdKjFAUVCCiABpNB");

    const std::vector<unsigned char> vchPubKey = ParseHex(
        "023a3891f00650b2971ec94383bc6949b672a498baa19b6e3421ccde196ccc64d6");

    const CPubKey pubKey(vchPubKey.begin(), vchPubKey.end());

    const std::vector<unsigned char> vchPayload = ParseHex(
        // Transaction version: 0
        "0000"
        // Transaction type: Create Crowdsale (51)
        "0033"
        // Eco system: Main (1)
        "01"
        // Property type: Indivisible tokens (1)
        "0001"
        // Previous property identifier: None (0)
        "00000000"
        // Category: "Crowdsale"
        "43726f776473616c6500"
        // Sub category: "MaidSafe"
        "4d6169645361666500"
        // Property name: "MaidSafeCoin"
        "4d61696453616665436f696e00"
        // URL: "www.buysafecoins.com"
        "7777772e62757973616665636f696e732e636f6d00"
        // Information: "SAFE Network Crowdsale (MSAFE)"
        "53414645204e6574776f726b2043726f776473616c6520284d534146452900"
        // Desired property: Mastercoin (SP #1)
        "00000001"
        // Amount per unit invested: 3400
        "0000000000000d48"
        // Deadline: Thu, 22 May 2014 09:00:00 UTC (1400749200)
        "00000000537dbc90"
        // Early bird bonus: 10 % per week
        "0a"
        // Percentage for issuer: 0 %
        "00");

    std::vector<std::pair<CScript, int64_t> > vTxOuts;
    BOOST_CHECK(OmniCore_Encode_ClassB(strSeed, pubKey, vchPayload, vTxOuts));
    BOOST_CHECK_EQUAL(vTxOuts.size(), 3);

    const CScript& scriptPubKeyA = vTxOuts[0].first;
    const CScript& scriptPubKeyB = vTxOuts[1].first;
    const CScript& scriptPubKeyC = vTxOuts[2].first;

    txnouttype outtypeA;
    BOOST_CHECK(GetOutputType(scriptPubKeyA, outtypeA));
    BOOST_CHECK_EQUAL(outtypeA, TX_MULTISIG);
    txnouttype outtypeB;
    BOOST_CHECK(GetOutputType(scriptPubKeyB, outtypeB));
    BOOST_CHECK_EQUAL(outtypeB, TX_MULTISIG);
    txnouttype outtypeC;
    BOOST_CHECK(GetOutputType(scriptPubKeyC, outtypeC));
    BOOST_CHECK_EQUAL(outtypeC, TX_PUBKEYHASH);

    std::vector<std::string> vstrSolutions;
    BOOST_CHECK(GetScriptPushes(scriptPubKeyA, vstrSolutions));
    BOOST_CHECK(GetScriptPushes(scriptPubKeyB, vstrSolutions));
    BOOST_CHECK_EQUAL(vstrSolutions.size(), 6);

    // Vout 0
    BOOST_CHECK_EQUAL(vstrSolutions[0],
            "023a3891f00650b2971ec94383bc6949b672a498baa19b6e3421ccde196ccc64d6");
    BOOST_CHECK_EQUAL(vstrSolutions[1].substr(2, 62), // Remove prefix ...
            "7fd7b343383ca5eaef5df96686019f3c7a9099adad462c82f3a05743b20061");
    BOOST_CHECK_EQUAL(vstrSolutions[2].substr(2, 62), // ... and ECDSA byte
            "02bad9e784b341c0f7c4f6688dd217c3c1116c767c053ed1fe4e9cb3fc807c");
    // Vout 1
    BOOST_CHECK_EQUAL(vstrSolutions[3],
            "023a3891f00650b2971ec94383bc6949b672a498baa19b6e3421ccde196ccc64d6");
    BOOST_CHECK_EQUAL(vstrSolutions[4].substr(2, 62), // Because these ...
            "0125898553c44bb40e4aa5d02af1b357d7ab1b62d7e1b1d522d7c154f84efa");
    BOOST_CHECK_EQUAL(vstrSolutions[5].substr(2, 62), // ... are semi-random
            "0c994fe86b2578f2e60c5aa6f19eb88f718a5ca6a723959d83200b360f5101");
}

BOOST_AUTO_TEST_CASE(class_b_tetherus)
{
    // Transaction hash (mainnet):
    // 5ed3694e8a4fa8d3ec5c75eb6789492c69e65511522b220e94ab51da2b6dd53f
    const std::string strSeed("3MbYQMMmSkC3AgWkj9FMo5LsPTW1zBTwXL");

    const std::vector<unsigned char> vchPubKey = ParseHex(
        "04ad90e5b6bc86b3ec7fac2c5fbda7423fc8ef0d58df594c773fa05e2c281b2bfe"
        "877677c668bd13603944e34f4818ee03cadd81a88542b8b4d5431264180e2c28");

    const CPubKey pubKey(vchPubKey.begin(), vchPubKey.end());

    const std::vector<unsigned char> vchPayload = ParseHex(
        "000000360100020000000046696e616e6369616c20616e6420696e737572"
        "616e63652061637469766974696573004163746976697469657320617578"
        "696c6961727920746f2066696e616e6369616c207365727669636520616e"
        "6420696e737572616e636520616374697669746965730054657468657255"
        "530068747470733a2f2f7465746865722e746f00546865206e6578742070"
        "6172616469676d206f66206d6f6e65792e00");

    std::vector<std::pair<CScript, int64_t> > vTxOuts;
    BOOST_CHECK(OmniCore_Encode_ClassB(strSeed, pubKey, vchPayload, vTxOuts));
    BOOST_CHECK_EQUAL(vTxOuts.size(), 4);

    const CScript& scriptPubKeyA = vTxOuts[0].first;
    const CScript& scriptPubKeyB = vTxOuts[1].first;
    const CScript& scriptPubKeyC = vTxOuts[2].first;
    const CScript& scriptPubKeyD = vTxOuts[3].first;

    txnouttype outtypeA;
    BOOST_CHECK(GetOutputType(scriptPubKeyA, outtypeA));
    BOOST_CHECK_EQUAL(outtypeA, TX_MULTISIG);
    txnouttype outtypeB;
    BOOST_CHECK(GetOutputType(scriptPubKeyB, outtypeB));
    BOOST_CHECK_EQUAL(outtypeB, TX_MULTISIG);
    txnouttype outtypeC;
    BOOST_CHECK(GetOutputType(scriptPubKeyC, outtypeC));
    BOOST_CHECK_EQUAL(outtypeC, TX_MULTISIG);
    txnouttype outtypeD;
    BOOST_CHECK(GetOutputType(scriptPubKeyD, outtypeD));
    BOOST_CHECK_EQUAL(outtypeD, TX_PUBKEYHASH);

    std::vector<std::string> vstrSolutions;
    BOOST_CHECK(GetScriptPushes(scriptPubKeyA, vstrSolutions));
    BOOST_CHECK(GetScriptPushes(scriptPubKeyB, vstrSolutions));
    BOOST_CHECK(GetScriptPushes(scriptPubKeyC, vstrSolutions));
    BOOST_CHECK_EQUAL(vstrSolutions.size(), 9);

    // Vout 0
    BOOST_CHECK_EQUAL(vstrSolutions[0], HexStr(pubKey.begin(), pubKey.end()));
    BOOST_CHECK_EQUAL(vstrSolutions[1].substr(2, 62),
            "f88f01791557f6d57e6b7ddf86d2de2117e6cc4ba325a4e309d4a1a55015d7");
    BOOST_CHECK_EQUAL(vstrSolutions[2].substr(2, 62),
            "a94f47f4c3b8c36876399f19ecd61cf452248330fa5da9a1947d6dc7a189a1");
    // Vout 1
    BOOST_CHECK_EQUAL(vstrSolutions[3], HexStr(pubKey.begin(), pubKey.end()));
    BOOST_CHECK_EQUAL(vstrSolutions[4].substr(2, 62),
            "6d7e7235fc2c6769e351196c9ccdc4c804184b5bb9b210f27d3f0a613654fe");
    BOOST_CHECK_EQUAL(vstrSolutions[5].substr(2, 62),
            "8991cff7cc6d93c266615d2a9223cef4d7b11c05c16b0cec12a90ee7b39cf8");
    // Vout 2
    BOOST_CHECK_EQUAL(vstrSolutions[6], HexStr(pubKey.begin(), pubKey.end()));
    BOOST_CHECK_EQUAL(vstrSolutions[7].substr(2, 62),
            "29b3e0919adc41a316aad4f41444d9bf3a9b639550f2aa735676ffff25ba38");
    BOOST_CHECK_EQUAL(vstrSolutions[8].substr(2, 62),
            "f15446771c5c585dd25d8d62df5195b77799aa8eac2f2196c54b73ca05f72f");
}

BOOST_AUTO_TEST_SUITE_END()
