#include "catalog/schema.hpp"
#include <sstream>
#include <stdexcept>
#include <utility>

namespace velodb {

Column::Column(std::string name, std::unique_ptr<DataType> type, bool nullable)
    : name_(std::move(name))
    , type_(std::move(type))
    , nullable_(nullable)
{
}

Column::Column(Column&& other) noexcept
    : name_(std::move(other.name_))
    , type_(std::move(other.type_))
    , nullable_(other.nullable_)
    , offset_(other.offset_)
{
}

Column& Column::operator=(Column&& other) noexcept
{
    if (this != &other) {
        name_ = std::move(other.name_);
        type_ = std::move(other.type_);
        nullable_ = other.nullable_;
        offset_ = other.offset_;
    }
    return *this;
}

Column Column::clone() const
{
    return Column(name_, DataType::createType(type_->getTypeId(), type_->getSize()), nullable_);
}

std::string Column::toString() const
{
    std::stringstream ss;
    ss << name_ << " " << type_->toString();
    if (!nullable_)
        ss << " NOT NULL";
    return ss.str();
}

Schema::Schema(std::vector<Column> columns)
    : columns_(std::move(columns))
{
    computeOffsets();
}

void Schema::addColumn(Column column)
{
    column_name_to_index_[column.getName()] = columns_.size();
    columns_.push_back(std::move(column));
    computeOffsets();
}

const Column& Schema::getColumn(size_t index) const
{
    if (index >= columns_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return columns_[index];
}

const Column& Schema::getColumn(const std::string& name) const
{
    auto it = column_name_to_index_.find(name);
    if (it == column_name_to_index_.end()) {
        throw std::invalid_argument("Column not found: " + name);
    }
    return columns_[it->second];
}

size_t Schema::getColumnIndex(const std::string& name) const
{
    auto it = column_name_to_index_.find(name);
    if (it == column_name_to_index_.end()) {
        throw std::invalid_argument("Column not found: " + name);
    }
    return it->second;
}

bool Schema::hasColumn(const std::string& name) const
{
    return column_name_to_index_.find(name) != column_name_to_index_.end();
}

std::unique_ptr<Schema> Schema::clone() const
{
    std::vector<Column> cloned_columns;
    cloned_columns.reserve(columns_.size());
    for (const auto& column : columns_) {
        cloned_columns.push_back(column.clone());
    }
    return std::make_unique<Schema>(std::move(cloned_columns));
}

std::string Schema::toString() const
{
    std::stringstream ss;
    ss << "(";
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (i > 0)
            ss << ", ";
        ss << columns_[i].toString();
    }
    ss << ")";
    return ss.str();
}

void Schema::computeOffsets()
{
    column_name_to_index_.clear();
    tuple_size_ = 0;

    for (size_t i = 0; i < columns_.size(); ++i) {
        column_name_to_index_[columns_[i].getName()] = i;
        columns_[i].setOffset(tuple_size_);
        if (columns_[i].getType().isFixedSize()) {
            tuple_size_ += columns_[i].getType().getSize();
        } else {
            tuple_size_ += sizeof(void*); // Pointer to variable-length data
        }
    }
}

std::unique_ptr<Schema> Schema::scanFilterSchema()
{
    std::vector<Column> columns;
    columns.emplace_back("._rowid", DataType::createType(DataTypeId::INTEGER), false);
    columns.emplace_back("._mask", DataType::createType(DataTypeId::BOOLEAN), false);
    return std::make_unique<Schema>(std::move(columns));
}

} // namespace velodb
