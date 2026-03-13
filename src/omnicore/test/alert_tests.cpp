#include <omnicore/notifications.h>
#include <omnicore/version.h>

#include <util/system.h>
#include <test/test_bitcoin.h>
#include <tinyformat.h>

#include <boost/test/unit_test.hpp>

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(omnicore_alert_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(alert_positive_authorization)
{
    // Confirm authorized sources for mainnet
    BOOST_CHECK(CheckAlertAuthorization("LTceXoduS2cetpWJSe47M25i5oKjEccN1h"));
}

BOOST_AUTO_TEST_CASE(alert_unauthorized_source)
{
    // Confirm unauthorized sources are not accepted
    BOOST_CHECK(!CheckAlertAuthorization("1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T"));
}

BOOST_AUTO_TEST_CASE(alert_manual_sources)
{
    std::vector<std::string> omnialertallowsender = gArgs.GetArgs("-omnialertallowsender");
    std::vector<std::string> omnialertignoresender = gArgs.GetArgs("-omnialertignoresender");

    // Add 1JwSSu as allowed source for alerts
    gArgs.ForceSetArg("-omnialertallowsender", "1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T");
    BOOST_CHECK_EQUAL(gArgs.GetArgs("-omnialertallowsender").size(), 1);
    BOOST_CHECK(CheckAlertAuthorization("1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T"));

    // Then ignore some sources explicitly
    gArgs.ForceSetArgs("-omnialertignoresender", {"1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T", "16oDZYCspsczfgKXVj3xyvsxH21NpEj94F"});
    BOOST_CHECK(CheckAlertAuthorization("LTceXoduS2cetpWJSe47M25i5oKjEccN1h")); // should still be authorized
    BOOST_CHECK(!CheckAlertAuthorization("1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T"));
    BOOST_CHECK(!CheckAlertAuthorization("16oDZYCspsczfgKXVj3xyvsxH21NpEj94F"));

    gArgs.ForceSetArgs("-omnialertallowsender", omnialertallowsender);
    gArgs.ForceSetArgs("-omnialertignoresender", omnialertignoresender);
}

BOOST_AUTO_TEST_CASE(alert_authorize_any_source)
{
    std::vector<std::string> omnialertallowsender = gArgs.GetArgs("-omnialertallowsender");

    // Allow any source (e.g. for tests!)
    gArgs.ForceSetArg("-omnialertallowsender", "any");
    BOOST_CHECK(CheckAlertAuthorization("1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T"));
    BOOST_CHECK(CheckAlertAuthorization("137uFtQ5EgMsreg4FVvL3xuhjkYGToVPqs"));
    BOOST_CHECK(CheckAlertAuthorization("16oDZYCspsczfgKXVj3xyvsxH21NpEj94F"));

    gArgs.ForceSetArgs("-omnialertallowsender", omnialertallowsender);
}

BOOST_AUTO_TEST_SUITE_END()
