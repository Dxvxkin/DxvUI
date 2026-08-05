#include <DxvUI/UIBinding.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace DxvUI;

// --- Test Fixture for UIBinding ---
class UIBindingTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Setup can be used for common test resources
    }

    void TearDown() override {
        // Teardown to clean up resources
    }
};

// --- Basic Functionality Tests ---

TEST_F(UIBindingTest, CreationAndInitialValue) {
    auto binding = UIBinding::create(42);
    ASSERT_TRUE(binding->getInt().has_value());
    EXPECT_EQ(binding->getInt().value(), 42);
}

TEST_F(UIBindingTest, SetAndGetValue) {
    auto binding = UIBinding::create("initial");
    ASSERT_TRUE(binding->getString().has_value());
    EXPECT_EQ(binding->getString().value(), "initial");

    binding->set("new_value");
    ASSERT_TRUE(binding->getString().has_value());
    EXPECT_EQ(binding->getString().value(), "new_value");
}

// --- Type Conversion Tests ---

TEST_F(UIBindingTest, GetIntFromString) {
    auto binding = UIBinding::create(std::string("123"));
    ASSERT_TRUE(binding->getInt().has_value());
    EXPECT_EQ(binding->getInt().value(), 123);
}

TEST_F(UIBindingTest, GetFloatFromString) {
    auto binding = UIBinding::create(std::string("123.45"));
    ASSERT_TRUE(binding->getFloat().has_value());
    EXPECT_FLOAT_EQ(binding->getFloat().value(), 123.45f);
}

TEST_F(UIBindingTest, GetStringFromInt) {
    auto binding = UIBinding::create(42);
    ASSERT_TRUE(binding->getString().has_value());
    EXPECT_EQ(binding->getString().value(), "42");
}

TEST_F(UIBindingTest, GetBoolFromString) {
    auto binding_true = UIBinding::create(std::string("true"));
    ASSERT_TRUE(binding_true->getBool().has_value());
    EXPECT_TRUE(binding_true->getBool().value());

    auto binding_false = UIBinding::create(std::string("0"));
    ASSERT_TRUE(binding_false->getBool().has_value());
    EXPECT_FALSE(binding_false->getBool().value());
}

TEST_F(UIBindingTest, InvalidConversionReturnsNullopt) {
    auto binding = UIBinding::create(std::string("not_a_number"));
    EXPECT_FALSE(binding->getInt().has_value());
    EXPECT_FALSE(binding->getFloat().has_value());
}

// --- Subscription and Notification Tests ---

TEST_F(UIBindingTest, SubscriberIsNotified) {
    auto binding = UIBinding::create(10);
    int received_value = 0;
    int call_count = 0;

    auto connection = binding->subscribe(
        [&](const UIBinding& bind) {
            received_value = bind.getInt().value();
            call_count++;
        },
        false);

    binding->set(20);
    EXPECT_EQ(call_count, 1);
    EXPECT_EQ(received_value, 20);

    binding->set(30);
    EXPECT_EQ(call_count, 2);
    EXPECT_EQ(received_value, 30);
}

TEST_F(UIBindingTest, NoNotificationIfValueIsUnchanged) {
    auto binding = UIBinding::create(10);
    int call_count = 0;

    auto connection = binding->subscribe([&](const UIBinding&) { call_count++; }, false);

    binding->set(10);  // Set the same value
    EXPECT_EQ(call_count, 0);
}

TEST_F(UIBindingTest, UnsubscribeViaConnectionDestruction) {
    auto binding = UIBinding::create(10);
    int call_count = 0;

    {
        auto connection = binding->subscribe([&](const UIBinding&) { call_count++; }, false);
        binding->set(20);
        EXPECT_EQ(call_count, 1);
    }  // `connection` is destroyed here, unsubscribing

    binding->set(30);
    EXPECT_EQ(call_count, 1);  // Should not have increased
}

// --- Thread Safety Stress Test ---

TEST_F(UIBindingTest, ThreadSafetyStressTest) {
    auto binding = UIBinding::create(0);
    const int num_threads = 8;
    const int num_iterations = 1000;
    std::atomic<int> read_mismatches = 0;

    std::vector<std::thread> threads;

    // Writer thread
    threads.emplace_back([&]() {
        for (int i = 1; i <= num_iterations; ++i) {
            binding->set(i);
        }
    });

    // Reader threads
    for (int i = 0; i < num_threads - 1; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < num_iterations; ++j) {
                auto val_opt = binding->getInt();
                if (val_opt.has_value()) {
                    // We can't check for exact value due to races,
                    // but we can check if the value is within a valid range.
                    int val = val_opt.value();
                    if (val < 0 || val > num_iterations) {
                        read_mismatches++;
                    }
                } else {
                    // This might happen if the value is a string during conversion
                    // but in this test it shouldn't.
                    read_mismatches++;
                }
                // Small sleep to allow writer to progress
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(read_mismatches.load(), 0);
    ASSERT_TRUE(binding->getInt().has_value());
    EXPECT_EQ(binding->getInt().value(), num_iterations);
}
