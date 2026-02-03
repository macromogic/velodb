if ! command -v conda &> /dev/null; then
    echo "Error: conda is not available"
    exit 1
fi

if ! conda env list | grep -q "^velodb "; then
    echo "Creating conda environment 'velodb'..."
    conda create -y -n velodb python=3.12
    conda run -n velodb pip install seaborn pandas==2.3.3 duckdb
fi

eval "$(conda shell.bash hook)"
conda activate velodb
