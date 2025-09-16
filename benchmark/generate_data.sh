#!/bin/bash
# TPC-H Data Generation and Setup Script for VeloDB

set -e

# Configuration
SCALE_FACTOR=${1:-0.01}
DATA_DIR="$(pwd)/benchmark/data"
TPCH_TOOLS_DIR="/tmp/tpch-tools"

echo "VeloDB TPC-H Data Generation Script"
echo "==================================="
echo "Scale Factor: $SCALE_FACTOR"
echo "Data Directory: $DATA_DIR"
echo ""

# Create data directory
mkdir -p "$DATA_DIR"

# Check if data files already exist
if [ -f "$DATA_DIR/customer.tbl" ] && [ -f "$DATA_DIR/lineitem.tbl" ]; then
    echo "TPC-H data files already exist in $DATA_DIR"
    echo "Use 'rm $DATA_DIR/*.tbl' to regenerate"
    exit 0
fi

# Check if TPC-H tools are available
if [ ! -f "$TPCH_TOOLS_DIR/dbgen" ]; then
    echo "TPC-H dbgen tool not found. Attempting to build..."

    # Clone and build TPC-H tools
    if [ ! -d "$TPCH_TOOLS_DIR" ]; then
        echo "Cloning TPC-H tools..."
        git clone https://github.com/electrum/tpch-dbgen.git "$TPCH_TOOLS_DIR" || {
            echo "Error: Failed to clone TPC-H tools"
            echo "Please manually install TPC-H dbgen tool"
            exit 1
        }
    fi

    cd "$TPCH_TOOLS_DIR"
    echo "Building TPC-H tools..."
    make || {
        echo "Error: Failed to build TPC-H tools"
        echo "Please check compilation requirements"
        exit 1
    }

    if [ ! -f "dbgen" ]; then
        echo "Error: dbgen not built successfully"
        exit 1
    fi

    cd - > /dev/null
fi

# Generate TPC-H data
echo "Generating TPC-H data (Scale Factor: $SCALE_FACTOR)..."
cd "$TPCH_TOOLS_DIR"

# Generate data files
./dbgen -s "$SCALE_FACTOR" -f

# Move generated files to data directory
echo "Moving data files to $DATA_DIR..."
mv *.tbl "$DATA_DIR/"

cd - > /dev/null

# Verify generated files
echo ""
echo "Generated TPC-H data files:"
echo "---------------------------"
for table in customer lineitem nation orders part partsupp region supplier; do
    file="$DATA_DIR/${table}.tbl"
    if [ -f "$file" ]; then
        size=$(du -h "$file" | cut -f1)
        lines=$(wc -l < "$file")
        echo "  $table.tbl: $size ($lines rows)"
    else
        echo "  $table.tbl: MISSING"
    fi
done

echo ""
echo "Data generation complete!"
echo ""
echo "To run benchmarks:"
echo "  ./bin/tpch_benchmark --quick                    # Quick test"
echo "  ./bin/tpch_benchmark --scale-factor $SCALE_FACTOR  # Custom scale factor"
echo ""

# Create a simple validation script
cat > "$DATA_DIR/validate_data.sh" << 'EOF'
#!/bin/bash
# Simple validation script for TPC-H data files

echo "TPC-H Data Validation"
echo "===================="

missing_files=0
for table in customer lineitem nation orders part partsupp region supplier; do
    file="${table}.tbl"
    if [ -f "$file" ]; then
        lines=$(wc -l < "$file")
        size=$(du -h "$file" | cut -f1)
        printf "  %-12s: %8d rows (%s)\n" "$table" "$lines" "$size"
    else
        echo "  $table: MISSING"
        ((missing_files++))
    fi
done

echo ""
if [ $missing_files -eq 0 ]; then
    echo "✓ All TPC-H data files present"
else
    echo "✗ $missing_files files missing"
fi
EOF

chmod +x "$DATA_DIR/validate_data.sh"

echo "Created validation script: $DATA_DIR/validate_data.sh"
