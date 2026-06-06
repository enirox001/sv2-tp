// Copyright (c) 2025 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <boost/test/unit_test.hpp>

#include <test/sv2_mock_mining.h>
#include <test/sv2_test_setup.h>          // Sv2BasicTestingSetup fixture
#include <test/sv2_tp_tester.h>

#include <chrono>
#include <memory>

/*
 * Regression / lifecycle test: construct and destruct TPTester multiple times
 * to ensure clean shutdown of the EventLoop, IPC proxies, and Template Provider.
 * This aims to catch reference counting or lingering thread issues early.
 */
BOOST_FIXTURE_TEST_SUITE(sv2_tester_lifecycle_tests, Sv2BasicTestingSetup)

BOOST_AUTO_TEST_CASE(tp_tester_repeated_construction)
{
    // Run a few iterations; keep count modest to stay fast in CI while
    // still exercising repeated setup/teardown paths.
    constexpr int ITERS = 2;
    for (int i = 0; i < ITERS; ++i) {
        BOOST_TEST_MESSAGE("Lifecycle iteration " << i);
        {
            Sv2TemplateProviderOptions opts;
            opts.is_test = false;
            opts.template_interval = std::chrono::seconds{5};

            auto tester{std::make_unique<TPTester>(opts)};
            // Perform a minimal handshake + setup so the Template Provider
            // allocates resources and creates at least one client connection.
            tester->handshake();

            tester->SendSetupConnection();
            tester->SendCoinbaseOutputConstraints();
            tester->ReceiveTemplatePair();

            BOOST_REQUIRE(tester->m_mining_control->WaitForWaitNext());

            const auto destroy_start{std::chrono::steady_clock::now()};
            tester.reset();
            const auto destroy_elapsed{std::chrono::steady_clock::now() - destroy_start};
            const auto destroy_elapsed_ms{std::chrono::duration_cast<std::chrono::milliseconds>(destroy_elapsed).count()};
            BOOST_CHECK_MESSAGE(destroy_elapsed_ms < 1500, "TPTester teardown took " << destroy_elapsed_ms << "ms");
        }
        // The explicit reset above should cleanly tear down.
        // If any dangling references or threads exist they should surface as
        // test hangs or use-after-frees under sanitizers / valgrind.
    }
}

BOOST_AUTO_TEST_SUITE_END()
