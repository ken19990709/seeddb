#ifndef SEEDDB_EXECUTOR_AGGREGATE_H
#define SEEDDB_EXECUTOR_AGGREGATE_H

#include <memory>
#include <unordered_set>
#include <string>
#include <functional>

#include "common/value.h"
#include "common/logical_type.h"

namespace seeddb {

// =============================================================================
// AggregateAccumulator - Abstract Base Class
// =============================================================================

/// Abstract base class for aggregate function accumulators.
/// Each aggregate function has its own accumulator that tracks state during grouping.
class AggregateAccumulator {
public:
    virtual ~AggregateAccumulator() = default;

    /// Accumulate a value into the aggregate state.
    /// @param val The value to accumulate (may be NULL).
    virtual void accumulate(const Value& val) = 0;

    /// Finalize and return the aggregate result.
    /// @return The final aggregate value.
    virtual Value finalize() = 0;

    /// Clone this accumulator (for creating per-group accumulators).
    /// @return A new accumulator with the same type but reset state.
    virtual std::unique_ptr<AggregateAccumulator> clone() const = 0;
};

// =============================================================================
// CountAccumulator - COUNT(*) and COUNT(column)
// =============================================================================

/// Accumulator for COUNT aggregate function.
/// For COUNT(*), counts all rows.
/// For COUNT(column), counts non-NULL values.
class CountAccumulator : public AggregateAccumulator {
public:
    /// @param is_star Whether this is COUNT(*)
    explicit CountAccumulator(bool is_star = false) : is_star_(is_star), count_(0) {}

    void accumulate(const Value& val) override {
        if (is_star_) {
            // COUNT(*) counts all rows regardless of NULL
            ++count_;
        } else {
            // COUNT(column) only counts non-NULL values
            if (!val.isNull()) {
                ++count_;
            }
        }
    }

    Value finalize() override {
        return Value::integer(static_cast<int32_t>(count_));
    }

    std::unique_ptr<AggregateAccumulator> clone() const override {
        return std::make_unique<CountAccumulator>(is_star_);
    }

private:
    bool is_star_;
    int64_t count_;
};

// =============================================================================
// CountDistinctAccumulator - COUNT(DISTINCT column)
// =============================================================================

/// Hash function for Value (for use in unordered_set).
struct ValueHash {
    std::size_t operator()(const Value& val) const {
        if (val.isNull()) {
            return 0;
        }
        switch (val.typeId()) {
            case LogicalTypeId::INTEGER:
                return std::hash<int32_t>{}(val.asInt32());
            case LogicalTypeId::BIGINT:
                return std::hash<int64_t>{}(val.asInt64());
            case LogicalTypeId::FLOAT:
                return std::hash<float>{}(val.asFloat());
            case LogicalTypeId::DOUBLE:
                return std::hash<double>{}(val.asDouble());
            case LogicalTypeId::VARCHAR:
                return std::hash<std::string>{}(val.asString());
            case LogicalTypeId::BOOLEAN:
                return std::hash<bool>{}(val.asBool());
            default:
                return 0;
        }
    }
};

/// Equality function for Value (for use in unordered_set).
struct ValueEqual {
    bool operator()(const Value& a, const Value& b) const {
        return a.equals(b);
    }
};

/// Accumulator for COUNT(DISTINCT column).
/// Uses an unordered_set to track unique non-NULL values.
class CountDistinctAccumulator : public AggregateAccumulator {
public:
    CountDistinctAccumulator() = default;

    void accumulate(const Value& val) override {
        // Only insert non-NULL values
        if (!val.isNull()) {
            unique_values_.insert(val);
        }
    }

    Value finalize() override {
        return Value::integer(static_cast<int32_t>(unique_values_.size()));
    }

    std::unique_ptr<AggregateAccumulator> clone() const override {
        return std::make_unique<CountDistinctAccumulator>();
    }

private:
    std::unordered_set<Value, ValueHash, ValueEqual> unique_values_;
};

// =============================================================================
// SumAccumulator - SUM(column)
// =============================================================================

/// Accumulator for SUM aggregate function.
/// Handles numeric types, uses BIGINT for integer sums to prevent overflow.
class SumAccumulator : public AggregateAccumulator {
public:
    SumAccumulator() = default;

    void accumulate(const Value& val) override {
        if (val.isNull()) {
            return;  // Skip NULL values
        }
        has_value_ = true;

        switch (val.typeId()) {
            case LogicalTypeId::INTEGER:
                sum_ += val.asInt32();
                break;
            case LogicalTypeId::BIGINT:
                sum_ += static_cast<double>(val.asInt64());
                break;
            case LogicalTypeId::FLOAT:
                sum_ += val.asFloat();
                is_float_ = true;
                break;
            case LogicalTypeId::DOUBLE:
                sum_ += val.asDouble();
                is_float_ = true;
                break;
            default:
                // Non-numeric types - should be caught by validation
                break;
        }
    }

    Value finalize() override {
        if (!has_value_) {
            return Value::null();  // SUM of empty set is NULL
        }
        if (is_float_) {
            return Value::Double(sum_);
        }
        // Return BIGINT for integer sums
        return Value::bigint(static_cast<int64_t>(sum_));
    }

    std::unique_ptr<AggregateAccumulator> clone() const override {
        return std::make_unique<SumAccumulator>();
    }

private:
    double sum_ = 0.0;
    bool has_value_ = false;
    bool is_float_ = false;
};

// =============================================================================
// AvgAccumulator - AVG(column)
// =============================================================================

/// Accumulator for AVG aggregate function.
/// Tracks sum and count, always returns DOUBLE.
class AvgAccumulator : public AggregateAccumulator {
public:
    AvgAccumulator() = default;

    void accumulate(const Value& val) override {
        if (val.isNull()) {
            return;  // Skip NULL values
        }
        ++count_;

        switch (val.typeId()) {
            case LogicalTypeId::INTEGER:
                sum_ += val.asInt32();
                break;
            case LogicalTypeId::BIGINT:
                sum_ += static_cast<double>(val.asInt64());
                break;
            case LogicalTypeId::FLOAT:
                sum_ += val.asFloat();
                break;
            case LogicalTypeId::DOUBLE:
                sum_ += val.asDouble();
                break;
            default:
                // Non-numeric types - should be caught by validation
                break;
        }
    }

    Value finalize() override {
        if (count_ == 0) {
            return Value::null();  // AVG of empty set is NULL
        }
        return Value::Double(sum_ / static_cast<double>(count_));
    }

    std::unique_ptr<AggregateAccumulator> clone() const override {
        return std::make_unique<AvgAccumulator>();
    }

private:
    double sum_ = 0.0;
    int64_t count_ = 0;
};

// =============================================================================
// MinAccumulator - MIN(column)
// =============================================================================

/// Accumulator for MIN aggregate function.
/// Works with any comparable type.
class MinAccumulator : public AggregateAccumulator {
public:
    MinAccumulator() = default;

    void accumulate(const Value& val) override {
        if (val.isNull()) {
            return;  // Skip NULL values
        }
        if (!has_value_) {
            min_value_ = val;
            has_value_ = true;
        } else {
            // Compare and update if new value is smaller
            if (val.lessThan(min_value_)) {
                min_value_ = val;
            }
        }
    }

    Value finalize() override {
        if (!has_value_) {
            return Value::null();  // MIN of empty set is NULL
        }
        return min_value_;
    }

    std::unique_ptr<AggregateAccumulator> clone() const override {
        return std::make_unique<MinAccumulator>();
    }

private:
    Value min_value_;
    bool has_value_ = false;
};

// =============================================================================
// MaxAccumulator - MAX(column)
// =============================================================================

/// Accumulator for MAX aggregate function.
/// Works with any comparable type.
class MaxAccumulator : public AggregateAccumulator {
public:
    MaxAccumulator() = default;

    void accumulate(const Value& val) override {
        if (val.isNull()) {
            return;  // Skip NULL values
        }
        if (!has_value_) {
            max_value_ = val;
            has_value_ = true;
        } else {
            // Compare and update if new value is larger
            if (max_value_.lessThan(val)) {
                max_value_ = val;
            }
        }
    }

    Value finalize() override {
        if (!has_value_) {
            return Value::null();  // MAX of empty set is NULL
        }
        return max_value_;
    }

    std::unique_ptr<AggregateAccumulator> clone() const override {
        return std::make_unique<MaxAccumulator>();
    }

private:
    Value max_value_;
    bool has_value_ = false;
};

// =============================================================================
// GroupKey - Composite key for GROUP BY
// =============================================================================

/// GroupKey is a vector of values representing a composite group key.
using GroupKey = std::vector<Value>;

/// Hash function for GroupKey (for use in unordered_map).
struct GroupKeyHash {
    std::size_t operator()(const GroupKey& key) const {
        std::size_t hash = 0;
        ValueHash value_hash;
        for (const auto& val : key) {
            // Combine hashes using XOR and bit rotation
            hash ^= value_hash(val) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

/// Equality function for GroupKey (for use in unordered_map).
struct GroupKeyEqual {
    bool operator()(const GroupKey& a, const GroupKey& b) const {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            if (!a[i].equals(b[i])) {
                return false;
            }
        }
        return true;
    }
};

// =============================================================================
// AggregateState - Per-group accumulator state
// =============================================================================

/// Holds the accumulator state for a single group.
/// Each group has one AggregateState containing all aggregate accumulators.
class AggregateState {
public:
    AggregateState() = default;

    /// Add an accumulator for an aggregate function.
    /// @param acc The accumulator to add.
    void addAccumulator(std::unique_ptr<AggregateAccumulator> acc) {
        accumulators_.push_back(std::move(acc));
    }

    /// Accumulate a value into a specific accumulator.
    /// @param index The accumulator index.
    /// @param val The value to accumulate.
    void accumulate(size_t index, const Value& val) {
        if (index < accumulators_.size()) {
            accumulators_[index]->accumulate(val);
        }
    }

    /// Finalize and return all aggregate results.
    /// @return Vector of finalized values.
    std::vector<Value> finalize() {
        std::vector<Value> results;
        results.reserve(accumulators_.size());
        for (auto& acc : accumulators_) {
            results.push_back(acc->finalize());
        }
        return results;
    }

    /// Clone this state (creates new accumulators with reset state).
    /// @return A new AggregateState with cloned accumulators.
    std::unique_ptr<AggregateState> clone() const {
        auto state = std::make_unique<AggregateState>();
        for (const auto& acc : accumulators_) {
            state->addAccumulator(acc->clone());
        }
        return state;
    }

    /// Get the number of accumulators.
    size_t size() const { return accumulators_.size(); }

private:
    std::vector<std::unique_ptr<AggregateAccumulator>> accumulators_;
};

} // namespace seeddb

#endif // SEEDDB_EXECUTOR_AGGREGATE_H
