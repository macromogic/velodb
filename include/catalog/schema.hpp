#pragma once

#include "types/data_type.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace velodb {

class Column {
public:
    Column(std::string name, std::unique_ptr<DataType> type, bool nullable = true);
    ~Column() = default;

    // Move constructor and assignment
    Column(Column&& other) noexcept;
    Column& operator=(Column&& other) noexcept;

    // Delete copy constructor and assignment
    Column(const Column&) = delete;
    Column& operator=(const Column&) = delete;

    [[nodiscard]] const std::string& getName() const { return name_; }
    [[nodiscard]] const DataType& getType() const { return *type_; }
    [[nodiscard]] bool isNullable() const { return nullable_; }
    [[nodiscard]] size_t getOffset() const { return offset_; }
    void setOffset(size_t offset) { offset_ = offset; }

    // Create a deep copy of this column
    [[nodiscard]] Column clone() const;

    [[nodiscard]] std::string toString() const;

private:
    std::string name_;
    std::unique_ptr<DataType> type_;
    bool nullable_;
    size_t offset_ { 0 }; // Offset in tuple for fixed-size columns
};

class Schema {
public:
    Schema() = default;
    explicit Schema(std::vector<Column> columns);
    ~Schema() = default;

    // Move constructor and assignment
    Schema(Schema&& other) noexcept = default;
    Schema& operator=(Schema&& other) noexcept = default;

    // Delete copy constructor and assignment
    Schema(const Schema&) = delete;
    Schema& operator=(const Schema&) = delete;

    void addColumn(Column column);
    const Column& getColumn(size_t index) const;
    const Column& getColumn(const std::string& name) const;
    size_t getColumnIndex(const std::string& name) const;
    size_t getColumnCount() const { return columns_.size(); }

    bool hasColumn(const std::string& name) const;
    size_t getTupleSize() const { return tuple_size_; }

    // Create a deep copy of this schema
    std::unique_ptr<Schema> clone() const;

    std::string toString() const;

    // Iterator support
    auto begin() const { return columns_.begin(); }
    auto end() const { return columns_.end(); }

private:
    void computeOffsets();

    std::vector<Column> columns_;
    std::unordered_map<std::string, size_t> column_name_to_index_;
    size_t tuple_size_ = 0;
};

} // namespace velodb
